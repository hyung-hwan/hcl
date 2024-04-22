/*
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

#include "hcl-x.h"
#include "hcl-prv.h"
#include "hcl-tmr.h"
#include "hcl-xutl.h"

#include <stdio.h>
#include <string.h>
#include <errno.h>

#define HCL_SERVER_TOKEN_NAME_ALIGN 64
#define HCL_SERVER_WID_MAP_ALIGN 512
#define HCL_XPROTO_REPLY_BUF_SIZE 1300

#if defined(_WIN32)
#	include <windows.h>
#	include <tchar.h>
#elif defined(__OS2__)
#	define INCL_DOSMODULEMGR
#	define INCL_DOSPROCESS
#	define INCL_DOSERRORS
#	include <os2.h>
#elif defined(__DOS__)
#	include <dos.h>
#	include <time.h>
#	include <signal.h>
#elif defined(macintosh)
#	include <Timer.h>
#else

#	if defined(HAVE_TIME_H)
#		include <time.h>
#	endif
#	if defined(HAVE_SYS_TIME_H)
#		include <sys/time.h>
#	endif
#	if defined(HAVE_SIGNAL_H)
#		include <signal.h>
#	endif
#	if defined(HAVE_SYS_MMAN_H)
#		include <sys/mman.h>
#	endif
#	if defined(HAVE_SYS_UIO_H)
#		include <sys/uio.h>
#	endif
#	if defined(HAVE_SYS_EPOLL_H)
#		include <sys/epoll.h>
#	endif

#	include <unistd.h>
#	include <fcntl.h>
#	include <sys/types.h>
#	include <sys/socket.h>
#	include <netinet/in.h>
#	include <pthread.h>
#	include <poll.h>
#endif

struct client_hcl_xtn_t
{
	hcl_client_t* client;
};
typedef struct client_hcl_xtn_t client_hcl_xtn_t;

struct hcl_client_t
{
	hcl_oow_t   _instsize;
	hcl_mmgr_t* _mmgr;
	hcl_cmgr_t* _cmgr;

	hcl_client_prim_t prim;
	hcl_t* dummy_hcl;

	hcl_errnum_t errnum;
	struct
	{
	#if defined(HCL_OOCH_IS_BCH)
		hcl_uch_t  xerrmsg[HCL_ERRMSG_CAPA];
	#else
		hcl_bch_t  xerrmsg[HCL_ERRMSG_CAPA * 2];
	#endif
		hcl_ooch_t buf[HCL_ERRMSG_CAPA];
		hcl_oow_t len;
	} errmsg;

	struct
	{
		hcl_bitmask_t trait;
		hcl_bitmask_t logmask;
	} cfg;

	int sck;
	hcl_xproto_t* proto;
};


/* ========================================================================= */

static void client_log_write_for_dummy (hcl_t* hcl, hcl_bitmask_t mask, const hcl_ooch_t* msg, hcl_oow_t len)
{
	client_hcl_xtn_t* xtn = (client_hcl_xtn_t*)hcl_getxtn(hcl);
	hcl_client_t* client;

	client = xtn->client;
	client->prim.log_write (client, mask, msg, len);
}

