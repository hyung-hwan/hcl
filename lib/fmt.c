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

/*
 * This file contains a formatted output routine derived from kvprintf()
 * of FreeBSD. It has been heavily modified and bug-fixed.
 */

/*
 * Copyright (c) 1986, 1988, 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */


#include "hcl-prv.h"


#if defined(HCL_ENABLE_FLTFMT)

#include <stdio.h> /* for snrintf(). used for floating-point number formatting */
#if defined(_MSC_VER) || (defined(__BORLANDC__) && (__BORLANDC__ > 0x520)) || (defined(__WATCOMC__) && (__WATCOMC__ < 1200))
#	define snprintf _snprintf
#	if !defined(HAVE_SNPRINTF)
#		define HAVE_SNPRINTF
#	endif
#	if defined(__OS2__) && defined(__BORLANDC__)
#		undef HAVE_SNPRINTF
#	endif
#endif

#if defined(HAVE_QUADMATH_H)
#	include <quadmath.h> /* for quadmath_snprintf() */
#elif defined(HAVE_QUADMATH_SNPRINTF)
extern int quadmath_snprintf (const char *str, size_t size, const char *format, ...);
#endif

#endif

/* Max number conversion buffer length:
 * hcl_intmax_t in base 2, plus NUL byte. */
#define MAXNBUF (HCL_SIZEOF(hcl_intmax_t) * HCL_BITS_PER_BYTE + 1)

enum fmt_spec_t
{
	/* integer */
	LF_C = (1 << 0),
	LF_H = (1 << 1),
	LF_J = (1 << 2),
	LF_L = (1 << 3),
	LF_Q = (1 << 4),
	LF_T = (1 << 5),
	LF_Z = (1 << 6),

	/* long double */
	LF_LD = (1 << 7),
	/* __float128 */
	LF_QD = (1 << 8)
};

static struct
{
	hcl_uint8_t flag; /* for single occurrence */
	hcl_uint8_t dflag; /* for double occurrence */
} lm_tab[26] =
{
	{ 0,    0 }, /* a */
	{ 0,    0 }, /* b */
	{ 0,    0 }, /* c */
	{ 0,    0 }, /* d */
	{ 0,    0 }, /* e */
	{ 0,    0 }, /* f */
	{ 0,    0 }, /* g */
	{ LF_H, LF_C }, /* h */
	{ 0,    0 }, /* i */
	{ LF_J, 0 }, /* j */
	{ 0,    0 }, /* k */
	{ LF_L, LF_Q }, /* l */
	{ 0,    0 }, /* m */
	{ 0,    0 }, /* n */
	{ 0,    0 }, /* o */
	{ 0,    0 }, /* p */
	{ LF_Q, 0 }, /* q */
	{ 0,    0 }, /* r */
	{ 0,    0 }, /* s */
	{ LF_T, 0 }, /* t */
	{ 0,    0 }, /* u */
	{ 0,    0 }, /* v */
	{ 0,    0 }, /* w */
	{ 0,    0 }, /* z */
	{ 0,    0 }, /* y */
	{ LF_Z, 0 }, /* z */
};


enum
{
	FLAGC_DOT       = (1 << 0),
	FLAGC_SPACE     = (1 << 1),
	FLAGC_SHARP     = (1 << 2),
	FLAGC_SIGN      = (1 << 3),
	FLAGC_LEFTADJ   = (1 << 4),
	FLAGC_ZEROPAD   = (1 << 5),
	FLAGC_WIDTH     = (1 << 6),
	FLAGC_PRECISION = (1 << 7),
	FLAGC_STAR1     = (1 << 8),
	FLAGC_STAR2     = (1 << 9),
	FLAGC_LENMOD    = (1 << 10) /* length modifier */
};

static const hcl_bch_t hex2ascii_lower[] =
{
	'0','1','2','3','4','5','6','7','8','9',
	'a','b','c','d','e','f','g','h','i','j','k','l','m',
	'n','o','p','q','r','s','t','u','v','w','x','y','z'
};

static const hcl_bch_t hex2ascii_upper[] =
{
	'0','1','2','3','4','5','6','7','8','9',
	'A','B','C','D','E','F','G','H','I','J','K','L','M',
	'N','O','P','Q','R','S','T','U','V','W','X','H','Z'
};

static hcl_uch_t uch_nullstr[] = { '(','n','u','l','l', ')','\0' };
static hcl_bch_t bch_nullstr[] = { '(','n','u','l','l', ')','\0' };

/* ------------------------------------------------------------------------- */

/*define static int fmt_uintmax_to_bcstr(...)*/
#undef char_t
#undef fmt_uintmax
#define char_t hcl_bch_t
#define fmt_uintmax fmt_uintmax_to_bcstr
#include "fmt-imp.h"

/*define static int fmt_uintmax_to_ucstr(...)*/
#undef char_t
#undef fmt_uintmax
#define char_t hcl_uch_t
#define fmt_uintmax fmt_uintmax_to_ucstr
#include "fmt-imp.h"

int hcl_fmt_intmax_to_bcstr (
	hcl_bch_t* buf, int size,
	hcl_intmax_t value, int base_and_flags, int prec,
	hcl_bch_t fillchar, const hcl_bch_t* prefix)
{
	hcl_bch_t signchar;
	hcl_uintmax_t absvalue;

	if (value < 0)
	{
		signchar = '-';
		absvalue = -value;
	}
	else if (base_and_flags & HCL_FMT_INTMAX_TO_BCSTR_PLUSSIGN)
	{
		signchar = '+';
		absvalue = value;
	}
	else if (base_and_flags & HCL_FMT_INTMAX_TO_BCSTR_EMPTYSIGN)
	{
		signchar = ' ';
		absvalue = value;
	}
	else
	{
		signchar = '\0';
		absvalue = value;
	}

	return fmt_uintmax_to_bcstr(buf, size, absvalue, base_and_flags, prec, fillchar, signchar, prefix);
}

int hcl_fmt_uintmax_to_bcstr (
	hcl_bch_t* buf, int size,
	hcl_uintmax_t value, int base_and_flags, int prec,
	hcl_bch_t fillchar, const hcl_bch_t* prefix)
{
	hcl_bch_t signchar;

	/* determine if a sign character is needed */
	if (base_and_flags & HCL_FMT_INTMAX_TO_BCSTR_PLUSSIGN)
	{
		signchar = '+';
	}
	else if (base_and_flags & HCL_FMT_INTMAX_TO_BCSTR_EMPTYSIGN)
	{
		signchar = ' ';
	}
	else
	{
		signchar = '\0';
	}

	return fmt_uintmax_to_bcstr(buf, size, value, base_and_flags, prec, fillchar, signchar, prefix);
}

/* ==================== wide-char ===================================== */

int hcl_fmt_intmax_to_ucstr (
	hcl_uch_t* buf, int size,
	hcl_intmax_t value, int base_and_flags, int prec,
	hcl_uch_t fillchar, const hcl_uch_t* prefix)
{
	hcl_uch_t signchar;
	hcl_uintmax_t absvalue;

	if (value < 0)
	{
		signchar = '-';
		absvalue = -value;
	}
	else if (base_and_flags & HCL_FMT_INTMAX_TO_UCSTR_PLUSSIGN)
	{
		signchar = '+';
		absvalue = value;
	}
	else if (base_and_flags & HCL_FMT_INTMAX_TO_UCSTR_EMPTYSIGN)
	{
		signchar = ' ';
		absvalue = value;
	}
	else
	{
		signchar = '\0';
		absvalue = value;
	}

	return fmt_uintmax_to_ucstr(buf, size, absvalue, base_and_flags, prec, fillchar, signchar, prefix);
}

int hcl_fmt_uintmax_to_ucstr (
	hcl_uch_t* buf, int size,
	hcl_uintmax_t value, int base_and_flags, int prec,
	hcl_uch_t fillchar, const hcl_uch_t* prefix)
{
	hcl_uch_t signchar;

	/* determine if a sign character is needed */
	if (base_and_flags & HCL_FMT_INTMAX_TO_UCSTR_PLUSSIGN)
	{
		signchar = '+';
	}
	else if (base_and_flags & HCL_FMT_INTMAX_TO_UCSTR_EMPTYSIGN)
	{
		signchar = ' ';
	}
	else
	{
		signchar = '\0';
	}

	return fmt_uintmax_to_ucstr(buf, size, value, base_and_flags, prec, fillchar, signchar, prefix);
}

/* ------------------------------------------------------------------------- */
/*
 * Put a NUL-terminated ASCII number (base <= 36) in a buffer in reverse
 * order; return an optional length and a pointer to the last character
 * written in the buffer (i.e., the first character of the string).
 * The buffer pointed to by `nbuf' must have length >= MAXNBUF.
 */

static hcl_bch_t* sprintn_lower (hcl_bch_t* nbuf, hcl_uintmax_t num, int base, hcl_ooi_t* lenp)
{
	hcl_bch_t* p;

	p = nbuf;
	*p = '\0';
	do { *++p = hex2ascii_lower[num % base]; } while (num /= base);

	if (lenp) *lenp = p - nbuf;
	return p; /* returns the end */
}

static hcl_bch_t* sprintn_upper (hcl_bch_t* nbuf, hcl_uintmax_t num, int base, hcl_ooi_t* lenp)
{
	hcl_bch_t* p;

	p = nbuf;
	*p = '\0';
	do { *++p = hex2ascii_upper[num % base]; } while (num /= base);

	if (lenp) *lenp = p - nbuf;
	return p; /* returns the end */
}

/* ------------------------------------------------------------------------- */
#define PUT_BCH(hcl,fmtout,c,n) do { \
	hcl_oow_t _yy; \
	hcl_bch_t _cc = c; \
	for (_yy = 0; _yy < n; _yy++) \
	{ \
		int _xx; \
		if ((_xx = fmtout->putbchars(hcl, fmtout, &_cc, 1)) <= -1) goto oops; \
		if (_xx == 0) goto done; \
		fmtout->count++; \
	} \
} while (0)

#define PUT_BCS(hcl,fmtout,ptr,len) do { \
	if (len > 0) { \
		int _xx; \
		if ((_xx = fmtout->putbchars(hcl, fmtout, ptr, len)) <= -1) goto oops; \
		if (_xx == 0) goto done; \
		fmtout->count += len; \
	} \
} while (0)

#define PUT_UCH(hcl,fmtout,c,n) do { \
	hcl_oow_t _yy; \
	hcl_uch_t _cc = c; \
	for (_yy = 0; _yy < n; _yy++) \
	{ \
		int _xx; \
		if ((_xx = fmtout->putuchars(hcl, fmtout, &_cc, 1)) <= -1) goto oops; \
		if (_xx == 0) goto done; \
		fmtout->count++; \
	} \
} while (0)

#define PUT_UCS(hcl,fmtout,ptr,len) do { \
	if (len > 0) { \
		int _xx; \
		if ((_xx = fmtout->putuchars(hcl, fmtout, ptr, len)) <= -1) goto oops; \
		if (_xx == 0) goto done; \
		fmtout->count += len; \
	} \
} while (0)


#if defined(HCL_OOCH_IS_BCH)
#	define PUT_OOCH(hcl,fmtout,c,n) PUT_BCH(hcl,fmtout,c,n)
#	define PUT_OOCS(hcl,fmtout,ptr,len) PUT_BCS(hcl,fmtout,ptr,len)
#else
#	define PUT_OOCH(hcl,fmtout,c,n) PUT_UCH(hcl,fmtout,c,n)
#	define PUT_OOCS(hcl,fmtout,ptr,len) PUT_UCS(hcl,fmtout,ptr,len)
#endif

#define BYTE_PRINTABLE(x) ((x >= 'a' && x <= 'z') || (x >= 'A' &&  x <= 'Z') || (x >= '0' && x <= '9') || (x == ' '))


#define PUT_BYTE_IN_HEX(hcl,fmtout,byte,extra_flags) do { \
	hcl_bch_t __xbuf[3]; \
	hcl_byte_to_bcstr ((byte), __xbuf, HCL_COUNTOF(__xbuf), (16 | (extra_flags)), '0'); \
	PUT_BCH(hcl, fmtout, __xbuf[0], 1); \
	PUT_BCH(hcl, fmtout, __xbuf[1], 1); \
} while (0)

