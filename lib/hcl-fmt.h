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

#ifndef _HCL_FMT_H_
#define _HCL_FMT_H_

#include <hcl-cmn.h>
#include <stdarg.h>

/** \file
 * This file defines various formatting functions.
 */

/**
 * The hcl_fmt_intmax_flag_t type defines enumerators to change the
 * behavior of hcl_fmt_intmax() and hcl_fmt_uintmax().
 */
enum hcl_fmt_intmax_flag_t
{
	/* Use lower 6 bits to represent base between 2 and 36 inclusive.
	 * Upper bits are used for these flag options */

	/** Don't truncate if the buffer is not large enough */
	HCL_FMT_INTMAX_NOTRUNC = (0x40 << 0),
#define HCL_FMT_INTMAX_NOTRUNC             HCL_FMT_INTMAX_NOTRUNC
#define HCL_FMT_UINTMAX_NOTRUNC            HCL_FMT_INTMAX_NOTRUNC
#define HCL_FMT_INTMAX_TO_BCSTR_NOTRUNC    HCL_FMT_INTMAX_NOTRUNC
#define HCL_FMT_UINTMAX_TO_BCSTR_NOTRUNC   HCL_FMT_INTMAX_NOTRUNC
#define HCL_FMT_INTMAX_TO_UCSTR_NOTRUNC    HCL_FMT_INTMAX_NOTRUNC
#define HCL_FMT_UINTMAX_TO_UCSTR_NOTRUNC   HCL_FMT_INTMAX_NOTRUNC
#define HCL_FMT_INTMAX_TO_OOCSTR_NOTRUNC   HCL_FMT_INTMAX_NOTRUNC
#define HCL_FMT_UINTMAX_TO_OOCSTR_NOTRUNC  HCL_FMT_INTMAX_NOTRUNC

	/** Don't append a terminating null */
	HCL_FMT_INTMAX_NONULL = (0x40 << 1),
#define HCL_FMT_INTMAX_NONULL             HCL_FMT_INTMAX_NONULL
#define HCL_FMT_UINTMAX_NONULL            HCL_FMT_INTMAX_NONULL
#define HCL_FMT_INTMAX_TO_BCSTR_NONULL    HCL_FMT_INTMAX_NONULL
#define HCL_FMT_UINTMAX_TO_BCSTR_NONULL   HCL_FMT_INTMAX_NONULL
#define HCL_FMT_INTMAX_TO_UCSTR_NONULL    HCL_FMT_INTMAX_NONULL
#define HCL_FMT_UINTMAX_TO_UCSTR_NONULL   HCL_FMT_INTMAX_NONULL
#define HCL_FMT_INTMAX_TO_OOCSTR_NONULL   HCL_FMT_INTMAX_NONULL
#define HCL_FMT_UINTMAX_TO_OOCSTR_NONULL  HCL_FMT_INTMAX_NONULL

	/** Produce no digit for a value of zero  */
	HCL_FMT_INTMAX_NOZERO = (0x40 << 2),
#define HCL_FMT_INTMAX_NOZERO             HCL_FMT_INTMAX_NOZERO
#define HCL_FMT_UINTMAX_NOZERO            HCL_FMT_INTMAX_NOZERO
#define HCL_FMT_INTMAX_TO_BCSTR_NOZERO    HCL_FMT_INTMAX_NOZERO
#define HCL_FMT_UINTMAX_TO_BCSTR_NOZERO   HCL_FMT_INTMAX_NOZERO
#define HCL_FMT_INTMAX_TO_UCSTR_NOZERO    HCL_FMT_INTMAX_NOZERO
#define HCL_FMT_UINTMAX_TO_UCSTR_NOZERO   HCL_FMT_INTMAX_NOZERO
#define HCL_FMT_INTMAX_TO_OOCSTR_NOZERO   HCL_FMT_INTMAX_NOZERO
#define HCL_FMT_UINTMAX_TO_OOCSTR_NOZERO  HCL_FMT_INTMAX_NOZERO