hcl_client_t* hcl_client_open (hcl_mmgr_t* mmgr, hcl_oow_t xtnsize, hcl_client_prim_t* prim, hcl_errnum_t* errnum)
{
	hcl_client_t* client;
	hcl_t* hcl;
	client_hcl_xtn_t* xtn;

	client = (hcl_client_t*)HCL_MMGR_ALLOC(mmgr, HCL_SIZEOF(*client) + xtnsize);
	if (HCL_UNLIKELY(!client))
	{
		if (errnum) *errnum = HCL_ESYSMEM;
		return HCL_NULL;
	}

	hcl = hcl_openstdwithmmgr(mmgr, HCL_SIZEOF(*xtn), errnum);
	if (HCL_UNLIKELY(!hcl))
	{
		HCL_MMGR_FREE (mmgr, client);
		return HCL_NULL;
	}

	/* replace the vmprim.log_write function */
	hcl->vmprim.log_write = client_log_write_for_dummy;

	xtn = (client_hcl_xtn_t*)hcl_getxtn(hcl);
	xtn->client = client;

	HCL_MEMSET (client, 0, HCL_SIZEOF(*client) + xtnsize);
	client->_instsize = HCL_SIZEOF(*client);
	client->_mmgr = mmgr;
	client->_cmgr = hcl_get_utf8_cmgr();
	client->prim = *prim;
	client->dummy_hcl = hcl;
	client->sck = -1;

	client->cfg.logmask = ~(hcl_bitmask_t)0;

	/* the dummy hcl is used for this client to perform primitive operations
	 * such as getting system time or logging. so the heap size doesn't
	 * need to be changed from the tiny value set above. */
	hcl_setoption (client->dummy_hcl, HCL_LOG_MASK, &client->cfg.logmask);
	hcl_setcmgr (client->dummy_hcl, client->_cmgr);

	return client;
}

void hcl_client_close (hcl_client_t* client)
{
	if (client->proto) hcl_xproto_close (client->proto);
	if (client->sck >= 0) close (client->sck);
	hcl_close (client->dummy_hcl);
	HCL_MMGR_FREE (client->_mmgr, client);
}

int hcl_client_setoption (hcl_client_t* client, hcl_client_option_t id, const void* value)
{
	switch (id)
	{
		case HCL_CLIENT_TRAIT:
			client->cfg.trait = *(const hcl_bitmask_t*)value;
			return 0;

		case HCL_CLIENT_LOG_MASK:
			client->cfg.logmask = *(const hcl_bitmask_t*)value;
			if (client->dummy_hcl)
			{
				/* setting this affects the dummy hcl immediately.
				 * existing hcl instances inside worker threads won't get
				 * affected. new hcl instances to be created later
				 * is supposed to use the new value */
				hcl_setoption (client->dummy_hcl, HCL_LOG_MASK, value);
			}
			return 0;
	}

	hcl_client_seterrnum (client, HCL_EINVAL);
	return -1;
}

int hcl_client_getoption (hcl_client_t* client, hcl_client_option_t id, void* value)
{
	switch (id)
	{
		case HCL_CLIENT_TRAIT:
			*(hcl_bitmask_t*)value = client->cfg.trait;
			return 0;

		case HCL_CLIENT_LOG_MASK:
			*(hcl_bitmask_t*)value = client->cfg.logmask;
			return 0;
	};

	hcl_client_seterrnum (client, HCL_EINVAL);
	return -1;
}


void* hcl_client_getxtn (hcl_client_t* client)
{
	return (void*)((hcl_uint8_t*)client + client->_instsize);
}

hcl_mmgr_t* hcl_client_getmmgr (hcl_client_t* client)
{
	return client->_mmgr;
}

hcl_cmgr_t* hcl_client_getcmgr (hcl_client_t* client)
{
	return client->_cmgr;
}

void hcl_client_setcmgr (hcl_client_t* client, hcl_cmgr_t* cmgr)
{
	client->_cmgr = cmgr;
}

hcl_errnum_t hcl_client_geterrnum (hcl_client_t* client)
{
	return client->errnum;
}

const hcl_ooch_t* hcl_client_geterrstr (hcl_client_t* client)
{
	return hcl_errnum_to_errstr(client->errnum);
}

const hcl_ooch_t* hcl_client_geterrmsg (hcl_client_t* client)
{
	if (client->errmsg.len <= 0) return hcl_errnum_to_errstr(client->errnum);
	return client->errmsg.buf;
}

const hcl_bch_t* hcl_client_geterrbmsg (hcl_client_t* client)
{
#if defined(HCL_OOCH_IS_BCH)
	return (client->errmsg.len <= 0)? hcl_errnum_to_errstr(client->errnum): client->errmsg.buf;
#else
	const hcl_ooch_t* msg;
	hcl_oow_t wcslen, mbslen;

	msg = (client->errmsg.len <= 0)? hcl_errnum_to_errstr(client->errnum): client->errmsg.buf;

	mbslen = HCL_COUNTOF(client->errmsg.xerrmsg);
	hcl_conv_ucstr_to_bcstr_with_cmgr (msg, &wcslen, client->errmsg.xerrmsg, &mbslen, client->_cmgr);

	return client->errmsg.xerrmsg;
#endif
}

