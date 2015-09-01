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

#include <h2/cmn/utf16.h>

#define LEAD_SURROGATE_MIN  0xD800lu
#define LEAD_SURROGATE_MAX  0xDBFFlu
#define TRAIL_SURROGATE_MIN 0xDC00lu
#define TRAIL_SURROGATE_MAX 0xDFFFlu

#define IS_SURROGATE(x) ((x) >= LEAD_SURROGATE_MIN && (x) <= TRAIL_SURROGATE_MAX)
#define IS_LEAD_SURROGATE(x) ((x) >= LEAD_SURROGATE_MIN && (x) <= LEAD_SURROGATE_MAX)
#define IS_TRAIL_SURROGATE(x) ((x) >= TRAIL_SURROGATE_MIN && (x) <= TRAIL_SURROGATE_MAX)

h2_size_t h2_uctoutf16 (h2_wxchar_t uc, h2_wchar_t* utf16, h2_size_t size)
{
#if (H2_SIZEOF_WXCHAR_T > H2_SIZEOF_WCHAR_T)

	/*if (IS_SURROGATE(uc)) return 0;*/ /* illegal character */

	if (uc <= 0xFFFFlu)
	{
		if (utf16 && size >= 1) *utf16 = uc;
		return 1;
	}
	if (uc >= 0x10000lu && uc <= 0x10FFFlu)
	{
		if (utf16 && size >= 2) 
		{
			h2_uint32_t tmp;
			tmp = uc - 0x10000lu;
			utf16[0] = LEAD_SURROGATE_MIN | (tmp >> 10);
			utf16[1] = TRAIL_SURROGATE_MIN | (tmp & 0x3FFlu);
		}

		return 2;
	}
	else
	{
		return 0; /* illegal character */
	}

#elif (H2_SIZEOF_WXCHAR_T == H2_SIZEOF_WCHAR_T)

	/*if (IS_SURROGATE(uc)) return 0;*/ /* illegal character */

	/* ucs2 is assumed in this case */
	if (utf16 && size >= 1) *utf16 = uc;

	/* small buffer is also indicated by this return value
	 * greater than 'size'. */
	return 1;

#else
#	error Unsupported size of h2_wxchar_t
#endif
}

h2_size_t h2_utf16touc (
	const h2_wchar_t* utf16, h2_size_t size, h2_wxchar_t* uc)
{
	H2_ASSERT (utf16 != H2_NULL);
	H2_ASSERT (size > 0);

#if (H2_SIZEOF_WXCHAR_T > H2_SIZEOF_WCHAR_T)

	if (IS_LEAD_SURROGATE(utf16[0]))
	{
		if (size >= 2)
		{
			if (IS_TRAIL_SURROGATE(utf16[1]))
			{
				if (uc)
				{
					h2_uint32_t tmp = 0x10000lu;
					tmp += (utf16[0] & 0x3FFlu) << 10;
					tmp += (utf16[1] & 0x3FFlu);
					*uc = tmp;
				}

				/* this return value can indicate both 
				 *    the correct length (size >= 2) 
				 * and 
				 *    the incomplete seqeunce error (size < 2).
				 */
				return 2;
			}
			else
			{
				return 0; /* invalid sequence */
			}
		}
		else return 2; /* this should indicate the incomplete sequence */
	}
	else if (IS_TRAIL_SURROGATE(utf16[0]))
	{
		return 0; /* invalid sequence */
	}
	else
	{
		if (uc) *uc = utf16[0];
		return 1;
	}

#elif (H2_SIZEOF_WXCHAR_T == H2_SIZEOF_WCHAR_T)

	/*if (IS_SURROGATE(utf16[0])) return 0;*/ /* illegal character */

	/* ucs2 is assumed in this case */
	if (uc) *uc = utf16[0];

	/* small buffer is also indicated by this return value
	 * greater than 'size'. */
	return 1;

#else
#	error Unsupported size of h2_wxchar_t
#endif
}

h2_size_t h2_utf16len (const h2_wchar_t* utf16, h2_size_t size)
{
	return h2_utf16touc (utf16, size, H2_NULL);
}

h2_size_t h2_utf16lenmax (void)
{
	return H2_UTF16LEN_MAX;
}

