/*
 * $Id$
 *
    Copyright (c) 2014-2016 Chung, Hyung-Hwan. All rights reserved.

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

/*#include <stdio.h>*/ /* for snrintf(). used for floating-point number formatting */
#include <stdarg.h>

#if defined(_MSC_VER) || defined(__BORLANDC__) || (defined(__WATCOMC__) && (__WATCOMC__ < 1200))
#	define snprintf _snprintf 
#	if !defined(HAVE_SNPRINTF)
#		define HAVE_SNPRINTF
#	endif
#endif
#if defined(HAVE_QUADMATH_H)
#	include <quadmath.h> /* for quadmath_snprintf() */
#endif
/* TODO: remove stdio.h and quadmath.h once snprintf gets replaced by own 
floting-point conversion implementation*/

/* Max number conversion buffer length: 
 * hcl_intmax_t in base 2, plus NUL byte. */
#define MAXNBUF (HCL_SIZEOF(hcl_intmax_t) * 8 + 1)

enum
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

static hcl_ooch_t ooch_nullstr[] = { '(','n','u','l','l', ')','\0' };
static hcl_bch_t bch_nullstr[] = { '(','n','u','l','l', ')','\0' };

typedef int (*hcl_fmtout_putch_t) (
	hcl_t*      hcl,
	hcl_oow_t mask,
	hcl_ooch_t  c,
	hcl_oow_t   len
);

typedef int (*hcl_fmtout_putcs_t) (
	hcl_t*            hcl,
	hcl_oow_t         mask,
	const hcl_ooch_t* ptr,
	hcl_oow_t         len
);

typedef struct hcl_fmtout_t hcl_fmtout_t;
struct hcl_fmtout_t
{
	hcl_oow_t            count; /* out */
	hcl_oow_t            mask;  /* in */
	hcl_fmtout_putch_t   putch; /* in */
	hcl_fmtout_putcs_t   putcs; /* in */
};

/* ------------------------------------------------------------------------- */
/*
 * Put a NUL-terminated ASCII number (base <= 36) in a buffer in reverse
 * order; return an optional length and a pointer to the last character
 * written in the buffer (i.e., the first character of the string).
 * The buffer pointed to by `nbuf' must have length >= MAXNBUF.
 */

static hcl_bch_t* sprintn_lower (hcl_bch_t* nbuf, hcl_uintmax_t num, int base, hcl_ooi_t *lenp)
{
	hcl_bch_t* p;

	p = nbuf;
	*p = '\0';
	do { *++p = hex2ascii_lower[num % base]; } while (num /= base);

	if (lenp) *lenp = p - nbuf;
	return p; /* returns the end */
}

static hcl_bch_t* sprintn_upper (hcl_bch_t* nbuf, hcl_uintmax_t num, int base, hcl_ooi_t *lenp)
{
	hcl_bch_t* p;

	p = nbuf;
	*p = '\0';
	do { *++p = hex2ascii_upper[num % base]; } while (num /= base);

	if (lenp) *lenp = p - nbuf;
	return p; /* returns the end */
}

/* ------------------------------------------------------------------------- */
static int put_ooch (hcl_t* hcl, hcl_oow_t mask, hcl_ooch_t ch, hcl_oow_t len)
{
	if (len <= 0) return 1;

	if (hcl->log.len > 0 && hcl->log.last_mask != mask)
	{
		/* the mask has changed. commit the buffered text */
/* TODO: HANDLE LINE ENDING CONVENTION BETTER... */
		if (hcl->log.ptr[hcl->log.len - 1] != '\n')
		{
			/* no line ending - append a line terminator */
			hcl->log.ptr[hcl->log.len++] = '\n';
		}

		hcl->vmprim.log_write (hcl, hcl->log.last_mask, hcl->log.ptr, hcl->log.len);
		hcl->log.len = 0;
	}

redo:
	if (len > hcl->log.capa - hcl->log.len)
	{
		hcl_oow_t newcapa;
		hcl_ooch_t* tmp;

		if (len > HCL_TYPE_MAX(hcl_oow_t) - hcl->log.len) 
		{
			/* data too big */
			hcl->errnum = HCL_ETOOBIG;
			return -1;
		}

		newcapa = HCL_ALIGN(hcl->log.len + len, 512); /* TODO: adjust this capacity */
		tmp = hcl_reallocmem (hcl, hcl->log.ptr, newcapa * HCL_SIZEOF(*tmp));
		if (!tmp) 
		{
			if (hcl->log.len > 0)
			{
				/* can't expand the buffer. just flush the existing contents */
				hcl->vmprim.log_write (hcl, hcl->log.last_mask, hcl->log.ptr, hcl->log.len);
				hcl->log.len = 0;
				goto redo;
			}
			return -1;
		}

		hcl->log.ptr = tmp;
		hcl->log.capa = newcapa - 1; /* -1 to handle line ending injection more easily */
	}

	while (len > 0)
	{
		hcl->log.ptr[hcl->log.len++] = ch;
		len--;
	}

	hcl->log.last_mask = mask;
	return 1; /* success */
}