const hcl_uch_t* hcl_client_geterrumsg (hcl_client_t* client)
{
#if defined(HCL_OOCH_IS_BCH)
	const hcl_ooch_t* msg;
	hcl_oow_t wcslen, mbslen;

	msg = (client->errmsg.len <= 0)? hcl_errnum_to_errstr(client->errnum): client->errmsg.buf;

	wcslen = HCL_COUNTOF(client->errmsg.xerrmsg);
	hcl_conv_bcstr_to_ucstr_with_cmgr (msg, &mbslen, client->errmsg.xerrmsg, &wcslen, client->_cmgr, 1);

	return client->errmsg.xerrmsg;
#else
	return (client->errmsg.len == '\0')? hcl_errnum_to_errstr(client->errnum): client->errmsg.buf;
#endif
}

void hcl_client_seterrnum (hcl_client_t* client, hcl_errnum_t errnum)
{
	/*if (client->shuterr) return; */
	client->errnum = errnum;
	client->errmsg.len = 0;
}

void hcl_client_seterrbfmt (hcl_client_t* client, hcl_errnum_t errnum, const hcl_bch_t* fmt, ...)
{
	va_list ap;

	va_start (ap, fmt);
	hcl_seterrbfmtv (client->dummy_hcl, errnum, fmt, ap);
	va_end (ap);

	HCL_ASSERT (client->dummy_hcl, HCL_COUNTOF(client->errmsg.buf) == HCL_COUNTOF(client->dummy_hcl->errmsg.buf));
	client->errnum = errnum;
	hcl_copy_oochars (client->errmsg.buf, client->dummy_hcl->errmsg.buf, HCL_COUNTOF(client->errmsg.buf));
	client->errmsg.len = client->dummy_hcl->errmsg.len;
}

void hcl_client_seterrufmt (hcl_client_t* client, hcl_errnum_t errnum, const hcl_uch_t* fmt, ...)
{
	va_list ap;

	va_start (ap, fmt);
	hcl_seterrufmtv (client->dummy_hcl, errnum, fmt, ap);
	va_end (ap);

	HCL_ASSERT (client->dummy_hcl, HCL_COUNTOF(client->errmsg.buf) == HCL_COUNTOF(client->dummy_hcl->errmsg.buf));
	client->errnum = errnum;
	hcl_copy_oochars (client->errmsg.buf, client->dummy_hcl->errmsg.buf, HCL_COUNTOF(client->errmsg.buf));
	client->errmsg.len = client->dummy_hcl->errmsg.len;
}

/* ========================================================================= */

void hcl_client_logbfmt (hcl_client_t* client, hcl_bitmask_t mask, const hcl_bch_t* fmt, ...)
{
	va_list ap;
	va_start (ap, fmt);
	hcl_logbfmtv (client->dummy_hcl, mask, fmt, ap);
	va_end (ap);
}

void hcl_client_logufmt (hcl_client_t* client, hcl_bitmask_t mask, const hcl_uch_t* fmt, ...)
{
	va_list ap;
	va_start (ap, fmt);
	hcl_logufmtv (client->dummy_hcl, mask, fmt, ap);
	va_end (ap);
}

/* ========================================================================= */

void* hcl_client_allocmem (hcl_client_t* client, hcl_oow_t size)
{
	void* ptr;

	ptr = HCL_MMGR_ALLOC(client->_mmgr, size);
	if (!ptr) hcl_client_seterrnum (client, HCL_ESYSMEM);
	return ptr;
}

void* hcl_client_callocmem (hcl_client_t* client, hcl_oow_t size)
{
	void* ptr;

	ptr = HCL_MMGR_ALLOC(client->_mmgr, size);
	if (!ptr) hcl_client_seterrnum (client, HCL_ESYSMEM);
	else HCL_MEMSET (ptr, 0, size);
	return ptr;
}