/* ------------------------------------------------------------------------- */
static int fmt_outv (hcl_t* hcl, hcl_fmtout_t* fmtout, va_list ap)
{
	const hcl_uint8_t* fmtptr, * percent;
	int fmtchsz;

	hcl_uch_t uch;
	hcl_bch_t bch;
	hcl_ooch_t padc;

	int n, base, neg, sign;
	hcl_ooi_t tmp, width, precision;
	int lm_flag, lm_dflag, flagc, numlen;

	hcl_uintmax_t num = 0;
	hcl_bch_t nbuf[MAXNBUF];
	const hcl_bch_t* nbufp;
	int stop = 0;

#if defined(HCL_ENABLE_FLTFMT)
	struct
	{
		struct
		{
			hcl_bch_t  sbuf[32];
			hcl_bch_t* ptr;
			hcl_oow_t  capa;
		} fmt;
		struct
		{
			hcl_bch_t  sbuf[64];
			hcl_bch_t* ptr;
			hcl_oow_t  capa;
		} out;
	} fb; /* some buffers for handling float-point number formatting */
#endif

	hcl_bch_t* (*sprintn) (hcl_bch_t* nbuf, hcl_uintmax_t num, int base, hcl_ooi_t* lenp);

	fmtptr = (const hcl_uint8_t*)fmtout->fmt_str;
	switch (fmtout->fmt_type)
	{
		case HCL_FMTOUT_FMT_TYPE_BCH:
			fmtchsz = HCL_SIZEOF_BCH_T;
			break;
		case HCL_FMTOUT_FMT_TYPE_UCH:
			fmtchsz = HCL_SIZEOF_UCH_T;
			break;
	}

	/* this is an internal function. it doesn't reset count to 0 */
	/* fmtout->count = 0; */
#if defined(HCL_ENABLE_FLTFMT)
	fb.fmt.ptr = fb.fmt.sbuf;
	fb.fmt.capa = HCL_COUNTOF(fb.fmt.sbuf) - 1;
	fb.out.ptr = fb.out.sbuf;
	fb.out.capa = HCL_COUNTOF(fb.out.sbuf) - 1;
#endif

	while (1)
	{
	#if defined(HAVE_LABELS_AS_VALUES)
		static void* before_percent_tab[] = { &&before_percent_bch, &&before_percent_uch };
		goto *before_percent_tab[fmtout->fmt_type];
	#else
		switch (fmtout->fmt_type)
		{
			case HCL_FMTOUT_FMT_TYPE_BCH:
				goto before_percent_bch;
			case HCL_FMTOUT_FMT_TYPE_UCH:
				goto before_percent_uch;
		}
	#endif

	before_percent_bch:
		{
			const hcl_bch_t* start, * end;
			start = end = (const hcl_bch_t*)fmtptr;
			while ((bch = *end++) != '%' || stop)
			{
				if (bch == '\0')
				{
					PUT_BCS (hcl, fmtout, start, end - start - 1);
					goto done;
				}
			}
			PUT_BCS (hcl, fmtout, start, end - start - 1);
			fmtptr = (const hcl_uint8_t*)end;
			percent = (const hcl_uint8_t*)(end - 1);
		}
		goto handle_percent;

	before_percent_uch:
		{
			const hcl_uch_t* start, * end;
			start = end = (const hcl_uch_t*)fmtptr;
			while ((uch = *end++) != '%' || stop)
			{
				if (uch == '\0')
				{
					PUT_UCS (hcl, fmtout, start, end - start - 1);
					goto done;
				}
			}
			PUT_UCS (hcl, fmtout, start, end - start - 1);
			fmtptr = (const hcl_uint8_t*)end;
			percent = (const hcl_uint8_t*)(end - 1);
		}
		goto handle_percent;

	handle_percent:
		padc = ' ';
		width = 0; precision = 0; neg = 0; sign = 0;
		lm_flag = 0; lm_dflag = 0; flagc = 0;
		sprintn = sprintn_lower;

	reswitch:
		switch (fmtout->fmt_type)
		{
			case HCL_FMTOUT_FMT_TYPE_BCH:
				uch = *(const hcl_bch_t*)fmtptr;
				break;
			case HCL_FMTOUT_FMT_TYPE_UCH:
				uch = *(const hcl_uch_t*)fmtptr;
				break;
		}
		fmtptr += fmtchsz;

		switch (uch)
		{
		case '%': /* %% */
			bch = uch;
			goto print_lowercase_c;

		/* flag characters */
		case '.':
			if (flagc & FLAGC_DOT) goto invalid_format;
			flagc |= FLAGC_DOT;
			goto reswitch;

		case '#':
			if (flagc & (FLAGC_WIDTH | FLAGC_DOT | FLAGC_LENMOD)) goto invalid_format;
			flagc |= FLAGC_SHARP;
			goto reswitch;

		case ' ':
			if (flagc & (FLAGC_WIDTH | FLAGC_DOT | FLAGC_LENMOD)) goto invalid_format;
			flagc |= FLAGC_SPACE;
			goto reswitch;

		case '+': /* place sign for signed conversion */
			if (flagc & (FLAGC_WIDTH | FLAGC_DOT | FLAGC_LENMOD)) goto invalid_format;
			flagc |= FLAGC_SIGN;
			goto reswitch;

		case '-': /* left adjusted */
			if (flagc & (FLAGC_WIDTH | FLAGC_DOT | FLAGC_LENMOD)) goto invalid_format;
			if (flagc & FLAGC_DOT)
			{
				goto invalid_format;
			}
			else
			{
				flagc |= FLAGC_LEFTADJ;
				if (flagc & FLAGC_ZEROPAD)
				{
					padc = ' ';
					flagc &= ~FLAGC_ZEROPAD;
				}
			}

			goto reswitch;

		case '*': /* take the length from the parameter */
			if (flagc & FLAGC_DOT)
			{
				if (flagc & (FLAGC_STAR2 | FLAGC_PRECISION)) goto invalid_format;
				flagc |= FLAGC_STAR2;

				precision = va_arg(ap, hcl_ooi_t); /* this deviates from the standard printf that accepts 'int' */
				if (precision < 0)
				{
					/* if precision is less than 0,
					 * treat it as if no .precision is specified */
					flagc &= ~FLAGC_DOT;
					precision = 0;
				}
			}
			else
			{
				if (flagc & (FLAGC_STAR1 | FLAGC_WIDTH)) goto invalid_format;
				flagc |= FLAGC_STAR1;

				width = va_arg(ap, hcl_ooi_t); /* it deviates from the standard printf that accepts 'int' */
				if (width < 0)
				{
					/*
					if (flagc & FLAGC_LEFTADJ)
						flagc  &= ~FLAGC_LEFTADJ;
					else
					*/
						flagc |= FLAGC_LEFTADJ;
					width = -width;
				}
			}
			goto reswitch;

		case '0': /* zero pad */
			if (flagc & FLAGC_LENMOD) goto invalid_format;
			if (!(flagc & (FLAGC_DOT | FLAGC_LEFTADJ)))
			{
				padc = '0';
				flagc |= FLAGC_ZEROPAD;
				goto reswitch;
			}
		/* end of flags characters */

		case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9':
		{
			if (flagc & FLAGC_LENMOD) goto invalid_format;
			for (n = 0;; fmtptr += fmtchsz)
			{
				n = n * 10 + uch - '0';
				switch (fmtout->fmt_type)
				{
					case HCL_FMTOUT_FMT_TYPE_BCH:
						uch = *(const hcl_bch_t*)fmtptr;
						break;
					case HCL_FMTOUT_FMT_TYPE_UCH:
						uch = *(const hcl_uch_t*)fmtptr;
						break;
				}
				if (uch < '0' || uch > '9') break;
			}
			if (flagc & FLAGC_DOT)
			{
				if (flagc & FLAGC_STAR2) goto invalid_format;
				precision = n;
				flagc |= FLAGC_PRECISION;
			}
			else
			{
				if (flagc & FLAGC_STAR1) goto invalid_format;
				width = n;
				flagc |= FLAGC_WIDTH;
			}
			goto reswitch;
		}

		/* length modifiers */
		case 'h': /* short int */
		case 'l': /* long int */
		case 'q': /* long long int */
		case 'j': /* hcl_intmax_t/hcl_uintmax_t */
		case 'z': /* hcl_ooi_t/hcl_oow_t */
		case 't': /* ptrdiff_t */
			if (lm_flag & (LF_LD | LF_QD)) goto invalid_format;

			flagc |= FLAGC_LENMOD;
			if (lm_dflag)
			{
				/* error */
				goto invalid_format;
			}
			else if (lm_flag)
			{
				if (lm_tab[uch - 'a'].dflag && lm_flag == lm_tab[uch - 'a'].flag)
				{
					lm_flag &= ~lm_tab[uch - 'a'].flag;
					lm_flag |= lm_tab[uch - 'a'].dflag;
					lm_dflag |= lm_flag;
					goto reswitch;
				}
				else
				{
					/* error */
					goto invalid_format;
				}
			}
			else
			{
				lm_flag |= lm_tab[uch - 'a'].flag;
				goto reswitch;
			}
			break;

		case 'L': /* long double */
			if (flagc & FLAGC_LENMOD)
			{
				/* conflict with other length modifier */
				goto invalid_format;
			}
			flagc |= FLAGC_LENMOD;
			lm_flag |= LF_LD;
			goto reswitch;

		case 'Q': /* __float128 */
			if (flagc & FLAGC_LENMOD)
			{
				/* conflict with other length modifier */
				goto invalid_format;
			}
			flagc |= FLAGC_LENMOD;
			lm_flag |= LF_QD;
			goto reswitch;
		/* end of length modifiers */

		case 'n': /* number of characters printed so far */
			if (lm_flag & LF_J) /* j */
				*(va_arg(ap, hcl_intmax_t*)) = fmtout->count;
			else if (lm_flag & LF_Z) /* z */
				*(va_arg(ap, hcl_ooi_t*)) = fmtout->count;
		#if (HCL_SIZEOF_LONG_LONG > 0)
			else if (lm_flag & LF_Q) /* ll */
				*(va_arg(ap, long long int*)) = fmtout->count;
		#endif
			else if (lm_flag & LF_L) /* l */
				*(va_arg(ap, long int*)) = fmtout->count;
			else if (lm_flag & LF_H) /* h */
				*(va_arg(ap, short int*)) = fmtout->count;
			else if (lm_flag & LF_C) /* hh */
				*(va_arg(ap, char*)) = fmtout->count;
			else if (flagc & FLAGC_LENMOD)
				goto invalid_format;
			else
				*(va_arg(ap, int*)) = fmtout->count;
			break;

		/* signed integer conversions */
		case 'd':
		case 'i': /* signed conversion */
			base = 10;
			sign = 1;
			goto handle_sign;
		/* end of signed integer conversions */

		/* unsigned integer conversions */
		case 'o':
			base = 8;
			goto handle_nosign;
		case 'u':
			base = 10;
			goto handle_nosign;
		case 'X':
			sprintn = sprintn_upper;
		case 'x':
			base = 16;
			goto handle_nosign;
		case 'b':
			base = 2;
			goto handle_nosign;
		/* end of unsigned integer conversions */

		case 'p': /* pointer */
			base = 16;

			if (width == 0) flagc |= FLAGC_SHARP;
			else flagc &= ~FLAGC_SHARP;

			num = (hcl_uintptr_t)va_arg(ap, void*);
			goto number;

		case 'c':
		{
			/* zeropad must not take effect for 'c' */
			if (flagc & FLAGC_ZEROPAD) padc = ' ';
			if (lm_flag & LF_L) goto uppercase_c;
		#if defined(HCL_OOCH_IS_UCH)
			if (lm_flag & LF_J) goto uppercase_c;
		#endif
		lowercase_c:
			bch = HCL_SIZEOF(hcl_bch_t) < HCL_SIZEOF(int)? va_arg(ap, int): va_arg(ap, hcl_bch_t);

		print_lowercase_c:
			/* precision 0 doesn't kill the letter */
			width--;
			if (!(flagc & FLAGC_LEFTADJ) && width > 0) PUT_BCH (hcl, fmtout, padc, width);
			PUT_BCH (hcl, fmtout, bch, 1);
			if ((flagc & FLAGC_LEFTADJ) && width > 0) PUT_BCH (hcl, fmtout, padc, width);
			break;
		}

		case 'C':
		{
			/* zeropad must not take effect for 'C' */
			if (flagc & FLAGC_ZEROPAD) padc = ' ';
			if (lm_flag & LF_H) goto lowercase_c;
		#if defined(HCL_OOCH_IS_BCH)
			if (lm_flag & LF_J) goto lowercase_c;
		#endif
		uppercase_c:
			uch = HCL_SIZEOF(hcl_uch_t) < HCL_SIZEOF(int)? va_arg(ap, int): va_arg(ap, hcl_uch_t);

			/* precision 0 doesn't kill the letter */
			width--;
			if (!(flagc & FLAGC_LEFTADJ) && width > 0) PUT_UCH (hcl, fmtout, padc, width);
			PUT_UCH (hcl, fmtout, uch, 1);
			if ((flagc & FLAGC_LEFTADJ) && width > 0) PUT_UCH (hcl, fmtout, padc, width);
			break;
		}

		case 's':
		{
			const hcl_bch_t* bsp;

			/* zeropad must not take effect for 'S' */
			if (flagc & FLAGC_ZEROPAD) padc = ' ';
			if (lm_flag & LF_L) goto uppercase_s;
		#if defined(HCL_OOCH_IS_UCH)
			if (lm_flag & LF_J) goto uppercase_s;
		#endif
		lowercase_s:
			bsp = va_arg(ap, hcl_bch_t*);
			if (!bsp) bsp = bch_nullstr;

			n = 0;
			if (flagc & FLAGC_DOT)
			{
				while (n < precision && bsp[n]) n++;
			}
			else
			{
				while (bsp[n]) n++;
			}

			width -= n;

			if (!(flagc & FLAGC_LEFTADJ) && width > 0) PUT_BCH (hcl, fmtout, padc, width);
			PUT_BCS (hcl, fmtout, bsp, n);
			if ((flagc & FLAGC_LEFTADJ) && width > 0) PUT_BCH (hcl, fmtout, padc, width);
			break;
		}

		case 'S':
		{
			const hcl_uch_t* usp;

			/* zeropad must not take effect for 's' */
			if (flagc & FLAGC_ZEROPAD) padc = ' ';
			if (lm_flag & LF_H) goto lowercase_s;
		#if defined(HCL_OOCH_IS_BCH)
			if (lm_flag & LF_J) goto lowercase_s;
		#endif
		uppercase_s:
			usp = va_arg(ap, hcl_uch_t*);
			if (!usp) usp = uch_nullstr;

			n = 0;
			if (flagc & FLAGC_DOT)
			{
				while (n < precision && usp[n]) n++;
			}
			else
			{
				while (usp[n]) n++;
			}

			width -= n;

			if (!(flagc & FLAGC_LEFTADJ) && width > 0) PUT_UCH (hcl, fmtout, padc, width);
			PUT_UCS (hcl, fmtout, usp, n);
			if ((flagc & FLAGC_LEFTADJ) && width > 0) PUT_UCH (hcl, fmtout, padc, width);

			break;
		}

		case 'k':
		case 'K':
		{
			/* byte or multibyte character string in escape sequence */
			const hcl_uint8_t* bsp;
			hcl_oow_t k_hex_width;

			/* zeropad must not take effect for 'k' and 'K'
			 *
 			 * 'h' & 'l' is not used to differentiate hcl_bch_t and hcl_uch_t
			 * because 'k' means hcl_byte_t.
			 * 'l', results in uppercase hexadecimal letters.
			 * 'h' drops the leading \x in the output
			 * --------------------------------------------------------
			 * hk -> \x + non-printable in lowercase hex
			 * k -> all in lowercase hex
			 * lk -> \x +  all in lowercase hex
			 * --------------------------------------------------------
			 * hK -> \x + non-printable in uppercase hex
			 * K -> all in uppercase hex
			 * lK -> \x +  all in uppercase hex
			 * --------------------------------------------------------
			 * with 'k' or 'K', i don't substitute "(null)" for the NULL pointer
			 */
			if (flagc & FLAGC_ZEROPAD) padc = ' ';

			bsp = va_arg(ap, hcl_uint8_t*);
			k_hex_width = (lm_flag & (LF_H | LF_L))? 4: 2;

			if (lm_flag& LF_H)
			{
				if (flagc & FLAGC_DOT)
				{
					/* if precision is specifed, it doesn't stop at the value of zero unlike 's' or 'S' */
					for (n = 0; n < precision; n++) width -= BYTE_PRINTABLE(bsp[n])? 1: k_hex_width;
				}
				else
				{
					for (n = 0; bsp[n]; n++) width -= BYTE_PRINTABLE(bsp[n])? 1: k_hex_width;
				}
			}
			else
			{
				if (flagc & FLAGC_DOT)
				{
					/* if precision is specifed, it doesn't stop at the value of zero unlike 's' or 'S' */
					n = precision;
				}
				else
				{
					for (n = 0; bsp[n]; n++) /* nothing */;
				}
				width -= (n * k_hex_width);
			}

			if (!(flagc & FLAGC_LEFTADJ) && width > 0) PUT_OOCH (hcl, fmtout, padc, width);

			while (n--)
			{
				if ((lm_flag & LF_H) && BYTE_PRINTABLE(*bsp))
				{
					PUT_BCH (hcl, fmtout, *bsp, 1);
				}
				else
				{
					hcl_bch_t xbuf[3];
					hcl_byte_to_bcstr (*bsp, xbuf, HCL_COUNTOF(xbuf), (16 | (uch == 'k'? HCL_BYTE_TO_BCSTR_LOWERCASE: 0)), '0');
					if (lm_flag & (LF_H | LF_L)) PUT_BCS (hcl, fmtout, "\\x", 2);
					PUT_BCS (hcl, fmtout, xbuf, 2);
				}
				bsp++;
			}

			if ((flagc & FLAGC_LEFTADJ) && width > 0) PUT_OOCH (hcl, fmtout, padc, width);
			break;
		}

		case 'w':
		case 'W':
		{
			/* unicode string in unicode escape sequence.
			 *
			 * hw -> \uXXXX, \UXXXXXXXX, printable-byte(only in ascii range)
			 * w -> \uXXXX, \UXXXXXXXX
			 * lw -> all in \UXXXXXXXX
			 */
			const hcl_uch_t* usp;
			hcl_oow_t uwid;

			if (flagc & FLAGC_ZEROPAD) padc = ' ';
			usp = va_arg(ap, hcl_uch_t*);

			if (flagc & FLAGC_DOT)
			{
				/* if precision is specifed, it doesn't stop at the value of zero unlike 's' or 'S' */
				for (n = 0; n < precision; n++)
				{
					if ((lm_flag & LF_H) && BYTE_PRINTABLE(usp[n])) uwid = 1;
					else if (!(lm_flag & LF_L) && usp[n] <= 0xFFFF) uwid = 6;
					else uwid = 10;
					width -= uwid;
				}
			}
			else
			{
				for (n = 0; usp[n]; n++)
				{
					if ((lm_flag & LF_H) && BYTE_PRINTABLE(usp[n])) uwid = 1;
					else if (!(lm_flag & LF_L) && usp[n] <= 0xFFFF) uwid = 6;
					else uwid = 10;
					width -= uwid;
				}
			}

			if (!(flagc & FLAGC_LEFTADJ) && width > 0) PUT_OOCH (hcl, fmtout, padc, width);

			while (n--)
			{
				if ((lm_flag & LF_H) && BYTE_PRINTABLE(*usp))
				{
					PUT_OOCH (hcl, fmtout, *usp, 1);
				}
				else if (!(lm_flag & LF_L) && *usp <= 0xFFFF)
				{
					hcl_uint16_t u16 = *usp;
					int extra_flags = ((uch) == 'w'? HCL_BYTE_TO_BCSTR_LOWERCASE: 0);
					PUT_BCS (hcl, fmtout, "\\u", 2);
					PUT_BYTE_IN_HEX (hcl, fmtout, (u16 >> 8) & 0xFF, extra_flags);
					PUT_BYTE_IN_HEX (hcl, fmtout, u16 & 0xFF, extra_flags);
				}
				else
				{
					hcl_uint32_t u32 = *usp;
					int extra_flags = ((uch) == 'w'? HCL_BYTE_TO_BCSTR_LOWERCASE: 0);
					PUT_BCS (hcl, fmtout, "\\u", 2);
					PUT_BYTE_IN_HEX (hcl, fmtout, (u32 >> 24) & 0xFF, extra_flags);
					PUT_BYTE_IN_HEX (hcl, fmtout, (u32 >> 16) & 0xFF, extra_flags);
					PUT_BYTE_IN_HEX (hcl, fmtout, (u32 >> 8) & 0xFF, extra_flags);
					PUT_BYTE_IN_HEX (hcl, fmtout, u32 & 0xFF, extra_flags);
				}
				usp++;
			}

			if ((flagc & FLAGC_LEFTADJ) && width > 0) PUT_OOCH (hcl, fmtout, padc, width);
			break;
		}

		case 'O': /* object - ignore precision, width, adjustment */
		{
			if (HCL_UNLIKELY(!fmtout->putobj)) goto invalid_format;
			if (fmtout->putobj(hcl, fmtout, va_arg(ap, hcl_oop_t)) <= -1) goto oops;
			break;
		}

		case 'J':
		{
			hcl_bitmask_t tmp;
			if (HCL_UNLIKELY(!fmtout->putobj)) goto invalid_format;
			tmp = fmtout->mask;
			fmtout->mask |= HCL_LOG_PREFER_JSON;
			if (fmtout->putobj(hcl, fmtout, va_arg(ap, hcl_oop_t)) <= -1) goto oops;
			fmtout->mask = tmp;
			break;
		}

#if defined(HCL_ENABLE_FLTFMT)
		case 'e':
		case 'E':
		case 'f':
		case 'F':
		case 'g':
		case 'G':
		/*
		case 'a':
		case 'A':
		*/
		{
			/* let me rely on snprintf until i implement float-point to string conversion */
			int q;
			hcl_oow_t fmtlen;
			union
			{
			#if (HCL_SIZEOF___FLOAT128 > 0) && defined(HAVE_QUADMATH_SNPRINTF)
				__float128 qd;
			#endif
				long double ld;
				double d;
			} v;
			int dtype = 0;
			hcl_oow_t newcapa;
			hcl_bch_t* bsp;

			if (lm_flag & LF_J)
			{
			#if (HCL_SIZEOF___FLOAT128 > 0) && defined(HAVE_QUADMATH_SNPRINTF) && (HCL_SIZEOF_FLTMAX_T == HCL_SIZEOF___FLOAT128)
				v.qd = va_arg(ap, hcl_fltmax_t);
				dtype = LF_QD;
			#elif HCL_SIZEOF_FLTMAX_T == HCL_SIZEOF_DOUBLE
				v.d = va_arg(ap, hcl_fltmax_t);
			#elif HCL_SIZEOF_FLTMAX_T == HCL_SIZEOF_LONG_DOUBLE
				v.ld = va_arg(ap, hcl_fltmax_t);
				dtype = LF_LD;
			#else
				#error Unsupported hcl_flt_t
			#endif
			}
			else if (lm_flag & LF_Z)
			{
				/* hcl_flt_t is limited to double or long double */

				/* precedence goes to double if sizeof(double) == sizeof(long double)
				 * for example, %Lf didn't work on some old platforms.
				 * so i prefer the format specifier with no modifier.
				 */
			#if HCL_SIZEOF_FLT_T == HCL_SIZEOF_DOUBLE
				v.d = va_arg(ap, hcl_flt_t);
			#elif HCL_SIZEOF_FLT_T == HCL_SIZEOF_LONG_DOUBLE
				v.ld = va_arg(ap, hcl_flt_t);
				dtype = LF_LD;
			#else
				#error Unsupported hcl_flt_t
			#endif
			}
			else if (lm_flag & (LF_LD | LF_L))
			{
				v.ld = va_arg(ap, long double);
				dtype = LF_LD;
			}
		#if (HCL_SIZEOF___FLOAT128 > 0) && defined(HAVE_QUADMATH_SNPRINTF)
			else if (lm_flag & (LF_QD | LF_Q))
			{
				v.qd = va_arg(ap, __float128);
				dtype = LF_QD;
			}
		#endif
			else if (flagc & FLAGC_LENMOD)
			{
				goto invalid_format;
			}
			else
			{
				v.d = va_arg(ap, double);
			}

			fmtlen = fmtptr - percent;
			if (fmtlen > fb.fmt.capa)
			{
				if (fb.fmt.ptr == fb.fmt.sbuf)
				{
					fb.fmt.ptr = (hcl_bch_t*)HCL_MMGR_ALLOC(fmtout->mmgr, HCL_SIZEOF(*fb.fmt.ptr) * (fmtlen + 1));
					if (!fb.fmt.ptr) goto oops;
				}
				else
				{
					hcl_bch_t* tmpptr;

					tmpptr = (hcl_bch_t*)HCL_MMGR_REALLOC(fmtout->mmgr, fb.fmt.ptr, HCL_SIZEOF(*fb.fmt.ptr) * (fmtlen + 1));
					if (!tmpptr) goto oops;
					fb.fmt.ptr = tmpptr;
				}

				fb.fmt.capa = fmtlen;
			}

			/* compose back the format specifier */
			fmtlen = 0;
			fb.fmt.ptr[fmtlen++] = '%';
			if (flagc & FLAGC_SPACE) fb.fmt.ptr[fmtlen++] = ' ';
			if (flagc & FLAGC_SHARP) fb.fmt.ptr[fmtlen++] = '#';
			if (flagc & FLAGC_SIGN) fb.fmt.ptr[fmtlen++] = '+';
			if (flagc & FLAGC_LEFTADJ) fb.fmt.ptr[fmtlen++] = '-';
			if (flagc & FLAGC_ZEROPAD) fb.fmt.ptr[fmtlen++] = '0';

			if (flagc & FLAGC_STAR1) fb.fmt.ptr[fmtlen++] = '*';
			else if (flagc & FLAGC_WIDTH)
			{
				fmtlen += hcl_fmt_uintmax_to_bcstr(
					&fb.fmt.ptr[fmtlen], fb.fmt.capa - fmtlen,
					width, 10, -1, '\0', HCL_NULL);
			}
			if (flagc & FLAGC_DOT) fb.fmt.ptr[fmtlen++] = '.';
			if (flagc & FLAGC_STAR2) fb.fmt.ptr[fmtlen++] = '*';
			else if (flagc & FLAGC_PRECISION)
			{
				fmtlen += hcl_fmt_uintmax_to_bcstr(
					&fb.fmt.ptr[fmtlen], fb.fmt.capa - fmtlen,
					precision, 10, -1, '\0', HCL_NULL);
			}

			if (dtype == LF_LD)
				fb.fmt.ptr[fmtlen++] = 'L';
		#if (HCL_SIZEOF___FLOAT128 > 0)
			else if (dtype == LF_QD)
				fb.fmt.ptr[fmtlen++] = 'Q';
		#endif

			fb.fmt.ptr[fmtlen++] = uch;
			fb.fmt.ptr[fmtlen] = '\0';

		#if defined(HAVE_SNPRINTF)
			/* nothing special here */
		#else
			/* best effort to avoid buffer overflow when no snprintf is available.
			 * i really can't do much if it happens. */
			newcapa = precision + width + 32;
			if (fb.out.capa < newcapa)
			{
				/*HCL_ASSERT (hcl, fb.out.ptr == fb.out.sbuf);*/

				fb.out.ptr = (hcl_bch_t*)HCL_MMGR_ALLOC(fmtout->mmgr, HCL_SIZEOF(hcl_bch_t) * (newcapa + 1));
				if (!fb.out.ptr) goto oops;
				fb.out.capa = newcapa;
			}
		#endif

			while (1)
			{
				if (dtype == LF_LD)
				{
				#if defined(HAVE_SNPRINTF)
					q = snprintf((hcl_bch_t*)fb.out.ptr, fb.out.capa + 1, fb.fmt.ptr, v.ld);
				#else
					q = sprintf((hcl_bch_t*)fb.out.ptr, fb.fmt.ptr, v.ld);
				#endif
				}
			#if (HCL_SIZEOF___FLOAT128 > 0) && defined(HAVE_QUADMATH_SNPRINTF)
				else if (dtype == LF_QD)
				{
					q = quadmath_snprintf((hcl_bch_t*)fb.out.ptr, fb.out.capa + 1, fb.fmt.ptr, v.qd);
				}
			#endif
				else
				{
				#if defined(HAVE_SNPRINTF)
					q = snprintf((hcl_bch_t*)fb.out.ptr, fb.out.capa + 1, fb.fmt.ptr, v.d);
				#else
					q = sprintf((hcl_bch_t*)fb.out.ptr, fb.fmt.ptr, v.d);
				#endif
				}
				if (q <= -1) goto oops;
				if (q <= fb.out.capa) break;

				newcapa = fb.out.capa * 2;
				if (newcapa < q) newcapa = q;

				if (fb.out.ptr == fb.out.sbuf)
				{
					fb.out.ptr = (hcl_bch_t*)HCL_MMGR_ALLOC(fmtout->mmgr, HCL_SIZEOF(hcl_bch_t) * (newcapa + 1));
					if (!fb.out.ptr) goto oops;
				}
				else
				{
					hcl_bch_t* tmpptr;
					tmpptr = (hcl_bch_t*)HCL_MMGR_REALLOC(fmtout->mmgr, fb.out.ptr, HCL_SIZEOF(hcl_bch_t) * (newcapa + 1));
					if (!tmpptr) goto oops;
					fb.out.ptr = tmpptr;
				}
				fb.out.capa = newcapa;
			}

			bsp = fb.out.ptr;
			n = 0; while (bsp[n] != '\0') n++;
			PUT_BCS (hcl, fmtout, bsp, n);
			break;
		}
#endif

		handle_nosign:
			sign = 0;
			if (lm_flag & LF_J)
			{
			#if 1 && !defined(__clang__) && defined(__GNUC__) && \
			    (HCL_SIZEOF_UINTMAX_T > HCL_SIZEOF_OOW_T) && \
			    (HCL_SIZEOF_UINTMAX_T != HCL_SIZEOF_LONG_LONG) && \
			    (HCL_SIZEOF_UINTMAX_T != HCL_SIZEOF_LONG)
				/* GCC-compiled binaries crashed when getting hcl_uintmax_t with va_arg.
				 * This is just a work-around for it */
				int i;
				for (i = 0, num = 0; i < HCL_SIZEOF(hcl_uintmax_t) / HCL_SIZEOF(hcl_oow_t); i++)
				{
				#if defined(HCL_ENDIAN_BIG)
					num = num << (8 * HCL_SIZEOF(hcl_oow_t)) | (va_arg (ap, hcl_oow_t));
				#else
					register int shift = i * HCL_SIZEOF(hcl_oow_t);
					hcl_oow_t x = va_arg (ap, hcl_oow_t);
					num |= (hcl_uintmax_t)x << (shift * HCL_BITS_PER_BYTE);
				#endif
				}
			#else
				num = va_arg (ap, hcl_uintmax_t);
			#endif
			}
#if 0
			else if (lm_flag & LF_T)
				num = va_arg(ap, hcl_ptrdiff_t);
#endif
			else if (lm_flag & LF_Z)
				num = va_arg(ap, hcl_oow_t);
			#if (HCL_SIZEOF_LONG_LONG > 0)
			else if (lm_flag & LF_Q)
				num = va_arg(ap, unsigned long long int);
			#endif
			else if (lm_flag & (LF_L | LF_LD))
				num = va_arg(ap, unsigned long int);
			else if (lm_flag & LF_H)
				num = (unsigned short int)va_arg(ap, int);
			else if (lm_flag & LF_C)
				num = (unsigned char)va_arg(ap, int);
			else
				num = va_arg(ap, unsigned int);
			goto number;

		handle_sign:
			if (lm_flag & LF_J)
			{
			#if 1 && !defined(__clang__) && defined(__GNUC__) && \
			    (HCL_SIZEOF_INTMAX_T > HCL_SIZEOF_OOI_T) && \
			    (HCL_SIZEOF_UINTMAX_T != HCL_SIZEOF_LONG_LONG) && \
			    (HCL_SIZEOF_UINTMAX_T != HCL_SIZEOF_LONG)
				/* GCC-compiled binraries crashed when getting hcl_uintmax_t with va_arg.
				 * This is just a work-around for it */
				int i;
				for (i = 0, num = 0; i < HCL_SIZEOF(hcl_intmax_t) / HCL_SIZEOF(hcl_oow_t); i++)
				{
				#if defined(HCL_ENDIAN_BIG)
					num = num << (8 * HCL_SIZEOF(hcl_oow_t)) | (va_arg (ap, hcl_oow_t));
				#else
					register int shift = i * HCL_SIZEOF(hcl_oow_t);
					hcl_oow_t x = va_arg (ap, hcl_oow_t);
					num |= (hcl_uintmax_t)x << (shift * HCL_BITS_PER_BYTE);
				#endif
				}
			#else
				num = va_arg (ap, hcl_intmax_t);
			#endif
			}

#if 0
			else if (lm_flag & LF_T)
				num = va_arg(ap, hcl_ptrdiff_t);
#endif
			else if (lm_flag & LF_Z)
				num = va_arg (ap, hcl_ooi_t);
			#if (HCL_SIZEOF_LONG_LONG > 0)
			else if (lm_flag & LF_Q)
				num = va_arg (ap, long long int);
			#endif
			else if (lm_flag & (LF_L | LF_LD))
				num = va_arg (ap, long int);
			else if (lm_flag & LF_H)
				num = (short int)va_arg (ap, int);
			else if (lm_flag & LF_C)
				num = (char)va_arg (ap, int);
			else
				num = va_arg (ap, int);

		number:
			if (sign && (hcl_intmax_t)num < 0)
			{
				neg = 1;
				num = -(hcl_intmax_t)num;
			}

			nbufp = sprintn(nbuf, num, base, &tmp);
			if ((flagc & FLAGC_SHARP) && num != 0)
			{
				/* #b #o #x */
				if (base == 2 || base == 8 || base == 16) tmp += 2;
			}
			if (neg) tmp++;
			else if (flagc & FLAGC_SIGN) tmp++;
			else if (flagc & FLAGC_SPACE) tmp++;

			numlen = (int)((const hcl_bch_t*)nbufp - (const hcl_bch_t*)nbuf);
			if ((flagc & FLAGC_DOT) && precision > numlen)
			{
				/* extra zeros for precision specified */
				tmp += (precision - numlen);
			}

			if (!(flagc & FLAGC_LEFTADJ) && !(flagc & FLAGC_ZEROPAD) && width > 0 && (width -= tmp) > 0)
			{
				PUT_OOCH (hcl, fmtout, padc, width);
				width = 0;
			}

			if (neg) PUT_OOCH (hcl, fmtout, '-', 1);
			else if (flagc & FLAGC_SIGN) PUT_OOCH (hcl, fmtout, '+', 1);
			else if (flagc & FLAGC_SPACE) PUT_OOCH (hcl, fmtout, ' ', 1);

			if ((flagc & FLAGC_SHARP) && num != 0)
			{
				if (base == 2)
				{
					PUT_OOCH (hcl, fmtout, '0', 1);
					PUT_OOCH (hcl, fmtout, 'b', 1);
				}
				if (base == 8)
				{
					PUT_OOCH (hcl, fmtout, '0', 1);
					PUT_OOCH (hcl, fmtout, 'o', 1);
				}
				else if (base == 16)
				{
					PUT_OOCH (hcl, fmtout, '0', 1);
					PUT_OOCH (hcl, fmtout, 'x', 1);
				}
			}

			if ((flagc & FLAGC_DOT) && precision > numlen)
			{
				/* extra zeros for precision specified */
				PUT_OOCH (hcl, fmtout, '0', precision - numlen);
			}

			if (!(flagc & FLAGC_LEFTADJ) && width > 0 && (width -= tmp) > 0)
			{
				PUT_OOCH (hcl, fmtout, padc, width);
			}

			while (*nbufp) PUT_OOCH (hcl, fmtout, *nbufp--, 1); /* output actual digits */

			if ((flagc & FLAGC_LEFTADJ) && width > 0 && (width -= tmp) > 0)
			{
				PUT_OOCH (hcl, fmtout, padc, width);
			}
			break;

		invalid_format:
			switch (fmtout->fmt_type)
			{
				case HCL_FMTOUT_FMT_TYPE_BCH:
					PUT_BCS (hcl, fmtout, (const hcl_bch_t*)percent, (fmtptr - percent) / fmtchsz);
					break;
				case HCL_FMTOUT_FMT_TYPE_UCH:
					PUT_UCS (hcl, fmtout, (const hcl_uch_t*)percent, (fmtptr - percent) / fmtchsz);
					break;
			}
			break;

		default:
			switch (fmtout->fmt_type)
			{
				case HCL_FMTOUT_FMT_TYPE_BCH:
					PUT_BCS (hcl, fmtout, (const hcl_bch_t*)percent, (fmtptr - percent) / fmtchsz);
					break;
				case HCL_FMTOUT_FMT_TYPE_UCH:
					PUT_UCS (hcl, fmtout, (const hcl_uch_t*)percent, (fmtptr - percent) / fmtchsz);
					break;
			}
			/*
			 * Since we ignore an formatting argument it is no
			 * longer safe to obey the remaining formatting
			 * arguments as the arguments will no longer match
			 * the format specs.
			 */
			stop = 1;
			break;
		}
	}

done:
#if defined(HCL_ENABLE_FLTFMT)
	if (fb.fmt.ptr != fb.fmt.sbuf) HCL_MMGR_FREE (fmtout->mmgr, fb.fmt.ptr);
	if (fb.out.ptr != fb.out.sbuf) HCL_MMGR_FREE (fmtout->mmgr, fb.out.ptr);
#endif
	return 0;

oops:
#if defined(HCL_ENABLE_FLTFMT)
	if (fb.fmt.ptr != fb.fmt.sbuf) HCL_MMGR_FREE (fmtout->mmgr, fb.fmt.ptr);
	if (fb.out.ptr != fb.out.sbuf) HCL_MMGR_FREE (fmtout->mmgr, fb.out.ptr);
#endif
	return -1;
}

