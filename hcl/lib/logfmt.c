/*
 * $Id$
 *
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

#include "hcl-prv.h"

/*#include <stdio.h>*/ /* for snrintf(). used for floating-point number formatting */

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

static hcl_uch_t uch_nullstr[] = { '(','n','u','l','l', ')','\0' };
static hcl_bch_t bch_nullstr[] = { '(','n','u','l','l', ')','\0' };

typedef int (*hcl_fmtout_putch_t) (
	hcl_t*      hcl,
	int         mask,
	hcl_ooch_t  c,
	hcl_oow_t   len
);

typedef int (*hcl_fmtout_putcs_t) (
	hcl_t*            hcl,
	int               mask,
	const hcl_ooch_t* ptr,
	hcl_oow_t         len
);

typedef struct hcl_fmtout_t hcl_fmtout_t;
struct hcl_fmtout_t
{
	hcl_oow_t            count; /* out */
	int                  mask;  /* in */
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
static int put_ooch (hcl_t* hcl, int mask, hcl_ooch_t ch, hcl_oow_t len)
{
	/* this is not equivalent to put_oocs(hcl,mask,&ch,1);
	 * this function is to emit a single character multiple times */
	hcl_oow_t rem;

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
		vmprim_log_write (hcl, hcl->log.last_mask, hcl->log.ptr, hcl->log.len);
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

		newcapa = HCL_ALIGN_POW2(hcl->log.len + len, HCL_LOG_CAPA_ALIGN); /* TODO: adjust this capacity */
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
		tmp = (hcl_ooch_t*)hcl_reallocmem (hcl, hcl->log.ptr, (newcapa + 1) * HCL_SIZEOF(*tmp)); 
		if (!tmp) 
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
				vmprim_log_write (hcl, hcl->log.last_mask, hcl->log.ptr, hcl->log.len);
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

	while (len > 0)
	{
		hcl->log.ptr[hcl->log.len++] = ch;
		len--;
	}
	hcl->log.last_mask = mask;

	if (rem > 0)
	{
		len = rem;
		goto redo;
	}

	return 1; /* success */
}

static int put_oocs (hcl_t* hcl, int mask, const hcl_ooch_t* ptr, hcl_oow_t len)
{
	hcl_oow_t rem;

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

		vmprim_log_write (hcl, hcl->log.last_mask, hcl->log.ptr, hcl->log.len);
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
		tmp = (hcl_ooch_t*)hcl_reallocmem (hcl, hcl->log.ptr, (newcapa + 1) * HCL_SIZEOF(*tmp));
		if (!tmp) 
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
				vmprim_log_write (hcl, hcl->log.last_mask, hcl->log.ptr, hcl->log.len);
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
	hcl->log.last_mask = mask;

	if (rem > 0)
	{
		ptr += len;
		len = rem;
		goto redo;
	}

	return 1; /* success */
}

/* ------------------------------------------------------------------------- */

#undef FMTCHAR_IS_BCH
#undef FMTCHAR_IS_UCH
#undef FMTCHAR_IS_OOCH
#undef fmtchar_t
#undef logfmtv
#define fmtchar_t hcl_bch_t
#define logfmtv __logbfmtv
#define FMTCHAR_IS_BCH
#if defined(HCL_OOCH_IS_BCH)
#	define FMTCHAR_IS_OOCH
#endif
#include "logfmtv.h"

#undef FMTCHAR_IS_BCH
#undef FMTCHAR_IS_UCH
#undef FMTCHAR_IS_OOCH
#undef fmtchar_t
#undef logfmtv
#define fmtchar_t hcl_uch_t
#define logfmtv __logufmtv
#define FMTCHAR_IS_UCH
#if defined(HCL_OOCH_IS_UCH)
#	define FMTCHAR_IS_OOCH
#endif
#include "logfmtv.h" 


static int _logbfmtv (hcl_t* hcl, const hcl_bch_t* fmt, hcl_fmtout_t* data, va_list ap)
{
	return __logbfmtv (hcl, fmt, data, ap, hcl_logbfmt);
}

static int _logufmtv (hcl_t* hcl, const hcl_uch_t* fmt, hcl_fmtout_t* data, va_list ap)
{
	return __logufmtv (hcl, fmt, data, ap, hcl_logbfmt);
}

hcl_ooi_t hcl_logbfmt (hcl_t* hcl, int mask, const hcl_bch_t* fmt, ...)
{
	int x;
	va_list ap;
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

	fo.mask = mask;
	fo.putch = put_ooch;
	fo.putcs = put_oocs;

	va_start (ap, fmt);
	x = _logbfmtv (hcl, fmt, &fo, ap);
	va_end (ap);

	if (hcl->log.len > 0 && hcl->log.ptr[hcl->log.len - 1] == '\n')
	{
		vmprim_log_write (hcl, hcl->log.last_mask, hcl->log.ptr, hcl->log.len);
		hcl->log.len = 0;
	}
	return (x <= -1)? -1: fo.count;
}

hcl_ooi_t hcl_logufmt (hcl_t* hcl, int mask, const hcl_uch_t* fmt, ...)
{
	int x;
	va_list ap;
	hcl_fmtout_t fo;

	if (hcl->log.default_type_mask & HCL_LOG_ALL_TYPES) 
	{
		mask &= ~HCL_LOG_UNTYPED;
		mask |= (hcl->log.default_type_mask & HCL_LOG_ALL_TYPES);
	}

	fo.mask = mask;
	fo.putch = put_ooch;
	fo.putcs = put_oocs;

	va_start (ap, fmt);
	x = _logufmtv (hcl, fmt, &fo, ap);
	va_end (ap);

	if (hcl->log.len > 0 && hcl->log.ptr[hcl->log.len - 1] == '\n')
	{
		vmprim_log_write (hcl, hcl->log.last_mask, hcl->log.ptr, hcl->log.len);
		hcl->log.len = 0;
	}

	return (x <= -1)? -1: fo.count;
}

/* -------------------------------------------------------------------------- 
 * HELPER FOR PRINTING
 * -------------------------------------------------------------------------- */

static int put_prch (hcl_t* hcl, int mask, hcl_ooch_t ch, hcl_oow_t len)
{
/* TODO: better error handling, buffering.
 *       should buffering be done by the printer callback? */
	hcl_ooi_t n;
	hcl_ooch_t str[256];
	hcl_oow_t seglen, i;

	while (len > 0)
	{
		seglen = (len > HCL_COUNTOF(str))? len = HCL_COUNTOF(str): len;
		for (i = 0; i < seglen; i++) str[i] = ch;

		hcl->c->outarg.ptr = str;
		hcl->c->outarg.len = seglen;

		n = hcl->c->printer(hcl, HCL_IO_WRITE, &hcl->c->outarg);

		if (n <= -1) return -1;
		if (n == 0) return 0;

		len -= seglen;
	}

	return 1; /* success */
}

static int put_prcs (hcl_t* hcl, int mask, const hcl_ooch_t* ptr, hcl_oow_t len)
{
/* TODO: better error handling, buffering 
 *       should buffering be done by the printer callback? */
	hcl_ooi_t n;

	hcl->c->outarg.ptr = (hcl_ooch_t*)ptr;
	hcl->c->outarg.len = len;

	n = hcl->c->printer(hcl, HCL_IO_WRITE, &hcl->c->outarg);

	if (n <= -1) return -1;
	if (n == 0) return 0;
	return 1; /* success */
}

static hcl_ooi_t __prbfmtv (hcl_t* hcl, int mask, const hcl_bch_t* fmt, ...);

static int _prbfmtv (hcl_t* hcl, const hcl_bch_t* fmt, hcl_fmtout_t* data, va_list ap)
{
	return __logbfmtv (hcl, fmt, data, ap, __prbfmtv);
}

static int _prufmtv (hcl_t* hcl, const hcl_uch_t* fmt, hcl_fmtout_t* data, va_list ap)
{
	return __logufmtv (hcl, fmt, data, ap, __prbfmtv);
}

static hcl_ooi_t __prbfmtv (hcl_t* hcl, int mask, const hcl_bch_t* fmt, ...)
{
	va_list ap;
	hcl_fmtout_t fo;

	fo.mask = 0; /* not used */
	fo.putch = put_prch;
	fo.putcs = put_prcs;

	va_start (ap, fmt);
	_prbfmtv (hcl, fmt, &fo, ap);
	va_end (ap);

	return fo.count;
}

hcl_ooi_t hcl_proutbfmt (hcl_t* hcl, int mask, const hcl_bch_t* fmt, ...)
{
	int x;
	va_list ap;
	hcl_fmtout_t fo;

	fo.mask = 0; /* not used */
	fo.putch = put_prch;
	fo.putcs = put_prcs;

	va_start (ap, fmt);
	x = _prbfmtv (hcl, fmt, &fo, ap);
	va_end (ap);

	return (x <= -1)? -1: fo.count;
}

hcl_ooi_t hcl_proutufmt (hcl_t* hcl, int mask, const hcl_uch_t* fmt, ...)
{
	int x;
	va_list ap;
	hcl_fmtout_t fo;

	fo.mask = 0; /* not used */
	fo.putch = put_prch;
	fo.putcs = put_prcs;

	va_start (ap, fmt);
	x = _prufmtv (hcl, fmt, &fo, ap);
	va_end (ap);

	return (x <= -1)? -1: fo.count;
}
 
/* -------------------------------------------------------------------------- 
 * ERROR MESSAGE FORMATTING
 * -------------------------------------------------------------------------- */

static int put_errch (hcl_t* hcl, int mask, hcl_ooch_t ch, hcl_oow_t len)
{
	hcl_oow_t max;

	max = HCL_COUNTOF(hcl->errmsg.buf) - hcl->errmsg.len - 1;
	if (len > max) len = max;

	if (len <= 0) return 1;

	while (len > 0)
	{
		hcl->errmsg.buf[hcl->errmsg.len++] = ch;
		len--;
	}
	hcl->errmsg.buf[hcl->errmsg.len] = '\0';

	return 1; /* success */
}

static int put_errcs (hcl_t* hcl, int mask, const hcl_ooch_t* ptr, hcl_oow_t len)
{
	hcl_oow_t max;

	max = HCL_COUNTOF(hcl->errmsg.buf) - hcl->errmsg.len - 1;
	if (len > max) len = max;

	if (len <= 0) return 1;

	HCL_MEMCPY (&hcl->errmsg.buf[hcl->errmsg.len], ptr, len * HCL_SIZEOF(*ptr));
	hcl->errmsg.len += len;
	hcl->errmsg.buf[hcl->errmsg.len] = '\0';

	return 1; /* success */
}


static hcl_ooi_t __errbfmtv (hcl_t* hcl, int mask, const hcl_bch_t* fmt, ...);

static int _errbfmtv (hcl_t* hcl, const hcl_bch_t* fmt, hcl_fmtout_t* data, va_list ap)
{
	return __logbfmtv (hcl, fmt, data, ap, __errbfmtv);
}

static int _errufmtv (hcl_t* hcl, const hcl_uch_t* fmt, hcl_fmtout_t* data, va_list ap)
{
	return __logufmtv (hcl, fmt, data, ap, __errbfmtv);
}

static hcl_ooi_t __errbfmtv (hcl_t* hcl, int mask, const hcl_bch_t* fmt, ...)
{
	va_list ap;
	hcl_fmtout_t fo;

	fo.mask = 0; /* not used */
	fo.putch = put_errch;
	fo.putcs = put_errcs;

	va_start (ap, fmt);
	_errbfmtv (hcl, fmt, &fo, ap);
	va_end (ap);

	return fo.count;
}

void hcl_seterrbfmt (hcl_t* hcl, hcl_errnum_t errnum, const hcl_bch_t* fmt, ...)
{
	va_list ap;
	hcl_fmtout_t fo;

	if (hcl->shuterr) return;

	hcl->errnum = errnum;
	hcl->errmsg.len = 0;

	fo.mask = 0; /* not used */
	fo.putch = put_errch;
	fo.putcs = put_errcs;

	va_start (ap, fmt);
	_errbfmtv (hcl, fmt, &fo, ap);
	va_end (ap);
}

void hcl_seterrufmt (hcl_t* hcl, hcl_errnum_t errnum, const hcl_uch_t* fmt, ...)
{
	va_list ap;
	hcl_fmtout_t fo;

	if (hcl->shuterr) return;

	hcl->errnum = errnum;
	hcl->errmsg.len = 0;

	fo.mask = 0; /* not used */
	fo.putch = put_errch;
	fo.putcs = put_errcs;

	va_start (ap, fmt);
	_errufmtv (hcl, fmt, &fo, ap);
	va_end (ap);
}


void hcl_seterrbfmtv (hcl_t* hcl, hcl_errnum_t errnum, const hcl_bch_t* fmt, va_list ap)
{
	hcl_fmtout_t fo;

	if (hcl->shuterr) return;

	hcl->errnum = errnum;
	hcl->errmsg.len = 0;

	fo.mask = 0; /* not used */
	fo.putch = put_errch;
	fo.putcs = put_errcs;

	_errbfmtv (hcl, fmt, &fo, ap);
}

void hcl_seterrufmtv (hcl_t* hcl, hcl_errnum_t errnum, const hcl_uch_t* fmt, va_list ap)
{
	hcl_fmtout_t fo;

	if (hcl->shuterr) return;

	hcl->errnum = errnum;
	hcl->errmsg.len = 0;

	fo.mask = 0; /* not used */
	fo.putch = put_errch;
	fo.putcs = put_errcs;

	_errufmtv (hcl, fmt, &fo, ap);
}


/* -------------------------------------------------------------------------- 
 * SUPPORT FOR THE BUILTIN PRINTF PRIMITIVE FUNCTION
 * -------------------------------------------------------------------------- */

#if 1
static int put_formatted_chars (hcl_t* hcl, int mask, const hcl_ooch_t ch, hcl_oow_t len)
{
/* TODO: better error handling, buffering.
 *       should buffering be done by the printer callback? */
	hcl_ooi_t n;
	hcl_ooch_t str[256];
	hcl_oow_t seglen, i;

	while (len > 0)
	{
		seglen = (len > HCL_COUNTOF(str))? len = HCL_COUNTOF(str): len;
		for (i = 0; i < seglen; i++) str[i] = ch;

		hcl->c->outarg.ptr = str;
		hcl->c->outarg.len = seglen;

		n = hcl->c->printer(hcl, HCL_IO_WRITE, &hcl->c->outarg);

		if (n <= -1) return -1;
		if (n == 0) return 0; /* eof. stop printign */

		len -= seglen;
	}

	return 1; /* success */
}

static int put_formatted_string (hcl_t* hcl, int mask, const hcl_ooch_t* ptr, hcl_oow_t len)
{
/* TODO: better error handling, buffering 
 *       should be done by the printer callback? */
	hcl_ooi_t n;

	hcl->c->outarg.ptr = (hcl_ooch_t*)ptr;
	hcl->c->outarg.len = len;

	n = hcl->c->printer(hcl, HCL_IO_WRITE, &hcl->c->outarg);

	if (n <= -1) return -1;
	if (n == 0) return 0; /* eof. stop printing */
	return 1; /* success */
}


#define PRINT_OOCH(c,n) do { \
	if (n > 0) { \
		int xx; \
		if ((xx = put_formatted_chars(hcl, data->mask, c, n)) <= -1) goto oops; \
		if (xx == 0) goto done; \
		data->count += n; \
	} \
} while (0)