void* hcl_client_reallocmem (hcl_client_t* client, void* ptr, hcl_oow_t size)
{
	ptr = HCL_MMGR_REALLOC(client->_mmgr, ptr, size);
	if (!ptr) hcl_client_seterrnum (client, HCL_ESYSMEM);
	return ptr;
}

void hcl_client_freemem (hcl_client_t* client, void* ptr)
{
	HCL_MMGR_FREE (client->_mmgr, ptr);
}

/* ========================================================================= */

struct proto_xtn_t
{
	hcl_client_t* client;
};
typedef struct proto_xtn_t proto_xtn_t;

static int handle_packet (hcl_xproto_t* proto, hcl_xpkt_type_t type, const void* data, hcl_oow_t len)
{
	if (type == HCL_XPKT_STDOUT)
	{
		/*if (len > 0) fwrite (data, 1, len, stdout); */
		if (len > 0) fprintf (stdout, "%.*s", (int)len, data);
	}
	return 1;
}

#if 0

int hcl_client_process (hcl_client_t* client, const char* ipaddr, const char* script, int reuse_addr, int shut_wr_after_req)
{
	hcl_oow_t used, avail;
	int x;
	hcl_bch_t buf[256];
	ssize_t n;
	const char* scptr;
	const char* sccur;
	hcl_xproto_t* proto = HCL_NULL;

	proto_xtn_t* proto_xtn;
	hcl_xproto_cb_t proto_cb;

	scptr = sccur = script;
	while (1)
	{
		struct pollfd pfd;

		pfd.fd = sck;
		pfd.events = POLLIN;
		if (*sccur != '\0') pfd.events |= POLLOUT;
		pfd.revents = 0;

		n = poll(&pfd, 1, 1000);
		if (n <= -1)
		{
			fprintf (stderr, "poll error on %d - %s\n", sck, strerror(n));
			goto oops;
		}

		if (n == 0)
		{
			/* TODO: proper timeout handling */
			continue;
		}

		if (pfd.revents & POLLERR)
		{
			fprintf (stderr, "error condition detected on %d\n", sck);
			goto oops;
		}

		if (pfd.revents & POLLOUT)
		{
			hcl_xpkt_hdr_t hdr;
			struct iovec iov[2];
			hcl_uint16_t seglen;

			while (*sccur != '\0' && sccur - scptr < HCL_XPKT_MAX_PLD_LEN) sccur++;

			seglen = sccur - scptr;

			hdr.id = 1; /* TODO: */
			hdr.type = HCL_XPKT_CODE | (((seglen >> 8) & 0x0F) << 4);
			hdr.len = seglen & 0xFF;

			iov[0].iov_base = &hdr;
			iov[0].iov_len = HCL_SIZEOF(hdr);
			iov[1].iov_base = scptr;
			iov[1].iov_len = seglen;

			hcl_sys_send_iov (sck, iov, 2); /* TODO: error check */

			scptr = sccur;

			if (*sccur == '\0')
			{
				hdr.id = 1; /* TODO: */
				hdr.type = HCL_XPKT_EXECUTE;
				hdr.len = 0;

				iov[0].iov_base = &hdr;
				iov[0].iov_len = HCL_SIZEOF(hdr);
				hcl_sys_send_iov (sck, iov, 1);

				if (shut_wr_after_req)
				{
					shutdown (sck, SHUT_WR);
				}
				else
				{
					hdr.type = HCL_XPKT_DISCONNECT;
					hdr.id = 1; /* TODO: */
					hdr.len = 0;

					iov[0].iov_base = &hdr;
					iov[0].iov_len = HCL_SIZEOF(hdr);
					hcl_sys_send_iov (sck, iov, 1);
				}
			}
		}

		if (pfd.revents & POLLIN)
		{
			hcl_oow_t bcap;
			hcl_uint8_t* bptr;

			bptr = hcl_xproto_getbuf(proto, &bcap);;
			x = recv(sck, bptr, bcap, 0);
			if (x <= -1)
			{
				if (errno == EINTR) goto carry_on; /* didn't read read */
				/*hcl_seterrwithsyserr (hcl, 0, errno); */
				/* TODO: error info set... */
				return -1;
			}
			if (x == 0) hcl_xproto_seteof(proto, 1);
			hcl_xproto_advbuf (proto, x);
		}


	carry_on:
		while (hcl_xproto_ready(proto))
		{
			if ((n = hcl_xproto_process(proto)) <= -1)
			{
				/* TODO: proper error message */
				return -1;
			}
			if (n == 0)
			{
				/* TODO: chceck if there is remaining data in the buffer...?? */
				printf ("NO MORE DATA. EXITING...\n");
				goto done;
			}
		}

		if (hcl_xproto_geteof(proto)) break;
	}
done:

/* TODO: we can check if the buffer has all been consumed. if not, there is trailing garbage.. */
	/*{
		struct linger linger;
		linger.l_onoff = 1;
		linger.l_linger = 0;
		setsockopt (sck, SOL_SOCKET, SO_LINGER, (char *) &linger, sizeof(linger));
	}*/

	hcl_xproto_close (proto);
	close (sck);
	return 0;

oops:
	if (proto) hcl_xproto_close (proto);
	if (sck >= 0) close (sck);
	return -1;
}
#endif

