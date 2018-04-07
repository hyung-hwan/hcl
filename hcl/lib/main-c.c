/*
 * $Id$
 *
    Copyright (c) 2016-2018 Chung, Hyung-Hwan. All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions
    are met:
    1. Redistributions of source code must retain the above copyright
       notice, this list of conditions and the following disclaimer.
    2. Redistributions in binary form must reproduce the above copyright
       notice, this list of conditions and the following disclaimer in the
       documentation and/or other materials provided with the distribution.

    THIS SOFTWARE IS PROVIDED BY THE AUTHOR "AS IS" AND ANY EXPRESS OR
    IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
    OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
    IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
    INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
    NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
    DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
    THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
    THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "hcl-c.h"
#include "hcl-opt.h"
#include "hcl-utl.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <locale.h>

#if defined(HAVE_TIME_H)
#	include <time.h>
#endif
#if defined(HAVE_SYS_TIME_H)
#	include <sys/time.h>
#endif
#if defined(HAVE_SIGNAL_H)
#	include <signal.h>
#endif
#if defined(HAVE_SYS_UIO_H)
#	include <sys/uio.h>
#endif

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>

/* ========================================================================= */

typedef struct client_xtn_t client_xtn_t;
struct client_xtn_t
{
	int logfd;
	unsigned int logmask;
	int logfd_istty;
	
	struct
	{
		hcl_bch_t buf[4096];
		hcl_oow_t len;
	} logbuf;

	int reply_count;
	hcl_oow_t data_length;
};

/* ========================================================================= */

static void* sys_alloc (hcl_mmgr_t* mmgr, hcl_oow_t size)
{
	return malloc(size);
}

static void* sys_realloc (hcl_mmgr_t* mmgr, void* ptr, hcl_oow_t size)
{
	return realloc(ptr, size);
}

static void sys_free (hcl_mmgr_t* mmgr, void* ptr)
{
	free (ptr);
}

static hcl_mmgr_t sys_mmgr =
{
	sys_alloc,
	sys_realloc,
	sys_free,
	HCL_NULL
};
/* ========================================================================= */

static int write_all (int fd, const hcl_bch_t* ptr, hcl_oow_t len)
{
	while (len > 0)
	{
		hcl_ooi_t wr;

		wr = write(fd, ptr, len);

		if (wr <= -1)
		{
		#if defined(EAGAIN) && defined(EWOULDBLOCK) && (EAGAIN == EWOULDBLOCK)
			if (errno == EAGAIN) continue;
		#else
			#if defined(EAGAIN)
			if (errno == EAGAIN) continue;
			#elif defined(EWOULDBLOCK)
			if (errno == EWOULDBLOCK) continue;
			#endif
		#endif

		#if defined(EINTR)
			/* TODO: would this interfere with non-blocking nature of this VM? */
			if (errno == EINTR) continue;
		#endif
			return -1;
		}

		ptr += wr;
		len -= wr;
	}

	return 0;
}


static int write_log (hcl_client_t* client, int fd, const hcl_bch_t* ptr, hcl_oow_t len)
{
	client_xtn_t* xtn;


	xtn = hcl_client_getxtn(client);

	while (len > 0)
	{
		if (xtn->logbuf.len > 0)
		{
			hcl_oow_t rcapa, cplen;

			rcapa = HCL_COUNTOF(xtn->logbuf.buf) - xtn->logbuf.len;
			cplen = (len >= rcapa)? rcapa: len;

			memcpy (&xtn->logbuf.buf[xtn->logbuf.len], ptr, cplen);
			xtn->logbuf.len += cplen;
			ptr += cplen;
			len -= cplen;

			if (xtn->logbuf.len >= HCL_COUNTOF(xtn->logbuf.buf))
			{
				write_all(fd, xtn->logbuf.buf, xtn->logbuf.len);
				xtn->logbuf.len = 0;
			}
		}
		else
		{
			hcl_oow_t rcapa;

			rcapa = HCL_COUNTOF(xtn->logbuf.buf);
			if (len >= rcapa)
			{
				write_all (fd, ptr, rcapa);
				ptr += rcapa;
				len -= rcapa;
			}
			else
			{
				memcpy (xtn->logbuf.buf, ptr, len);
				xtn->logbuf.len += len;
				ptr += len;
				len -= len;
				
			}
		}
	}

	return 0;
}

