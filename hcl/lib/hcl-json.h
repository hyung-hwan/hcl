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

#ifndef _HCL_JSON_H_
#define _HCL_JSON_H_

#include <hcl.h>

typedef struct hcl_jsoner_t hcl_jsoner_t;

enum hcl_jsoner_option_t
{
	HCL_JSON_TRAIT,
	HCL_JSON_LOG_MASK,
};
typedef enum hcl_jsoner_option_t hcl_jsoner_option_t;

enum hcl_jsoner_trait_t
{
	/* no trait defined at this moment. XXXX is just a placeholder */
	HCL_JSON_XXXX  = (1 << 0)
};
typedef enum hcl_jsoner_trait_t hcl_jsoner_trait_t;

/* ========================================================================= */

enum hcl_jsoner_state_t
{
	HCL_JSON_STATE_START,
	HCL_JSON_STATE_ARRAY_STARTED,
	HCL_JSON_STATE_OBJECT_STARTED,

	HCL_JSON_STATE_IN_WORD_VALUE,
	HCL_JSON_STATE_IN_NUMERIC_VALUE,
	HCL_JSON_STATE_IN_QUOTED_VALUE
};
typedef enum hcl_jsoner_state_t hcl_jsoner_state_t;

/* ========================================================================= */

#if 0
struct hcl_jsoner_root_t
{
	int type;
	hcl_jsoner_value_t* value;
};

struct hcl_jsoner_list_t
{
	int type; /* array or table */
	hcl_jsoner_pair_t* cell;
};

struct hcl_jsoner_value_t
{
	int type; /* atom or pair */
	union
	{
		hcl_jsoner_value_t* value;
		hcl_jsoner_pair_t* cell;
	} u;
};

struct hcl_jsoner_atom_t
{
	int type; /* string, word, number */
};

struct hcl_jsoner_pair_t
{
	hcl_jsoner_atom_t* key;
	hcl_jsoner_value_t* value;
	hcl_jsoner_pair_t* next;
};
#endif
/* ========================================================================= */
enum hcl_jsoner_reply_type_t
{
	HCL_JSON_REPLY_TYPE_OK = 0,
	HCL_JSON_REPLY_TYPE_ERROR = 1
};
typedef enum hcl_jsoner_reply_type_t hcl_jsoner_reply_type_t;

typedef void (*hcl_jsoner_log_write_t) (
	hcl_jsoner_t*     json,
	unsigned int      mask,
	const hcl_ooch_t* msg,
	hcl_oow_t         len
);

typedef int (*hcl_jsoner_start_reply_t) (
	hcl_jsoner_t*           json,
	hcl_jsoner_reply_type_t type,
	const hcl_ooch_t*       dptr,
	hcl_oow_t               dlen
);

typedef int (*hcl_jsoner_feed_attr_t) (
	hcl_jsoner_t*     json,
	const hcl_oocs_t* key,
	const hcl_oocs_t* val
);

typedef int (*hcl_jsoner_start_data_t) (
	hcl_jsoner_t*     json
);

typedef int (*hcl_jsoner_feed_data_t) (
	hcl_jsoner_t*     json,
	const void*       ptr,
	hcl_oow_t         len
);

typedef int (*hcl_jsoner_end_data_t) (
	hcl_jsoner_t*     json
);

enum hcl_jsoner_end_reply_state_t
{
	HCL_JSON_END_REPLY_STATE_OK,
	HCL_JSON_END_REPLY_STATE_REVOKED
};
typedef enum hcl_jsoner_end_reply_state_t hcl_jsoner_end_reply_state_t;

typedef int (*hcl_jsoner_end_reply_t) (
	hcl_jsoner_t*                json,
	hcl_jsoner_end_reply_state_t state
);

struct hcl_jsoner_prim_t
{
	hcl_jsoner_log_write_t     log_write;