#define PRINT_OOCS(ptr,len) do { \
	if (len > 0) { \
		int xx; \
		if ((xx = put_formatted_string(hcl, data->mask, ptr, len)) <= -1) goto oops; \
		if (xx == 0) goto done; \
		data->count += len; \
	} \
} while (0)

static HCL_INLINE int print_formatted (hcl_t* hcl, hcl_ooi_t nargs, hcl_fmtout_t* data)
{
	hcl_oop_char_t fmtoop;

	//hcl_bch_t nbuf[MAXNBUF], bch;
	int n, base, neg, sign;
	hcl_ooi_t tmp, width, precision;
	hcl_ooch_t ch, padc, ooch;
	int lm_flag, lm_dflag, flagc;
	hcl_uintmax_t num = 0;
	int stop = 0;

	hcl_ooch_t* fmt, * fmtend;
	const hcl_ooch_t* checkpoint, * percent;

	hcl_oop_t arg;
	hcl_ooi_t argidx = 0;

	fmtoop = (hcl_oop_char_t)HCL_STACK_GETARG(hcl, nargs, argidx); argidx++;
	HCL_ASSERT (hcl, HCL_IS_STRING(hcl, fmtoop));

	fmt = HCL_OBJ_GET_CHAR_SLOT(fmtoop);
	fmtend = fmt + HCL_OBJ_GET_SIZE(fmtoop);

	data->count = 0;

/* TODO: traverse until fmtend... */
	while (1)
	{
		checkpoint = fmt;
		while ((ch = *fmt++) != '%' || stop) 
		{
			if (ch == '\0') 
			{
				PRINT_OOCS (checkpoint, fmt - checkpoint - 1);
				goto done;
			}
		}
		PRINT_OOCS (checkpoint, fmt - checkpoint - 1);

		percent = fmt - 1;

		padc = ' '; 
		width = 0; precision = 0;
		neg = 0; sign = 0;

		lm_flag = 0; lm_dflag = 0; flagc = 0; 
		//sprintn = sprintn_lower;

reswitch:
		switch (ch = *fmt++) 
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

				arg = HCL_STACK_GETARG(hcl, nargs, argidx); argidx++;
				if (hcl_inttoooi(hcl, arg, &precision) <= -1) goto oops;
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

				arg = HCL_STACK_GETARG(hcl, nargs, argidx); argidx++;
				if (hcl_inttoooi(hcl, arg, &width) <= -1) goto oops;
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
			if (flagc & FLAGC_LENMOD) goto invalid_format;
			for (n = 0;; ++fmt) 
			{
				n = n * 10 + ch - '0';
				ch = *fmt;
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
 
		/* integer conversions */
		case 'd':
		case 'i': /* signed conversion */
			base = 10;
			sign = 1;
			goto number;
		case 'o': 
			base = 8;
			goto number;
		case 'u':
			base = 10;
			goto number;
		case 'X':
			//sprintn = sprintn_upper; /* TODO: handle this */
		case 'x':
			base = 16;
			goto number;
		case 'b':
			base = 2;
			goto number;
		/* end of integer conversions */

		case 'c':
		case 'C':
		print_char:

			/* zeropad must not take effect for 'c' */
			if (flagc & FLAGC_ZEROPAD) padc = ' '; 

			//bch = HCL_SIZEOF(hcl_bch_t) < HCL_SIZEOF(int)? va_arg(ap, int): va_arg(ap, hcl_bch_t);
			arg = HCL_STACK_GETARG(hcl, nargs, argidx); argidx++;
			if (!HCL_OOP_IS_CHAR(arg))
			{
				/* TODO: error code ... */
				goto oops;
			}
			ooch = HCL_OOP_TO_CHAR(arg);

			/* precision 0 doesn't kill the letter */
			width--;
			if (!(flagc & FLAGC_LEFTADJ) && width > 0) PRINT_OOCH (padc, width);
			PRINT_OOCH (ooch, 1);
			if ((flagc & FLAGC_LEFTADJ) && width > 0) PRINT_OOCH (padc, width);
			break;

		case 's':
		case 'S':
		{
			const hcl_ooch_t* oosp;
			hcl_oow_t oosl;

			/* zeropad must not take effect for 'S' */
			if (flagc & FLAGC_ZEROPAD) padc = ' ';

			arg = HCL_STACK_GETARG(hcl, nargs, argidx); argidx++;
			if (!HCL_OOP_IS_POINTER(arg) || HCL_OBJ_GET_FLAGS_TYPE(arg) != HCL_OBJ_TYPE_CHAR)
			{
				goto oops;
			}

			oosp = HCL_OBJ_GET_CHAR_SLOT(arg);
			oosl = HCL_OBJ_GET_SIZE(arg);

			if (flagc & FLAGC_DOT)
			{
				if (oosl > precision) oosl = precision;
			}
			width -= oosl;

			if (!(flagc & FLAGC_LEFTADJ) && width > 0) PRINT_OOCH (padc, width);
			PRINT_OOCS (oosp, oosl);
			if ((flagc & FLAGC_LEFTADJ) && width > 0) PRINT_OOCH (padc, width);
			break;
		}


		case 'O': /* object - ignore precision, width, adjustment */
			arg = HCL_STACK_GETARG(hcl, nargs, argidx); argidx++;
			if (hcl_outfmtobj(hcl, 0, arg, hcl_proutbfmt) <= -1) goto oops;
			break;

		number:
		{
			const hcl_ooch_t* nsptr;
			hcl_oow_t nslen;
			arg = HCL_STACK_GETARG(hcl, nargs, argidx); argidx++;

			if (HCL_OOP_IS_CHAR(arg)) arg = HCL_SMOOI_TO_OOP(HCL_OOP_TO_CHAR(arg));
			
			if (!hcl_inttostr(hcl, arg, base, -1)) 
			{
				hcl_seterrbfmt (hcl, HCL_EINVAL, "not a valid number - %O", arg);
				goto oops;
			}

			nsptr = hcl->inttostr.xbuf.ptr;
			nslen = hcl->inttostr.xbuf.len;

			tmp = nslen;
/*
			if (sign && (hcl_intmax_t)num < 0) 
			{
				neg = 1;
				num = -(hcl_intmax_t)num;
			}

			nbufp = sprintn (nbuf, num, base, &tmp);
*/

			if ((flagc & FLAGC_SHARP) && num != 0) 
			{
				if (base == 8) tmp++;
				else if (base == 16) tmp += 2;
			}
			if (neg) tmp++;
			else if (flagc & FLAGC_SIGN) tmp++;
			else if (flagc & FLAGC_SPACE) tmp++;

/*
			numlen = (int)((const hcl_bch_t*)nbufp - (const hcl_bch_t*)nbuf);
*/
			if ((flagc & FLAGC_DOT) && precision > nslen) 
			{
				/* extra zeros for precision specified */
				tmp += (precision - nslen);
			}

			if (!(flagc & FLAGC_LEFTADJ) && !(flagc & FLAGC_ZEROPAD) && width > 0 && (width -= tmp) > 0)
			{
				PRINT_OOCH (padc, width);
				width = 0;
			}

			if (neg) PRINT_OOCH ('-', 1);
			else if (flagc & FLAGC_SIGN) PRINT_OOCH ('+', 1);
			else if (flagc & FLAGC_SPACE) PRINT_OOCH (' ', 1);

			if ((flagc & FLAGC_SHARP) && num != 0) 
			{
				if (base == 2) 
				{
					PRINT_OOCH ('0', 1);
					PRINT_OOCH ('b', 1);
				}
				if (base == 8) 
				{
					PRINT_OOCH ('0', 1);
				} 
				else if (base == 16) 
				{
					PRINT_OOCH ('0', 1);
					PRINT_OOCH ('x', 1);
				}
			}

			if ((flagc & FLAGC_DOT) && precision > nslen)
			{
				/* extra zeros for precision specified */
				PRINT_OOCH ('0', precision - nslen);
			}

			if (!(flagc & FLAGC_LEFTADJ) && width > 0 && (width -= tmp) > 0)
			{
				PRINT_OOCH (padc, width);
			}

			//while (*nbufp) PRINT_OOCH (*nbufp--, 1); /* output actual digits */
			PRINT_OOCS (nsptr, nslen);

			if ((flagc & FLAGC_LEFTADJ) && width > 0 && (width -= tmp) > 0)
			{
				PRINT_OOCH (padc, width);
			}
			break;
		}

invalid_format:
		#if defined(FMTCHAR_IS_OOCH)
			PRINT_OOCS (percent, fmt - percent);
		#else
			while (percent < fmt) PRINT_OOCH (*percent++, 1);
		#endif
			break;

		default:
		#if defined(FMTCHAR_IS_OOCH)
			PRINT_OOCS (percent, fmt - percent);
		#else
			while (percent < fmt) PRINT_OOCH (*percent++, 1);
		#endif
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
	return 0;

oops:
	return -1;
}

int hcl_print_formatted (hcl_t* hcl, hcl_ooi_t nargs)
{
	hcl_fmtout_t fo;
	HCL_MEMSET (&fo, 0, HCL_SIZEOF(fo));
	return print_formatted (hcl, nargs, &fo);
}
#endif