int hcl_bfmt_outv (hcl_t* hcl, hcl_fmtout_t* fmtout, const hcl_bch_t* fmt, va_list ap)
{
	int n;
	const void* fmt_str;
	hcl_fmtout_fmt_type_t fmt_type;

	fmt_str = fmtout->fmt_str;
	fmt_type = fmtout->fmt_type;

	fmtout->fmt_type = HCL_FMTOUT_FMT_TYPE_BCH;
	fmtout->fmt_str = fmt;

	n = fmt_outv(hcl, fmtout, ap);

	fmtout->fmt_str = fmt_str;
	fmtout->fmt_type = fmt_type;
	return n;
}

int hcl_ufmt_outv (hcl_t* hcl, hcl_fmtout_t* fmtout, const hcl_uch_t* fmt, va_list ap)
{
	int n;
	const void* fmt_str;
	hcl_fmtout_fmt_type_t fmt_type;

	fmt_str = fmtout->fmt_str;
	fmt_type = fmtout->fmt_type;

	fmtout->fmt_type = HCL_FMTOUT_FMT_TYPE_UCH;
	fmtout->fmt_str = fmt;

	n = fmt_outv(hcl, fmtout, ap);

	fmtout->fmt_str = fmt_str;
	fmtout->fmt_type = fmt_type;
	return n;
}