static void flush_log (hcl_client_t* client, int fd)
{
	client_xtn_t* xtn;
	xtn = hcl_client_getxtn(client);
	if (xtn->logbuf.len > 0)
	{
		write_all (fd, xtn->logbuf.buf, xtn->logbuf.len);
		xtn->logbuf.len = 0;
	}
}

static void log_write (hcl_client_t* client, unsigned int mask, const hcl_ooch_t* msg, hcl_oow_t len)
{
	hcl_bch_t buf[256];
	hcl_oow_t ucslen, bcslen;
	client_xtn_t* xtn;
	hcl_oow_t msgidx;
	int n, logfd;

	xtn = hcl_client_getxtn(client);

	if (mask & HCL_LOG_STDERR)
	{
		/* the messages that go to STDERR don't get masked out */
		logfd = 2;
	}
	else
	{
		if (!(xtn->logmask & mask & ~HCL_LOG_ALL_LEVELS)) return;  /* check log types */
		if (!(xtn->logmask & mask & ~HCL_LOG_ALL_TYPES)) return;  /* check log levels */

		if (mask & HCL_LOG_STDOUT) logfd = 1;
		else
		{
			logfd = xtn->logfd;
			if (logfd <= -1) return;
		}
	}

/* TODO: beautify the log message.
 *       do classification based on mask. */
	if (!(mask & (HCL_LOG_STDOUT | HCL_LOG_STDERR)))
	{
		time_t now;
		char ts[32];
		size_t tslen;
		struct tm tm, *tmp;

		now = time(NULL);

		tmp = localtime_r (&now, &tm);
		#if defined(HAVE_STRFTIME_SMALL_Z)
		tslen = strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S %z ", tmp);
		#else
		tslen = strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S %Z ", tmp); 
		#endif
		if (tslen == 0) 
		{
			strcpy (ts, "0000-00-00 00:00:00 +0000");
			tslen = 25; 
		}

		write_log (client, logfd, ts, tslen);
	}

	if (logfd == xtn->logfd && xtn->logfd_istty)
	{
		if (mask & HCL_LOG_FATAL) write_log (client, logfd, "\x1B[1;31m", 7);
		else if (mask & HCL_LOG_ERROR) write_log (client, logfd, "\x1B[1;32m", 7);
		else if (mask & HCL_LOG_WARN) write_log (client, logfd, "\x1B[1;33m", 7);
	}

#if defined(HCL_OOCH_IS_UCH)
	msgidx = 0;
	while (len > 0)
	{
		ucslen = len;
		bcslen = HCL_COUNTOF(buf);

		n = hcl_conv_oochars_to_bchars_with_cmgr(&msg[msgidx], &ucslen, buf, &bcslen, hcl_get_utf8_cmgr());
		if (n == 0 || n == -2)
		{
			/* n = 0: 
			 *   converted all successfully 
			 * n == -2: 
			 *    buffer not sufficient. not all got converted yet.
			 *    write what have been converted this round. */

			/*HCL_ASSERT (hcl, ucslen > 0); */ /* if this fails, the buffer size must be increased */
			/*assert (ucslen > 0);*/

			/* attempt to write all converted characters */
			if (write_log(client, logfd, buf, bcslen) <= -1) break;

			if (n == 0) break;
			else
			{
				msgidx += ucslen;
				len -= ucslen;
			}
		}
		else if (n <= -1)
		{
			/* conversion error */
			break;
		}
	}
#else
	write_log (client, logfd, msg, len);
#endif

	if (logfd == xtn->logfd && xtn->logfd_istty)
	{
		if (mask & (HCL_LOG_FATAL | HCL_LOG_ERROR | HCL_LOG_WARN)) write_log (client, logfd, "\x1B[0m", 4);
	}

	flush_log (client, logfd);
}

/* ========================================================================= */

static hcl_client_t* g_client = HCL_NULL;

/* ========================================================================= */

typedef void (*signal_handler_t) (int, siginfo_t*, void*);

static void handle_sigint (int sig, siginfo_t* siginfo, void* ctx)
{
	/*if (g_client) hcl_client_stop (g_client);*/
}

