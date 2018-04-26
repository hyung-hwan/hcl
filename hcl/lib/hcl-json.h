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

/** 
 * The hcl_json_t type defines a simple json parser.
 */
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
	HCL_JSON_STATE_IN_ARRAY,
	HCL_JSON_STATE_IN_DIC,

	HCL_JSON_STATE_IN_WORD_VALUE,
	HCL_JSON_STATE_IN_NUMERIC_VALUE,
	HCL_JSON_STATE_IN_STRING_VALUE,
	HCL_JSON_STATE_IN_CHARACTER_VALUE
};
typedef enum hcl_json_state_t hcl_json_state_t;


/* ========================================================================= */
enum hcl_json_inst_t
{
	HCL_JSON_INST_START_ARRAY,
	HCL_JSON_INST_END_ARRAY,
	HCL_JSON_INST_START_DIC,
	HCL_JSON_INST_END_DIC,

	HCL_JSON_INST_KEY,

	HCL_JSON_INST_CHARACTER, /* there is no such element as character in real JSON */
	HCL_JSON_INST_STRING,
	HCL_JSON_INST_NUMBER,
	HCL_JSON_INST_NIL,
	HCL_JSON_INST_TRUE,
	HCL_JSON_INST_FALSE,
};
typedef enum hcl_json_inst_t hcl_json_inst_t;

typedef void (*hcl_json_log_write_t) (
	hcl_json_t*       json,
	hcl_bitmask_t   mask,
	const hcl_ooch_t* msg,
	hcl_oow_t         len
);

typedef int (*hcl_json_instcb_t) (
	hcl_json_t*           json,
	hcl_json_inst_t       inst,
	const hcl_oocs_t*     str
);

struct hcl_json_prim_t
{
	hcl_json_log_write_t     log_write;
	hcl_json_instcb_t        instcb;
};
typedef struct hcl_json_prim_t hcl_json_prim_t;

/* ========================================================================= */

#if defined(__cplusplus)
extern "C" {
#endif

HCL_EXPORT hcl_json_t* hcl_json_open (
	hcl_mmgr_t*        mmgr,
	hcl_oow_t          xtnsize,
	hcl_json_prim_t*   prim,
	hcl_errnum_t*      errnum
);

HCL_EXPORT void hcl_json_close (
	hcl_json_t* json
);

HCL_EXPORT void hcl_json_reset (
	hcl_json_t* json
);

HCL_EXPORT int hcl_json_feed (
	hcl_json_t*   json,
	const void*   ptr,
	hcl_oow_t     len,
	hcl_oow_t*    xlen
);

HCL_EXPORT hcl_json_state_t hcl_json_getstate (
	hcl_json_t* json
);

HCL_EXPORT int hcl_json_setoption (
	hcl_json_t*         json,
	hcl_json_option_t   id,
	const void*         value
);

HCL_EXPORT int hcl_json_getoption (
	hcl_json_t*         json,
	hcl_json_option_t   id,
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
	hcl_json_t*      json,
	hcl_bitmask_t  mask,
	const hcl_bch_t* fmt,
	...
);

HCL_EXPORT void hcl_json_logufmt (
	hcl_json_t*      json,
	hcl_bitmask_t  mask,
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
