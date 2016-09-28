
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

#include "hcl-utl.h"

hcl_oow_t hcl_hashbytes (const hcl_oob_t* ptr, hcl_oow_t len)
{
	hcl_oow_t h = 0;
	const hcl_uint8_t* bp, * be;

	bp = ptr; be = bp + len;
	while (bp < be) h = h * 31 + *bp++;

	return h;
}

hcl_oow_t hcl_hashuchars (const hcl_uch_t* ptr, hcl_oow_t len)
{
	return hcl_hashbytes ((const hcl_oob_t *)ptr, len * HCL_SIZEOF(*ptr));
}

int hcl_equalchars (const hcl_uch_t* str1, const hcl_uch_t* str2, hcl_oow_t len)
{
	hcl_oow_t i;

	for (i = 0; i < len; i++)
	{
		if (str1[i] != str2[i]) return 0;
	}

	return 1;
}

int hcl_compucstr (const hcl_uch_t* str1, const hcl_uch_t* str2)
{
	while (*str1 == *str2)
	{
		if (*str1 == '\0') return 0;
		str1++, str2++;
	}

	return (*str1 > *str2)? 1: -1;
}

int hcl_compbcstr (const hcl_bch_t* str1, const hcl_bch_t* str2)
{
	while (*str1 == *str2)
	{
		if (*str1 == '\0') return 0;
		str1++, str2++;
	}

	return (*str1 > *str2)? 1: -1;
}

int hcl_compucbcstr (const hcl_uch_t* str1, const hcl_bch_t* str2)
{
	while (*str1 == *str2)
	{
		if (*str1 == '\0') return 0;
		str1++, str2++;
	}

	return (*str1 > *str2)? 1: -1;
}

int hcl_compucxbcstr (const hcl_uch_t* str1, hcl_oow_t len, const hcl_bch_t* str2)
{
	const hcl_uch_t* end = str1 + len;
	while (str1 < end && *str2 != '\0' && *str1 == *str2) str1++, str2++;
	if (str1 == end && *str2 == '\0') return 0;
	if (*str1 == *str2) return (str1 < end)? 1: -1;
	return (*str1 > *str2)? 1: -1;
}

void hcl_copyuchars (hcl_uch_t* dst, const hcl_uch_t* src, hcl_oow_t len)
{
	hcl_oow_t i;
	for (i = 0; i < len; i++) dst[i] = src[i];
}

void hcl_copybchars (hcl_bch_t* dst, const hcl_bch_t* src, hcl_oow_t len)
{
	hcl_oow_t i;
	for (i = 0; i < len; i++) dst[i] = src[i];
}

void hcl_copybchtouchars (hcl_uch_t* dst, const hcl_bch_t* src, hcl_oow_t len)
{
	hcl_oow_t i;
	for (i = 0; i < len; i++) dst[i] = src[i];
}

hcl_oow_t hcl_copyucstr (hcl_uch_t* dst, hcl_oow_t len, const hcl_uch_t* src)
{
	hcl_uch_t* p, * p2;

	p = dst; p2 = dst + len - 1;

	while (p < p2)
	{
		 if (*src == '\0') break;
		 *p++ = *src++;
	}

	if (len > 0) *p = '\0';
	return p - dst;
}

hcl_oow_t hcl_copybcstr (hcl_bch_t* dst, hcl_oow_t len, const hcl_bch_t* src)
{
	hcl_bch_t* p, * p2;

	p = dst; p2 = dst + len - 1;

	while (p < p2)
	{
		 if (*src == '\0') break;
		 *p++ = *src++;
	}

	if (len > 0) *p = '\0';
	return p - dst;
}

hcl_oow_t hcl_countucstr (const hcl_uch_t* str)
{
	const hcl_uch_t* ptr = str;
	while (*ptr != '\0') ptr++;
	return ptr - str;
}

hcl_oow_t hcl_countbcstr (const hcl_bch_t* str)
{
	const hcl_bch_t* ptr = str;
	while (*ptr != '\0') ptr++;
	return ptr - str;
}

hcl_uch_t* hcl_finduchar (const hcl_uch_t* ptr, hcl_oow_t len, hcl_uch_t c)
{
	const hcl_uch_t* end;

	end = ptr + len;
	while (ptr < end)
	{
		if (*ptr == c) return (hcl_uch_t*)ptr;
		ptr++;
	}

	return HCL_NULL;
}

hcl_bch_t* hcl_findbchar (const hcl_bch_t* ptr, hcl_oow_t len, hcl_bch_t c)
{
	const hcl_bch_t* end;

	end = ptr + len;
	while (ptr < end)
	{
		if (*ptr == c) return (hcl_bch_t*)ptr;
		ptr++;
	}

	return HCL_NULL;
}