int hcl_bfmt_out (hcl_t* hcl, hcl_fmtout_t* fmtout, const hcl_bch_t* fmt, ...)
{
	va_list ap;
	int n;
	const void* fmt_str;
	hcl_fmtout_fmt_type_t fmt_type;

	fmt_str = fmtout->fmt_str;
	fmt_type = fmtout->fmt_type;

	fmtout->fmt_type = HCL_FMTOUT_FMT_TYPE_BCH;
	fmtout->fmt_str = fmt;

	va_start (ap, fmt);
	n = fmt_outv(hcl, fmtout, ap);
	va_end (ap);

	fmtout->fmt_str = fmt_str;
	fmtout->fmt_type = fmt_type;
	return n;
}

int hcl_ufmt_out (hcl_t* hcl, hcl_fmtout_t* fmtout, const hcl_uch_t* fmt, ...)
{
	va_list ap;
	int n;
	const void* fmt_str;
	hcl_fmtout_fmt_type_t fmt_type;

	fmt_str = fmtout->fmt_str;
	fmt_type = fmtout->fmt_type;

	fmtout->fmt_type = HCL_FMTOUT_FMT_TYPE_UCH;
	fmtout->fmt_str = fmt;

	va_start (ap, fmt);
	n = fmt_outv(hcl, fmtout, ap);
	va_end (ap);

	fmtout->fmt_str = fmt_str;
	fmtout->fmt_type = fmt_type;
	return n;
}

/* --------------------------------------------------------------------------
 * FORMATTED LOG OUTPUT
 * -------------------------------------------------------------------------- */

