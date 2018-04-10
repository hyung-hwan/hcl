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

#ifndef _HCL_S_H_
#define _HCL_S_H_

#include <hcl.h>

typedef struct hcl_server_proto_t hcl_server_proto_t;
typedef struct hcl_server_worker_t hcl_server_worker_t;
typedef struct hcl_server_t hcl_server_t;

enum hcl_server_option_t
{
	HCL_SERVER_TRAIT,
	HCL_SERVER_LOG_MASK,
	HCL_SERVER_WORKER_MAX_COUNT,
	HCL_SERVER_WORKER_STACK_SIZE,
	HCL_SERVER_WORKER_IDLE_TIMEOUT,
	HCL_SERVER_ACTOR_HEAP_SIZE,
	HCL_SERVER_ACTOR_MAX_RUNTIME,
	HCL_SERVER_SCRIPT_INCLUDE_PATH,
	HCL_SERVER_MODULE_INCTX
};
typedef enum hcl_server_option_t hcl_server_option_t;

enum hcl_server_trait_t
{
#if defined(HCL_BUILD_DEBUG)
	HCL_SERVER_TRAIT_DEBUG_GC         = (1 << 0),
	HCL_SERVER_TRAIT_DEBUG_BIGINT     = (1 << 1),
#endif

	HCL_SERVER_TRAIT_USE_LARGE_PAGES  = (1 << 2)
};
typedef enum hcl_server_trait_t hcl_server_trait_t;

#define HCL_SERVER_WID_INVALID ((hcl_oow_t)-1)
#define HCL_SERVER_WID_MAX (HCL_SERVER_WID_INVALID - 1)

typedef void (*hcl_server_log_write_t) (
	hcl_server_t*     server,
	hcl_oow_t         wid,
	unsigned int      mask,
	const hcl_ooch_t* msg,
	hcl_oow_t         len
);

struct hcl_server_prim_t
{
	hcl_server_log_write_t log_write;
};
typedef struct hcl_server_prim_t hcl_server_prim_t;


#if defined(__cplusplus)
extern "C" {
#endif

HCL_EXPORT hcl_server_t* hcl_server_open (
	hcl_mmgr_t*        mmgr,
	hcl_oow_t          xtnsize,
	hcl_server_prim_t* prim,
	hcl_errnum_t*      errnum
);

HCL_EXPORT void hcl_server_close (
	hcl_server_t* server
);

HCL_EXPORT int hcl_server_start (
	hcl_server_t*    server,
	const hcl_bch_t* addrs
);

HCL_EXPORT void hcl_server_stop (
	hcl_server_t* server
);

HCL_EXPORT int hcl_server_setoption (
	hcl_server_t*       server,
	hcl_server_option_t id,
	const void*         value
);

HCL_EXPORT int hcl_server_getoption (
	hcl_server_t*       server,
	hcl_server_option_t id,
	void*               value
);

HCL_EXPORT void* hcl_server_getxtn (
	hcl_server_t* server
);

HCL_EXPORT hcl_mmgr_t* hcl_server_getmmgr (
	hcl_server_t* server
);


HCL_EXPORT hcl_cmgr_t* hcl_server_getcmgr (
	hcl_server_t* server
);

HCL_EXPORT void hcl_server_setcmgr (
	hcl_server_t* server,
	hcl_cmgr_t*   cmgr
);

HCL_EXPORT hcl_errnum_t hcl_server_geterrnum (
	hcl_server_t* server
);

HCL_EXPORT const hcl_ooch_t* hcl_server_geterrstr (
	hcl_server_t* server
);

HCL_EXPORT const hcl_ooch_t* hcl_server_geterrmsg (
	hcl_server_t* server
);

HCL_EXPORT void hcl_server_seterrnum (
	hcl_server_t* server,
	hcl_errnum_t  errnum
);

HCL_EXPORT void hcl_server_seterrbfmt (
	hcl_server_t*    server,
	hcl_errnum_t     errnum,
	const hcl_bch_t* fmt,
	...
);

HCL_EXPORT void hcl_server_seterrufmt (
	hcl_server_t*    server,
	hcl_errnum_t     errnum,
	const hcl_uch_t* fmt,
	...
);

HCL_EXPORT void hcl_server_logbfmt (
	hcl_server_t*    server,
	unsigned int     mask,
	const hcl_bch_t* fmt,
	...
);

HCL_EXPORT void hcl_server_logufmt (
	hcl_server_t*    server,
	unsigned int     mask,
	const hcl_uch_t* fmt,
	...
);


HCL_EXPORT void* hcl_server_allocmem (
	hcl_server_t* server,
	hcl_oow_t     size
);

HCL_EXPORT void* hcl_server_callocmem (
	hcl_server_t* server,
	hcl_oow_t     size
);

HCL_EXPORT void* hcl_server_reallocmem (
	hcl_server_t* server,
	void*         ptr,
	hcl_oow_t     size
);


HCL_EXPORT void hcl_server_freemem (
	hcl_server_t* server,
	void*         ptr
);

HCL_EXPORT int hcl_server_proto_feed_reply (
	hcl_server_proto_t* proto,
	const hcl_ooch_t*   ptr,
	hcl_oow_t           len,
	int                 escape
);

#if defined(__cplusplus)
}
#endif

#endif