static void set_signal (int sig, signal_handler_t handler)
{
	struct sigaction sa;

	memset (&sa, 0, sizeof(sa));
	/*sa.sa_handler = handler;*/
	sa.sa_flags = SA_SIGINFO;
	sa.sa_sigaction = handler;
	sigemptyset (&sa.sa_mask);

	sigaction (sig, &sa, NULL);
}

static void set_signal_to_ignore (int sig)
{
	struct sigaction sa; 

	memset (&sa, 0, sizeof(sa));
	sa.sa_handler = SIG_IGN;
	sa.sa_flags = 0;
	sigemptyset (&sa.sa_mask);

	sigaction (sig, &sa, NULL);
}

static void set_signal_to_default (int sig)
{
	struct sigaction sa; 

	memset (&sa, 0, sizeof(sa));
	sa.sa_handler = SIG_DFL;
	sa.sa_flags = 0;
	sigemptyset (&sa.sa_mask);

	sigaction (sig, &sa, NULL);
}

/* ========================================================================= */

static int handle_logopt (hcl_client_t* client, const hcl_bch_t* str)
{
	hcl_bch_t* xstr = (hcl_bch_t*)str;
	hcl_bch_t* cm, * flt;
	unsigned int logmask;
	client_xtn_t* xtn;

	xtn = (client_xtn_t*)hcl_client_getxtn(client);

	cm = hcl_find_bchar_in_bcstr(xstr, ',');
	if (cm) 
	{
		/* i duplicate this string for open() below as open() doesn't 
		 * accept a length-bounded string */
		xstr = strdup(str);
		if (!xstr) 
		{
			fprintf (stderr, "ERROR: out of memory in duplicating %s\n", str);
			return -1;
		}

		cm = hcl_find_bchar_in_bcstr(xstr, ',');
		*cm = '\0';

		logmask = xtn->logmask;
		do
		{
			flt = cm + 1;

			cm = hcl_find_bchar_in_bcstr(flt, ',');
			if (cm) *cm = '\0';

			if (hcl_comp_bcstr(flt, "app") == 0) logmask |= HCL_LOG_APP;
			else if (hcl_comp_bcstr(flt, "compiler") == 0) logmask |= HCL_LOG_COMPILER;
			else if (hcl_comp_bcstr(flt, "vm") == 0) logmask |= HCL_LOG_VM;
			else if (hcl_comp_bcstr(flt, "mnemonic") == 0) logmask |= HCL_LOG_MNEMONIC;
			else if (hcl_comp_bcstr(flt, "gc") == 0) logmask |= HCL_LOG_GC;
			else if (hcl_comp_bcstr(flt, "ic") == 0) logmask |= HCL_LOG_IC;
			else if (hcl_comp_bcstr(flt, "primitive") == 0) logmask |= HCL_LOG_PRIMITIVE;

			else if (hcl_comp_bcstr(flt, "fatal") == 0) logmask |= HCL_LOG_FATAL;
			else if (hcl_comp_bcstr(flt, "error") == 0) logmask |= HCL_LOG_ERROR;
			else if (hcl_comp_bcstr(flt, "warn") == 0) logmask |= HCL_LOG_WARN;
			else if (hcl_comp_bcstr(flt, "info") == 0) logmask |= HCL_LOG_INFO;
			else if (hcl_comp_bcstr(flt, "debug") == 0) logmask |= HCL_LOG_DEBUG;

			else if (hcl_comp_bcstr(flt, "fatal+") == 0) logmask |= HCL_LOG_FATAL;
			else if (hcl_comp_bcstr(flt, "error+") == 0) logmask |= HCL_LOG_FATAL | HCL_LOG_ERROR;
			else if (hcl_comp_bcstr(flt, "warn+") == 0) logmask |= HCL_LOG_FATAL | HCL_LOG_ERROR | HCL_LOG_WARN;
			else if (hcl_comp_bcstr(flt, "info+") == 0) logmask |= HCL_LOG_FATAL | HCL_LOG_ERROR | HCL_LOG_WARN | HCL_LOG_INFO;
			else if (hcl_comp_bcstr(flt, "debug+") == 0) logmask |= HCL_LOG_FATAL | HCL_LOG_ERROR | HCL_LOG_WARN | HCL_LOG_INFO | HCL_LOG_DEBUG;

			else
			{
				fprintf (stderr, "ERROR: unknown log option value - %s\n", flt);
				if (str != xstr) free (xstr);
				return -1;
			}
		}
		while (cm);

		if (!(logmask & HCL_LOG_ALL_TYPES)) logmask |= HCL_LOG_ALL_TYPES;  /* no types specified. force to all types */
		if (!(logmask & HCL_LOG_ALL_LEVELS)) logmask |= HCL_LOG_ALL_LEVELS;  /* no levels specified. force to all levels */
	}
	else
	{
		logmask = HCL_LOG_ALL_LEVELS | HCL_LOG_ALL_TYPES;
	}

	xtn->logfd = open(xstr, O_CREAT | O_WRONLY | O_APPEND , 0644);
	if (xtn->logfd == -1)
	{
		fprintf (stderr, "ERROR: cannot open a log file %s\n", xstr);
		if (str != xstr) free (xstr);
		return -1;
	}

	xtn->logmask = logmask;
#if defined(HAVE_ISATTY)
	xtn->logfd_istty = isatty(xtn->logfd);
#endif

	if (str != xstr) free (xstr);
	return 0;
}

