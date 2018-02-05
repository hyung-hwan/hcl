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
	hcl_oow_t   mask,
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
static int put_ooch (hcl_t* hcl, hcl_oow_t mask, hcl_ooch_t ch, hcl_oow_t len)
{
	/* this is not equivalent to put_oocs(hcl,mask,&ch, 1);
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
		hcl->vmprim.log_write (hcl, hcl->log.last_mask, hcl->log.ptr, hcl->log.len);
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
		tmp = hcl_reallocmem (hcl, hcl->log.ptr, (newcapa + 1) * HCL_SIZEOF(*tmp)); 
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
				hcl->vmprim.log_write (hcl, hcl->log.last_mask, hcl->log.ptr, hcl->log.len);
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

static int put_oocs (hcl_t* hcl, hcl_oow_t mask, const hcl_ooch_t* ptr, hcl_oow_t len)
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

		hcl->vmprim.log_write (hcl, hcl->log.last_mask, hcl->log.ptr, hcl->log.len);
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
		tmp = hcl_reallocmem (hcl, hcl->log.ptr, (newcapa + 1) * HCL_SIZEOF(*tmp));
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
				hcl->vmprim.log_write (hcl, hcl->log.last_mask, hcl->log.ptr, hcl->log.len);
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

typedef hcl_ooi_t (*outbfmt_t) (hcl_t* hcl, hcl_oow_t mask, const hcl_bch_t* fmt, ...);


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

#if 0
static void print_object (hcl_t* hcl, hcl_oow_t mask, hcl_oop_t oop, outbfmt_t outbfmt)
{


	if (oop == hcl->_nil)
	{
		outbfmt (hcl, mask, "nil");
	}
	else if (oop == hcl->_true)
	{
		outbfmt (hcl, mask, "true");
	}
	else if (oop == hcl->_false)
	{
		outbfmt (hcl, mask, "false");
	}
	else if (HCL_OOP_IS_SMOOI(oop))
	{
		outbfmt (hcl, mask, "%zd", HCL_OOP_TO_SMOOI(oop));
	}
	else if (HCL_OOP_IS_SMPTR(oop))
	{
		outbfmt (hcl, mask, "%p", HCL_OOP_TO_SMPTR(oop));
	}
	else if (HCL_OOP_IS_CHAR(oop))
	{
		outbfmt (hcl, mask, "$%.1C", HCL_OOP_TO_CHAR(oop));
	}
	else if (HCL_OOP_IS_ERROR(oop))
	{
		outbfmt (hcl, mask, "error(%zd)", HCL_OOP_TO_ERROR(oop));
	}
	else
	{
		hcl_oop_class_t c;
		hcl_oow_t i;

		HCL_ASSERT (hcl, HCL_OOP_IS_POINTER(oop));
		c = (hcl_oop_class_t)HCL_OBJ_GET_CLASS(oop); /*HCL_CLASSOF(hcl, oop);*/

		if (c == hcl->_large_negative_integer)
		{
			hcl_oow_t i;
			outbfmt (hcl, mask, "-16r");
			for (i = HCL_OBJ_GET_SIZE(oop); i > 0;)
			{
				outbfmt (hcl, mask, "%0*lX", (int)(HCL_SIZEOF(hcl_liw_t) * 2), (unsigned long)((hcl_oop_liword_t)oop)->slot[--i]);
			}
		}
		else if (c == hcl->_large_positive_integer)
		{
			hcl_oow_t i;
			outbfmt (hcl, mask, "16r");
			for (i = HCL_OBJ_GET_SIZE(oop); i > 0;)
			{
				outbfmt (hcl, mask, "%0*lX", (int)(HCL_SIZEOF(hcl_liw_t) * 2), (unsigned long)((hcl_oop_liword_t)oop)->slot[--i]);
			}
		}
		else if (HCL_OBJ_GET_FLAGS_TYPE(oop) == HCL_OBJ_TYPE_CHAR)
		{
			if (c == hcl->_symbol) 
			{
				outbfmt (hcl, mask, "#%.*js", HCL_OBJ_GET_SIZE(oop), ((hcl_oop_char_t)oop)->slot);
			}
			else /*if ((hcl_oop_t)c == hcl->_string)*/
			{
				hcl_ooch_t ch;
				int escape = 0;

				for (i = 0; i < HCL_OBJ_GET_SIZE(oop); i++)
				{
					ch = ((hcl_oop_char_t)oop)->slot[i];
					if (ch < ' ') 
					{
						escape = 1;
						break;
					}
				}

				if (escape)
				{
					hcl_ooch_t escaped;

					outbfmt (hcl, mask, "S'");
					for (i = 0; i < HCL_OBJ_GET_SIZE(oop); i++)
					{
						ch = ((hcl_oop_char_t)oop)->slot[i];
						if (ch < ' ') 
						{
							switch (ch)
							{
								case '\0':
									escaped = '0';
									break;
								case '\n':
									escaped = 'n';
									break;
								case '\r':
									escaped = 'r';
									break;
								case '\t':
									escaped = 't';
									break;
								case '\f':
									escaped = 'f';
									break;
								case '\b':
									escaped = 'b';
									break;
								case '\v':
									escaped = 'v';
									break;
								case '\a':
									escaped = 'a';
									break;
								default:
									escaped = ch;
									break;
							}

							if (escaped == ch)
								outbfmt (hcl, mask, "\\x%X", ch);
							else
								outbfmt (hcl, mask, "\\%jc", escaped);
						}
						else
						{
							outbfmt (hcl, mask, "%jc", ch);
						}
					}
					
					outbfmt (hcl, mask, "'");
				}
				else
				{
					outbfmt (hcl, mask, "'%.*js'", HCL_OBJ_GET_SIZE(oop), ((hcl_oop_char_t)oop)->slot);
				}
			}
		}
		else if (HCL_OBJ_GET_FLAGS_TYPE(oop) == HCL_OBJ_TYPE_BYTE)
		{
			outbfmt (hcl, mask, "#[");
			for (i = 0; i < HCL_OBJ_GET_SIZE(oop); i++)
			{
				outbfmt (hcl, mask, " %d", ((hcl_oop_byte_t)oop)->slot[i]);
			}
			outbfmt (hcl, mask, "]");
		}
		
		else if (HCL_OBJ_GET_FLAGS_TYPE(oop) == HCL_OBJ_TYPE_HALFWORD)
		{
			outbfmt (hcl, mask, "#[["); /* TODO: fix this symbol/notation */
			for (i = 0; i < HCL_OBJ_GET_SIZE(oop); i++)
			{
				outbfmt (hcl, mask, " %zX", (hcl_oow_t)((hcl_oop_halfword_t)oop)->slot[i]);
			}
			outbfmt (hcl, mask, "]]");
		}
		else if (HCL_OBJ_GET_FLAGS_TYPE(oop) == HCL_OBJ_TYPE_WORD)
		{
			outbfmt (hcl, mask, "#[[["); /* TODO: fix this symbol/notation */
			for (i = 0; i < HCL_OBJ_GET_SIZE(oop); i++)
			{
				outbfmt (hcl, mask, " %zX", ((hcl_oop_word_t)oop)->slot[i]);
			}
			outbfmt (hcl, mask, "]]]");
		}
		else if (c == hcl->_array)
		{
			outbfmt (hcl, mask, "#(");
			for (i = 0; i < HCL_OBJ_GET_SIZE(oop); i++)
			{
				outbfmt (hcl, mask, " ");
				print_object (hcl, mask, ((hcl_oop_oop_t)oop)->slot[i], outbfmt);
			}
			outbfmt (hcl, mask, ")");
		}
		else if (c == hcl->_class)
		{
			/* print the class name */
			outbfmt (hcl, mask, "%.*js", HCL_OBJ_GET_SIZE(((hcl_oop_class_t)oop)->name), ((hcl_oop_class_t)oop)->name->slot);
		}
		else if (c == hcl->_association)
		{
			outbfmt (hcl, mask, "%O -> %O", ((hcl_oop_association_t)oop)->key, ((hcl_oop_association_t)oop)->value);
		}
		else
		{
			outbfmt (hcl, mask, "<<%.*js>>", HCL_OBJ_GET_SIZE(c->name), ((hcl_oop_char_t)c->name)->slot);
		}
	}
}
#endif

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

