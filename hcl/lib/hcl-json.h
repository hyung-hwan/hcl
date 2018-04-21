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

typedef struct hcl_json_t hcl_json_t;

enum hcl_json_option_t
{
	HCL_JSON_TRAIT,
	HCL_JSON_LOG_MASK,
};
typedef enum hcl_json_option_t hcl_json_option_t;

enum hcl_json_trait_t
{
	/* no trait defined at this moment. XXXX is just a placeholder */
	HCL_JSON_XXXX  = (1 << 0)
};
typedef enum hcl_json_trait_t hcl_json_trait_t;

/* ========================================================================= */

enum hcl_json_state_t
{
	HCL_JSON_STATE_START,
	HCL_JSON_STATE_ARRAY_STARTED,
	HCL_JSON_STATE_OBJECT_STARTED,

	HCL_JSON_STATE_IN_WORD_VALUE,
	HCL_JSON_STATE_IN_NUMERIC_VALUE,
	HCL_JSON_STATE_IN_QUOTED_VALUE
};
typedef enum hcl_json_state_t hcl_json_state_t;

/* ========================================================================= */

#if 0
struct hcl_json_root_t
{
	int type;
	hcl_json_value_t* value;
};

struct hcl_json_list_t
{
	int type; /* array or table */
	hcl_json_pair_t* cell;
};

struct hcl_json_value_t
{
	int type; /* atom or pair */
	union
	{
		hcl_json_value_t* value;
		hcl_json_pair_t* cell;
	} u;
};

struct hcl_json_atom_t
{
	int type; /* string, word, number */
};

struct hcl_json_pair_t
{
	hcl_json_atom_t* key;
	hcl_json_value_t* value;
	hcl_json_pair_t* next;
};
#endif
/* ========================================================================= */
enum hcl_json_reply_type_t
{
	HCL_JSON_REPLY_TYPE_OK = 0,
	HCL_JSON_REPLY_TYPE_ERROR = 1
};
typedef enum hcl_json_reply_type_t hcl_json_reply_type_t;

typedef void (*hcl_json_log_write_t) (
	hcl_json_t*     json,
	unsigned int      mask,
	const hcl_ooch_t* msg,
	hcl_oow_t         len
);

typedef int (*hcl_json_start_reply_t) (
	hcl_json_t*           json,
	hcl_json_reply_type_t type,
	const hcl_ooch_t*       dptr,
	hcl_oow_t               dlen
);

typedef int (*hcl_json_feed_attr_t) (
	hcl_json_t*     json,
	const hcl_oocs_t* key,
	const hcl_oocs_t* val
);

typedef int (*hcl_json_start_data_t) (
	hcl_json_t*     json
);

typedef int (*hcl_json_feed_data_t) (
	hcl_json_t*     json,
	const void*       ptr,
	hcl_oow_t         len
);

typedef int (*hcl_json_end_data_t) (
	hcl_json_t*     json
);

enum hcl_json_end_reply_state_t
{
	HCL_JSON_END_REPLY_STATE_OK,
	HCL_JSON_END_REPLY_STATE_REVOKED
};
typedef enum hcl_json_end_reply_state_t hcl_json_end_reply_state_t;

typedef int (*hcl_json_end_reply_t) (
	hcl_json_t*                json,
	hcl_json_end_reply_state_t state
);

struct hcl_json_prim_t
{
	hcl_json_log_write_t     log_write;

	hcl_json_start_reply_t   start_reply;   /* mandatory */
	hcl_json_feed_attr_t     feed_attr; /* optional */
	hcl_json_feed_data_t     feed_data;     /* optional */
	hcl_json_end_reply_t     end_reply;     /* mandatory */
};
typedef struct hcl_json_prim_t hcl_json_prim_t;

/* ========================================================================= */

#if defined(__cplusplus)
extern "C" {
#endif

HCL_EXPORT hcl_json_t* hcl_json_open (
	hcl_mmgr_t*        mmgr,
	hcl_oow_t          xtnsize,
	hcl_json_prim_t* prim,
	hcl_errnum_t*      errnum
);

HCL_EXPORT void hcl_json_close (
	hcl_json_t* json
);

HCL_EXPORT void hcl_json_reset (
	hcl_json_t* json
);

HCL_EXPORT int hcl_json_feed (
	hcl_json_t* json,
	const void*   ptr,
	hcl_oow_t     len,
	hcl_oow_t*    xlen
);

HCL_EXPORT hcl_json_state_t hcl_json_getstate (
	hcl_json_t* json
);

HCL_EXPORT int hcl_json_setoption (
	hcl_json_t*       json,
	hcl_json_option_t id,
	const void*         value
);

HCL_EXPORT int hcl_json_getoption (
	hcl_json_t*       json,
	hcl_json_option_t id,
	void*               value
);


HCL_EXPORT void* hcl_json_getxtn (
	hcl_json_t* json
);

HCL_EXPORT hcl_mmgr_t* hcl_json_getmmgr (
	hcl_json_t* json
);

HCL_EXPORT hcl_cmgr_t* hcl_json_getcmgr (
	hcl_json_t* json
);

HCL_EXPORT void hcl_json_setcmgr (
	hcl_json_t* json,
	hcl_cmgr_t*   cmgr
);


HCL_EXPORT hcl_errnum_t hcl_json_geterrnum (
	hcl_json_t* json
);

HCL_EXPORT const hcl_ooch_t* hcl_json_geterrstr (
	hcl_json_t* json
);

HCL_EXPORT const hcl_ooch_t* hcl_json_geterrmsg (
	hcl_json_t* json
);

HCL_EXPORT void hcl_json_seterrnum (
	hcl_json_t* json,
	hcl_errnum_t  errnum
);

HCL_EXPORT void hcl_json_seterrbfmt (
	hcl_json_t*    json,
	hcl_errnum_t     errnum,
	const hcl_bch_t* fmt,
	...
);

HCL_EXPORT void hcl_json_seterrufmt (
	hcl_json_t*    json,
	hcl_errnum_t     errnum,
	const hcl_uch_t* fmt,
	...
);

HCL_EXPORT void hcl_json_logbfmt (
	hcl_json_t*    json,
	unsigned int     mask,
	const hcl_bch_t* fmt,
	...
);

HCL_EXPORT void hcl_json_logufmt (
	hcl_json_t*    json,
	unsigned int     mask,
	const hcl_uch_t* fmt,
	...
);

HCL_EXPORT void* hcl_json_allocmem (
	hcl_json_t* json,
	hcl_oow_t     size
);

HCL_EXPORT void* hcl_json_callocmem (
	hcl_json_t* json,
	hcl_oow_t     size
);

HCL_EXPORT void* hcl_json_reallocmem (
	hcl_json_t* json,
	void*         ptr,
	hcl_oow_t     size
);


HCL_EXPORT void hcl_json_freemem (
	hcl_json_t* json,
	void*         ptr
);

#if defined(__cplusplus)
}
#endif

#endif