static int start_reply (hcl_client_t* client, hcl_client_reply_type_t type, const hcl_ooch_t* dptr, hcl_oow_t dlen)
{
	client_xtn_t* client_xtn;
	client_xtn = hcl_client_getxtn(client);

	if (client_xtn->reply_count > 0)
	{
		hcl_client_seterrbfmt (client, HCL_EFLOOD, "\n<<WARNING>> redundant reply received\n");
		return -1;
	}

	if (dptr)
	{
		/* short-form response - no end_reply will be called */
		if (type == HCL_CLIENT_REPLY_TYPE_ERROR)
		{
		#if defined(HCL_OOCH_IS_UCH)
			hcl_bch_t bcs[256];
			hcl_oow_t bcslen;

			hcl_conv_uchars_to_bchars_with_cmgr (dptr, &dlen, bcs, &bcslen, hcl_client_getcmgr(client));
			printf ("\nERROR - [%.*s]\n", (int)bcslen, bcs);
		#else
			printf ("\nERROR - [%.*s]\n", (int)dlen, dptr);
		#endif
		}
		else
		{
			printf ("\nTOTAL DATA %lu bytes\n", (unsigned long int)client_xtn->data_length);
		}

		/*fflush (stdout);*/
		client_xtn->reply_count++;

	}
	else
	{
		/* long-form response */
	}
	return 0;
}

static int end_reply (hcl_client_t* client, hcl_client_end_reply_state_t state)
{
	client_xtn_t* client_xtn;
	client_xtn = hcl_client_getxtn(client);

	if (state == HCL_CLIENT_END_REPLY_STATE_REVOKED)
	{
		/* nothing to do here */
		printf ("\n<<WARNING>> REPLY(%lu bytes) received so far has been revoked\n", (unsigned long int)client_xtn->data_length);
		client_xtn->data_length = 0;
	}
	else
	{
		client_xtn->reply_count++;
		/*fflush (stdout);*/
		printf ("\nTOTAL DATA %lu bytes\n", (unsigned long int)client_xtn->data_length);
	}
	return 0;
}

static int feed_attr (hcl_client_t* client, const hcl_oocs_t* key, const hcl_oocs_t* val)
{
	return 0;
}

static int feed_data (hcl_client_t* client, const void* ptr, hcl_oow_t len)
{
	client_xtn_t* client_xtn;

	client_xtn = hcl_client_getxtn(client);
	client_xtn->data_length += len;

	if (write_all(0, ptr, len) <= -1)
	{
		hcl_client_seterrbfmt (client, HCL_EIOERR, "unable to write data");
		return -1;
	}

	return 0;
}

/* ========================================================================= */

