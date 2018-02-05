/*
 * $Id$
 *
    Copyright (c) 2014-2017 Chung, Hyung-Hwan. All rights reserved.

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

#include "hcl-prv.h"

#define HCL_BCLEN_MAX 6

/* some naming conventions
 *  bchars, uchars -> pointer and length
 *  bcstr, ucstr -> null-terminated string pointer
 *  btouchars -> bchars to uchars
 *  utobchars -> uchars to bchars
 *  btoucstr -> bcstr to ucstr
 *  utobcstr -> ucstr to bcstr
 */

hcl_oow_t hcl_hashbytes (const hcl_oob_t* ptr, hcl_oow_t len)
{
	hcl_oow_t h = 0;
	const hcl_uint8_t* bp, * be;

	bp = ptr; be = bp + len;
	while (bp < be) h = h * 31 + *bp++;

	/* constrain the hash value to be representable in a small integer
	 * for convenience sake */
	return h % ((hcl_oow_t)HCL_SMOOI_MAX + 1);
}

int hcl_equaluchars (const hcl_uch_t* str1, const hcl_uch_t* str2, hcl_oow_t len)
{
	hcl_oow_t i;

	/* NOTE: you should call this function after having ensured that
	 *       str1 and str2 are in the same length */

	for (i = 0; i < len; i++)
	{
		if (str1[i] != str2[i]) return 0;
	}

	return 1;
}

int hcl_equalbchars (const hcl_bch_t* str1, const hcl_bch_t* str2, hcl_oow_t len)
{
	hcl_oow_t i;

	/* NOTE: you should call this function after having ensured that
	 *       str1 and str2 are in the same length */

	for (i = 0; i < len; i++)
	{
		if (str1[i] != str2[i]) return 0;
	}

	return 1;
}

int hcl_compuchars (const hcl_uch_t* str1, hcl_oow_t len1, const hcl_uch_t* str2, hcl_oow_t len2)
{
	hcl_uchu_t c1, c2;
	const hcl_uch_t* end1 = str1 + len1;
	const hcl_uch_t* end2 = str2 + len2;

	while (str1 < end1)
	{
		c1 = *str1;
		if (str2 < end2) 
		{
			c2 = *str2;
			if (c1 > c2) return 1;
			if (c1 < c2) return -1;
		}
		else return 1;
		str1++; str2++;
	}

	return (str2 < end2)? -1: 0;
}

int hcl_compbchars (const hcl_bch_t* str1, hcl_oow_t len1, const hcl_bch_t* str2, hcl_oow_t len2)
{
	hcl_bchu_t c1, c2;
	const hcl_bch_t* end1 = str1 + len1;
	const hcl_bch_t* end2 = str2 + len2;

	while (str1 < end1)
	{
		c1 = *str1;
		if (str2 < end2) 
		{
			c2 = *str2;
			if (c1 > c2) return 1;
			if (c1 < c2) return -1;
		}
		else return 1;
		str1++; str2++;
	}

	return (str2 < end2)? -1: 0;
}

int hcl_compucstr (const hcl_uch_t* str1, const hcl_uch_t* str2)
{
	while (*str1 == *str2)
	{
		if (*str1 == '\0') return 0;
		str1++; str2++;
	}

	return ((hcl_uchu_t)*str1 > (hcl_uchu_t)*str2)? 1: -1;
}

int hcl_compbcstr (const hcl_bch_t* str1, const hcl_bch_t* str2)
{
	while (*str1 == *str2)
	{
		if (*str1 == '\0') return 0;
		str1++; str2++;
	}

	return ((hcl_bchu_t)*str1 > (hcl_bchu_t)*str2)? 1: -1;
}

int hcl_compucbcstr (const hcl_uch_t* str1, const hcl_bch_t* str2)
{
	while (*str1 == *str2)
	{
		if (*str1 == '\0') return 0;
		str1++; str2++;
	}

	return ((hcl_uchu_t)*str1 > (hcl_bchu_t)*str2)? 1: -1;
}

