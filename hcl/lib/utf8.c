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

#include "hcl-prv.h"

#define HCL_BCLEN_MAX 6

/*
 * from RFC 2279 UTF-8, a transformation format of ISO 10646
 *
 *     UCS-4 range (hex.)  UTF-8 octet sequence (binary)
 * 1:2 00000000-0000007F  0xxxxxxx
 * 2:2 00000080-000007FF  110xxxxx 10xxxxxx
 * 3:2 00000800-0000FFFF  1110xxxx 10xxxxxx 10xxxxxx
 * 4:4 00010000-001FFFFF  11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
 * inv 00200000-03FFFFFF  111110xx 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx
 * inv 04000000-7FFFFFFF  1111110x 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx
 */

struct __utf8_t
{
	hcl_uint32_t  lower;
	hcl_uint32_t  upper;
	hcl_uint8_t   fbyte;  /* mask to the first utf8 byte */
	hcl_uint8_t   mask;
	hcl_uint8_t   fmask;
	int            length; /* number of bytes */
};

typedef struct __utf8_t __utf8_t;

static __utf8_t utf8_table[] = 
{
	{0x00000000ul, 0x0000007Ful, 0x00, 0x80, 0x7F, 1},
	{0x00000080ul, 0x000007FFul, 0xC0, 0xE0, 0x1F, 2},
	{0x00000800ul, 0x0000FFFFul, 0xE0, 0xF0, 0x0F, 3},
	{0x00010000ul, 0x001FFFFFul, 0xF0, 0xF8, 0x07, 4},
	{0x00200000ul, 0x03FFFFFFul, 0xF8, 0xFC, 0x03, 5},
	{0x04000000ul, 0x7FFFFFFFul, 0xFC, 0xFE, 0x01, 6}
};

static HCL_INLINE __utf8_t* get_utf8_slot (hcl_uch_t uc)
{
	__utf8_t* cur, * end;

	HCL_ASSERT (HCL_SIZEOF(hcl_bch_t) == 1);
	HCL_ASSERT (HCL_SIZEOF(hcl_uch_t) >= 2);

	end = utf8_table + HCL_COUNTOF(utf8_table);
	cur = utf8_table;

	while (cur < end) 
	{
		if (uc >= cur->lower && uc <= cur->upper) return cur;
		cur++;
	}

	return HCL_NULL; /* invalid character */
}

hcl_oow_t hcl_uctoutf8 (hcl_uch_t uc, hcl_bch_t* utf8, hcl_oow_t size)
{
	__utf8_t* cur = get_utf8_slot (uc);

	if (cur == HCL_NULL) return 0; /* illegal character */

	if (utf8 && cur->length <= size)
	{
		int index = cur->length;
		while (index > 1) 
		{
			/*
			 * 0x3F: 00111111
			 * 0x80: 10000000
			 */
			utf8[--index] = (uc & 0x3F) | 0x80;
			uc >>= 6;
		}

		utf8[0] = uc | cur->fbyte;
	}

	/* small buffer is also indicated by this return value
	 * greater than 'size'. */
	return (hcl_oow_t)cur->length;
}

hcl_oow_t hcl_utf8touc (const hcl_bch_t* utf8, hcl_oow_t size, hcl_uch_t* uc)
{
	__utf8_t* cur, * end;

	HCL_ASSERT (utf8 != HCL_NULL);
	HCL_ASSERT (size > 0);
	HCL_ASSERT (HCL_SIZEOF(hcl_bch_t) == 1);
	HCL_ASSERT (HCL_SIZEOF(hcl_uch_t) >= 2);

	end = utf8_table + HCL_COUNTOF(utf8_table);
	cur = utf8_table;

	while (cur < end) 
	{
		if ((utf8[0] & cur->mask) == cur->fbyte) 
		{

			/* if size is less that cur->length, the incomplete-seqeunce 
			 * error is naturally indicated. so validate the string
			 * only if size is as large as cur->length. */

			if (size >= cur->length) 
			{
				int i;

				if (uc)
				{
					hcl_uch_t w;

					w = utf8[0] & cur->fmask;
					for (i = 1; i < cur->length; i++)
					{
						/* in utf8, trailing bytes are all
						 * set with 0x80. 
						 *
						 *   10XXXXXX & 11000000 => 10000000
						 *
						 * if not, invalid. */
						if ((utf8[i] & 0xC0) != 0x80) return 0; 
						w = (w << 6) | (utf8[i] & 0x3F);
					}
					*uc = w;
				}
				else
				{
					for (i = 1; i < cur->length; i++)
					{
						/* in utf8, trailing bytes are all
						 * set with 0x80. 
						 *
						 *   10XXXXXX & 11000000 => 10000000
						 *
						 * if not, invalid. */
						if ((utf8[i] & 0xC0) != 0x80) return 0; 
					}
				}
			}

			/* this return value can indicate both 
			 *    the correct length (size >= cur->length) 
			 * and 
			 *    the incomplete seqeunce error (size < cur->length).
			 */
			return (hcl_oow_t)cur->length;
		}
		cur++;
	}

	return 0; /* error - invalid sequence */
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
			HCL_ASSERT (n <= HCL_COUNTOF(bcsbuf));

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
			HCL_ASSERT (n <= HCL_COUNTOF(bcs));

			p++; mlen += n;
		}

		/* this length holds the number of resulting multi-byte characters 
		 * excluding the terminating null character */
		*bcslen = mlen;
	}

	*ucslen = p - ucs;  /* the number of wide characters handled. */
	return ret;
}

static hcl_cmgr_t utf8_cmgr =
{
	hcl_utf8touc,
	hcl_uctoutf8
};

int hcl_utf8toucs (const hcl_bch_t* bcs, hcl_oow_t* bcslen, hcl_uch_t* ucs, hcl_oow_t* ucslen)
{
	if (*bcslen == ~(hcl_oow_t)0)
	{
		/* the source is null-terminated. */
		return bcs_to_ucs_with_cmgr (bcs, bcslen, ucs, ucslen, &utf8_cmgr, 0);
	}
	else
	{
		/* the source is length bound */
		return bcsn_to_ucsn_with_cmgr (bcs, bcslen, ucs, ucslen, &utf8_cmgr, 0);
	}
}

int hcl_ucstoutf8 (const hcl_uch_t* ucs, hcl_oow_t *ucslen, hcl_bch_t* bcs, hcl_oow_t* bcslen)
{
	if (*ucslen == ~(hcl_oow_t)0)
	{
		/* null-terminated */
		return ucs_to_bcs_with_cmgr (ucs, ucslen, bcs, bcslen, &utf8_cmgr);
	}
	else
	{
		/* length bound */
		return ucsn_to_bcsn_with_cmgr (ucs, ucslen, bcs, bcslen, &utf8_cmgr);
	}
}

/*
hcl_oow_t hcl_ucslen (const hcl_uch_t* ucs)
{
	const hcl_uch_t* ptr = ucs;
	while  (*ptr) ptr = HCL_INCPTR(const hcl_uch_t, ptr, 1);
	return HCL_SUBPTR(const hcl_uch_t, ptr, ucs);
}
*/