int hcl_client_connect (hcl_client_t* client, const char* ipaddr, int reuse_addr)
{
	hcl_sckaddr_t sckaddr;
	hcl_scklen_t scklen;
	int sckfam;
	int sck = -1;
	hcl_xproto_t* proto = HCL_NULL;

	proto_xtn_t* proto_xtn;
	hcl_xproto_cb_t proto_cb;

	sckfam = hcl_bchars_to_sckaddr(ipaddr, strlen(ipaddr), &sckaddr, &scklen);
	if (sckfam <= -1)
	{
		hcl_client_seterrbfmt (client, HCL_EINVAL, "cannot convert ip address - %hs", ipaddr);
		goto oops;
	}

	sck = socket(sckfam, SOCK_STREAM, 0);
	if (sck <= -1)
	{
		hcl_client_seterrbfmt (client, HCL_ESYSERR, "cannot create socket - %hs", strerror(errno));
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
			if (bind(sck, (struct sockaddr *)&anyaddr, scklen) <= -1)
			{
				hcl_client_seterrbfmt (client, HCL_ESYSERR,
					"cannot bind socket %d - %hs", sck, strerror(errno));
				goto oops;
			}
		}
		else if (sckfam == AF_INET6)
		{
			struct sockaddr_in6 anyaddr;
			int opt = 1;
			setsockopt(sck, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt));
			memset (&anyaddr, 0, HCL_SIZEOF(anyaddr));
			anyaddr.sin6_family = sckfam;
			if (bind(sck, (struct sockaddr *)&anyaddr, scklen) <= -1)
			{
				hcl_client_seterrbfmt (client, HCL_ESYSERR,
					"cannot bind socket %d - %hs", sck, strerror(errno));
				goto oops;
			}
		}
	}

	if (connect(sck, (struct sockaddr*)&sckaddr, scklen) <= -1)
	{
		hcl_client_seterrbfmt (client, HCL_ESYSERR,
			"cannot connect socket %d to %hs - %hs", sck, ipaddr, strerror(errno));
		goto oops;
	}


	memset (&proto, 0, HCL_SIZEOF(proto_cb));
	proto_cb.on_packet = handle_packet;

	proto = hcl_xproto_open(hcl_client_getmmgr(client), &proto_cb, HCL_SIZEOF(*proto_xtn));
	if (HCL_UNLIKELY(!proto))
	{
		fprintf (stderr, "cannot open protocol to %s\n", ipaddr);
		goto oops;
	}
	proto_xtn = hcl_xproto_getxtn(proto);
	proto_xtn->client = client;

	client->sck = sck;
	client->proto = proto;
	return 0;

oops:
	if (proto) hcl_xproto_close (proto);
	if (sck >= 0) close (sck);
	return -1;
}