int hcl_compucharsucstr (const hcl_uch_t* str1, hcl_oow_t len, const hcl_uch_t* str2)
{
	/* for "abc\0" of length 4 vs "abc", the fourth character
	 * of the first string is equal to the terminating null of
	 * the second string. the first string is still considered 
	 * bigger */
	const hcl_uch_t* end = str1 + len;
	while (str1 < end && *str2 != '\0') 
	{
		if (*str1 != *str2) return ((hcl_uchu_t)*str1 > (hcl_uchu_t)*str2)? 1: -1;
		str1++; str2++;
	}
	return (str1 < end)? 1: (*str2 == '\0'? 0: -1);
}

int hcl_compucharsbcstr (const hcl_uch_t* str1, hcl_oow_t len, const hcl_bch_t* str2)
{
	const hcl_uch_t* end = str1 + len;
	while (str1 < end && *str2 != '\0') 
	{
		if (*str1 != *str2) return ((hcl_uchu_t)*str1 > (hcl_bchu_t)*str2)? 1: -1;
		str1++; str2++;
	}
	return (str1 < end)? 1: (*str2 == '\0'? 0: -1);
}

int hcl_compbcharsbcstr (const hcl_bch_t* str1, hcl_oow_t len, const hcl_bch_t* str2)
{
	const hcl_bch_t* end = str1 + len;
	while (str1 < end && *str2 != '\0') 
	{
		if (*str1 != *str2) return ((hcl_bchu_t)*str1 > (hcl_bchu_t)*str2)? 1: -1;
		str1++; str2++;
	}
	return (str1 < end)? 1: (*str2 == '\0'? 0: -1);
}

int hcl_compbcharsucstr (const hcl_bch_t* str1, hcl_oow_t len, const hcl_uch_t* str2)
{
	const hcl_bch_t* end = str1 + len;
	while (str1 < end && *str2 != '\0') 
	{
		if (*str1 != *str2) return ((hcl_bchu_t)*str1 > (hcl_uchu_t)*str2)? 1: -1;
		str1++; str2++;
	}
	return (str1 < end)? 1: (*str2 == '\0'? 0: -1);
}

void hcl_copyuchars (hcl_uch_t* dst, const hcl_uch_t* src, hcl_oow_t len)
{
	/* take note of no forced null termination */
	hcl_oow_t i;
	for (i = 0; i < len; i++) dst[i] = src[i];
}

void hcl_copybchars (hcl_bch_t* dst, const hcl_bch_t* src, hcl_oow_t len)
{
	/* take note of no forced null termination */
	hcl_oow_t i;
	for (i = 0; i < len; i++) dst[i] = src[i];
}

