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
#include "hcl-prv.h"

#include <stdio.h>
#include <string.h>
#include <errno.h>

#define HCL_SERVER_TOKEN_NAME_ALIGN 64
#define HCL_SERVER_WID_MAP_ALIGN 512
#define HCL_SERVER_PROTO_REPLY_BUF_SIZE 1300

#if defined(_WIN32)
#	include <windows.h>
#	include <tchar.h>
#	if defined(HCL_HAVE_CFG_H) && defined(HCL_ENABLE_LIBLTDL)
#		include <ltdl.h>
#		define USE_LTDL
#	endif
#elif defined(__OS2__)
#	define INCL_DOSMODULEMGR
#	define INCL_DOSPROCESS
#	define INCL_DOSERRORS
#	include <os2.h>
#elif defined(__MSDOS__)
#	include <dos.h>
#	include <time.h>
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

#	include <unistd.h>
#	include <fcntl.h>
#	include <sys/types.h>
#	include <sys/socket.h>
#	include <netinet/in.h>
#	include <pthread.h>
#	include <poll.h>
#endif

enum hcl_client_reply_type_t
{
	HCL_CLIENT_REPLY_TYPE_OK = 0,
	HCL_CLIENT_REPLY_TYPE_ERROR = 1
};
typedef enum hcl_client_reply_type_t hcl_client_reply_type_t;

enum hcl_client_state_t
{
	HCL_CLIENT_STATE_START,
	HCL_CLIENT_STATE_IN_REPLY_NAME,
	HCL_CLIENT_STATE_IN_REPLY_VALUE,
	HCL_CLIENT_STATE_IN_HEADER_NAME,
	HCL_CLIENT_STATE_IN_HEADER_VALUE,
	HCL_CLIENT_STATE_IN_DATA,
	HCL_CLIENT_STATE_IN_BINARY_DATA,
	HCL_CLIENT_STATE_IN_CHUNK,
};
typedef enum hcl_client_state_t hcl_client_state_t;

struct hcl_client_t
{
	hcl_mmgr_t* mmgr;
	hcl_cmgr_t* cmgr;
	/*hcl_t* dummy_hcl;*/

	hcl_errnum_t errnum;
	struct
	{
		hcl_ooch_t buf[HCL_ERRMSG_CAPA];
		hcl_oow_t len;
	} errmsg;
	int stopreq;

	struct
	{
		unsigned int trait;
		unsigned int logmask;
		hcl_oow_t worker_max_count;
		hcl_oow_t worker_stack_size;
		hcl_ntime_t worker_idle_timeout;
	} cfg;


	struct
	{
		hcl_bch_t* ptr;
		hcl_oow_t len;
		hcl_oow_t capa;
	} req;

	hcl_client_state_t state;
	struct
	{
		/*
		hcl_ooch_t* ptr;
		hcl_oow_t len;
		hcl_oow_t capa;
		*/
		struct
		{
			hcl_ooch_t* ptr;
			hcl_oow_t len;
			hcl_oow_t capa;
		} tok;

		hcl_client_reply_type_t type;
	} rep;
};

/* ========================================================================= */


static HCL_INLINE int is_spacechar (hcl_bch_t c)
{
	/* TODO: handle other space unicode characters */
	switch (c)
	{
		case ' ':
		case '\f': /* formfeed */
		case '\r': /* carriage return */
		case '\t': /* horizon tab */
		case '\v': /* vertical tab */
			return 1;

		default:
			return 0;
	}
}

