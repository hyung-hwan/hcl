/*
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

#include <hcl-chr.h>

/*#define RETAIN_RFC2279 1*/

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
 *
 * RFC3629 limits the ranges like this:
 * 1:2 00000000-0000007F  0xxxxxxx
 * 2:2 00000080-000007FF  110xxxxx 10xxxxxx
 * 3:2 00000800-0000FFFF  1110xxxx 10xxxxxx 10xxxxxx
 * 4:4 00010000-0010FFFF  11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
 */

struct __utf8_t
{
	hcl_uint32_t  lower;
	hcl_uint32_t  upper;
	hcl_uint8_t   fbyte;  /* mask to the first utf8 byte */
	hcl_uint8_t   mask;
	hcl_uint8_t   fmask;
	int           length; /* number of bytes */
};

typedef struct __utf8_t __utf8_t;

static __utf8_t utf8_table[] =
{
	{0x00000000ul, 0x0000007Ful, 0x00, 0x80, 0x7F, 1},
	{0x00000080ul, 0x000007FFul, 0xC0, 0xE0, 0x1F, 2},
	{0x00000800ul, 0x0000FFFFul, 0xE0, 0xF0, 0x0F, 3},
#if defined(RETAIN_RFC2279)
	{0x00010000ul, 0x001FFFFFul, 0xF0, 0xF8, 0x07, 4},
	{0x00200000ul, 0x03FFFFFFul, 0xF8, 0xFC, 0x03, 5},
	{0x04000000ul, 0x7FFFFFFFul, 0xFC, 0xFE, 0x01, 6}
#else
	{0x00010000ul, 0x0010FFFFul, 0xF0, 0xF8, 0x07, 4}
#endif
};

static HCL_INLINE __utf8_t* get_utf8_slot (hcl_uch_t uc)
{
	__utf8_t* cur, * end;

	/*HCL_ASSERT (hcl, HCL_SIZEOF(hcl_bch_t) == 1);
	HCL_ASSERT (hcl, HCL_SIZEOF(hcl_uch_t) >= 2);*/

	end = utf8_table + HCL_COUNTOF(utf8_table);
	cur = utf8_table;

	while (cur < end)
	{
		if (uc >= cur->lower && uc <= cur->upper) return cur;
		cur++;
	}

	return HCL_NULL; /* invalid character */
}

hcl_oow_t hcl_uc_to_utf8 (hcl_uch_t uc, hcl_bch_t* utf8, hcl_oow_t size)
{
	__utf8_t* cur = get_utf8_slot(uc);

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

hcl_oow_t hcl_utf8_to_uc (const hcl_bch_t* utf8, hcl_oow_t size, hcl_uch_t* uc)
{
	__utf8_t* cur, * end;

	/*HCL_ASSERT (hcl, utf8 != HCL_NULL);
	HCL_ASSERT (hcl, size > 0);
	HCL_ASSERT (hcl, HCL_SIZEOF(hcl_bch_t) == 1);
	HCL_ASSERT (hcl, HCL_SIZEOF(hcl_uch_t) >= 2);*/

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

