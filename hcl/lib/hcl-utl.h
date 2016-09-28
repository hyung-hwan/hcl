/*
 * $Id$
 *
    Copyright (c) 2014-2015 Chung, Hyung-Hwan. All rights reserved.

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

#ifndef _HCL_UTL_H_
#define _HCL_UTL_H_

#include "hcl-cmn.h"

#if defined(__cplusplus)
extern "C" {
#endif

#if defined(HCL_OOCH_IS_UCH)
#	define hcl_hashchars(ptr,len) hcl_hashuchars(ptr,len)
#	define hcl_compoocbcstr(str1,str2) hcl_compucbcstr(str1,str2)
#	define hcl_compoocstr(str1,str2) hcl_compucstr(str1,str2)
#	define hcl_copyoochars(dst,src,len) hcl_copyuchars(dst,src,len)
#	define hcl_copybchtooochars(dst,src,len) hcl_copybchtouchars(dst,src,len)
#	define hcl_copyoocstr(dst,len,src) hcl_copyucstr(dst,len,src)
#	define hcl_findoochar(ptr,len,c) hcl_finduchar(ptr,len,c)
#	define hcl_countoocstr(str) hcl_countucstr(str)
#else
#	define hcl_hashchars(ptr,len) hcl_hashbchars(ptr,len)
#	define hcl_compoocbcstr(str1,str2) hcl_compbcstr(str1,str2)
#	define hcl_compoocstr(str1,str2) hcl_compbcstr(str1,str2)
#	define hcl_copyoochars(dst,src,len) hcl_copybchars(dst,src,len)
#	define hcl_copybchtooochars(dst,src,len) hcl_copybchars(dst,src,len)
#	define hcl_copyoocstr(dst,len,src) hcl_copybcstr(dst,len,src)
#	define hcl_findoochar(ptr,len,c) hcl_findbchar(ptr,len,c)
#	define hcl_countoocstr(str) hcl_countbcstr(str)
#endif


/* ========================================================================= */
/* hcl-utl.c                                                                */
/* ========================================================================= */
hcl_oow_t hcl_hashbytes (
	const hcl_oob_t* ptr,
	hcl_oow_t        len
);

hcl_oow_t hcl_hashuchars (
	const hcl_uch_t*  ptr,
	hcl_oow_t        len
);

#define hcl_hashbchars(ptr,len) hcl_hashbytes(ptr,len)


int hcl_equalchars (
	const hcl_uch_t*  str1,
	const hcl_uch_t*  str2,
	hcl_oow_t        len
);

int hcl_compucstr (
	const hcl_uch_t* str1,
	const hcl_uch_t* str2
);

int hcl_compbcstr (
	const hcl_bch_t* str1,
	const hcl_bch_t* str2
);

int hcl_compucbcstr (
	const hcl_uch_t* str1,
	const hcl_bch_t* str2
);

int hcl_compucxbcstr (
	const hcl_uch_t* str1,
	hcl_oow_t       len,
	const hcl_bch_t* str2
);

void hcl_copyuchars (
	hcl_uch_t*       dst,
	const hcl_uch_t* src,
	hcl_oow_t       len
);

void hcl_copybchars (
	hcl_bch_t*       dst,
	const hcl_bch_t* src,
	hcl_oow_t       len
);

void hcl_copybchtouchars (
	hcl_uch_t*       dst,
	const hcl_bch_t* src,
	hcl_oow_t       len
);

hcl_oow_t hcl_copyucstr (
	hcl_uch_t*       dst,
	hcl_oow_t       len,
	const hcl_uch_t* src
);

hcl_oow_t hcl_copybcstr (
	hcl_bch_t*       dst,
	hcl_oow_t       len,
	const hcl_bch_t* src
);

hcl_uch_t* hcl_finduchar (
	const hcl_uch_t* ptr,
	hcl_oow_t       len,
	hcl_uch_t        c
);

hcl_bch_t* hcl_findbchar (
	const hcl_bch_t* ptr,
	hcl_oow_t       len,
	hcl_bch_t        c
);

hcl_oow_t hcl_countucstr (
	const hcl_uch_t* str
);

hcl_oow_t hcl_countbcstr (
	const hcl_bch_t* str
);

#if defined(__cplusplus)
}
#endif


#endif
