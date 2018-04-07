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

#include "hcl-s.h"
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

#include <unistd.h>
#include <fcntl.h>

/* ========================================================================= */

typedef struct server_xtn_t server_xtn_t;
struct server_xtn_t
{
	int logfd;
	unsigned int logmask;
	int logfd_istty;
	
	struct
	{
		hcl_bch_t buf[4096];
		hcl_oow_t len;
	} logbuf;
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


static int write_log (hcl_server_t* server, int fd, const hcl_bch_t* ptr, hcl_oow_t len)
{
	server_xtn_t* xtn;


	xtn = hcl_server_getxtn(server);

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

static void flush_log (hcl_server_t* server, int fd)
{
	server_xtn_t* xtn;
	xtn = hcl_server_getxtn(server);
	if (xtn->logbuf.len > 0)
	{
		write_all (fd, xtn->logbuf.buf, xtn->logbuf.len);
		xtn->logbuf.len = 0;
	}
}

static void log_write (hcl_server_t* server, hcl_oow_t wid, unsigned int mask, const hcl_ooch_t* msg, hcl_oow_t len)
{
	hcl_bch_t buf[256];
	hcl_oow_t ucslen, bcslen;
	server_xtn_t* xtn;
	hcl_oow_t msgidx;
	int n, logfd;

	xtn = hcl_server_getxtn(server);

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

		write_log (server, logfd, ts, tslen);

		if (wid != HCL_SERVER_WID_INVALID)
		{
			/* TODO: check if the underlying snprintf support %zd */
			tslen = snprintf (ts, sizeof(ts), "[%zu] ", wid);
			write_log (server, logfd, ts, tslen);
		}
	}

	if (logfd == xtn->logfd && xtn->logfd_istty)
	{
		if (mask & HCL_LOG_FATAL) write_log (server, logfd, "\x1B[1;31m", 7);
		else if (mask & HCL_LOG_ERROR) write_log (server, logfd, "\x1B[1;32m", 7);
		else if (mask & HCL_LOG_WARN) write_log (server, logfd, "\x1B[1;33m", 7);
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
			if (write_log(server, logfd, buf, bcslen) <= -1) break;

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
	write_log (server, logfd, msg, len);
#endif

	if (logfd == xtn->logfd && xtn->logfd_istty)
	{
		if (mask & (HCL_LOG_FATAL | HCL_LOG_ERROR | HCL_LOG_WARN)) write_log (server, logfd, "\x1B[0m", 4);
	}

	flush_log (server, logfd);
}

/* ========================================================================= */

static hcl_server_t* g_server = HCL_NULL;

/* ========================================================================= */

typedef void (*signal_handler_t) (int, siginfo_t*, void*);

static void handle_sigint (int sig, siginfo_t* siginfo, void* ctx)
{
	if (g_server) hcl_server_stop (g_server);
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

static int handle_logopt (hcl_server_t* server, const hcl_bch_t* str)
{
	hcl_bch_t* xstr = (hcl_bch_t*)str;
	hcl_bch_t* cm, * flt;
	unsigned int logmask;
	server_xtn_t* xtn;

	xtn = (server_xtn_t*)hcl_server_getxtn(server);

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

#if defined(HCL_BUILD_DEBUG)
static int handle_dbgopt (hcl_server_t* server, const char* str)
{
	const hcl_bch_t* cm, * flt;
	hcl_oow_t len;
	unsigned int trait;

	hcl_server_getoption (server, HCL_SERVER_TRAIT, &trait);

	cm = str - 1;
	do
	{
		flt = cm + 1;

		cm = hcl_find_bchar_in_bcstr(flt, ',');
		len = cm? (cm - flt): hcl_count_bcstr(flt);
		if (hcl_comp_bchars_bcstr(flt, len, "gc") == 0)  trait |= HCL_SERVER_TRAIT_DEBUG_GC;
		else if (hcl_comp_bchars_bcstr(flt, len, "bigint") == 0)  trait |= HCL_SERVER_TRAIT_DEBUG_BIGINT;
		else
		{
			fprintf (stderr, "ERROR: unknown debug option value - %.*s\n", (int)len, flt);
			return -1;
		}
	}
	while (cm);

	hcl_server_setoption (server, HCL_SERVER_TRAIT, &trait);
	return 0;
}
#endif

static int handle_incpath (hcl_server_t* server, const char* str)
{
#if defined(HCL_OOCH_IS_UCH)
	hcl_ooch_t incpath[HCL_PATH_MAX + 1];
	hcl_oow_t bcslen, ucslen;

	ucslen = HCL_COUNTOF(incpath);
	if (hcl_conv_bcs_to_ucs_with_cmgr(str, &bcslen, incpath, &ucslen, hcl_server_getcmgr(server), 1) <= -1) return -1;
	return hcl_server_setoption(server, HCL_SERVER_SCRIPT_INCLUDE_PATH, incpath);
#else
	return hcl_server_setoption(server, HCL_SERVER_SCRIPT_INCLUDE_PATH, str);
#endif
}

/* ========================================================================= */

#define MIN_WORKER_STACK_SIZE 512000ul
#define MIN_ACTOR_HEAP_SIZE 512000ul
 
int main (int argc, char* argv[])
{
	hcl_bci_t c;
	static hcl_bopt_lng_t lopt[] =
	{
		{ ":log",                  'l'  },
		{ "large-pages",           '\0' },
		{ ":worker-max-count",     '\0' },
		{ ":worker-stack-size",    '\0' },
		{ ":worker-idle-timeout",  '\0' },
		{ ":actor-heap-size",      'm'  },
		{ ":actor-max-runtime",    '\0' },
		{ ":script-include-path",  '\0' },
	#if defined(HCL_BUILD_DEBUG)
		{ ":debug",       '\0' }, /* NOTE: there is no short option for --debug */
	#endif
		{ HCL_NULL,       '\0' }
	};
	static hcl_bopt_t opt =
	{
		"l:m:",
		lopt
	};

	hcl_server_t* server;
	server_xtn_t* xtn;
	hcl_server_prim_t server_prim;
	int n;

	const char* logopt = HCL_NULL;
	const char* dbgopt = HCL_NULL;
	const char* incpath = HCL_NULL;
	hcl_oow_t worker_max_count = 0;
	hcl_oow_t worker_stack_size = MIN_ACTOR_HEAP_SIZE;
	hcl_ntime_t worker_idle_timeout = { 0, 0 };
	hcl_oow_t actor_heap_size = MIN_ACTOR_HEAP_SIZE;
	hcl_ntime_t actor_max_runtime = { 0, 0 };
	int large_pages = 0;
	unsigned int trait;

	setlocale (LC_ALL, "");

	if (argc < 2)
	{
	print_usage:
		fprintf (stderr, "Usage: %s bind-address:port\n", argv[0]);
		return -1;
	}

	while ((c = hcl_getbopt (argc, argv, &opt)) != HCL_BCI_EOF)
	{
		switch (c)
		{
			case 'l':
				logopt = opt.arg;
				break;

			case 'm':
				actor_heap_size = strtoul(opt.arg, HCL_NULL, 0);
				if (actor_heap_size <= MIN_ACTOR_HEAP_SIZE) actor_heap_size = MIN_ACTOR_HEAP_SIZE;
				break;

			case '\0':
				if (hcl_comp_bcstr(opt.lngopt, "large-pages") == 0)
				{
					large_pages = 1;
				}
				else if (hcl_comp_bcstr(opt.lngopt, "worker-max-count") == 0)
				{
					worker_max_count = strtoul(opt.arg, HCL_NULL, 0);
				}
				else if (hcl_comp_bcstr(opt.lngopt, "worker-stack-size") == 0)
				{
					worker_stack_size = strtoul(opt.arg, HCL_NULL, 0);
					if (worker_stack_size <= MIN_WORKER_STACK_SIZE) worker_stack_size = MIN_WORKER_STACK_SIZE;
				}
				else if (hcl_comp_bcstr(opt.lngopt, "worker-idle-timeout") == 0)
				{
					worker_idle_timeout.sec = strtoul(opt.arg, HCL_NULL, 0);
				}
				else if (hcl_comp_bcstr(opt.lngopt, "actor-max-runtime") == 0)
				{
					actor_max_runtime.sec = strtoul(opt.arg, HCL_NULL, 0);
				}
				else if (hcl_comp_bcstr(opt.lngopt, "script-include-path") == 0)
				{
					incpath = opt.arg;
				}
			#if defined(HCL_BUILD_DEBUG)
				else if (hcl_comp_bcstr(opt.lngopt, "debug") == 0)
				{
					dbgopt = opt.arg;
				}
			#endif
				else goto print_usage;
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

	if (opt.ind >= argc) goto print_usage;

	memset (&server_prim, 0, HCL_SIZEOF(server_prim));
	server_prim.log_write = log_write;

	server = hcl_server_open(&sys_mmgr, HCL_SIZEOF(server_xtn_t), &server_prim, HCL_NULL);
	if (!server)
	{
		fprintf (stderr, "cannot open server\n");
		return -1;
	}

	xtn = (server_xtn_t*)hcl_server_getxtn(server);
	xtn->logfd = -1;
	xtn->logfd_istty = 0;

	if (logopt)
	{
		if (handle_logopt(server, logopt) <= -1) goto oops;
	}
	else
	{
		/* default logging mask when no logging option is set */
		xtn->logmask = HCL_LOG_ALL_TYPES | HCL_LOG_ERROR | HCL_LOG_FATAL;
	}

#if defined(HCL_BUILD_DEBUG)
	if (dbgopt)
	{
		if (handle_dbgopt(server, dbgopt) <= -1) goto oops;
	}
#endif

	if (incpath)
	{
		if (handle_incpath(server, incpath) <= -1) goto oops;
	}

	hcl_server_getoption (server, HCL_SERVER_TRAIT, &trait);
	if (large_pages) trait |= HCL_SERVER_TRAIT_USE_LARGE_PAGES;
	else trait &= ~HCL_SERVER_TRAIT_USE_LARGE_PAGES;
	hcl_server_setoption (server, HCL_SERVER_TRAIT, &trait);

	hcl_server_setoption (server, HCL_SERVER_WORKER_MAX_COUNT, &worker_max_count);
	hcl_server_setoption (server, HCL_SERVER_WORKER_STACK_SIZE, &worker_stack_size);
	hcl_server_setoption (server, HCL_SERVER_WORKER_IDLE_TIMEOUT, &worker_idle_timeout);
	hcl_server_setoption (server, HCL_SERVER_ACTOR_HEAP_SIZE, &actor_heap_size);
	hcl_server_setoption (server, HCL_SERVER_ACTOR_MAX_RUNTIME, &actor_max_runtime);

	g_server = server;
	set_signal (SIGINT, handle_sigint);
	set_signal_to_ignore (SIGPIPE);

	n = hcl_server_start(server, argv[opt.ind]);

	set_signal_to_default (SIGINT);
	set_signal_to_default (SIGPIPE);
	g_server = NULL;

	if (n <= -1)
	{
		hcl_server_logbfmt (server, HCL_LOG_APP | HCL_LOG_FATAL, "server error[%d] - %js\n", hcl_server_geterrnum(server), hcl_server_geterrmsg(server));
	}

	if (xtn->logfd >= 0)
	{
		close (xtn->logfd);
		xtn->logfd = -1;
		xtn->logfd_istty = 0;
	}

	hcl_server_close (server);
	return n;

oops:
	if (server) hcl_server_close (server);
	return -1;
}