	/** Produce a leading zero for a non-zero value */
	HCL_FMT_INTMAX_ZEROLEAD = (0x40 << 3),
#define HCL_FMT_INTMAX_ZEROLEAD             HCL_FMT_INTMAX_ZEROLEAD
#define HCL_FMT_UINTMAX_ZEROLEAD            HCL_FMT_INTMAX_ZEROLEAD
#define HCL_FMT_INTMAX_TO_BCSTR_ZEROLEAD    HCL_FMT_INTMAX_ZEROLEAD
#define HCL_FMT_UINTMAX_TO_BCSTR_ZEROLEAD   HCL_FMT_INTMAX_ZEROLEAD
#define HCL_FMT_INTMAX_TO_UCSTR_ZEROLEAD    HCL_FMT_INTMAX_ZEROLEAD
#define HCL_FMT_UINTMAX_TO_UCSTR_ZEROLEAD   HCL_FMT_INTMAX_ZEROLEAD
#define HCL_FMT_INTMAX_TO_OOCSTR_ZEROLEAD   HCL_FMT_INTMAX_ZEROLEAD
#define HCL_FMT_UINTMAX_TO_OOCSTR_ZEROLEAD  HCL_FMT_INTMAX_ZEROLEAD

	/** Use uppercase letters for alphabetic digits */
	HCL_FMT_INTMAX_UPPERCASE = (0x40 << 4),
#define HCL_FMT_INTMAX_UPPERCASE             HCL_FMT_INTMAX_UPPERCASE
#define HCL_FMT_UINTMAX_UPPERCASE            HCL_FMT_INTMAX_UPPERCASE
#define HCL_FMT_INTMAX_TO_BCSTR_UPPERCASE    HCL_FMT_INTMAX_UPPERCASE
#define HCL_FMT_UINTMAX_TO_BCSTR_UPPERCASE   HCL_FMT_INTMAX_UPPERCASE
#define HCL_FMT_INTMAX_TO_UCSTR_UPPERCASE    HCL_FMT_INTMAX_UPPERCASE
#define HCL_FMT_UINTMAX_TO_UCSTR_UPPERCASE   HCL_FMT_INTMAX_UPPERCASE
#define HCL_FMT_INTMAX_TO_OOCSTR_UPPERCASE   HCL_FMT_INTMAX_UPPERCASE
#define HCL_FMT_UINTMAX_TO_OOCSTR_UPPERCASE  HCL_FMT_INTMAX_UPPERCASE

	/** Insert a plus sign for a positive integer including 0 */
	HCL_FMT_INTMAX_PLUSSIGN = (0x40 << 5),
#define HCL_FMT_INTMAX_PLUSSIGN             HCL_FMT_INTMAX_PLUSSIGN
#define HCL_FMT_UINTMAX_PLUSSIGN            HCL_FMT_INTMAX_PLUSSIGN
#define HCL_FMT_INTMAX_TO_BCSTR_PLUSSIGN    HCL_FMT_INTMAX_PLUSSIGN
#define HCL_FMT_UINTMAX_TO_BCSTR_PLUSSIGN   HCL_FMT_INTMAX_PLUSSIGN
#define HCL_FMT_INTMAX_TO_UCSTR_PLUSSIGN    HCL_FMT_INTMAX_PLUSSIGN
#define HCL_FMT_UINTMAX_TO_UCSTR_PLUSSIGN   HCL_FMT_INTMAX_PLUSSIGN
#define HCL_FMT_INTMAX_TO_OOCSTR_PLUSSIGN   HCL_FMT_INTMAX_PLUSSIGN
#define HCL_FMT_UINTMAX_TO_OOCSTR_PLUSSIGN  HCL_FMT_INTMAX_PLUSSIGN