static int log_oocs (hcl_t* hcl, hcl_fmtout_t* fmtout, const hcl_ooch_t* ptr, hcl_oow_t len)
{
	hcl_oow_t rem;

	if (len <= 0) return 1;

	if (hcl->log.len > 0 && hcl->log.last_mask != fmtout->mask)
	{
		/* the mask has changed. commit the buffered text */
/* TODO: HANDLE LINE ENDING CONVENTION BETTER... */
		if (hcl->log.ptr[hcl->log.len - 1] != '\n')
		{
			/* no line ending - append a line terminator */
			hcl->log.ptr[hcl->log.len++] = '\n';
		}

		HCL_VMPRIM_LOG_WRITE (hcl, hcl->log.last_mask, hcl->log.ptr, hcl->log.len);
		hcl->log.len = 0;
	}

redo:
	rem = 0;
	if (len > hcl->log.capa - hcl->log.len)
	{
		hcl_oow_t newcapa, max;
		hcl_ooch_t* tmp;

		max = HCL_TYPE_MAX(hcl_oow_t) - hcl->log.len;
		if (len > max)
		{
			/* data too big. */
			rem += len - max;
			len = max;
		}

		newcapa = HCL_ALIGN_POW2(hcl->log.len + len, 512); /* TODO: adjust this capacity */
		if (newcapa > hcl->option.log_maxcapa)
		{
			/* [NOTE]
			 * it doesn't adjust newcapa to hcl->option.log_maxcapa.
			 * nor does it cut the input to fit it into the adjusted capacity.
			 * if maxcapa set is not aligned to HCL_LOG_CAPA_ALIGN,
			 * the largest buffer capacity may be suboptimal */
			goto make_do;
		}

		/* +1 to handle line ending injection more easily */
		tmp = (hcl_ooch_t*)hcl_reallocmem(hcl, hcl->log.ptr, (newcapa + 1) * HCL_SIZEOF(*tmp));
		if (HCL_UNLIKELY(!tmp))
		{
		make_do:
			if (hcl->log.len > 0)
			{
				/* can't expand the buffer. just flush the existing contents */
				/* TODO: HANDLE LINE ENDING CONVENTION BETTER... */
				if (hcl->log.ptr[hcl->log.len - 1] != '\n')
				{
					/* no line ending - append a line terminator */
					hcl->log.ptr[hcl->log.len++] = '\n';
				}
				HCL_VMPRIM_LOG_WRITE (hcl, hcl->log.last_mask, hcl->log.ptr, hcl->log.len);
				hcl->log.len = 0;
			}

			if (len > hcl->log.capa)
			{
				rem += len - hcl->log.capa;
				len = hcl->log.capa;
			}
		}
		else
		{
			hcl->log.ptr = tmp;
			hcl->log.capa = newcapa;
		}
	}

	HCL_MEMCPY (&hcl->log.ptr[hcl->log.len], ptr, len * HCL_SIZEOF(*ptr));
	hcl->log.len += len;
	hcl->log.last_mask = fmtout->mask;

	if (rem > 0)
	{
		ptr += len;
		len = rem;
		goto redo;
	}

	return 1; /* success */
}

#if defined(HCL_OOCH_IS_BCH)
#define log_bcs log_oocs

static int log_ucs (hcl_t* hcl, hcl_fmtout_t* fmtout, const hcl_uch_t* ptr, hcl_oow_t len)
{
	hcl_bch_t bcs[128];
	hcl_oow_t bcslen, rem;

	rem = len;
	while (rem > 0)
	{
		len = rem;
		bcslen = HCL_COUNTOF(bcs);
		hcl_conv_uchars_to_bchars_with_cmgr (ptr, &len, bcs, &bcslen, HCL_CMGR(hcl));
		log_bcs (hcl, fmtout, bcs, bcslen);
		rem -= len;
		ptr += len;
	}
	return 1;
}


#else

#define log_ucs log_oocs

static int log_bcs (hcl_t* hcl, hcl_fmtout_t* fmtout, const hcl_bch_t* ptr, hcl_oow_t len)
{
	hcl_uch_t ucs[64];
	hcl_oow_t ucslen, rem;

	rem = len;
	while (rem > 0)
	{
		len = rem;
		ucslen = HCL_COUNTOF(ucs);
		hcl_conv_bchars_to_uchars_with_cmgr (ptr, &len, ucs, &ucslen, HCL_CMGR(hcl), 1);
		log_ucs (hcl, fmtout, ucs, ucslen);
		rem -= len;
		ptr += len;
	}
	return 1;
}

#endif

hcl_ooi_t hcl_logbfmtv (hcl_t* hcl, hcl_bitmask_t mask, const hcl_bch_t* fmt, va_list ap)

{
	int x;
	hcl_fmtout_t fo;

	if (hcl->log.default_type_mask & HCL_LOG_ALL_TYPES)
	{
		/* if a type is given, it's not untyped any more.
		 * mask off the UNTYPED bit */
		mask &= ~HCL_LOG_UNTYPED;

		/* if the default_type_mask has the UNTYPED bit on,
		 * it'll get turned back on */
		mask |= (hcl->log.default_type_mask & HCL_LOG_ALL_TYPES);
	}
	else if (!(mask & HCL_LOG_ALL_TYPES))
	{
		/* no type is set in the given mask and no default type is set.
		 * make it UNTYPED. */
		mask |= HCL_LOG_UNTYPED;
	}

	if (!fmt)
	{
		/* perform flushing only if fmt is NULL */
		if (hcl->log.len > 0)
		{
			HCL_VMPRIM_LOG_WRITE (hcl, hcl->log.last_mask, hcl->log.ptr, hcl->log.len);
			hcl->log.len = 0;
		}
		HCL_VMPRIM_LOG_WRITE (hcl, hcl->log.last_mask, HCL_NULL, 0); /* forced flushing */
		return 0;
	}

	HCL_MEMSET (&fo, 0, HCL_SIZEOF(fo));
	fo.fmt_type = HCL_FMTOUT_FMT_TYPE_BCH;
	fo.fmt_str = fmt;
	fo.mask = mask;
	fo.mmgr = HCL_MMGR(hcl);
	fo.putbchars = log_bcs;
	fo.putuchars = log_ucs;
	fo.putobj = hcl_fmt_object;

	x = fmt_outv(hcl, &fo, ap);

	if (hcl->log.len > 0 && hcl->log.ptr[hcl->log.len - 1] == '\n')
	{
		HCL_VMPRIM_LOG_WRITE (hcl, hcl->log.last_mask, hcl->log.ptr, hcl->log.len);
		hcl->log.len = 0;
	}

	return (x <= -1)? -1: fo.count;
}

hcl_ooi_t hcl_logbfmt (hcl_t* hcl, hcl_bitmask_t mask, const hcl_bch_t* fmt, ...)
{
	hcl_ooi_t x;
	va_list ap;

	va_start (ap, fmt);
	x = hcl_logbfmtv(hcl, mask, fmt, ap);
	va_end (ap);

	return x;
}

hcl_ooi_t hcl_logufmtv (hcl_t* hcl, hcl_bitmask_t mask, const hcl_uch_t* fmt, va_list ap)
{
	int x;
	hcl_fmtout_t fo;

	if (hcl->log.default_type_mask & HCL_LOG_ALL_TYPES)
	{
		/* if a type is given, it's not untyped any more.
		 * mask off the UNTYPED bit */
		mask &= ~HCL_LOG_UNTYPED;

		/* if the default_type_mask has the UNTYPED bit on,
		 * it'll get turned back on */
		mask |= (hcl->log.default_type_mask & HCL_LOG_ALL_TYPES);
	}
	else if (!(mask & HCL_LOG_ALL_TYPES))
	{
		/* no type is set in the given mask and no default type is set.
		 * make it UNTYPED. */
		mask |= HCL_LOG_UNTYPED;
	}


	if (!fmt)
	{
		/* perform flushing only if fmt is NULL */
		if (hcl->log.len > 0)
		{
			HCL_VMPRIM_LOG_WRITE (hcl, hcl->log.last_mask, hcl->log.ptr, hcl->log.len);
			hcl->log.len = 0;
		}
		HCL_VMPRIM_LOG_WRITE (hcl, hcl->log.last_mask, HCL_NULL, 0); /* forced flushing */
		return 0;
	}

	HCL_MEMSET (&fo, 0, HCL_SIZEOF(fo));
	fo.fmt_type = HCL_FMTOUT_FMT_TYPE_UCH;
	fo.fmt_str = fmt;
	fo.mask = mask;
	fo.mmgr = HCL_MMGR(hcl);
	fo.putbchars = log_bcs;
	fo.putuchars = log_ucs;
	fo.putobj = hcl_fmt_object;

	x = fmt_outv(hcl, &fo, ap);

	if (hcl->log.len > 0 && hcl->log.ptr[hcl->log.len - 1] == '\n')
	{
		HCL_VMPRIM_LOG_WRITE (hcl, hcl->log.last_mask, hcl->log.ptr, hcl->log.len);
		hcl->log.len = 0;
	}

	return (x <= -1)? -1: fo.count;
}

hcl_ooi_t hcl_logufmt (hcl_t* hcl, hcl_bitmask_t mask, const hcl_uch_t* fmt, ...)
{
	hcl_ooi_t x;
	va_list ap;

	va_start (ap, fmt);
	x = hcl_logufmtv(hcl, mask, fmt, ap);
	va_end (ap);

	return x;
}

/* --------------------------------------------------------------------------
 * PRINT SUPPORT
 * -------------------------------------------------------------------------- */

static int print_bcs (hcl_t* hcl, hcl_fmtout_t* fmtout, const hcl_bch_t* ptr, hcl_oow_t len)
{
	hcl_bch_t* optr;

	if (HCL_UNLIKELY(!hcl->io.udo_wrtr))
	{
		hcl_seterrbmsg (hcl, HCL_EINVAL, "no user-defined output handler");
		return -1;
	}

	optr = (hcl_bch_t*)ptr;
	while (len > 0)
	{
		hcl->io.udo_arg.ptr = optr;
		hcl->io.udo_arg.len = len;

		if (hcl->io.udo_wrtr(hcl, HCL_IO_WRITE_BYTES, &hcl->io.udo_arg) <= -1) return -1;
		if (hcl->io.udo_arg.xlen <= 0) return 0; /* end of stream. but not failure */

		HCL_ASSERT (hcl, hcl->io.udo_arg.xlen <= len);
		optr += hcl->io.udo_arg.xlen;
		len -= hcl->io.udo_arg.xlen;
	}

	return 1; /* success */
}

static int print_ucs (hcl_t* hcl, hcl_fmtout_t* fmtout, const hcl_uch_t* ptr, hcl_oow_t len)
{
#if defined(HCL_OOCH_IS_UCH)
	hcl_uch_t* optr;
#else
	hcl_oow_t bcslen, ucslen;
	hcl_ooch_t bcsbuf[64], * bcsptr;

#endif

	if (HCL_UNLIKELY(!hcl->io.udo_wrtr))
	{
		hcl_seterrbmsg (hcl, HCL_EINVAL, "no user-defined output handler");
		return -1;
	}

#if defined(HCL_OOCH_IS_UCH)
	optr = (hcl_uch_t*)ptr;
	while (len > 0)
	{
		hcl->io.udo_arg.ptr = optr;
		hcl->io.udo_arg.len = len;

		if (hcl->io.udo_wrtr(hcl, HCL_IO_WRITE, &hcl->io.udo_arg) <= -1) return -1;
		if (hcl->io.udo_arg.xlen <= 0) return 0; /* end of stream. but not failure */

		HCL_ASSERT (hcl, hcl->io.udo_arg.xlen <= len);
		optr += hcl->io.udo_arg.xlen;
		len -= hcl->io.udo_arg.xlen;
	}
#else
	while (len > 0)
	{
		ucslen = len;
		bcslen = HCL_COUNTOF(bcsbuf);
		hcl_conv_uchars_to_bchars_with_cmgr(ptr, &ucslen, bcsbuf, &bcslen, HCL_CMGR(hcl));

		bcsptr = bcsbuf;
		while (bcslen > 0)
		{
			hcl->io.udo_arg.ptr = bcsptr;
			hcl->io.udo_arg.len = bcslen;

			if (hcl->io.udo_wrtr(hcl, HCL_IO_WRITE, &hcl->io.udo_arg) <= -1) return -1;
			if (hcl->io.udo_arg.xlen <= 0) return 0; /* end of stream. but not failure */

			HCL_ASSERT (hcl, hcl->io.udo_arg.xlen <= len);
			bcsptr += hcl->io.udo_arg.xlen;
			bcslen -= hcl->io.udo_arg.xlen;
		}

		ptr += ucslen;
		len -= ucslen;
	}
#endif

	return 1; /* success */
}


hcl_ooi_t hcl_prbfmtv (hcl_t* hcl, const hcl_bch_t* fmt, va_list ap)
{
	int x;
	hcl_fmtout_t fo;

	HCL_MEMSET (&fo, 0, HCL_SIZEOF(fo));
	fo.fmt_type = HCL_FMTOUT_FMT_TYPE_BCH;
	fo.fmt_str = fmt;
	fo.mask = 0;
	fo.mmgr = HCL_MMGR(hcl);
	fo.putbchars = print_bcs;
	fo.putuchars = print_ucs;
	fo.putobj = hcl_fmt_object;

	x = fmt_outv(hcl, &fo, ap);

	return (x <= -1)? -1: fo.count;
}

hcl_ooi_t hcl_prbfmt (hcl_t* hcl, const hcl_bch_t* fmt, ...)
{
	hcl_ooi_t x;
	va_list ap;

	va_start (ap, fmt);
	x = hcl_prbfmtv(hcl, fmt, ap);
	va_end (ap);

	return x;
}

hcl_ooi_t hcl_prufmtv (hcl_t* hcl, const hcl_uch_t* fmt, va_list ap)
{
	int x;

	hcl_fmtout_t fo;

	HCL_MEMSET (&fo, 0, HCL_SIZEOF(fo));
	fo.fmt_type = HCL_FMTOUT_FMT_TYPE_UCH;
	fo.fmt_str = fmt;
	fo.mask = 0;
	fo.mmgr = HCL_MMGR(hcl);
	fo.putbchars = print_bcs;
	fo.putuchars = print_ucs;
	fo.putobj = hcl_fmt_object;

	x = fmt_outv(hcl, &fo, ap);

	return (x <= -1)? -1: fo.count;
}

