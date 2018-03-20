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

#ifndef _HCL_C_H_
#define _HCL_C_H_

#include <hcl.h>


typedef struct hcl_client_t hcl_client_t;

enum hcl_client_option_t
{
	HCL_CLIENT_TRAIT,
	HCL_CLIENT_LOG_MASK,
	HCL_CLIENT_WORKER_MAX_COUNT,
	HCL_CLIENT_WORKER_STACK_SIZE,
	HCL_CLIENT_WORKER_IDLE_TIMEOUT
};
typedef enum hcl_client_option_t hcl_client_option_t;

enum hcl_client_trait_t
{
#if defined(HCL_BUILD_DEBUG)
	HCL_CLIENT_TRAIT_DEBUG_GC         = (1 << 0),
	HCL_CLIENT_TRAIT_DEBUG_BIGINT     = (1 << 1),
#endif

	HCL_CLIENT_TRAIT_READABLE_PROTO   = (1 << 2),
	HCL_CLIENT_TRAIT_USE_LARGE_PAGES  = (1 << 3)
};
typedef enum hcl_client_trait_t hcl_client_trait_t;

typedef void (*hcl_client_log_write_t) (
	hcl_client_t*     client,
	hcl_oow_t         wid,
	unsigned int      mask,
	const hcl_ooch_t* msg,
	hcl_oow_t         len
);

struct hcl_client_prim_t
{
	hcl_client_log_write_t log_write;
};
typedef struct hcl_client_prim_t hcl_client_prim_t;


#if defined(__cplusplus)
extern "C" {
#endif

HCL_EXPORT hcl_client_t* hcl_client_open (
	hcl_mmgr_t*        mmgr,
	hcl_oow_t          xtnsize,
	hcl_client_prim_t* prim,
	hcl_errnum_t*      errnum
);

HCL_EXPORT void hcl_client_close (
	hcl_client_t* client
);

HCL_EXPORT int hcl_client_start (
	hcl_client_t*    client,
	const hcl_bch_t* addrs
);

HCL_EXPORT void hcl_client_stop (
	hcl_client_t* client
);

HCL_EXPORT int hcl_client_setoption (
	hcl_client_t*       client,
	hcl_client_option_t id,
	const void*         value
);

HCL_EXPORT int hcl_client_getoption (
	hcl_client_t*       client,
	hcl_client_option_t id,
	void*               value
);


HCL_EXPORT void* hcl_client_getxtn (
	hcl_client_t* client
);

HCL_EXPORT hcl_mmgr_t* hcl_client_getmmgr (
	hcl_client_t* client
);

HCL_EXPORT hcl_cmgr_t* hcl_client_getcmgr (
	hcl_client_t* client
);

HCL_EXPORT void hcl_client_setcmgr (
	hcl_client_t* client,
	hcl_cmgr_t*   cmgr
);


HCL_EXPORT hcl_errnum_t hcl_client_geterrnum (
	hcl_client_t* client
);

HCL_EXPORT const hcl_ooch_t* hcl_client_geterrstr (
	hcl_client_t* client
);

HCL_EXPORT const hcl_ooch_t* hcl_client_geterrmsg (
	hcl_client_t* client
);

HCL_EXPORT void hcl_client_seterrnum (
	hcl_client_t* client,
	hcl_errnum_t  errnum
);





HCL_EXPORT void* hcl_client_allocmem (
	hcl_client_t* client,
	hcl_oow_t     size
);

HCL_EXPORT void* hcl_client_callocmem (
	hcl_client_t* client,
	hcl_oow_t     size
);

HCL_EXPORT void* hcl_client_reallocmem (
	hcl_client_t* client,
	void*         ptr,
	hcl_oow_t     size
);


HCL_EXPORT void hcl_client_freemem (
	hcl_client_t* client,
	void*         ptr
);


#if defined(__cplusplus)
}
#endif

#endif