static HCL_INLINE int is_alphachar (hcl_ooci_t c)
{
/* TODO: support full unicode */
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

static HCL_INLINE int is_digitchar (hcl_ooci_t c)
{
/* TODO: support full unicode */
	return (c >= '0' && c <= '9');
}

static void clear_reply_token (hcl_client_t* client)
{
	client->rep.tok.len = 0;
}

static int add_to_reply_token (hcl_client_t* client, hcl_ooch_t ch)
{
	if (client->rep.tok.len >= client->rep.tok.capa)
	{
		hcl_ooch_t* tmp;
		hcl_oow_t newcapa;

		newcapa = HCL_ALIGN_POW2(client->rep.tok.len + 1, 128);
		tmp = hcl_client_reallocmem(client, client->rep.tok.ptr, newcapa * HCL_SIZEOF(*tmp));
		if (!tmp) return -1;

		client->rep.tok.capa = newcapa;
		client->rep.tok.ptr = tmp;
	}

	client->rep.tok.ptr[client->rep.tok.len++] = ch;
	return 0;
}

static int feed_reply_data (hcl_client_t* client, const hcl_bch_t* data, hcl_oow_t len, hcl_oow_t* xlen)
{
	const hcl_bch_t* ptr;
	const hcl_bch_t* end;

	ptr = data;
	end = ptr + len;

printf ("<<<%.*s>>>\n", (int)len, data);
	if (client->state == HCL_CLIENT_STATE_IN_BINARY_DATA)
	{
	}
	else
	{
		while (ptr < end)
		{
			hcl_ooch_t c;

	#if defined(HCL_OOCH_IS_UCH)
			hcl_oow_t bcslen, ucslen;
			int n;

			bcslen = end - ptr;
			ucslen = 1;

			n = hcl_conv_bcsn_to_ucsn_with_cmgr(ptr, &bcslen, &c, &ucslen, client->cmgr, 0);
			if (n <= -1)
			{
				if (n == -3)
				{
					/* incomplete sequence */
					printf ("incompelete....feed me more\n");
					*xlen = ptr - data;
					return 0; /* feed more for incomplete sequence */
				}
			}

			ptr += bcslen;
	#else
			c = *ptr++;
	#endif

printf ("[%lc]\n", c);
			switch (client->state)
			{
				case HCL_CLIENT_STATE_START:
					if (c == '.')
					{
						client->state = HCL_CLIENT_STATE_IN_REPLY_NAME;
						clear_reply_token (client);
						if (add_to_reply_token(client, c) <= -1) goto oops;
					}
					else if (!is_spacechar(c)) 
					{
						/* TODO: set error code? or log error messages? */
						goto oops;
					}
					break;
				
				case HCL_CLIENT_STATE_IN_REPLY_NAME:
					if (is_alphachar(c) || (client->rep.tok.len > 2 && c == '-'))
					{
						if (add_to_reply_token(client, c) <= -1) goto oops;
					}
					else
					{
						if (hcl_compoocharsbcstr(client->rep.tok.ptr, client->rep.tok.len, ".OK") == 0)
						{
							client->rep.type = HCL_CLIENT_REPLY_TYPE_OK;
						}
						else if (hcl_compoocharsbcstr(client->rep.tok.ptr, client->rep.tok.len, ".ERROR") == 0)
						{
							client->rep.type = HCL_CLIENT_REPLY_TYPE_ERROR;
						}

						clear_reply_token (client);
						if (add_to_reply_token(client, c) <= -1) goto oops;
					}
					break;
				/*case HCL_CLIENT_STATE_IN_START_LINE:*/

				case HCL_CLIENT_STATE_IN_HEADER_NAME:

					break;
					
				case HCL_CLIENT_STATE_IN_HEADER_VALUE:

					break;
			}
		}
	}

	*xlen = ptr - data;
	return 1;

oops:
	/* TODO: compute the number of processes bytes so far and return it via a parameter??? */
printf ("feed oops....\n");
	return -1;
}

/* ========================================================================= */

hcl_client_t* hcl_client_open (hcl_mmgr_t* mmgr, hcl_oow_t xtnsize, hcl_client_prim_t* prim, hcl_errnum_t* errnum)
{
	hcl_client_t* client;
#if 0
	hcl_t* hcl;
	hcl_vmprim_t vmprim;
	hcl_tmr_t* tmr;
	dummy_hcl_xtn_t* xtn;
	int pfd[2], fcv;
	unsigned int trait;
#endif

	client = (hcl_client_t*)HCL_MMGR_ALLOC(mmgr, HCL_SIZEOF(*client) + xtnsize);
	if (!client) 
	{
		if (errnum) *errnum = HCL_ESYSMEM;
		return HCL_NULL;
	}

#if 0
	HCL_MEMSET (&vmprim, 0, HCL_SIZEOF(vmprim));
	vmprim.log_write = log_write_for_dummy;
	vmprim.syserrstrb = syserrstrb;
	vmprim.dl_open = dl_open;
	vmprim.dl_close = dl_close;
	vmprim.dl_getsym = dl_getsym;
	vmprim.vm_gettime = vm_gettime;
	vmprim.vm_sleep = vm_sleep;

	hcl = hcl_open(mmgr, HCL_SIZEOF(*xtn), 2048, &vmprim, errnum);
	if (!hcl) 
	{
		HCL_MMGR_FREE (mmgr, client);
		return HCL_NULL;
	}

	tmr = hcl_tmr_open(hcl, 0, 1024); /* TOOD: make the timer's default size configurable */
	if (!tmr)
	{
		hcl_close (hcl);
		HCL_MMGR_FREE (mmgr, client);
		return HCL_NULL;
	}

	if (pipe(pfd) <= -1)
	{
		hcl_tmr_close (tmr);
		hcl_close (hcl);
		HCL_MMGR_FREE (mmgr, client);
		return HCL_NULL;
	}

#if defined(O_CLOEXEC)
	fcv = fcntl(pfd[0], F_GETFD, 0);
	if (fcv >= 0) fcntl(pfd[0], F_SETFD, fcv | O_CLOEXEC);
	fcv = fcntl(pfd[1], F_GETFD, 0);
	if (fcv >= 0) fcntl(pfd[1], F_SETFD, fcv | O_CLOEXEC);
#endif
#if defined(O_NONBLOCK)
	fcv = fcntl(pfd[0], F_GETFL, 0);
	if (fcv >= 0) fcntl(pfd[0], F_SETFL, fcv | O_NONBLOCK);
	fcv = fcntl(pfd[1], F_GETFL, 0);
	if (fcv >= 0) fcntl(pfd[1], F_SETFL, fcv | O_NONBLOCK);
#endif
	
	xtn = (dummy_hcl_xtn_t*)hcl_getxtn(hcl);
	xtn->client = client;

	HCL_MEMSET (client, 0, HCL_SIZEOF(*client) + xtnsize);
	client->mmgr = mmgr;
	client->cmgr = hcl_get_utf8_cmgr();
	client->prim = *prim;
	client->dummy_hcl = hcl;
	client->tmr = tmr;

	client->cfg.logmask = ~0u;
	client->cfg.worker_stack_size = 512000UL; 
	client->cfg.actor_heap_size = 512000UL;

	HCL_INITNTIME (&client->cfg.worker_idle_timeout, 0, 0);
	HCL_INITNTIME (&client->cfg.actor_max_runtime, 0, 0);

	client->mux_pipe[0] = pfd[0];
	client->mux_pipe[1] = pfd[1];

	client->wid_map.free_first = HCL_SERVER_WID_INVALID;
	client->wid_map.free_last = HCL_SERVER_WID_INVALID;
	
	pthread_mutex_init (&client->worker_mutex, HCL_NULL);
	pthread_mutex_init (&client->tmr_mutex, HCL_NULL);
	pthread_mutex_init (&client->log_mutex, HCL_NULL);

	/* the dummy hcl is used for this client to perform primitive operations
	 * such as getting system time or logging. so the heap size doesn't 
	 * need to be changed from the tiny value set above. */
	hcl_setoption (client->dummy_hcl, HCL_LOG_MASK, &client->cfg.logmask);
	hcl_setcmgr (client->dummy_hcl, client->cmgr);
	hcl_getoption (client->dummy_hcl, HCL_TRAIT, &trait);
#if defined(HCL_BUILD_DEBUG)
	if (client->cfg.trait & HCL_SERVER_TRAIT_DEBUG_GC) trait |= HCL_DEBUG_GC;
	if (client->cfg.trait & HCL_SERVER_TRAIT_DEBUG_BIGINT) trait |= HCL_DEBUG_BIGINT;
#endif
	hcl_setoption (client->dummy_hcl, HCL_TRAIT, &trait);
#else

	HCL_MEMSET (client, 0, HCL_SIZEOF(*client) + xtnsize);
	client->mmgr = mmgr;
	client->cmgr = hcl_get_utf8_cmgr();

#endif

	return client;
}

void hcl_client_close (hcl_client_t* client)
{
	HCL_MMGR_FREE (client->mmgr, client);
}


int hcl_client_setoption (hcl_client_t* client, hcl_client_option_t id, const void* value)
{
	switch (id)
	{
		case HCL_CLIENT_TRAIT:
			client->cfg.trait = *(const unsigned int*)value;
		#if 0
			if (client->dummy_hcl)
			{
				/* setting this affects the dummy hcl immediately.
				 * existing hcl instances inside worker threads won't get 
				 * affected. new hcl instances to be created later 
				 * is supposed to use the new value */
				unsigned int trait;

				hcl_getoption (client->dummy_hcl, HCL_TRAIT, &trait);
			#if defined(HCL_BUILD_DEBUG)
				if (client->cfg.trait & HCL_CLIENT_TRAIT_DEBUG_GC) trait |= HCL_DEBUG_GC;
				if (client->cfg.trait & HCL_CLIENT_TRAIT_DEBUG_BIGINT) trait |= HCL_DEBUG_BIGINT;
			#endif
				hcl_setoption (client->dummy_hcl, HCL_TRAIT, &trait);
			}
		#endif
			return 0;

		case HCL_CLIENT_LOG_MASK:
			client->cfg.logmask = *(const unsigned int*)value;
		#if 0
			if (client->dummy_hcl) 
			{
				/* setting this affects the dummy hcl immediately.
				 * existing hcl instances inside worker threads won't get 
				 * affected. new hcl instances to be created later 
				 * is supposed to use the new value */
				hcl_setoption (client->dummy_hcl, HCL_LOG_MASK, value);
			}
		#endif
			return 0;

		case HCL_CLIENT_WORKER_MAX_COUNT:
			client->cfg.worker_max_count = *(hcl_oow_t*)value;
			return 0;

		case HCL_CLIENT_WORKER_STACK_SIZE:
			client->cfg.worker_stack_size = *(hcl_oow_t*)value;
			return 0;

		case HCL_CLIENT_WORKER_IDLE_TIMEOUT:
			client->cfg.worker_idle_timeout = *(hcl_ntime_t*)value;
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
			*(unsigned int*)value = client->cfg.trait;
			return 0;

		case HCL_CLIENT_LOG_MASK:
			*(unsigned int*)value = client->cfg.logmask;
			return 0;

		case HCL_CLIENT_WORKER_MAX_COUNT:
			*(hcl_oow_t*)value = client->cfg.worker_max_count;
			return 0;

		case HCL_CLIENT_WORKER_STACK_SIZE:
			*(hcl_oow_t*)value = client->cfg.worker_stack_size;
			return 0;

		case HCL_CLIENT_WORKER_IDLE_TIMEOUT:
			*(hcl_ntime_t*)value = client->cfg.worker_idle_timeout;
			return 0;
	};

	hcl_client_seterrnum (client, HCL_EINVAL);
	return -1;
}


void* hcl_client_getxtn (hcl_client_t* client)
{
	return (void*)(client + 1);
}

hcl_mmgr_t* hcl_client_getmmgr (hcl_client_t* client)
{
	return client->mmgr;
}

hcl_cmgr_t* hcl_client_getcmgr (hcl_client_t* client)
{
	return client->cmgr;
}

void hcl_client_setcmgr (hcl_client_t* client, hcl_cmgr_t* cmgr)
{
	client->cmgr = cmgr;
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

void hcl_client_seterrnum (hcl_client_t* client, hcl_errnum_t errnum)
{
	/*if (client->shuterr) return; */
	client->errnum = errnum;
	client->errmsg.len = 0;
}

void* hcl_client_allocmem (hcl_client_t* client, hcl_oow_t size)
{
	void* ptr;

	ptr = HCL_MMGR_ALLOC(client->mmgr, size);
	if (!ptr) hcl_client_seterrnum (client, HCL_ESYSMEM);
	return ptr;
}

void* hcl_client_callocmem (hcl_client_t* client, hcl_oow_t size)
{
	void* ptr;

	ptr = HCL_MMGR_ALLOC(client->mmgr, size);
	if (!ptr) hcl_client_seterrnum (client, HCL_ESYSMEM);
	else HCL_MEMSET (ptr, 0, size);
	return ptr;
}

void* hcl_client_reallocmem (hcl_client_t* client, void* ptr, hcl_oow_t size)
{
	ptr = HCL_MMGR_REALLOC(client->mmgr, ptr, size);
	if (!ptr) hcl_client_seterrnum (client, HCL_ESYSMEM);
	return ptr;
}

void hcl_client_freemem (hcl_client_t* client, void* ptr)
{
	HCL_MMGR_FREE (client->mmgr, ptr);
}


/* ========================================================================= */

static int feed_all_reply_data (hcl_client_t* client, hcl_bch_t* buf, hcl_oow_t n, hcl_oow_t* xlen)
{
	int x;
	hcl_oow_t total, ylen;

	total = 0;
	while (total < n)
	{
		x = feed_reply_data(client, &buf[total], n - total, &ylen);
		if (x <= -1) return -1;

		total += ylen;
		if (ylen == 0) break; /* incomplete sequence */
	}

	*xlen = total;
	return 0;
}

int hcl_client_start (hcl_client_t* client, const hcl_bch_t* addrs)
{

	/* connect */
	/* send request */

	hcl_oow_t xlen, offset;
	int x;
	int fd = 0; /* read from stdin for testing */
	hcl_bch_t buf[256];
	ssize_t n;

	client->stopreq = 0;
	offset = 0;

	while (1)
	{
		n = read(fd, &buf[offset], HCL_SIZEOF(buf) - offset); /* switch to recv  */
		if (n <= -1) 
		{
			printf ("error....%s\n", strerror(n));
			return -1;
		}
		if (n == 0) 
		{
			/* TODO: check if there is residue in the receiv buffer */
			break;
		}

		x = feed_all_reply_data (client, buf, n, &xlen);
		if (x <= -1) return -1;

		offset = n - xlen;
		if (offset > 0) HCL_MEMMOVE (&buf[0], &buf[xlen], offset);
	}

printf ("hcl client start returns success\n");
	return 0;
}

void hcl_client_stop (hcl_client_t* client)
{
	client->stopreq = 1;
}