hcl_ooi_t hcl_prufmt (hcl_t* hcl, const hcl_uch_t* fmt, ...)
{
	hcl_ooi_t x;
	va_list ap;

	va_start (ap, fmt);
	x = hcl_prufmtv(hcl, fmt, ap);
	va_end (ap);

	return x;
}

/* --------------------------------------------------------------------------
 * SUPPORT FOR FORMATTED OUTPUT TO BE USED BY BUILTIN PRIMITIVE FUNCTIONS
 * -------------------------------------------------------------------------- */

static int sprint_bcs (hcl_t* hcl, hcl_fmtout_t* fmtout, const hcl_bch_t* ptr, hcl_oow_t len)
{
	hcl_oow_t unused, oolen, blen;

	unused = hcl->sprintf.xbuf.capa - hcl->sprintf.xbuf.len;

#if defined(HCL_OOCH_IS_UCH)
	blen = len;
	hcl_conv_bchars_to_uchars_with_cmgr (ptr, &blen, HCL_NULL, &oolen, HCL_CMGR(hcl), 1);
#else
	oolen = len;
#endif

	if (oolen > unused)
	{
		hcl_ooch_t* tmp;
		hcl_oow_t newcapa;

		newcapa = hcl->sprintf.xbuf.len + oolen + 1;
		newcapa = HCL_ALIGN_POW2(newcapa, 256);

		tmp = (hcl_ooch_t*)hcl_reallocmem(hcl, hcl->sprintf.xbuf.ptr, newcapa * HCL_SIZEOF(*tmp));
		if (!tmp) return -1;

		hcl->sprintf.xbuf.ptr = tmp;
		hcl->sprintf.xbuf.capa = newcapa;
	}

#if defined(HCL_OOCH_IS_UCH)
	hcl_conv_bchars_to_uchars_with_cmgr (ptr, &len, &hcl->sprintf.xbuf.ptr[hcl->sprintf.xbuf.len], &oolen, HCL_CMGR(hcl), 1);
#else
	HCL_MEMCPY (&hcl->sprintf.xbuf.ptr[hcl->sprintf.xbuf.len], ptr, len * HCL_SIZEOF(*ptr));
#endif
	hcl->sprintf.xbuf.len += oolen;

	return 1; /* success */
}

static int sprint_ucs (hcl_t* hcl, hcl_fmtout_t* fmtout, const hcl_uch_t* ptr, hcl_oow_t len)
{
	hcl_oow_t unused, oolen, ulen;

	unused = hcl->sprintf.xbuf.capa - hcl->sprintf.xbuf.len;

#if defined(HCL_OOCH_IS_UCH)
	oolen = len;
#else
	ulen = len;
	hcl_conv_uchars_to_bchars_with_cmgr (ptr, &ulen, HCL_NULL, &oolen, HCL_CMGR(hcl));
#endif

	if (oolen > unused)
	{
		hcl_ooch_t* tmp;
		hcl_oow_t newcapa;

		newcapa = hcl->sprintf.xbuf.len + oolen + 1;
		newcapa = HCL_ALIGN_POW2(newcapa, 256);

		tmp = (hcl_ooch_t*)hcl_reallocmem(hcl, hcl->sprintf.xbuf.ptr, newcapa * HCL_SIZEOF(*tmp));
		if (!tmp) return -1;

		hcl->sprintf.xbuf.ptr = tmp;
		hcl->sprintf.xbuf.capa = newcapa;
	}

#if defined(HCL_OOCH_IS_UCH)
	HCL_MEMCPY (&hcl->sprintf.xbuf.ptr[hcl->sprintf.xbuf.len], ptr, len * HCL_SIZEOF(*ptr));
#else
	hcl_conv_uchars_to_bchars_with_cmgr (ptr, &len, &hcl->sprintf.xbuf.ptr[hcl->sprintf.xbuf.len], &oolen, HCL_CMGR(hcl));
#endif
	hcl->sprintf.xbuf.len += oolen;

	return 1; /* success */
}

#define GET_NEXT_ARG_TO(hcl,nargs,arg_state,arg) do { \
	if ((arg_state)->idx >= nargs) { (arg_state)->stop = 1;  goto invalid_format; } \
	arg = HCL_STACK_GETARG(hcl, nargs, (arg_state)->idx); \
	(arg_state)->idx++; \
} while(0)

#define GET_NEXT_CHAR_TO(hcl,fmt,fmtend,ch) do { \
	if (fmt >= fmtend) ch = HCL_OOCI_EOF; \
	else { ch = *(fmt); (fmt)++; }\
} while(0)