hcl_ooi_t hcl_logbfmt (hcl_t* hcl, hcl_oow_t mask, const hcl_bch_t* fmt, ...)
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
		hcl->vmprim.log_write (hcl, hcl->log.last_mask, hcl->log.ptr, hcl->log.len);
		hcl->log.len = 0;
	}
	return (x <= -1)? -1: fo.count;
}

hcl_ooi_t hcl_logufmt (hcl_t* hcl, hcl_oow_t mask, const hcl_uch_t* fmt, ...)
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
		hcl->vmprim.log_write (hcl, hcl->log.last_mask, hcl->log.ptr, hcl->log.len);
		hcl->log.len = 0;
	}

	return (x <= -1)? -1: fo.count;
}


/* -------------------------------------------------------------------------- 
 * ERROR MESSAGE FORMATTING
 * -------------------------------------------------------------------------- */

static int put_errch (hcl_t* hcl, hcl_oow_t mask, hcl_ooch_t ch, hcl_oow_t len)
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

static int put_errcs (hcl_t* hcl, hcl_oow_t mask, const hcl_ooch_t* ptr, hcl_oow_t len)
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


static hcl_ooi_t __errbfmtv (hcl_t* hcl, hcl_oow_t mask, const hcl_bch_t* fmt, ...);

static int _errbfmtv (hcl_t* hcl, const hcl_bch_t* fmt, hcl_fmtout_t* data, va_list ap)
{
	return __logbfmtv (hcl, fmt, data, ap, __errbfmtv);
}

static int _errufmtv (hcl_t* hcl, const hcl_uch_t* fmt, hcl_fmtout_t* data, va_list ap)
{
	return __logufmtv (hcl, fmt, data, ap, __errbfmtv);
}

static hcl_ooi_t __errbfmtv (hcl_t* hcl, hcl_oow_t mask, const hcl_bch_t* fmt, ...)
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

	hcl->errnum = errnum;
	hcl->errmsg.len = 0;

	fo.mask = 0; /* not used */
	fo.putch = put_errch;
	fo.putcs = put_errcs;

	_errufmtv (hcl, fmt, &fo, ap);
}