	/** Insert a space for a positive integer including 0 */
	HCL_FMT_INTMAX_EMPTYSIGN = (0x40 << 6),
#define HCL_FMT_INTMAX_EMPTYSIGN             HCL_FMT_INTMAX_EMPTYSIGN
#define HCL_FMT_UINTMAX_EMPTYSIGN            HCL_FMT_INTMAX_EMPTYSIGN
#define HCL_FMT_INTMAX_TO_BCSTR_EMPTYSIGN    HCL_FMT_INTMAX_EMPTYSIGN
#define HCL_FMT_UINTMAX_TO_BCSTR_EMPTYSIGN   HCL_FMT_INTMAX_EMPTYSIGN
#define HCL_FMT_INTMAX_TO_UCSTR_EMPTYSIGN    HCL_FMT_INTMAX_EMPTYSIGN
#define HCL_FMT_UINTMAX_TO_UCSTR_EMPTYSIGN   HCL_FMT_INTMAX_EMPTYSIGN

	/** Fill the right part of the string */
	HCL_FMT_INTMAX_FILLRIGHT = (0x40 << 7),
#define HCL_FMT_INTMAX_FILLRIGHT             HCL_FMT_INTMAX_FILLRIGHT
#define HCL_FMT_UINTMAX_FILLRIGHT            HCL_FMT_INTMAX_FILLRIGHT
#define HCL_FMT_INTMAX_TO_BCSTR_FILLRIGHT    HCL_FMT_INTMAX_FILLRIGHT
#define HCL_FMT_UINTMAX_TO_BCSTR_FILLRIGHT   HCL_FMT_INTMAX_FILLRIGHT
#define HCL_FMT_INTMAX_TO_UCSTR_FILLRIGHT    HCL_FMT_INTMAX_FILLRIGHT
#define HCL_FMT_UINTMAX_TO_UCSTR_FILLRIGHT   HCL_FMT_INTMAX_FILLRIGHT
#define HCL_FMT_INTMAX_TO_OOCSTR_FILLRIGHT   HCL_FMT_INTMAX_FILLRIGHT
#define HCL_FMT_UINTMAX_TO_OOCSTR_FILLRIGHT  HCL_FMT_INTMAX_FILLRIGHT

	/** Fill between the sign chacter and the digit part */
	HCL_FMT_INTMAX_FILLCENTER = (0x40 << 8)
#define HCL_FMT_INTMAX_FILLCENTER             HCL_FMT_INTMAX_FILLCENTER
#define HCL_FMT_UINTMAX_FILLCENTER            HCL_FMT_INTMAX_FILLCENTER
#define HCL_FMT_INTMAX_TO_BCSTR_FILLCENTER    HCL_FMT_INTMAX_FILLCENTER
#define HCL_FMT_UINTMAX_TO_BCSTR_FILLCENTER   HCL_FMT_INTMAX_FILLCENTER
#define HCL_FMT_INTMAX_TO_UCSTR_FILLCENTER    HCL_FMT_INTMAX_FILLCENTER
#define HCL_FMT_UINTMAX_TO_UCSTR_FILLCENTER   HCL_FMT_INTMAX_FILLCENTER
#define HCL_FMT_INTMAX_TO_OOCSTR_FILLCENTER   HCL_FMT_INTMAX_FILLCENTER
#define HCL_FMT_UINTMAX_TO_OOCSTR_FILLCENTER  HCL_FMT_INTMAX_FILLCENTER
};

/* =========================================================================
 * FORMATTED OUTPUT
 * ========================================================================= */
typedef struct hcl_fmtout_t hcl_fmtout_t;

typedef int (*hcl_fmtout_putbchars_t) (
	hcl_t*            hcl,
	hcl_fmtout_t*     fmtout,
	const hcl_bch_t*  ptr,
	hcl_oow_t         len
);

typedef int (*hcl_fmtout_putuchars_t) (
	hcl_t*            hcl,
	hcl_fmtout_t*     fmtout,
	const hcl_uch_t*  ptr,
	hcl_oow_t         len
);