static HCL_INLINE int format_stack_args (hcl_t* hcl, hcl_fmtout_t* fmtout, hcl_ooi_t nargs, int rcv_is_fmtstr)
{
	const hcl_ooch_t* fmtptr, * fmtend;
	const hcl_ooch_t* checkpoint, * percent;

	int n, radix, neg, sign, radix_flags;
	hcl_ooi_t extra, width, precision;
	hcl_ooch_t padc, ooch;
	hcl_ooci_t ch;
	int flagc, lm_flag;

	struct
	{
		hcl_ooi_t idx;
		int stop;
	} arg_state;
	hcl_oop_t arg;

	HCL_ASSERT (hcl, fmtout->putobj != HCL_NULL);

	fmtout->count = 0;

	if (rcv_is_fmtstr)
	{
		arg = HCL_STACK_GETRCV(hcl, nargs);
		arg_state.idx = 0;
	}
	else
	{
		arg = HCL_STACK_GETARG(hcl, nargs, 0);
		arg_state.idx = 1;
	}

	if (!HCL_OOP_IS_POINTER(arg) || HCL_OBJ_GET_FLAGS_TYPE(arg) != HCL_OBJ_TYPE_CHAR)
	{
		hcl_ooi_t i;
		/* if the first argument is not a valid formatting string,
		 * print all arguments as objects */
		if (fmtout->putobj(hcl, fmtout, arg) <= -1) goto oops;
		for (i = arg_state.idx; i < nargs; i++)
		{
			arg = HCL_STACK_GETARG(hcl, nargs, i);
			if (fmtout->putobj(hcl, fmtout, arg) <= -1) goto oops;
		}
		return 0;
	}

	arg_state.stop = 0;

	fmtptr = HCL_OBJ_GET_CHAR_SLOT(arg);
	fmtend = fmtptr + HCL_OBJ_GET_SIZE(arg);

	while (1)
	{
		checkpoint = fmtptr;

		while (1)
		{
			GET_NEXT_CHAR_TO (hcl, fmtptr, fmtend, ch);
			if (ch == '%' && !arg_state.stop) break;

			if (ch == HCL_OOCI_EOF)
			{
				/* fmt is not advanced when it is length-bounded.
				 * so not fmt - checkpoint - 1 */
				PUT_OOCS (hcl, fmtout, checkpoint, fmtptr - checkpoint);
				goto done;
			}
		}
		PUT_OOCS (hcl, fmtout, checkpoint, fmtptr - checkpoint - 1);

		percent = fmtptr - 1;

		padc = ' ';
		width = 0; precision = 0;
		neg = 0; sign = 0;

		lm_flag = 0; flagc = 0;
		radix_flags = HCL_INTTOSTR_NONEWOBJ;

	reswitch:
		GET_NEXT_CHAR_TO (hcl, fmtptr, fmtend, ch);
		switch (ch)
		{
		case '%': /* %% */
			ooch = ch;
			goto print_char;

		/* flag characters */
		case '.':
			if (flagc & FLAGC_DOT) goto invalid_format;
			flagc |= FLAGC_DOT;
			goto reswitch;

		case '#':
			if (flagc & (FLAGC_WIDTH | FLAGC_DOT | FLAGC_LENMOD)) goto invalid_format;
			flagc |= FLAGC_SHARP;
			goto reswitch;

		case ' ':
			if (flagc & (FLAGC_WIDTH | FLAGC_DOT | FLAGC_LENMOD)) goto invalid_format;
			flagc |= FLAGC_SPACE;
			goto reswitch;

		case '+': /* place sign for signed conversion */
			if (flagc & (FLAGC_WIDTH | FLAGC_DOT | FLAGC_LENMOD)) goto invalid_format;
			flagc |= FLAGC_SIGN;
			goto reswitch;

		case '-': /* left adjusted */
			if (flagc & (FLAGC_WIDTH | FLAGC_DOT | FLAGC_LENMOD)) goto invalid_format;
			if (flagc & FLAGC_DOT)
			{
				goto invalid_format;
			}
			else
			{
				flagc |= FLAGC_LEFTADJ;
				if (flagc & FLAGC_ZEROPAD)
				{
					padc = ' ';
					flagc &= ~FLAGC_ZEROPAD;
				}
			}

			goto reswitch;

		case '*': /* take the length from the parameter */
			if (flagc & FLAGC_DOT)
			{
				if (flagc & (FLAGC_STAR2 | FLAGC_PRECISION)) goto invalid_format;
				flagc |= FLAGC_STAR2;

				GET_NEXT_ARG_TO (hcl, nargs, &arg_state, arg);
				if (hcl_inttoooi(hcl, arg, &precision) <= -1) goto invalid_format;
				if (precision < 0)
				{
					/* if precision is less than 0,
					 * treat it as if no .precision is specified */
					flagc &= ~FLAGC_DOT;
					precision = 0;
				}
			}
			else
			{
				if (flagc & (FLAGC_STAR1 | FLAGC_WIDTH)) goto invalid_format;
				flagc |= FLAGC_STAR1;

				GET_NEXT_ARG_TO (hcl, nargs, &arg_state, arg);
				if (hcl_inttoooi(hcl, arg, &width) <= -1) goto invalid_format;
				if (width < 0)
				{
					flagc |= FLAGC_LEFTADJ;
					width = -width;
				}
			}
			goto reswitch;

		case '0': /* zero pad */
			if (flagc & FLAGC_LENMOD) goto invalid_format;
			if (!(flagc & (FLAGC_DOT | FLAGC_LEFTADJ)))
			{
				padc = '0';
				flagc |= FLAGC_ZEROPAD;
				goto reswitch;
			}
		/* end of flags characters */

		case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9':
			if (flagc & FLAGC_LENMOD) goto invalid_format;
			for (n = 0;; ++fmtptr)
			{
				n = n * 10 + ch - '0';
				ch = *fmtptr;
				if (ch < '0' || ch > '9') break;
			}
			if (flagc & FLAGC_DOT)
			{
				if (flagc & FLAGC_STAR2) goto invalid_format;
				precision = n;
				flagc |= FLAGC_PRECISION;
			}
			else
			{
				if (flagc & FLAGC_STAR1) goto invalid_format;
				width = n;
				flagc |= FLAGC_WIDTH;
			}
			goto reswitch;

		/* length modifiers - used for k/K. not useful for s/S/d/i/o/u/x/X/b/f */
		case 'h': /* short int */
		case 'l': /* long int */
			if (lm_flag & (LF_L | LF_H)) goto invalid_format;
			flagc |= FLAGC_LENMOD;
			lm_flag |= lm_tab[ch - 'a'].flag;
			goto reswitch;

		/* integer conversions */
		case 'd':
		case 'i': /* signed conversion */
			radix = 10;
			sign = 1;
			goto print_integer;
		case 'o':
			radix = 8;
			goto print_integer;
		case 'u':
			radix = 10;
			goto print_integer;
		case 'x':
			radix_flags |= HCL_INTTOSTR_LOWERCASE;
		case 'X':
			radix = 16;
			goto print_integer;
		case 'b':
			radix = 2;
			goto print_integer;
		/* end of integer conversions */

		case 'f':
		{
			const hcl_ooch_t* nsptr;
			hcl_oow_t nslen;
			hcl_oow_t scale = 0;

			GET_NEXT_ARG_TO (hcl, nargs, &arg_state, arg);
			if (HCL_OOP_IS_CHAR(arg))
			{
				arg = HCL_SMOOI_TO_OOP(HCL_OOP_TO_CHAR(arg));
			}
			else if (HCL_IS_FPDEC(hcl, arg))
			{
				hcl_oop_fpdec_t fa = (hcl_oop_fpdec_t)arg;
				scale = HCL_OOP_TO_SMOOI(fa->scale);
				arg = fa->value;
			}

			if (!hcl_inttostr(hcl, arg, 10 | HCL_INTTOSTR_NONEWOBJ))
			{
				HCL_LOG2 (hcl, HCL_LOG_WARN | HCL_LOG_UNTYPED, "unable to convert %O for float output\n", arg, hcl_geterrmsg(hcl));
				goto invalid_format;
			}

			nsptr = hcl->inttostr.xbuf.ptr;
			nslen = hcl->inttostr.xbuf.len;
			HCL_ASSERT (hcl, nslen > 0);

			if (nsptr[0] == '-')
			{
				HCL_ASSERT (hcl, (HCL_OOP_IS_SMOOI(arg) && HCL_OOP_TO_SMOOI(arg) < 0) || HCL_IS_NBIGINT(hcl,arg));
				nsptr++;
				nslen--;
				neg = 1;
			}

			if (!(flagc & FLAGC_DOT))
			{
				precision = scale;
				if (precision <= 0) precision = 1;
			}

			if ((flagc & FLAGC_DOT) && precision < scale)
			{
				hcl_oow_t diff  = scale - precision;
				scale = precision;
				nslen = (nslen < diff)? 0: (nslen - diff);
			}

			if (nslen < scale + 1)
			{
				extra = 1;
				if (precision > 0) extra += 1 + scale;
			}
			else
			{
				extra = 0;
				if (nslen > 0) extra += nslen - scale;
				if (precision > 0)
				{
					extra += 1;
					if (nslen > 0) extra += scale;
				}
			}

			if (neg) extra++;
			else if (flagc & FLAGC_SIGN) extra++;
			else if (flagc & FLAGC_SPACE) extra++;

			if ((flagc & FLAGC_DOT) && precision > scale)
			{
				/* trailing zeros in the fractional part */
				extra += precision - scale;
			}

			if (!(flagc & FLAGC_LEFTADJ) && !(flagc & FLAGC_ZEROPAD) && width > extra)
			{
				width -= extra;
				PUT_OOCH (hcl, fmtout, padc, width);
				width = 0;
			}
			if (neg) PUT_OOCH (hcl, fmtout, '-', 1);
			else if (flagc & FLAGC_SIGN) PUT_OOCH (hcl, fmtout, '+', 1);
			else if (flagc & FLAGC_SPACE) PUT_OOCH (hcl, fmtout, ' ', 1);

			if (!(flagc & FLAGC_LEFTADJ) && width > extra)
			{
				width -= extra;
				PUT_OOCH (hcl, fmtout, padc, width);
			}

			if (nslen < scale + 1)
			{
				PUT_OOCH (hcl, fmtout, '0', 1);
				if (precision > 0)
				{
					PUT_OOCH (hcl, fmtout, '.', 1);
					PUT_OOCH (hcl, fmtout, '0', scale - nslen);
					PUT_OOCS (hcl, fmtout, nsptr, nslen);
				}
			}
			else
			{
				if (nslen > 0) PUT_OOCS (hcl, fmtout, nsptr, nslen - scale);
				if (precision > 0)
				{
					PUT_OOCH (hcl, fmtout, '.', 1);
					if (nslen > 0) PUT_OOCS (hcl, fmtout, &nsptr[nslen - scale], scale);
				}
			}
			if (precision > scale)
			{
				/* trailing zeros in the fractional part */
				PUT_OOCH (hcl, fmtout, '0', precision - scale);
			}

			if ((flagc & FLAGC_LEFTADJ) && width > extra)
			{
				width -= extra;
				PUT_OOCH (hcl, fmtout, padc, width);
			}
			break;
		}

		case 'c':
		case 'C':
			GET_NEXT_ARG_TO (hcl, nargs, &arg_state, arg);
			if (HCL_OOP_IS_SMOOI(arg)) arg = HCL_CHAR_TO_OOP(HCL_OOP_TO_SMOOI(arg));
			if (!HCL_OOP_IS_CHAR(arg)) goto invalid_format;
			ooch = HCL_OOP_TO_CHAR(arg);

		print_char:
			/* zeropad must not take effect for 'c' */
			if (flagc & FLAGC_ZEROPAD) padc = ' ';

			/* precision 0 doesn't kill the letter */
			width--;
			if (!(flagc & FLAGC_LEFTADJ) && width > 0) PUT_OOCH (hcl, fmtout, padc, width);
			PUT_OOCH (hcl, fmtout, ooch, 1);
			if ((flagc & FLAGC_LEFTADJ) && width > 0) PUT_OOCH (hcl, fmtout, padc, width);
			break;

		case 's':
		case 'S':
		{
			/* zeropad must not take effect for 'S' */
			if (flagc & FLAGC_ZEROPAD) padc = ' ';

			GET_NEXT_ARG_TO (hcl, nargs, &arg_state, arg);
			if (!HCL_OOP_IS_POINTER(arg)) goto invalid_format;
			switch (HCL_OBJ_GET_FLAGS_TYPE(arg))
			{
				case HCL_OBJ_TYPE_CHAR:
				{
					/* string, symbol */
					const hcl_ooch_t* oosp;
					hcl_oow_t oosl;

					oosp = HCL_OBJ_GET_CHAR_SLOT(arg);
					oosl = HCL_OBJ_GET_SIZE(arg);

					if (flagc & FLAGC_DOT)
					{
						if (oosl > precision) oosl = precision;
					}
					width -= oosl;

					if (!(flagc & FLAGC_LEFTADJ) && width > 0) PUT_OOCH (hcl, fmtout, padc, width);
					PUT_OOCS (hcl, fmtout, oosp, oosl);
					if ((flagc & FLAGC_LEFTADJ) && width > 0) PUT_OOCH (hcl, fmtout, padc, width);
					break;
				}

				case HCL_OBJ_TYPE_BYTE:
				{
					/* byte array */
					const hcl_uint8_t* bsp;
					hcl_oow_t bsl;

					bsp = HCL_OBJ_GET_BYTE_SLOT(arg);
					bsl = HCL_OBJ_GET_SIZE(arg);

					if (flagc & FLAGC_DOT)
					{
						if (bsl > precision) bsl = precision;
					}
					width -= bsl;

					if (!(flagc & FLAGC_LEFTADJ) && width > 0) PUT_OOCH (hcl, fmtout, padc, width);
					PUT_BCS (hcl, fmtout, (const hcl_bch_t*)bsp, bsl);
					if ((flagc & FLAGC_LEFTADJ) && width > 0) PUT_OOCH (hcl, fmtout, padc, width);
					break;
				}

				default:
					goto invalid_format;
			}

			break;
		}

	#if !defined(HCL_OOCH_IS_UCH)
		case 'w': /* the string object is not in the unicode encoding */
		case 'W': /* treat w/W like k/K */
	#endif
		case 'k':
		case 'K':
		{
			const hcl_uint8_t* bsp;
			hcl_oow_t bsl, k_hex_width;

			GET_NEXT_ARG_TO (hcl, nargs, &arg_state, arg);
			if (!HCL_OOP_IS_POINTER(arg)) goto invalid_format;

			if (flagc & FLAGC_ZEROPAD) padc = ' ';

			switch (HCL_OBJ_GET_FLAGS_TYPE(arg))
			{
				case HCL_OBJ_TYPE_CHAR:
					bsp = (const hcl_uint8_t*)HCL_OBJ_GET_CHAR_SLOT(arg);
					bsl = HCL_OBJ_GET_SIZE(arg) * HCL_SIZEOF_OOCH_T;
					goto format_byte_in_k;

				case HCL_OBJ_TYPE_BYTE:
					bsp = HCL_OBJ_GET_BYTE_SLOT(arg);
					bsl = HCL_OBJ_GET_SIZE(arg);

				format_byte_in_k:
					k_hex_width = (lm_flag & (LF_H | LF_L))? 4: 2;

					if (flagc & FLAGC_DOT)
					{
						n = (precision > bsl)? bsl: precision;
					}
					else n = bsl;

					if (lm_flag & LF_H)
					{
						hcl_oow_t i;
						for (i = 0; i < n; i++) width -= BYTE_PRINTABLE(bsp[i])? 1: k_hex_width;
					}
					else
					{
						width -= (n * k_hex_width);
					}

					if (!(flagc & FLAGC_LEFTADJ) && width > 0) PUT_OOCH (hcl, fmtout, padc, width);

					while (n--)
					{
						if ((lm_flag & LF_H) && BYTE_PRINTABLE(*bsp))
						{
							PUT_BCH (hcl, fmtout, *bsp, 1);
						}
						else
						{
							hcl_bch_t xbuf[3];
							int flagged_radix = 16;
						#if defined(HCL_OOCH_IS_UCH)
							if (ch == 'k') flagged_radix |= HCL_BYTE_TO_BCSTR_LOWERCASE;
						#else
							if (ch == 'k' || ch == 'w') flagged_radix |= HCL_BYTE_TO_BCSTR_LOWERCASE;
						#endif
							hcl_byte_to_bcstr (*bsp, xbuf, HCL_COUNTOF(xbuf), flagged_radix, '0');
							if (lm_flag & (LF_H | LF_L)) PUT_BCS (hcl, fmtout, "\\x", 2);
							PUT_BCS (hcl, fmtout, xbuf, 2);
						}
						bsp++;
					}

					if ((flagc & FLAGC_LEFTADJ) && width > 0) PUT_OOCH (hcl, fmtout, padc, width);
					break;

				default:
					goto invalid_format;
			}
			break;
		}

	#if defined(HCL_OOCH_IS_UCH)
		case 'w':
		case 'W':
		{
			/* unicode string in unicode escape sequence.
			 *
			 * hw -> \uXXXX, \UXXXXXXXX, printable-byte(only in ascii range)
			 * w -> \uXXXX, \UXXXXXXXX
			 * lw -> all in \UXXXXXXXX
			 */
			const hcl_uch_t* usp;
			hcl_oow_t usl, i, uwid;

			GET_NEXT_ARG_TO (hcl, nargs, &arg_state, arg);
			if (!HCL_OOP_IS_POINTER(arg) || HCL_OBJ_GET_FLAGS_TYPE(arg) != HCL_OBJ_TYPE_CHAR) goto invalid_format;

			if (flagc & FLAGC_ZEROPAD) padc = ' ';

			usp = HCL_OBJ_GET_CHAR_SLOT(arg);
			usl = HCL_OBJ_GET_SIZE(arg);

			if (flagc & FLAGC_DOT)
			{
				n = (precision > usl)? usl: precision;
			}
			else n = usl;

			for (i = 0; i < n; i++)
			{
				if ((lm_flag & LF_H) && BYTE_PRINTABLE(usp[n])) uwid = 1;
				else if (!(lm_flag & LF_L) && usp[n] <= 0xFFFF) uwid = 6;
				else uwid = 10;
				width -= uwid;
			}

			if (!(flagc & FLAGC_LEFTADJ) && width > 0) PUT_OOCH (hcl, fmtout, padc, width);

			while (n--)
			{
				if ((lm_flag & LF_H) && BYTE_PRINTABLE(*usp))
				{
					PUT_OOCH (hcl, fmtout, *usp, 1);
				}
				else if (!(lm_flag & LF_L) && *usp <= 0xFFFF)
				{
					hcl_uint16_t u16 = *usp;
					int extra_flags = ((ch) == 'w'? HCL_BYTE_TO_BCSTR_LOWERCASE: 0);
					PUT_BCS (hcl, fmtout, "\\u", 2);
					PUT_BYTE_IN_HEX (hcl, fmtout, (u16 >> 8) & 0xFF, extra_flags);
					PUT_BYTE_IN_HEX (hcl, fmtout, u16 & 0xFF, extra_flags);
				}
				else
				{
					hcl_uint32_t u32 = *usp;
					int extra_flags = ((ch) == 'w'? HCL_BYTE_TO_BCSTR_LOWERCASE: 0);
					PUT_BCS (hcl, fmtout, "\\u", 2);
					PUT_BYTE_IN_HEX (hcl, fmtout, (u32 >> 24) & 0xFF, extra_flags);
					PUT_BYTE_IN_HEX (hcl, fmtout, (u32 >> 16) & 0xFF, extra_flags);
					PUT_BYTE_IN_HEX (hcl, fmtout, (u32 >> 8) & 0xFF, extra_flags);
					PUT_BYTE_IN_HEX (hcl, fmtout, u32 & 0xFF, extra_flags);
				}
				usp++;
			}

			if ((flagc & FLAGC_LEFTADJ) && width > 0) PUT_OOCH (hcl, fmtout, padc, width);
			break;
		}
	#endif

		case 'O': /* object - ignore precision, width, adjustment */
			GET_NEXT_ARG_TO (hcl, nargs, &arg_state, arg);
			if (fmtout->putobj(hcl, fmtout, arg) <= -1) goto oops;
			break;

		case 'J':
		{
			hcl_bitmask_t tmp;
			GET_NEXT_ARG_TO (hcl, nargs, &arg_state, arg);
			tmp = fmtout->mask;
			fmtout->mask |= HCL_LOG_PREFER_JSON;
			if (fmtout->putobj(hcl, fmtout, arg) <= -1) goto oops;
			fmtout->mask = tmp;
			break;
		}

		print_integer:
		{
			const hcl_ooch_t* nsptr;
			hcl_oow_t nslen;

			GET_NEXT_ARG_TO (hcl, nargs, &arg_state, arg);
			if (HCL_OOP_IS_CHAR(arg))
			{
				arg = HCL_SMOOI_TO_OOP(HCL_OOP_TO_CHAR(arg));
			}
			else if (HCL_IS_FPDEC(hcl, arg))
			{
				hcl_oop_t nv;
				hcl_oop_fpdec_t fa = (hcl_oop_fpdec_t)arg;

				/* the given number for integer output is a fixed-point decimal.
				 * i will drop all digits after the fixed point */
				hcl_pushvolat (hcl, &arg);
				nv = hcl_truncfpdecval(hcl, fa->value, HCL_OOP_TO_SMOOI(fa->scale), 0);
				hcl_popvolat (hcl);
				if (!nv)
				{
					HCL_LOG1 (hcl, HCL_LOG_WARN | HCL_LOG_UNTYPED, "unable to truncate a fixed-point number %O to an integer for output\n", arg);
					goto invalid_format;
				}

				arg = nv;
			}

			if (!hcl_inttostr(hcl, arg, radix | radix_flags))
			{
				/*hcl_seterrbfmt (hcl, HCL_EINVAL, "not a valid number - %O", arg);
				goto oops;*/
				HCL_LOG2 (hcl, HCL_LOG_WARN | HCL_LOG_UNTYPED, "unable to convert %O for integer output - %js\n", arg, hcl_geterrmsg(hcl));
				goto invalid_format;
			}

			nsptr = hcl->inttostr.xbuf.ptr;
			nslen = hcl->inttostr.xbuf.len;

			HCL_ASSERT (hcl, nslen > 0);
			if (nsptr[0] == '-')
			{
				/* a negative number was given. i must skip the minus sign
				 * added by hcl_inttostr() for a negative number. */
				HCL_ASSERT (hcl, (HCL_OOP_IS_SMOOI(arg) && HCL_OOP_TO_SMOOI(arg) < 0) || HCL_IS_NBIGINT(hcl,arg));
				nsptr++;
				nslen--;
			}

			extra = nslen;
			if (sign && ((HCL_OOP_IS_SMOOI(arg) && HCL_OOP_TO_SMOOI(arg) < 0) || HCL_IS_NBIGINT(hcl,arg))) neg = 1;

			if ((flagc & FLAGC_SHARP) && arg != HCL_SMOOI_TO_OOP(0))
			{
				/* #b #o #x */
				if (radix == 2 || radix == 8 || radix == 16) extra += 2;
			}
			if (neg) extra++;
			else if (flagc & FLAGC_SIGN) extra++;
			else if (flagc & FLAGC_SPACE) extra++;

			if ((flagc & FLAGC_DOT) && precision > nslen)
			{
				/* extra zeros for precision specified */
				extra += (precision - nslen);
			}

			if (!(flagc & FLAGC_LEFTADJ) && !(flagc & FLAGC_ZEROPAD) && width > 0 && (width -= extra) > 0)
			{
				PUT_OOCH (hcl, fmtout, padc, width);
				width = 0;
			}

			if (neg) PUT_OOCH (hcl, fmtout, '-', 1);
			else if (flagc & FLAGC_SIGN) PUT_OOCH (hcl, fmtout, '+', 1);
			else if (flagc & FLAGC_SPACE) PUT_OOCH (hcl, fmtout, ' ', 1);

			if ((flagc & FLAGC_SHARP) && arg != HCL_SMOOI_TO_OOP(0))
			{
				if (radix == 2)
				{
					PUT_OOCH (hcl, fmtout, '0', 1);
					PUT_OOCH (hcl, fmtout, 'b', 1);
				}
				if (radix == 8)
				{
					PUT_OOCH (hcl, fmtout, '0', 1);
					PUT_OOCH (hcl, fmtout, 'o', 1);
				}
				else if (radix == 16)
				{
					PUT_OOCH (hcl, fmtout, '0', 1);
					PUT_OOCH (hcl, fmtout, 'x', 1);
				}
			}

			if ((flagc & FLAGC_DOT) && precision > nslen)
			{
				/* extra zeros for precision specified */
				PUT_OOCH (hcl, fmtout, '0', precision - nslen);
			}

			if (!(flagc & FLAGC_LEFTADJ) && width > 0 && (width -= extra) > 0)
			{
				PUT_OOCH (hcl, fmtout, padc, width);
			}

			PUT_OOCS (hcl, fmtout, nsptr, nslen);

			if ((flagc & FLAGC_LEFTADJ) && width > 0 && (width -= extra) > 0)
			{
				PUT_OOCH (hcl, fmtout, padc, width);
			}
			break;
		}

		invalid_format:
			PUT_OOCS (hcl, fmtout, percent, fmtptr - percent);
			break;

		default:
			PUT_OOCS (hcl, fmtout, percent, fmtptr - percent);
			/*
			 * Since we ignore an formatting argument it is no
			 * longer safe to obey the remaining formatting
			 * arguments as the arguments will no longer match
			 * the format specs.
			 */
			arg_state.stop = 1;
			break;
		}
	}

done:
	return 0;

oops:
	return -1;
}

