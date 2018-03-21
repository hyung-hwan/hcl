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
};
typedef enum hcl_client_option_t hcl_client_option_t;

enum hcl_client_trait_t
{
	/* no trait defined at this moment. XXXX is just a placeholder */
	HCL_CLIENT_XXXX  = (1 << 0)
};
typedef enum hcl_client_trait_t hcl_client_trait_t;

/* ========================================================================= */

enum hcl_client_reply_type_t
{
	HCL_CLIENT_REPLY_TYPE_OK = 0,
	HCL_CLIENT_REPLY_TYPE_ERROR = 1
};
typedef enum hcl_client_reply_type_t hcl_client_reply_type_t;

typedef void (*hcl_client_log_write_t) (
	hcl_client_t*     client,
	unsigned int      mask,
	const hcl_ooch_t* msg,
	hcl_oow_t         len
);

typedef void (*hcl_client_start_reply_t) (
	hcl_client_t*           client,
	hcl_client_reply_type_t type,
	const hcl_ooch_t*       dptr,
	hcl_oow_t               dlen
);

typedef void (*hcl_client_feed_attr_t) (
	hcl_client_t*     client,
	const hcl_oocs_t* key,
	const hcl_oocs_t* val
);

typedef void (*hcl_client_start_data_t) (
	hcl_client_t*     client
);

typedef void (*hcl_client_feed_data_t) (
	hcl_client_t*     client,
	const void*       ptr,
	hcl_oow_t         len
);

typedef void (*hcl_client_end_data_t) (
	hcl_client_t*     client
);

enum hcl_client_end_reply_state_t
{
	HCL_CLIENT_END_REPLY_STATE_OK,
	HCL_CLIENT_END_REPLY_STATE_REVOKED,
	HCL_CLIENT_END_REPLY_STATE_ERROR
};
typedef enum hcl_client_end_reply_state_t hcl_client_end_reply_state_t;

typedef void (*hcl_client_end_reply_t) (
	hcl_client_t*                client,
	hcl_client_end_reply_state_t state
);

struct hcl_client_prim_t
{
	hcl_client_log_write_t     log_write;

	hcl_client_start_reply_t   start_reply;   /* mandatory */
	hcl_client_feed_attr_t     feed_attr; /* optional */
	hcl_client_feed_data_t     feed_data;     /* optional */
	hcl_client_end_reply_t     end_reply;     /* mandatory */
};
typedef struct hcl_client_prim_t hcl_client_prim_t;

/* ========================================================================= */

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

HCL_EXPORT void hcl_client_seterrbfmt (
	hcl_client_t*    client,
	hcl_errnum_t     errnum,
	const hcl_bch_t* fmt,
	...
);

HCL_EXPORT void hcl_client_seterrufmt (
	hcl_client_t*    client,
	hcl_errnum_t     errnum,
	const hcl_uch_t* fmt,
	...
);

HCL_EXPORT void hcl_client_logbfmt (
	hcl_client_t*    client,
	unsigned int     mask,
	const hcl_bch_t* fmt,
	...
);

HCL_EXPORT void hcl_client_logufmt (
	hcl_client_t*    client,
	unsigned int     mask,
	const hcl_uch_t* fmt,
	...
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