typedef int (*hcl_fmtout_putobj_t) (
	hcl_t*            hcl,
	hcl_fmtout_t*     fmtout,
	hcl_oop_t         obj
);

enum hcl_fmtout_fmt_type_t
{
	HCL_FMTOUT_FMT_TYPE_BCH = 0,
	HCL_FMTOUT_FMT_TYPE_UCH
};
typedef enum hcl_fmtout_fmt_type_t hcl_fmtout_fmt_type_t;

struct hcl_fmtout_t
{
	hcl_oow_t              count; /* out */

	hcl_mmgr_t*            mmgr; /* in */
	hcl_fmtout_putbchars_t putbchars; /* in */
	hcl_fmtout_putuchars_t putuchars; /* in */
	hcl_fmtout_putobj_t    putobj; /* in - %O is not handled if it's not set. */
	hcl_bitmask_t          mask;   /* in */
	void*                  ctx;    /* in */

	/* internally set as input */
	hcl_fmtout_fmt_type_t  fmt_type;
	const void*            fmt_str;
};

/* =========================================================================
 * FORMATTED INPUT
 * ========================================================================= */

typedef struct hcl_fmtin_t hcl_fmtin_t;

struct hcl_fmtin_t
{
#if 0
	hcl_fmtin_getbchars_t getbchars; /* in */
	hcl_fmtin_getuchars_t getuchars; /* in */
#endif
	void*                 ctx; /* in */
};

