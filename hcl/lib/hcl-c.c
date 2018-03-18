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

enum hcl_reply_type_t
{
	HCL_REPLY_TYPE_OK = 0,
	HCL_REPLY_TYPE_ERROR = 1
};
typedef enum hcl_reply_type_t hcl_reply_type_t;

enum hcl_client_proto_state_t
{
	HCL_CLIENT_PROTO_STATE_START,
	HCL_CLIENT_PROTO_STATE_IN_REPLY_NAME,
	HCL_CLIENT_PROTO_STATE_IN_REPLY_VALUE,
	HCL_CLIENT_PROTO_STATE_IN_HEADER_NAME,
	HCL_CLIENT_PROTO_STATE_IN_HEADER_VALUE,
	HCL_CLIENT_PROTO_STATE_IN_DATA,
	HCL_CLIENT_PROTO_STATE_IN_BINARY_DATA,
	HCL_CLIENT_PROTO_STATE_IN_CHUNK,
};
typedef enum hcl_client_proto_state_t hcl_client_proto_state_t;

struct hcl_client_proto_t
{
	hcl_client_t* client;

	struct
	{
		hcl_bch_t* ptr;
		hcl_oow_t len;
		hcl_oow_t capa;
	} req;

	hcl_client_proto_state_t state;
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

		hcl_reply_type_t type;
	} rep;
};
typedef struct hcl_client_proto_t hcl_client_proto_t;

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
};

/* ========================================================================= */


#if 0
static void proto_start_request (hcl_client_proto_t* proto)
{
	proto->req.len = 0;
}


static void proto_feed_request_data (hcl_client_proto_t* proto, xxxx);
{
}


static int proto_end_request (hcl_client_proto_t* proto)
{
	if (proto->req.len <= 0) return -1;

	return 0;
}
#endif


static void proto_start_response (hcl_client_proto_t* proto)
{
	proto->state = HCL_CLIENT_PROTO_STATE_START;
	//proto->rep.len = 0;
}


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

static void clear_reply_token (hcl_client_proto_t* proto)
{
	proto->rep.tok.len = 0;
}

static int add_to_reply_token (hcl_client_proto_t* proto, hcl_ooch_t ch)
{
	if (proto->rep.tok.len >= proto->rep.tok.capa)
	{
		hcl_ooch_t* tmp;
		hcl_oow_t newcapa;

		newcapa = HCL_ALIGN_POW2(proto->rep.tok.len + 1, 128);
		tmp = hcl_client_reallocmem(proto->client, proto->rep.tok.ptr, newcapa * HCL_SIZEOF(*tmp));
		if (!tmp) return -1;
		
		proto->rep.tok.capa = newcapa;
		proto->rep.tok.ptr = tmp;
	}

	proto->rep.tok.ptr[proto->rep.tok.len++] = ch;
	return -1;
}

static int proto_feed_reply_data (hcl_client_proto_t* proto, const hcl_bch_t* data, hcl_oow_t len)
{
	const hcl_bch_t* ptr;
	const hcl_bch_t* end;
	
	ptr = data;
	end = ptr + len;

	if (proto->state == HCL_CLIENT_PROTO_STATE_IN_BINARY_DATA)
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

			n = hcl_conv_bcsn_to_ucsn_with_cmgr(ptr, &bcslen, &c, &ucslen, proto->client->cmgr, 0);
			if (n <= -1)
			{
				if (n == -3)
				{
					/* incomplete sequence */
					
				}
				
			}
	#else
			c = *ptr++;
	#endif

			switch (proto->state)
			{
				case HCL_CLIENT_PROTO_STATE_START:
					if (c == '.')
					{
						proto->state = HCL_CLIENT_PROTO_STATE_IN_REPLY_NAME;
						clear_reply_token (proto);
						if (add_to_reply_token(proto, c) <= -1) goto oops;
						break;
					}
					else if (!is_spacechar(c)) 
					{
						/* TODO: set error code? or log error messages? */
						goto oops;
					}
					break;
				
				case HCL_CLIENT_PROTO_STATE_IN_REPLY_NAME:
					if (is_alphachar(c) || (proto->rep.tok.len > 2 && c == '-'))
					{
						if (add_to_reply_token(proto, c) <= -1) goto oops;	
					}
					else
					{
						if (hcl_compoocharsbcstr(proto->rep.tok.ptr, proto->rep.tok.len, ".OK") == 0)
						{
							proto->rep.type = HCL_REPLY_TYPE_OK;
						}
						else if (hcl_compoocharsbcstr(proto->rep.tok.ptr, proto->rep.tok.len, ".ERROR") == 0)
						{
							proto->rep.type = HCL_REPLY_TYPE_ERROR;
						}

						clear_reply_token (proto);
						if (add_to_reply_token(proto, c) <= -1) goto oops;
					}
					break;
				/*case HCL_CLIENT_PROTO_STATE_IN_START_LINE:*/

				case HCL_CLIENT_PROTO_STATE_IN_HEADER_NAME:

					break;
					
				case HCL_CLIENT_PROTO_STATE_IN_HEADER_VALUE:

					break;
			}
		}
	}
	return 0;


oops:
	/* TODO: compute the number of processes bytes so far and return it via a parameter??? */
	return -1;
}

/* ========================================================================= */

hcl_client_t* hcl_client_open (hcl_mmgr_t* mmgr, hcl_oow_t xtnsize, hcl_errnum_t* errnum)
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

int hcl_client_start (hcl_client_t* client, const hcl_bch_t* addrs)
{

	/* connect */
	/* send request */
}


int hcl_client_sendreq (hcl_client_t* client, const hcl_bch_t* addrs)
{
}



