/*
 * $Id$
 *
    Copyright 2006-2014 Chung, Hyung-Hwan.
    This file is part of H2.

    H2 is free software: you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as 
    published by the Free Software Foundation, either version 3 of 
    the License, or (at your option) any later version.

    H2 is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public 
    License along with H2. If not, see <http://www.gnu.org/licenses/>.
 */

#include <h2/cmn/utf8.h>

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
	h2_uint32_t  lower;
	h2_uint32_t  upper;
	h2_uint8_t   fbyte; 
	h2_uint8_t   mask;   /* mask for getting the fixed bits in the first byte (First byte & mask => fbyte) */
	h2_uint8_t   fmask;  /* mask for getting the actual values bits off the first byte */
	int          length; /* number of bytes */
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

static H2_INLINE __utf8_t* get_utf8_slot (h2_wxchar_t uc)
{
	__utf8_t* cur, * end;

	H2_ASSERT (H2_SIZEOF(h2_mchar_t) == 1);
	H2_ASSERT (H2_SIZEOF(h2_wxchar_t) >= 2);

	end = utf8_table + H2_COUNTOF(utf8_table);
	cur = utf8_table;

	while (cur < end) 
	{
		if (uc >= cur->lower && uc <= cur->upper) return cur;
		cur++;
	}

	return H2_NULL; /* invalid character */
}

h2_size_t h2_uctoutf8 (h2_wxchar_t uc, h2_mchar_t* utf8, h2_size_t size)
{
	__utf8_t* cur = get_utf8_slot (uc);

	if (cur == H2_NULL) return 0; /* illegal character */

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
	return (h2_size_t)cur->length;
}

h2_size_t h2_utf8touc (
	const h2_mchar_t* utf8, h2_size_t size, h2_wxchar_t* uc)
{
	__utf8_t* cur, * end;

	H2_ASSERT (utf8 != H2_NULL);
	H2_ASSERT (size > 0);
	H2_ASSERT (H2_SIZEOF(h2_mchar_t) == 1);
	H2_ASSERT (H2_SIZEOF(h2_wxchar_t) >= 2);

	end = utf8_table + H2_COUNTOF(utf8_table);
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
					h2_wxchar_t w;

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
			 *    the correct length (len >= cur->length) 
			 * and 
			 *    the incomplete seqeunce error (len < cur->length).
			 */
			return (h2_size_t)cur->length;
		}
		cur++;
	}

	return 0; /* error - invalid sequence */
}

h2_size_t h2_utf8len (const h2_mchar_t* utf8, h2_size_t size)
{
	return h2_utf8touc (utf8, size, H2_NULL);
}

h2_size_t h2_utf8lenmax (void)
{
	return H2_UTF8LEN_MAX;
}