	hcl_jsoner_start_reply_t   start_reply;   /* mandatory */
	hcl_jsoner_feed_attr_t     feed_attr; /* optional */
	hcl_jsoner_feed_data_t     feed_data;     /* optional */
	hcl_jsoner_end_reply_t     end_reply;     /* mandatory */
};
typedef struct hcl_jsoner_prim_t hcl_jsoner_prim_t;

/* ========================================================================= */

#if defined(__cplusplus)
extern "C" {
#endif

HCL_EXPORT hcl_jsoner_t* hcl_jsoner_open (
	hcl_mmgr_t*        mmgr,
	hcl_oow_t          xtnsize,
	hcl_jsoner_prim_t* prim,
	hcl_errnum_t*      errnum
);

HCL_EXPORT void hcl_jsoner_close (
	hcl_jsoner_t* json
);

HCL_EXPORT void hcl_jsoner_reset (
	hcl_jsoner_t* json
);

HCL_EXPORT int hcl_jsoner_feed (
	hcl_jsoner_t* json,
	const void*   ptr,
	hcl_oow_t     len,
	hcl_oow_t*    xlen
);

HCL_EXPORT hcl_jsoner_state_t hcl_jsoner_getstate (
	hcl_jsoner_t* json
);

HCL_EXPORT int hcl_jsoner_setoption (
	hcl_jsoner_t*       json,
	hcl_jsoner_option_t id,
	const void*         value
);

HCL_EXPORT int hcl_jsoner_getoption (
	hcl_jsoner_t*       json,
	hcl_jsoner_option_t id,
	void*               value
);


HCL_EXPORT void* hcl_jsoner_getxtn (
	hcl_jsoner_t* json
);

HCL_EXPORT hcl_mmgr_t* hcl_jsoner_getmmgr (
	hcl_jsoner_t* json
);

HCL_EXPORT hcl_cmgr_t* hcl_jsoner_getcmgr (
	hcl_jsoner_t* json
);

HCL_EXPORT void hcl_jsoner_setcmgr (
	hcl_jsoner_t* json,
	hcl_cmgr_t*   cmgr
);


HCL_EXPORT hcl_errnum_t hcl_jsoner_geterrnum (
	hcl_jsoner_t* json
);

HCL_EXPORT const hcl_ooch_t* hcl_jsoner_geterrstr (
	hcl_jsoner_t* json
);

HCL_EXPORT const hcl_ooch_t* hcl_jsoner_geterrmsg (
	hcl_jsoner_t* json
);

HCL_EXPORT void hcl_jsoner_seterrnum (
	hcl_jsoner_t* json,
	hcl_errnum_t  errnum
);

HCL_EXPORT void hcl_jsoner_seterrbfmt (
	hcl_jsoner_t*    json,
	hcl_errnum_t     errnum,
	const hcl_bch_t* fmt,
	...
);

HCL_EXPORT void hcl_jsoner_seterrufmt (
	hcl_jsoner_t*    json,
	hcl_errnum_t     errnum,
	const hcl_uch_t* fmt,
	...
);

HCL_EXPORT void hcl_jsoner_logbfmt (
	hcl_jsoner_t*    json,
	unsigned int     mask,
	const hcl_bch_t* fmt,
	...
);

HCL_EXPORT void hcl_jsoner_logufmt (
	hcl_jsoner_t*    json,
	unsigned int     mask,
	const hcl_uch_t* fmt,
	...
);

HCL_EXPORT void* hcl_jsoner_allocmem (
	hcl_jsoner_t* json,
	hcl_oow_t     size
);

HCL_EXPORT void* hcl_jsoner_callocmem (
	hcl_jsoner_t* json,
	hcl_oow_t     size
);

HCL_EXPORT void* hcl_jsoner_reallocmem (
	hcl_jsoner_t* json,
	void*         ptr,
	hcl_oow_t     size
);


HCL_EXPORT void hcl_jsoner_freemem (
	hcl_jsoner_t* json,
	void*         ptr
);

#if defined(__cplusplus)
}
#endif

#endif