static int put_oocs (hcl_t* hcl, hcl_oow_t mask, const hcl_ooch_t* ptr, hcl_oow_t len)
{
	if (len <= 0) return 1;

	if (hcl->log.len > 0 && hcl->log.last_mask != mask)
	{
		/* the mask has changed. commit the buffered text */
/* TODO: HANDLE LINE ENDING CONVENTION BETTER... */
		if (hcl->log.ptr[hcl->log.len - 1] != '\n')
		{
			/* no line ending - append a line terminator */
			hcl->log.ptr[hcl->log.len++] = '\n';
		}

		hcl->vmprim.log_write (hcl, hcl->log.last_mask, hcl->log.ptr, hcl->log.len);
		hcl->log.len = 0;
	}

	if (len > hcl->log.capa - hcl->log.len)
	{
		hcl_oow_t newcapa;
		hcl_ooch_t* tmp;

		if (len > HCL_TYPE_MAX(hcl_oow_t) - hcl->log.len) 
		{
			/* data too big */
			hcl->errnum = HCL_ETOOBIG;
			return -1;
		}

		newcapa = HCL_ALIGN(hcl->log.len + len, 512); /* TODO: adjust this capacity */
		tmp = hcl_reallocmem (hcl, hcl->log.ptr, newcapa * HCL_SIZEOF(*tmp));
		if (!tmp) return -1;

		hcl->log.ptr = tmp;
		hcl->log.capa = newcapa - 1; /* -1 to handle line ending injection more easily */
	}

	HCL_MEMCPY (&hcl->log.ptr[hcl->log.len], ptr, len * HCL_SIZEOF(*ptr));
	hcl->log.len += len;

	hcl->log.last_mask = mask;

	return 1; /* success */
}

/* ------------------------------------------------------------------------- */

static hcl_ooi_t log_object (hcl_t* hcl, hcl_iocmd_t cmd, void* arg)
{
	hcl_iooutarg_t* outarg = (hcl_iooutarg_t*)arg;
	put_oocs (hcl, (hcl_oow_t)outarg->handle, outarg->ptr, outarg->len);
	return outarg->len; /* don't really care about failure as it's for logging */
}

static int print_object (hcl_t* hcl, hcl_oow_t mask, hcl_oop_t obj)
{
	hcl_iooutarg_t outarg;
	outarg.handle = (void*)mask;
	return hcl_printobj (hcl, obj, log_object, &outarg);
}

/* ------------------------------------------------------------------------- */

#undef fmtchar_t
#undef logfmtv
#define fmtchar_t hcl_bch_t
#define FMTCHAR_IS_BCH
#define logfmtv hcl_logbfmtv
#include "logfmtv.h"

#undef fmtchar_t
#undef logfmtv
#define fmtchar_t hcl_ooch_t
#define logfmtv hcl_logoofmtv
#define FMTCHAR_IS_OOCH
#include "logfmtv.h"

hcl_ooi_t hcl_logbfmt (hcl_t* hcl, hcl_oow_t mask, const hcl_bch_t* fmt, ...)
{
	int x;
	va_list ap;
	hcl_fmtout_t fo;

	fo.mask = mask;
	fo.putch = put_ooch;
	fo.putcs = put_oocs;

	va_start (ap, fmt);
	x = hcl_logbfmtv (hcl, fmt, &fo, ap);
	va_end (ap);

	if (hcl->log.len > 0 && hcl->log.ptr[hcl->log.len - 1] == '\n')
	{
		hcl->vmprim.log_write (hcl, hcl->log.last_mask, hcl->log.ptr, hcl->log.len);
		hcl->log.len = 0;
	}
	return (x <= -1)? -1: fo.count;
}

hcl_ooi_t hcl_logoofmt (hcl_t* hcl, hcl_oow_t mask, const hcl_ooch_t* fmt, ...)
{
	int x;
	va_list ap;
	hcl_fmtout_t fo;

	fo.mask = mask;
	fo.putch = put_ooch;
	fo.putcs = put_oocs;

	va_start (ap, fmt);
	x = hcl_logoofmtv (hcl, fmt, &fo, ap);
	va_end (ap);

	if (hcl->log.len > 0 && hcl->log.ptr[hcl->log.len - 1] == '\n')
	{
		hcl->vmprim.log_write (hcl, hcl->log.last_mask, hcl->log.ptr, hcl->log.len);
		hcl->log.len = 0;
	}

	return (x <= -1)? -1: fo.count;
}