#if defined(__cplusplus)
extern "C" {
#endif

/**
 * The hcl_fmt_intmax_to_bcstr() function formats an integer \a value to a
 * multibyte string according to the given base and writes it to a buffer
 * pointed to by \a buf. It writes to the buffer at most \a size characters
 * including the terminating null. The base must be between 2 and 36 inclusive
 * and can be ORed with zero or more #hcl_fmt_intmax_to_bcstr_flag_t enumerators.
 * This ORed value is passed to the function via the \a base_and_flags
 * parameter. If the formatted string is shorter than \a bufsize, the redundant
 * slots are filled with the fill character \a fillchar if it is not a null
 * character. The filling behavior is determined by the flags shown below:
 *
 * - If #HCL_FMT_INTMAX_TO_BCSTR_FILLRIGHT is set in \a base_and_flags, slots
 *   after the formatting string are filled.
 * - If #HCL_FMT_INTMAX_TO_BCSTR_FILLCENTER is set in \a base_and_flags, slots
 *   before the formatting string are filled. However, if it contains the
 *   sign character, the slots between the sign character and the digit part
 *   are filled.
 * - If neither #HCL_FMT_INTMAX_TO_BCSTR_FILLRIGHT nor #HCL_FMT_INTMAX_TO_BCSTR_FILLCENTER
 *   , slots before the formatting string are filled.
 *
 * The \a precision parameter specified the minimum number of digits to
 * produce from the \a value. If \a value produces fewer digits than
 * \a precision, the actual digits are padded with '0' to meet the precision
 * requirement. You can pass a negative number if you don't wish to specify
 * precision.
 *
 * The terminating null is not added if #HCL_FMT_INTMAX_TO_BCSTR_NONULL is set;
 * The #HCL_FMT_INTMAX_TO_BCSTR_UPPERCASE flag indicates that the function should
 * use the uppercase letter for a alphabetic digit;
 * You can set #HCL_FMT_INTMAX_TO_BCSTR_NOTRUNC if you require lossless formatting.
 * The #HCL_FMT_INTMAX_TO_BCSTR_PLUSSIGN flag and #HCL_FMT_INTMAX_TO_BCSTR_EMPTYSIGN
 * ensures that the plus sign and a space is added for a positive integer
 * including 0 respectively.
 * The #HCL_FMT_INTMAX_TO_BCSTR_ZEROLEAD flag ensures that the numeric string
 * begins with '0' before applying the prefix.
 * You can set the #HCL_FMT_INTMAX_TO_BCSTR_NOZERO flag if you want the value of
 * 0 to produce nothing. If both #HCL_FMT_INTMAX_TO_BCSTR_NOZERO and
 * #HCL_FMT_INTMAX_TO_BCSTR_ZEROLEAD are specified, '0' is still produced.
 *
 * If \a prefix is not #HCL_NULL, it is inserted before the digits.
 *
 * \return
 *  - -1 if the base is not between 2 and 36 inclusive.
 *  - negated number of characters required for lossless formatting
 *   - if \a bufsize is 0.
 *   - if #HCL_FMT_INTMAX_TO_BCSTR_NOTRUNC is set and \a bufsize is less than
 *     the minimum required for lossless formatting.
 *  - number of characters written to the buffer excluding a terminating
 *    null in all other cases.
 */
HCL_EXPORT int hcl_fmt_intmax_to_bcstr (
	hcl_bch_t*       buf,             /**< buffer pointer */
	int              bufsize,         /**< buffer size */
	hcl_intmax_t     value,           /**< integer to format */
	int              base_and_flags,  /**< base ORed with flags */
	int              precision,       /**< precision */
	hcl_bch_t        fillchar,        /**< fill character */
	const hcl_bch_t* prefix           /**< prefix */
);

/**
 * The hcl_fmt_intmax_to_ucstr() function formats an integer \a value to a
 * wide-character string according to the given base and writes it to a buffer
 * pointed to by \a buf. It writes to the buffer at most \a size characters
 * including the terminating null. The base must be between 2 and 36 inclusive
 * and can be ORed with zero or more #hcl_fmt_intmax_to_ucstr_flag_t enumerators.
 * This ORed value is passed to the function via the \a base_and_flags
 * parameter. If the formatted string is shorter than \a bufsize, the redundant
 * slots are filled with the fill character \a fillchar if it is not a null
 * character. The filling behavior is determined by the flags shown below:
 *
 * - If #HCL_FMT_INTMAX_TO_UCSTR_FILLRIGHT is set in \a base_and_flags, slots
 *   after the formatting string are filled.
 * - If #HCL_FMT_INTMAX_TO_UCSTR_FILLCENTER is set in \a base_and_flags, slots
 *   before the formatting string are filled. However, if it contains the
 *   sign character, the slots between the sign character and the digit part
 *   are filled.
 * - If neither #HCL_FMT_INTMAX_TO_UCSTR_FILLRIGHT nor #HCL_FMT_INTMAX_TO_UCSTR_FILLCENTER
 *   , slots before the formatting string are filled.
 *
 * The \a precision parameter specified the minimum number of digits to
 * produce from the \ value. If \a value produces fewer digits than
 * \a precision, the actual digits are padded with '0' to meet the precision
 * requirement. You can pass a negative number if don't wish to specify
 * precision.
 *
 * The terminating null is not added if #HCL_FMT_INTMAX_TO_UCSTR_NONULL is set;
 * The #HCL_FMT_INTMAX_TO_UCSTR_UPPERCASE flag indicates that the function should
 * use the uppercase letter for a alphabetic digit;
 * You can set #HCL_FMT_INTMAX_TO_UCSTR_NOTRUNC if you require lossless formatting.
 * The #HCL_FMT_INTMAX_TO_UCSTR_PLUSSIGN flag and #HCL_FMT_INTMAX_TO_UCSTR_EMPTYSIGN
 * ensures that the plus sign and a space is added for a positive integer
 * including 0 respectively.
 * The #HCL_FMT_INTMAX_TO_UCSTR_ZEROLEAD flag ensures that the numeric string
 * begins with 0 before applying the prefix.
 * You can set the #HCL_FMT_INTMAX_TO_UCSTR_NOZERO flag if you want the value of
 * 0 to produce nothing. If both #HCL_FMT_INTMAX_TO_UCSTR_NOZERO and
 * #HCL_FMT_INTMAX_TO_UCSTR_ZEROLEAD are specified, '0' is still produced.
 *
 * If \a prefix is not #HCL_NULL, it is inserted before the digits.
 *
 * \return
 *  - -1 if the base is not between 2 and 36 inclusive.
 *  - negated number of characters required for lossless formatting
 *   - if \a bufsize is 0.
 *   - if #HCL_FMT_INTMAX_TO_UCSTR_NOTRUNC is set and \a bufsize is less than
 *     the minimum required for lossless formatting.
 *  - number of characters written to the buffer excluding a terminating
 *    null in all other cases.
 */
HCL_EXPORT int hcl_fmt_intmax_to_ucstr (
	hcl_uch_t*       buf,             /**< buffer pointer */
	int              bufsize,         /**< buffer size */
	hcl_intmax_t     value,           /**< integer to format */
	int              base_and_flags,  /**< base ORed with flags */
	int              precision,       /**< precision */
	hcl_uch_t        fillchar,        /**< fill character */
	const hcl_uch_t* prefix           /**< prefix */
);

/**
 * The hcl_fmt_uintmax_to_bcstr() function formats an unsigned integer \a value
 * to a multibyte string buffer. It behaves the same as hcl_fmt_intmax_to_bcstr()
 * except that it handles an unsigned integer.
 */
HCL_EXPORT int hcl_fmt_uintmax_to_bcstr (
	hcl_bch_t*       buf,             /**< buffer pointer */
	int              bufsize,         /**< buffer size */
	hcl_uintmax_t    value,           /**< integer to format */
	int              base_and_flags,  /**< base ORed with flags */
	int              precision,       /**< precision */
	hcl_bch_t        fillchar,        /**< fill character */
	const hcl_bch_t* prefix           /**< prefix */
);

/**
 * The hcl_fmt_uintmax_to_ucstr() function formats an unsigned integer \a value
 * to a multibyte string buffer. It behaves the same as hcl_fmt_intmax_to_ucstr()
 * except that it handles an unsigned integer.
 */
HCL_EXPORT int hcl_fmt_uintmax_to_ucstr (
	hcl_uch_t*       buf,             /**< buffer pointer */
	int              bufsize,         /**< buffer size */
	hcl_uintmax_t    value,           /**< integer to format */
	int              base_and_flags,  /**< base ORed with flags */
	int              precision,       /**< precision */
	hcl_uch_t        fillchar,        /**< fill character */
	const hcl_uch_t* prefix           /**< prefix */
);

#if defined(HCL_OOCH_IS_BCH)
#	define hcl_fmt_intmax_to_oocstr hcl_fmt_intmax_to_bcstr
#	define hcl_fmt_uintmax_to_oocstr hcl_fmt_uintmax_to_bcstr
#else
#	define hcl_fmt_intmax_to_oocstr hcl_fmt_intmax_to_ucstr
#	define hcl_fmt_uintmax_to_oocstr hcl_fmt_uintmax_to_ucstr
#endif


/* TODO: hcl_fmt_fltmax_to_bcstr()... hcl_fmt_fltmax_to_ucstr() */


/* =========================================================================
 * FORMATTED OUTPUT
 * ========================================================================= */
HCL_EXPORT int hcl_bfmt_outv (
	hcl_t*           hcl,
	hcl_fmtout_t*    fmtout,
	const hcl_bch_t* fmt,
	va_list          ap
);

HCL_EXPORT int hcl_ufmt_outv (
	hcl_t*           hcl,
	hcl_fmtout_t*    fmtout,
	const hcl_uch_t* fmt,
	va_list          ap
);


HCL_EXPORT int hcl_bfmt_out (
	hcl_t*           hcl,
	hcl_fmtout_t*    fmtout,
	const hcl_bch_t* fmt,
	...
);

HCL_EXPORT int hcl_ufmt_out (
	hcl_t*           hcl,
	hcl_fmtout_t*    fmtout,
	const hcl_uch_t* fmt,
	...
);

#if defined(__cplusplus)
}
#endif

#endif