static int handle_request (hcl_client_t* client, const char* ipaddr, const char* script, int reuse_addr, int shut_wr_after_req)
{
	hcl_sckaddr_t sckaddr;
	hcl_scklen_t scklen;
	int sckfam;
	int sck = -1;
	struct iovec iov[3];
	int index, count;

	hcl_oow_t used, avail;
	int x;
	hcl_bch_t buf[256];
	ssize_t n;

	client_xtn_t* client_xtn;

	client_xtn = hcl_client_getxtn(client);

	sckfam = hcl_bchars_to_sckaddr(ipaddr, strlen(ipaddr), &sckaddr, &scklen);
	if (sckfam <= -1) 
	{
		fprintf (stderr, "cannot convert ip address - %s\n", ipaddr);
		goto oops;
	}

	sck = socket (sckfam, SOCK_STREAM, 0);
	if (sck <= -1) 
	{
		fprintf (stderr, "cannot create a socket for %s - %s\n", ipaddr, strerror(errno));
		goto oops;
	}

	if (reuse_addr)
	{
		if (sckfam == AF_INET)
		{
			struct sockaddr_in anyaddr;
			int opt = 1;
			setsockopt(sck, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt));
			memset (&anyaddr, 0, HCL_SIZEOF(anyaddr));
			anyaddr.sin_family = sckfam;
			bind(sck, (struct sockaddr *)&anyaddr, scklen);
		}
		else if (sckfam == AF_INET6)
		{
			struct sockaddr_in6 anyaddr;
			int opt = 1;
			setsockopt(sck, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt));
			memset (&anyaddr, 0, HCL_SIZEOF(anyaddr));
			anyaddr.sin6_family = sckfam;
			bind(sck, (struct sockaddr *)&anyaddr, scklen);
		}
	}

	if (connect(sck, (struct sockaddr*)&sckaddr, scklen) <= -1)
	{
		fprintf (stderr, "cannot connect to %s - %s\n", ipaddr, strerror(errno));
		goto oops;
	}

	count = 0;
	iov[count].iov_base = ".SCRIPT (do ";
	iov[count++].iov_len = 12;
	iov[count].iov_base = (char*)script;
	iov[count++].iov_len = strlen(script);
	/* the script above must not include trailing newlines */
	iov[count].iov_base = ")\n";
	iov[count++].iov_len = 2;

	index = 0;
	while (1)
	{
		ssize_t nwritten;
		struct msghdr msg;

		memset (&msg, 0, HCL_SIZEOF(msg));
		msg.msg_iov = (struct iovec*)&iov[index];
		msg.msg_iovlen = count - index;
		nwritten = sendmsg(sck, &msg, 0);
		/*nwritten = writev(proto->worker->sck, (const struct iovec*)&iov[index], count - index);*/
		if (nwritten <= -1) 
		{
			/* error occurred inside the worker thread shouldn't affect the error information
			 * in the server object. so here, i just log a message */
			fprintf (stderr, "Unable to sendmsg on %d - %s\n", sck, strerror(errno));
			goto oops;
		}

		while (index < count && (size_t)nwritten >= iov[index].iov_len)
			nwritten -= iov[index++].iov_len;

		if (index == count) break;

		iov[index].iov_base = (void*)((hcl_uint8_t*)iov[index].iov_base + nwritten);
		iov[index].iov_len -= nwritten;
	}

	if (shut_wr_after_req) shutdown (sck, SHUT_WR);

	client_xtn->data_length = 0;
	client_xtn->reply_count = 0;

/* TODO: implement timeout? */
	avail = 0;
	while (client_xtn->reply_count == 0)
	{
		n = recv(sck, &buf[avail], HCL_SIZEOF(buf) - avail, 0);
		if (n <= -1) 
		{
			fprintf (stderr, "Unable to read from %d - %s\n", sck, strerror(n));
			goto oops;
		}
		if (n == 0) 
		{
			if (hcl_client_getstate(client) != HCL_CLIENT_STATE_START)
			{
				fprintf (stderr, "Sudden end of reply\n");
				goto oops;
			}
			break;
		}

		avail += n;;
		x = hcl_client_feed(client, buf, avail, &used);
		if (x <= -1) 
		{
		#if defined(HCL_OOCH_IS_UCH)
			hcl_errnum_t errnum = hcl_client_geterrnum(client);
			const hcl_ooch_t* errmsg = hcl_client_geterrmsg(client);
			hcl_bch_t errbuf[2048];
			hcl_oow_t ucslen, bcslen;

			bcslen = HCL_COUNTOF(errbuf);
			hcl_conv_ucs_to_bcs_with_cmgr (errmsg, &ucslen, errbuf, &bcslen, hcl_client_getcmgr(client));
			fprintf (stderr, "Client error [%d] - %s\n", (int)errnum, errbuf);
		#else
			fprintf (stderr, "Client error [%d] - %s\n", (int)hcl_client_geterrnum(client), hcl_client_geterrmsg(client));
		#endif
			goto oops;
		}

		avail -= used;
		if (avail > 0) memmove (&buf[0], &buf[used], avail);
	}