void hcl_copybtouchars (hcl_uch_t* dst, const hcl_bch_t* src, hcl_oow_t len)
{
	/* copy without conversions.
	 * use hcl_bctouchars() for conversion encoding */
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

hcl_uch_t* hcl_rfinduchar (const hcl_uch_t* ptr, hcl_oow_t len, hcl_uch_t c)
{
	const hcl_uch_t* cur;

	cur = ptr + len;
	while (cur > ptr)
	{
		--cur;
		if (*cur == c) return (hcl_uch_t*)cur;
	}

	return HCL_NULL;
}

hcl_bch_t* hcl_rfindbchar (const hcl_bch_t* ptr, hcl_oow_t len, hcl_bch_t c)
{
	const hcl_bch_t* cur;

	cur = ptr + len;
	while (cur > ptr)
	{
		--cur;
		if (*cur == c) return (hcl_bch_t*)cur;
	}

	return HCL_NULL;
}


hcl_uch_t* hcl_finducharinucstr (const hcl_uch_t* ptr, hcl_uch_t c)
{
	while (*ptr != '\0')
	{
		if (*ptr == c) return (hcl_uch_t*)ptr;
		ptr++;
	}

	return HCL_NULL;
}

hcl_bch_t* hcl_findbcharinbcstr (const hcl_bch_t* ptr, hcl_bch_t c)
{
	while (*ptr != '\0')
	{
		if (*ptr == c) return (hcl_bch_t*)ptr;
		ptr++;
	}

	return HCL_NULL;
}
/* ----------------------------------------------------------------------- */

int hcl_concatoocstrtosbuf (hcl_t* hcl, const hcl_ooch_t* str, int id)
{
	hcl_sbuf_t* p;
	hcl_oow_t len;

	p = &hcl->sbuf[id];
	len = hcl_countoocstr (str);

	if (len > p->capa - p->len)
	{
		hcl_oow_t newcapa;
		hcl_ooch_t* tmp;

		newcapa = HCL_ALIGN(p->len + len, 512); /* TODO: adjust this capacity */

		/* +1 to handle line ending injection more easily */
		tmp = hcl_reallocmem (hcl, p->ptr, (newcapa + 1) * HCL_SIZEOF(*tmp)); 
		if (!tmp) return -1;

		p->ptr = tmp;
		p->capa = newcapa;
	}

	hcl_copyoochars (&p->ptr[p->len], str, len);
	p->len += len;
	p->ptr[p->len] = '\0';

	return 0;
}

int hcl_copyoocstrtosbuf (hcl_t* hcl, const hcl_ooch_t* str, int id)
{
	hcl->sbuf[id].len = 0;;
	return hcl_concatoocstrtosbuf (hcl, str, id);
}


/* ----------------------------------------------------------------------- */

static HCL_INLINE int bcsn_to_ucsn_with_cmgr (
	const hcl_bch_t* bcs, hcl_oow_t* bcslen,
	hcl_uch_t* ucs, hcl_oow_t* ucslen, hcl_cmgr_t* cmgr, int all)
{
	const hcl_bch_t* p;
	int ret = 0;
	hcl_oow_t mlen;

	if (ucs)
	{
		/* destination buffer is specified. 
		 * copy the conversion result to the buffer */

		hcl_uch_t* q, * qend;

		p = bcs;
		q = ucs;
		qend = ucs + *ucslen;
		mlen = *bcslen;

		while (mlen > 0)
		{
			hcl_oow_t n;

			if (q >= qend)
			{
				/* buffer too small */
				ret = -2;
				break;
			}

			n = cmgr->bctouc (p, mlen, q);
			if (n == 0)
			{
				/* invalid sequence */
				if (all)
				{
					n = 1;
					*q = '?';
				}
				else
				{
					ret = -1;
					break;
				}
			}
			if (n > mlen)
			{
				/* incomplete sequence */
				if (all)
				{
					n = 1;
					*q = '?';
				}
				else
				{
					ret = -3;
					break;
				}
			}

			q++;
			p += n;
			mlen -= n;
		}

		*ucslen = q - ucs;
		*bcslen = p - bcs;
	}
	else
	{
		/* no destination buffer is specified. perform conversion
		 * but don't copy the result. the caller can call this function
		 * without a buffer to find the required buffer size, allocate
		 * a buffer with the size and call this function again with 
		 * the buffer. */

		hcl_uch_t w;
		hcl_oow_t wlen = 0;

		p = bcs;
		mlen = *bcslen;

		while (mlen > 0)
		{
			hcl_oow_t n;

			n = cmgr->bctouc (p, mlen, &w);
			if (n == 0)
			{
				/* invalid sequence */
				if (all) n = 1;
				else
				{
					ret = -1;
					break;
				}
			}
			if (n > mlen)
			{
				/* incomplete sequence */
				if (all) n = 1;
				else
				{
					ret = -3;
					break;
				}
			}

			p += n;
			mlen -= n;
			wlen += 1;
		}

		*ucslen = wlen;
		*bcslen = p - bcs;
	}

	return ret;
}

static HCL_INLINE int bcs_to_ucs_with_cmgr (
	const hcl_bch_t* bcs, hcl_oow_t* bcslen,
	hcl_uch_t* ucs, hcl_oow_t* ucslen, hcl_cmgr_t* cmgr, int all)
{
	const hcl_bch_t* bp;
	hcl_oow_t mlen, wlen;
	int n;

	for (bp = bcs; *bp != '\0'; bp++) /* nothing */ ;

	mlen = bp - bcs; wlen = *ucslen;
	n = bcsn_to_ucsn_with_cmgr (bcs, &mlen, ucs, &wlen, cmgr, all);
	if (ucs)
	{
		/* null-terminate the target buffer if it has room for it. */
		if (wlen < *ucslen) ucs[wlen] = '\0';
		else n = -2; /* buffer too small */
	}
	*bcslen = mlen; *ucslen = wlen;

	return n;
}

static HCL_INLINE int ucsn_to_bcsn_with_cmgr (
	const hcl_uch_t* ucs, hcl_oow_t* ucslen,
	hcl_bch_t* bcs, hcl_oow_t* bcslen, hcl_cmgr_t* cmgr)
{
	const hcl_uch_t* p = ucs;
	const hcl_uch_t* end = ucs + *ucslen;
	int ret = 0; 

	if (bcs)
	{
		hcl_oow_t rem = *bcslen;

		while (p < end) 
		{
			hcl_oow_t n;

			if (rem <= 0)
			{
				ret = -2; /* buffer too small */
				break;
			}

			n = cmgr->uctobc (*p, bcs, rem);
			if (n == 0) 
			{
				ret = -1;
				break; /* illegal character */
			}
			if (n > rem) 
			{
				ret = -2; /* buffer too small */
				break;
			}
			bcs += n; rem -= n; p++;
		}

		*bcslen -= rem; 
	}
	else
	{
		hcl_bch_t bcsbuf[HCL_BCLEN_MAX];
		hcl_oow_t mlen = 0;

		while (p < end)
		{
			hcl_oow_t n;

			n = cmgr->uctobc (*p, bcsbuf, HCL_COUNTOF(bcsbuf));
			if (n == 0) 
			{
				ret = -1;
				break; /* illegal character */
			}

			/* it assumes that bcsbuf is large enough to hold a character */
			/*HCL_ASSERT (hcl, n <= HCL_COUNTOF(bcsbuf));*/

			p++; mlen += n;
		}

		/* this length excludes the terminating null character. 
		 * this function doesn't even null-terminate the result. */
		*bcslen = mlen;
	}

	*ucslen = p - ucs;
	return ret;
}


static int ucs_to_bcs_with_cmgr (
	const hcl_uch_t* ucs, hcl_oow_t* ucslen,
	hcl_bch_t* bcs, hcl_oow_t* bcslen, hcl_cmgr_t* cmgr)
{
	const hcl_uch_t* p = ucs;
	int ret = 0;

	if (bcs)
	{
		hcl_oow_t rem = *bcslen;

		while (*p != '\0')
		{
			hcl_oow_t n;

			if (rem <= 0)
			{
				ret = -2;
				break;
			}
			
			n = cmgr->uctobc (*p, bcs, rem);
			if (n == 0) 
			{
				ret = -1;
				break; /* illegal character */
			}
			if (n > rem) 
			{
				ret = -2;
				break; /* buffer too small */
			}

			bcs += n; rem -= n; p++;
		}

		/* update bcslen to the length of the bcs string converted excluding
		 * terminating null */
		*bcslen -= rem; 

		/* null-terminate the multibyte sequence if it has sufficient space */
		if (rem > 0) *bcs = '\0';
		else 
		{
			/* if ret is -2 and cs[cslen] == '\0', 
			 * this means that the bcs buffer was lacking one
			 * slot for the terminating null */
			ret = -2; /* buffer too small */
		}
	}
	else
	{
		hcl_bch_t bcsbuf[HCL_BCLEN_MAX];
		hcl_oow_t mlen = 0;

		while (*p != '\0')
		{
			hcl_oow_t n;

			n = cmgr->uctobc (*p, bcsbuf, HCL_COUNTOF(bcsbuf));
			if (n == 0) 
			{
				ret = -1;
				break; /* illegal character */
			}

			/* it assumes that bcs is large enough to hold a character */
			/*HCL_ASSERT (hcl, n <= HCL_COUNTOF(bcs));*/

			p++; mlen += n;
		}

		/* this length holds the number of resulting multi-byte characters 
		 * excluding the terminating null character */
		*bcslen = mlen;
	}

	*ucslen = p - ucs;  /* the number of wide characters handled. */
	return ret;
}

/* ----------------------------------------------------------------------- */

static hcl_cmgr_t utf8_cmgr =
{
	hcl_utf8touc,
	hcl_uctoutf8
};

hcl_cmgr_t* hcl_getutf8cmgr (void)
{
	return &utf8_cmgr;
}

int hcl_convutf8touchars (const hcl_bch_t* bcs, hcl_oow_t* bcslen, hcl_uch_t* ucs, hcl_oow_t* ucslen)
{
	/* the source is length bound */
	return bcsn_to_ucsn_with_cmgr (bcs, bcslen, ucs, ucslen, &utf8_cmgr, 0);
}

int hcl_convutoutf8chars (const hcl_uch_t* ucs, hcl_oow_t* ucslen, hcl_bch_t* bcs, hcl_oow_t* bcslen)
{
	/* length bound */
	return ucsn_to_bcsn_with_cmgr (ucs, ucslen, bcs, bcslen, &utf8_cmgr);
}

int hcl_convutf8toucstr (const hcl_bch_t* bcs, hcl_oow_t* bcslen, hcl_uch_t* ucs, hcl_oow_t* ucslen)
{
	/* null-terminated. */
	return bcs_to_ucs_with_cmgr (bcs, bcslen, ucs, ucslen, &utf8_cmgr, 0);
}

int hcl_convutoutf8cstr (const hcl_uch_t* ucs, hcl_oow_t* ucslen, hcl_bch_t* bcs, hcl_oow_t* bcslen)
{
	/* null-terminated */
	return ucs_to_bcs_with_cmgr (ucs, ucslen, bcs, bcslen, &utf8_cmgr);
}

/* ----------------------------------------------------------------------- */

int hcl_convbtouchars (hcl_t* hcl, const hcl_bch_t* bcs, hcl_oow_t* bcslen, hcl_uch_t* ucs, hcl_oow_t* ucslen)
{
	/* length bound */
	int n;

	n = bcsn_to_ucsn_with_cmgr (bcs, bcslen, ucs, ucslen, hcl->cmgr, 0);

	if (n <= -1)
	{
		/* -1: illegal character, -2: buffer too small, -3: incomplete sequence */
		hcl_seterrnum (hcl, (n == -2)? HCL_EBUFFULL: HCL_EECERR);
	}

	return n;
}

int hcl_convutobchars (hcl_t* hcl, const hcl_uch_t* ucs, hcl_oow_t* ucslen, hcl_bch_t* bcs, hcl_oow_t* bcslen)
{
	/* length bound */
	int n;

	n = ucsn_to_bcsn_with_cmgr (ucs, ucslen, bcs, bcslen, hcl->cmgr);

	if (n <= -1)
	{
		hcl_seterrnum (hcl, (n == -2)? HCL_EBUFFULL: HCL_EECERR);
	}

	return n;
}

int hcl_convbtoucstr (hcl_t* hcl, const hcl_bch_t* bcs, hcl_oow_t* bcslen, hcl_uch_t* ucs, hcl_oow_t* ucslen)
{
	/* null-terminated. */
	int n;

	n = bcs_to_ucs_with_cmgr (bcs, bcslen, ucs, ucslen, hcl->cmgr, 0);

	if (n <= -1)
	{
		hcl_seterrnum (hcl, (n == -2)? HCL_EBUFFULL: HCL_EECERR);
	}

	return n;
}

int hcl_convutobcstr (hcl_t* hcl, const hcl_uch_t* ucs, hcl_oow_t* ucslen, hcl_bch_t* bcs, hcl_oow_t* bcslen)
{
	/* null-terminated */
	int n;

	n = ucs_to_bcs_with_cmgr (ucs, ucslen, bcs, bcslen, hcl->cmgr);

	if (n <= -1)
	{
		hcl_seterrnum (hcl, (n == -2)? HCL_EBUFFULL: HCL_EECERR);
	}

	return n;
}

/* ----------------------------------------------------------------------- */

HCL_INLINE hcl_uch_t* hcl_dupbtoucharswithheadroom (hcl_t* hcl, hcl_oow_t headroom_bytes, const hcl_bch_t* bcs, hcl_oow_t bcslen, hcl_oow_t* ucslen)
{
	hcl_oow_t inlen, outlen;
	hcl_uch_t* ptr;

	inlen = bcslen;
	if (hcl_convbtouchars (hcl, bcs, &inlen, HCL_NULL, &outlen) <= -1) 
	{
		/* note it's also an error if no full conversion is made in this function */
		return HCL_NULL;
	}

	ptr = hcl_allocmem (hcl, headroom_bytes + ((outlen + 1) * HCL_SIZEOF(hcl_uch_t)));
	if (!ptr) return HCL_NULL;

	inlen = bcslen;

	ptr = (hcl_uch_t*)((hcl_oob_t*)ptr + headroom_bytes);
	hcl_convbtouchars (hcl, bcs, &inlen, ptr, &outlen);

	/* hcl_convbtouchars() doesn't null-terminate the target. 
	 * but in hcl_dupbtouchars(), i allocate space. so i don't mind
	 * null-terminating it with 1 extra character overhead */
	ptr[outlen] = '\0'; 
	if (ucslen) *ucslen = outlen;
	return ptr;
}

hcl_uch_t* hcl_dupbtouchars (hcl_t* hcl, const hcl_bch_t* bcs, hcl_oow_t bcslen, hcl_oow_t* ucslen)
{
	return hcl_dupbtoucharswithheadroom (hcl, 0, bcs, bcslen, ucslen);
}

HCL_INLINE hcl_bch_t* hcl_duputobcharswithheadroom (hcl_t* hcl, hcl_oow_t headroom_bytes, const hcl_uch_t* ucs, hcl_oow_t ucslen, hcl_oow_t* bcslen)
{
	hcl_oow_t inlen, outlen;
	hcl_bch_t* ptr;

	inlen = ucslen;
	if (hcl_convutobchars (hcl, ucs, &inlen, HCL_NULL, &outlen) <= -1) 
	{
		/* note it's also an error if no full conversion is made in this function */
		return HCL_NULL;
	}

	ptr = hcl_allocmem (hcl, headroom_bytes + ((outlen + 1) * HCL_SIZEOF(hcl_bch_t)));
	if (!ptr) return HCL_NULL;

	inlen = ucslen;
	ptr = (hcl_bch_t*)((hcl_oob_t*)ptr + headroom_bytes);
	hcl_convutobchars (hcl, ucs, &inlen, ptr, &outlen);

	ptr[outlen] = '\0';
	if (bcslen) *bcslen = outlen;
	return ptr;
}

hcl_bch_t* hcl_duputobchars (hcl_t* hcl, const hcl_uch_t* ucs, hcl_oow_t ucslen, hcl_oow_t* bcslen)
{
	return hcl_duputobcharswithheadroom (hcl, 0, ucs, ucslen, bcslen);
}


/* ----------------------------------------------------------------------- */

HCL_INLINE hcl_uch_t* hcl_dupbtoucstrwithheadroom (hcl_t* hcl, hcl_oow_t headroom_bytes, const hcl_bch_t* bcs, hcl_oow_t* ucslen)
{
	hcl_oow_t inlen, outlen;
	hcl_uch_t* ptr;

	if (hcl_convbtoucstr (hcl, bcs, &inlen, HCL_NULL, &outlen) <= -1) 
	{
		/* note it's also an error if no full conversion is made in this function */
		return HCL_NULL;
	}

	outlen++;
	ptr = hcl_allocmem (hcl, headroom_bytes + (outlen * HCL_SIZEOF(hcl_uch_t)));
	if (!ptr) return HCL_NULL;

	hcl_convbtoucstr (hcl, bcs, &inlen, ptr, &outlen);
	if (ucslen) *ucslen = outlen;
	return ptr;
}

hcl_uch_t* hcl_dupbtoucstr (hcl_t* hcl, const hcl_bch_t* bcs, hcl_oow_t* ucslen)
{
	return hcl_dupbtoucstrwithheadroom (hcl, 0, bcs, ucslen);
}

HCL_INLINE hcl_bch_t* hcl_duputobcstrwithheadroom (hcl_t* hcl, hcl_oow_t headroom_bytes, const hcl_uch_t* ucs, hcl_oow_t* bcslen)
{
	hcl_oow_t inlen, outlen;
	hcl_bch_t* ptr;

	if (hcl_convutobcstr (hcl, ucs, &inlen, HCL_NULL, &outlen) <= -1) 
	{
		/* note it's also an error if no full conversion is made in this function */
		return HCL_NULL;
	}

	outlen++;
	ptr = hcl_allocmem (hcl, headroom_bytes + (outlen * HCL_SIZEOF(hcl_bch_t)));
	if (!ptr) return HCL_NULL;

	ptr = (hcl_bch_t*)((hcl_oob_t*)ptr + headroom_bytes);

	hcl_convutobcstr (hcl, ucs, &inlen, ptr, &outlen);
	if (bcslen) *bcslen = outlen;
	return ptr;
}

hcl_bch_t* hcl_duputobcstr (hcl_t* hcl, const hcl_uch_t* ucs, hcl_oow_t* bcslen)
{
	return hcl_duputobcstrwithheadroom (hcl, 0, ucs, bcslen);
}
/* ----------------------------------------------------------------------- */

hcl_uch_t* hcl_dupuchars (hcl_t* hcl, const hcl_uch_t* ucs, hcl_oow_t ucslen)
{
	hcl_uch_t* ptr;

	ptr = hcl_allocmem (hcl, (ucslen + 1) * HCL_SIZEOF(hcl_uch_t));
	if (!ptr) return HCL_NULL;

	hcl_copyuchars (ptr, ucs, ucslen);
	ptr[ucslen] = '\0';
	return ptr;
}

hcl_bch_t* hcl_dupbchars (hcl_t* hcl, const hcl_bch_t* bcs, hcl_oow_t bcslen)
{
	hcl_bch_t* ptr;

	ptr = hcl_allocmem (hcl, (bcslen + 1) * HCL_SIZEOF(hcl_bch_t));
	if (!ptr) return HCL_NULL;

	hcl_copybchars (ptr, bcs, bcslen);
	ptr[bcslen] = '\0';
	return ptr;
}

/* ----------------------------------------------------------------------- */


#if defined(HCL_HAVE_UINT16_T)

hcl_uint16_t hcl_ntoh16 (hcl_uint16_t x)
{
#if defined(HCL_ENDIAN_BIG)
	return x;
#elif defined(HCL_ENDIAN_LITTLE)
	hcl_uint8_t* c = (hcl_uint8_t*)&x;
	return (hcl_uint16_t)(
		((hcl_uint16_t)c[0] << 8) |
		((hcl_uint16_t)c[1] << 0));
#else
#	error Unknown endian
#endif
}

hcl_uint16_t hcl_hton16 (hcl_uint16_t x)
{
#if defined(HCL_ENDIAN_BIG)
	return x;
#elif defined(HCL_ENDIAN_LITTLE)
	hcl_uint8_t* c = (hcl_uint8_t*)&x;
	return (hcl_uint16_t)(
		((hcl_uint16_t)c[0] << 8) |
		((hcl_uint16_t)c[1] << 0));
#else
#	error Unknown endian
#endif
}

#endif

/* --------------------------------------------------------------- */

#if defined(HCL_HAVE_UINT32_T)

hcl_uint32_t hcl_ntoh32 (hcl_uint32_t x)
{
#if defined(HCL_ENDIAN_BIG)
	return x;
#elif defined(HCL_ENDIAN_LITTLE)
	hcl_uint8_t* c = (hcl_uint8_t*)&x;
	return (hcl_uint32_t)(
		((hcl_uint32_t)c[0] << 24) |
		((hcl_uint32_t)c[1] << 16) |
		((hcl_uint32_t)c[2] << 8) | 
		((hcl_uint32_t)c[3] << 0));
#else
#	error Unknown endian
#endif
}

hcl_uint32_t hcl_hton32 (hcl_uint32_t x)
{
#if defined(HCL_ENDIAN_BIG)
	return x;
#elif defined(HCL_ENDIAN_LITTLE)
	hcl_uint8_t* c = (hcl_uint8_t*)&x;
	return (hcl_uint32_t)(
		((hcl_uint32_t)c[0] << 24) |
		((hcl_uint32_t)c[1] << 16) |
		((hcl_uint32_t)c[2] << 8) | 
		((hcl_uint32_t)c[3] << 0));
#else
#	error Unknown endian
#endif
}
#endif


#if defined(HCL_HAVE_UINT64_T)

hcl_uint64_t hcl_ntoh64 (hcl_uint64_t x)
{
#if defined(HCL_ENDIAN_BIG)
	return x;
#elif defined(HCL_ENDIAN_LITTLE)
	hcl_uint8_t* c = (hcl_uint8_t*)&x;
	return (hcl_uint64_t)(
		((hcl_uint64_t)c[0] << 56) | 
		((hcl_uint64_t)c[1] << 48) | 
		((hcl_uint64_t)c[2] << 40) |
		((hcl_uint64_t)c[3] << 32) |
		((hcl_uint64_t)c[4] << 24) |
		((hcl_uint64_t)c[5] << 16) |
		((hcl_uint64_t)c[6] << 8)  |
		((hcl_uint64_t)c[7] << 0));
#else
#	error Unknown endian
#endif
}

hcl_uint64_t hcl_hton64 (hcl_uint64_t x)
{
#if defined(HCL_ENDIAN_BIG)
	return x;
#elif defined(HCL_ENDIAN_LITTLE)
	hcl_uint8_t* c = (hcl_uint8_t*)&x;
	return (hcl_uint64_t)(
		((hcl_uint64_t)c[0] << 56) | 
		((hcl_uint64_t)c[1] << 48) | 
		((hcl_uint64_t)c[2] << 40) |
		((hcl_uint64_t)c[3] << 32) |
		((hcl_uint64_t)c[4] << 24) |
		((hcl_uint64_t)c[5] << 16) |
		((hcl_uint64_t)c[6] << 8)  |
		((hcl_uint64_t)c[7] << 0));
#else
#	error Unknown endian
#endif
}

#endif

/* --------------------------------------------------------------- */

#if defined(HCL_HAVE_UINT128_T)

hcl_uint128_t hcl_ntoh128 (hcl_uint128_t x)
{
#if defined(HCL_ENDIAN_BIG)
	return x;
#elif defined(HCL_ENDIAN_LITTLE)
	hcl_uint8_t* c = (hcl_uint8_t*)&x;
	return (hcl_uint128_t)(
		((hcl_uint128_t)c[0]  << 120) | 
		((hcl_uint128_t)c[1]  << 112) | 
		((hcl_uint128_t)c[2]  << 104) |
		((hcl_uint128_t)c[3]  << 96) |
		((hcl_uint128_t)c[4]  << 88) |
		((hcl_uint128_t)c[5]  << 80) |
		((hcl_uint128_t)c[6]  << 72) |
		((hcl_uint128_t)c[7]  << 64) |
		((hcl_uint128_t)c[8]  << 56) | 
		((hcl_uint128_t)c[9]  << 48) | 
		((hcl_uint128_t)c[10] << 40) |
		((hcl_uint128_t)c[11] << 32) |
		((hcl_uint128_t)c[12] << 24) |
		((hcl_uint128_t)c[13] << 16) |
		((hcl_uint128_t)c[14] << 8)  |
		((hcl_uint128_t)c[15] << 0));
#else
#	error Unknown endian
#endif
}

hcl_uint128_t hcl_hton128 (hcl_uint128_t x)
{
#if defined(HCL_ENDIAN_BIG)
	return x;
#elif defined(HCL_ENDIAN_LITTLE)
	hcl_uint8_t* c = (hcl_uint8_t*)&x;
	return (hcl_uint128_t)(
		((hcl_uint128_t)c[0]  << 120) | 
		((hcl_uint128_t)c[1]  << 112) | 
		((hcl_uint128_t)c[2]  << 104) |
		((hcl_uint128_t)c[3]  << 96) |
		((hcl_uint128_t)c[4]  << 88) |
		((hcl_uint128_t)c[5]  << 80) |
		((hcl_uint128_t)c[6]  << 72) |
		((hcl_uint128_t)c[7]  << 64) |
		((hcl_uint128_t)c[8]  << 56) | 
		((hcl_uint128_t)c[9]  << 48) | 
		((hcl_uint128_t)c[10] << 40) |
		((hcl_uint128_t)c[11] << 32) |
		((hcl_uint128_t)c[12] << 24) |
		((hcl_uint128_t)c[13] << 16) |
		((hcl_uint128_t)c[14] << 8)  |
		((hcl_uint128_t)c[15] << 0));
#else
#	error Unknown endian
#endif
}

#endif