int hcl_strfmtcallstack (hcl_t* hcl, hcl_ooi_t nargs)
{
	/* format a string using the receiver and arguments on the stack */
	hcl_fmtout_t fo;

	HCL_MEMSET (&fo, 0, HCL_SIZEOF(fo));
	fo.putbchars = sprint_bcs;
	fo.putuchars = sprint_ucs;
	fo.putobj = hcl_fmt_object;
	/* format_stack_args doesn't use fmt_str and fmt_type.
	 * it takes the format string from the stack. */

	hcl->sprintf.xbuf.len = 0;
	return format_stack_args(hcl, &fo, nargs, 0);
}

int hcl_prfmtcallstack (hcl_t* hcl, hcl_ooi_t nargs)
{
	/* format a string using the receiver and arguments on the stack */
	hcl_fmtout_t fo;

	HCL_MEMSET (&fo, 0, HCL_SIZEOF(fo));
	fo.mask = 0;
	fo.mmgr = HCL_MMGR(hcl);
	fo.putbchars = print_bcs;
	fo.putuchars = print_ucs;
	fo.putobj = hcl_fmt_object;
	/* format_stack_args doesn't use fmt_str and fmt_type.
	 * it takes the format string from the stack. */
	return format_stack_args(hcl, &fo, nargs, 0);
}

int hcl_logfmtcallstack (hcl_t* hcl, hcl_ooi_t nargs)
{
	/* format a string using the receiver and arguments on the stack */
	hcl_fmtout_t fo;

	HCL_MEMSET (&fo, 0, HCL_SIZEOF(fo));

	fo.mask = HCL_LOG_FATAL | HCL_LOG_APP;
	if (hcl->log.default_type_mask & HCL_LOG_ALL_TYPES)
	{
		/* if a type is given, it's not untyped any more.
		 * mask off the UNTYPED bit */
		fo.mask &= ~HCL_LOG_UNTYPED;

		/* if the default_type_mask has the UNTYPED bit on,
		 * it'll get turned back on */
		fo.mask |= (hcl->log.default_type_mask & HCL_LOG_ALL_TYPES);
	}

	fo.mmgr = HCL_MMGR(hcl);
	fo.putbchars = log_bcs;
	fo.putuchars = log_ucs;
	fo.putobj = hcl_fmt_object;
	/* format_stack_args doesn't use fmt_str and fmt_type.
	 * it takes the format string from the stack. */

	return format_stack_args(hcl, &fo, nargs, 0);
}

/* --------------------------------------------------------------------------
 * FORMATTED INPUT
 * -------------------------------------------------------------------------- */


static int read_bcs (hcl_t* hcl, hcl_fmtin_t* fmtout, hcl_bch_t* buf, hcl_oow_t len)
{
	if (HCL_UNLIKELY(!hcl->io.udo_wrtr))
	{
		hcl_seterrbmsg (hcl, HCL_EINVAL, "no user-defined output handler");
		return -1;
	}

	return 0;
}

static int read_ucs (hcl_t* hcl, hcl_fmtin_t* fmtin, hcl_uch_t* buf, hcl_oow_t len)
{
	if (HCL_UNLIKELY(!hcl->io.udo_wrtr))
	{
		hcl_seterrbmsg (hcl, HCL_EINVAL, "no user-defined output handler");
		return -1;
	}

	return 0;
}

static HCL_INLINE int fmtin_stack_args (hcl_t* hcl, hcl_fmtin_t* fmtin, hcl_ooi_t nargs, int rcv_is_fmtstr)
{
	/* TODO: */
	return 0;
}

int hcl_scfmtcallstack (hcl_t* hcl, hcl_ooi_t nargs)
{
	hcl_fmtin_t fi;

	HCL_MEMSET (&fi, 0, HCL_SIZEOF(fi));
	/*
	 * TODO:
	fi.getbchars =
	fi.getuchars =
	*/

	return fmtin_stack_args(hcl, &fi, nargs, 0);
}

/* --------------------------------------------------------------------------
 * DYNAMIC STRING FORMATTING
 * -------------------------------------------------------------------------- */

struct fmt_uch_buf_t
{
	hcl_t* hcl;
	hcl_uch_t* ptr;
	hcl_oow_t len;
	hcl_oow_t capa;
};
typedef struct fmt_uch_buf_t fmt_uch_buf_t;

static int fmt_put_bchars_to_uch_buf (hcl_t* hcl, hcl_fmtout_t* fmtout, const hcl_bch_t* ptr, hcl_oow_t len)
{
	fmt_uch_buf_t* b = (fmt_uch_buf_t*)fmtout->ctx;
	hcl_oow_t bcslen, ucslen;
	int n;

	bcslen = len;
	ucslen = b->capa - b->len;
	n = hcl_conv_bchars_to_uchars_with_cmgr(ptr, &bcslen, &b->ptr[b->len], &ucslen, b->hcl->_cmgr, 1);
	b->len += ucslen;
	if (n <= -1)
	{
		if (n == -2)
		{
			return 0; /* buffer full. stop */
		}
		else
		{
			hcl_seterrnum (b->hcl, HCL_EECERR);
			return -1;
		}
	}

	return 1; /* success. carry on */
}

static int fmt_put_uchars_to_uch_buf (hcl_t* hcl, hcl_fmtout_t* fmtout, const hcl_uch_t* ptr, hcl_oow_t len)
{
	fmt_uch_buf_t* b = (fmt_uch_buf_t*)fmtout->ctx;
	hcl_oow_t n;

	/* this function null-terminates the destination. so give the restored buffer size */
	n = hcl_copy_uchars_to_ucstr(&b->ptr[b->len], b->capa - b->len + 1, ptr, len);
	b->len += n;
	if (n < len)
	{
		hcl_seterrnum (b->hcl, HCL_EBUFFULL);
		return 0; /* stop. insufficient buffer */
	}

	return 1; /* success */
}

hcl_oow_t hcl_vfmttoucstr (hcl_t* hcl, hcl_uch_t* buf, hcl_oow_t bufsz, const hcl_uch_t* fmt, va_list ap)
{
	hcl_fmtout_t fo;
	fmt_uch_buf_t fb;

	if (bufsz <= 0) return 0;

	HCL_MEMSET (&fo, 0, HCL_SIZEOF(fo));
	fo.mmgr = hcl->_mmgr;
	fo.putbchars = fmt_put_bchars_to_uch_buf;
	fo.putuchars = fmt_put_uchars_to_uch_buf;
	fo.putobj = hcl_fmt_object;
	fo.ctx = &fb;

	HCL_MEMSET (&fb, 0, HCL_SIZEOF(fb));
	fb.hcl = hcl;
	fb.ptr = buf;
	fb.capa = bufsz - 1;

	if (hcl_ufmt_outv(hcl, &fo, fmt, ap) <= -1) return -1;

	buf[fb.len] = '\0';
	return fb.len;
}

hcl_oow_t hcl_fmttoucstr (hcl_t* hcl, hcl_uch_t* buf, hcl_oow_t bufsz, const hcl_uch_t* fmt, ...)
{
	hcl_oow_t x;
	va_list ap;

	va_start (ap, fmt);
	x = hcl_vfmttoucstr(hcl, buf, bufsz, fmt, ap);
	va_end (ap);

	return x;
}

/* ------------------------------------------------------------------------ */

struct fmt_bch_buf_t
{
	hcl_t* hcl;
	hcl_bch_t* ptr;
	hcl_oow_t len;
	hcl_oow_t capa;
};
typedef struct fmt_bch_buf_t fmt_bch_buf_t;

static int fmt_put_bchars_to_bch_buf (hcl_t* hcl, hcl_fmtout_t* fmtout, const hcl_bch_t* ptr, hcl_oow_t len)
{
	fmt_bch_buf_t* b = (fmt_bch_buf_t*)fmtout->ctx;
	hcl_oow_t n;

	/* this function null-terminates the destination. so give the restored buffer size */
	n = hcl_copy_bchars_to_bcstr(&b->ptr[b->len], b->capa - b->len + 1, ptr, len);
	b->len += n;
	if (n < len)
	{
		hcl_seterrnum (b->hcl, HCL_EBUFFULL);
		return 0; /* stop. insufficient buffer */
	}

	return 1; /* success */
}

static int fmt_put_uchars_to_bch_buf (hcl_t* hcl, hcl_fmtout_t* fmtout, const hcl_uch_t* ptr, hcl_oow_t len)
{
	fmt_bch_buf_t* b = (fmt_bch_buf_t*)fmtout->ctx;
	hcl_oow_t bcslen, ucslen;
	int n;

	bcslen = b->capa - b->len;
	ucslen = len;
	n = hcl_conv_uchars_to_bchars_with_cmgr(ptr, &ucslen, &b->ptr[b->len], &bcslen, b->hcl->_cmgr);
	b->len += bcslen;
	if (n <= -1)
	{
		if (n == -2)
		{
			return 0; /* buffer full. stop */
		}
		else
		{
			hcl_seterrnum (b->hcl, HCL_EECERR);
			return -1;
		}
	}

	return 1; /* success. carry on */
}

hcl_oow_t hcl_vfmttobcstr (hcl_t* hcl, hcl_bch_t* buf, hcl_oow_t bufsz, const hcl_bch_t* fmt, va_list ap)
{
	hcl_fmtout_t fo;
	fmt_bch_buf_t fb;

	if (bufsz <= 0) return 0;

	HCL_MEMSET (&fo, 0, HCL_SIZEOF(fo));
	fo.mmgr = hcl->_mmgr;
	fo.putbchars = fmt_put_bchars_to_bch_buf;
	fo.putuchars = fmt_put_uchars_to_bch_buf;
	fo.putobj = hcl_fmt_object;
	fo.ctx = &fb;

	HCL_MEMSET (&fb, 0, HCL_SIZEOF(fb));
	fb.hcl = hcl;
	fb.ptr = buf;
	fb.capa = bufsz - 1;

	if (hcl_bfmt_outv(hcl, &fo, fmt, ap) <= -1) return -1;

	buf[fb.len] = '\0';
	return fb.len;
}

hcl_oow_t hcl_fmttobcstr (hcl_t* hcl, hcl_bch_t* buf, hcl_oow_t bufsz, const hcl_bch_t* fmt, ...)
{
	hcl_oow_t x;
	va_list ap;

	va_start (ap, fmt);
	x = hcl_vfmttobcstr(hcl, buf, bufsz, fmt, ap);
	va_end (ap);

	return x;
}