/* TODO: we can check if the buffer has all been consumed. if not, there is trailing garbage.. */

	/*shutdown (sck, (shut_wr_after_req? SHUT_RD: SHUT_RDWR));*/
	if (!shut_wr_after_req) shutdown (sck, SHUT_RDWR);

	/*{
		struct linger linger;
		linger.l_onoff = 1;
		linger.l_linger = 0;
		setsockopt (sck, SOL_SOCKET, SO_LINGER, (char *) &linger, sizeof(linger));
	}*/

	close (sck);
	return 0;

oops:
	if (sck >= 0) close (sck);
	return -1;
}

int main (int argc, char* argv[])
{
	hcl_bci_t c;
	static hcl_bopt_lng_t lopt[] =
	{
		{ ":log",                  'l'  },
		{ "reuseaddr",             '\0' },
		{ "shutwr",                '\0' },
		{ HCL_NULL,                '\0' }
	};
	static hcl_bopt_t opt =
	{
		"l:",
		lopt
	};

	hcl_client_t* client;
	client_xtn_t* xtn;
	hcl_client_prim_t client_prim;
	int n;
	const char* logopt = HCL_NULL;
	int reuse_addr = 0;
	int shut_wr_after_req = 0;

	setlocale (LC_ALL, "");

	if (argc < 2)
	{
	print_usage:
		fprintf (stderr, "Usage: %s [-l/--log log-options] [--reuseaddr] [--shutwr] bind-address:port script-to-run\n", argv[0]);
		return -1;
	}

	while ((c = hcl_getbopt (argc, argv, &opt)) != HCL_BCI_EOF)
	{
		switch (c)
		{
			case 'l':
				logopt = opt.arg;
				break;

			case '\0':
				if (hcl_comp_bcstr(opt.lngopt, "reuseaddr") == 0)
				{
					reuse_addr = 1;
				}
				else if (hcl_comp_bcstr(opt.lngopt, "shutwr") == 0)
				{
					shut_wr_after_req = 1;
				}
				else
				{
					goto print_usage;
				}
				break;

			case ':':
				if (opt.lngopt)
					fprintf (stderr, "bad argument for '%s'\n", opt.lngopt);
				else
					fprintf (stderr, "bad argument for '%c'\n", opt.opt);

				return -1;

			default:
				goto print_usage;
		}
	}

	/* needs 2 fixed arguments */
	if (opt.ind + 1 >= argc) goto print_usage;

	memset (&client_prim, 0, HCL_SIZEOF(client_prim));
	client_prim.log_write = log_write;
	client_prim.start_reply = start_reply;
	client_prim.feed_attr = feed_attr;
	client_prim.feed_data = feed_data;
	client_prim.end_reply = end_reply;

	client = hcl_client_open(&sys_mmgr, HCL_SIZEOF(client_xtn_t), &client_prim, HCL_NULL);
	if (!client)
	{
		fprintf (stderr, "cannot open client\n");
		return -1;
	}

	xtn = (client_xtn_t*)hcl_client_getxtn(client);
	xtn->logfd = -1;
	xtn->logfd_istty = 0;

	if (logopt)
	{
		if (handle_logopt(client, logopt) <= -1) goto oops;
	}
	else
	{
		/* default logging mask when no logging option is set */
		xtn->logmask = HCL_LOG_ALL_TYPES | HCL_LOG_ERROR | HCL_LOG_FATAL;
	}

	g_client = client;
	set_signal (SIGINT, handle_sigint);
	set_signal_to_ignore (SIGPIPE);

	n = handle_request (client, argv[opt.ind], argv[opt.ind + 1], reuse_addr, shut_wr_after_req);

	set_signal_to_default (SIGINT);
	set_signal_to_default (SIGPIPE);
	g_client = NULL;

	if (xtn->logfd >= 0)
	{
		close (xtn->logfd);
		xtn->logfd = -1;
		xtn->logfd_istty = 0;
	}

	hcl_client_close (client);
	return n;

oops:
	if (client) hcl_client_close (client);
	return -1;
}
