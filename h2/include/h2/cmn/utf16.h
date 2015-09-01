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

#ifndef _H2_CMN_UTF16_H_
#define _H2_CMN_UTF16_H_

#include <h2/types.h>
#include <h2/macros.h>

/** \file
 * This file provides functions, types, macros for utf16 conversion.
 */

/**
 * The H2_UTF16LEN_MAX macro defines the maximum number of qse_wchar_t
 * needed to form a single unicode character.
 */
#define H2_UTF16LEN_MAX (H2_SIZEOF_WXCHAR_T / H2_SIZEOF_WCHAR_T)

#ifdef __cplusplus
extern "C" {
#endif

/** 
 * The h2_uctoutf16() function converts a unicode character to a utf16 sequence.
 * \return 
 * - 0 is returned if \a uc is invalid. 
 * - An integer greater than \a size is returned if the \a utf16 sequence buffer 
 *   is not #H2_NULL and not large enough. This integer is actually the number 
 *   of bytes needed.
 * - If \a utf16 is #H2_NULL, the number of bytes that would have been stored 
 *   into \a utf16 if it had not been #H2_NULL is returned.
 * - An integer between 1 and size inclusive is returned in all other cases.
 * \note
 * This function doesn't check invalid unicode code points and performs
 * conversion compuationally.
 */
h2_size_t h2_uctoutf16 (
	h2_wxchar_t  uc,
	h2_wchar_t*  utf16,
	h2_size_t    size
);

/**
 * The h2_utf16touc() function converts a utf16 sequence to a unicode character.
 * \return
 * - 0 is returned if the \a utf16 sequence is invalid.
 * - An integer greater than \a size is returned if the \a utf16 sequence is 
 *   not complete.
 * - An integer between 1 and size inclusive is returned in all other cases.
 */
h2_size_t h2_utf16touc (
	const h2_wchar_t* utf16,
	h2_size_t         size, 
	h2_wxchar_t*      uc
);

/**
 * The h2_utf16len() function scans at most \a size bytes from the \a utf16 
 * sequence and returns the number of bytes needed to form a single unicode
 * character.
 * \return
 * - 0 is returned if the \a utf16 sequence is invalid.
 * - An integer greater than \a size is returned if the \a utf16 sequence is 
 *   not complete.
 * - An integer between 1 and size inclusive is returned in all other cases.
 */ 
h2_size_t h2_utf16len (
	const h2_wchar_t* utf16,
	h2_size_t         size
);

/**
 * The h2_utf16lenmax() function returns the maximum number of qse_wchar_t
 * needed to form a single unicode character. Use #H2_UTF16LEN_MAX if you 
 * need a compile-time constant.
 */
h2_size_t h2_utf16lenmax (
	void
);

#ifdef __cplusplus
}
#endif

#endif
