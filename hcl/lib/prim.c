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

struct pf_t
{
	hcl_oow_t minargs;
	hcl_oow_t maxargs;
	hcl_pfimpl_t impl;

	hcl_oow_t namelen;
	hcl_ooch_t name[10];
};
typedef struct pf_t pf_t;

/* ------------------------------------------------------------------------- */

hcl_oop_t hcl_makeprim (hcl_t* hcl, hcl_pfimpl_t primimpl, hcl_oow_t minargs, hcl_oow_t maxargs)
{
	hcl_oop_word_t obj;

	obj = (hcl_oop_word_t)hcl_allocwordobj (hcl, HCL_BRAND_PRIM, HCL_NULL, 3);
	if (obj)
	{
		obj->slot[0] = (hcl_oow_t)primimpl;
		obj->slot[1] = minargs;
		obj->slot[2] = maxargs;
	}

	return (hcl_oop_t)obj;
}

/* ------------------------------------------------------------------------- */

static void log_char_object (hcl_t* hcl, int mask, hcl_oop_char_t msg)
{
	hcl_ooi_t n;
	hcl_oow_t rem;
	const hcl_ooch_t* ptr;

	HCL_ASSERT (hcl, HCL_OBJ_GET_FLAGS_TYPE(msg) == HCL_OBJ_TYPE_CHAR);

	rem = HCL_OBJ_GET_SIZE(msg);
	ptr = msg->slot;

start_over:
	while (rem > 0)
	{
		if (*ptr == '\0') 
		{
			n = hcl_logbfmt (hcl, mask, "%jc", *ptr);
			HCL_ASSERT (hcl, n == 1);
			rem -= n;
			ptr += n;
			goto start_over;
		}

		n = hcl_logbfmt (hcl, mask, "%.*js", rem, ptr);
		if (n <= -1) break;
		if (n == 0) 
		{
			/* to skip the unprinted character. 
			 * actually, this check is not needed because of '\0' skipping
			 * at the beginning  of the loop */
			n = hcl_logbfmt (hcl, mask, "%jc", *ptr);
			HCL_ASSERT (hcl, n == 1);
		}
		rem -= n;
		ptr += n;
	}
}

static hcl_pfrc_t pf_log (hcl_t* hcl, hcl_ooi_t nargs)
{
/* TODO: accept log level */
	hcl_oop_t msg;
	int mask;
	hcl_ooi_t k;

	/*level = HCL_STACK_GET(hcl, hcl->sp - nargs + 1);
	if (!HCL_OOP_IS_SMOOI(level)) mask = HCL_LOG_APP | HCL_LOG_INFO; 
	else mask = HCL_LOG_APP | HCL_OOP_TO_SMOOI(level);*/
	mask = HCL_LOG_APP | HCL_LOG_FATAL; /* TODO: accept logging level .. */

	for (k = 0; k < nargs; k++)
	{
		msg = HCL_STACK_GETARG (hcl, nargs, k);

		if (msg == hcl->_nil || msg == hcl->_true || msg == hcl->_false) 
		{
			goto dump_object;
		}
		else if (HCL_OOP_IS_CHAR(msg))
		{
			hcl_logbfmt (hcl, mask, "%jc", HCL_OOP_TO_CHAR(msg));
		}
		else if (HCL_OOP_IS_POINTER(msg))
		{
			if (HCL_OBJ_GET_FLAGS_TYPE(msg) == HCL_OBJ_TYPE_CHAR)
			{
				log_char_object (hcl, mask, (hcl_oop_char_t)msg);
			}
			else if (HCL_OBJ_GET_FLAGS_TYPE(msg) == HCL_OBJ_TYPE_OOP)
			{
				/* visit only 1-level down into an array-like object */
				hcl_oop_t inner;
				hcl_oow_t i;
				int brand;

				brand = HCL_OBJ_GET_FLAGS_BRAND(msg);
				if (brand != HCL_BRAND_ARRAY) goto dump_object;

				for (i = 0; i < HCL_OBJ_GET_SIZE(msg); i++)
				{
					inner = ((hcl_oop_oop_t)msg)->slot[i];

					if (i > 0) hcl_logbfmt (hcl, mask, " ");
					if (HCL_OOP_IS_CHAR(inner))
					{
						hcl_logbfmt (hcl, mask, "%jc", HCL_OOP_TO_CHAR(inner));
					}
					else if (HCL_OOP_IS_POINTER(inner) && HCL_OBJ_GET_FLAGS_TYPE(inner) == HCL_OBJ_TYPE_CHAR)
					{
						log_char_object (hcl, mask, (hcl_oop_char_t)inner);
					}
					else
					{
						hcl_logbfmt (hcl, mask, "%O", inner);
					}
				}
			}
			else goto dump_object;
		}
		else
		{
		dump_object:
			hcl_logbfmt (hcl, mask, "%O", msg);
		}
	}

	HCL_STACK_SETRET (hcl, nargs, hcl->_nil);
	return HCL_PF_SUCCESS;
}

static hcl_pfrc_t pf_gc (hcl_t* hcl, hcl_ooi_t nargs)
{
	hcl_gc (hcl);
	HCL_STACK_SETRET (hcl, nargs, hcl->_nil);
	return HCL_PF_SUCCESS;
}

/* ------------------------------------------------------------------------- */
static hcl_pfrc_t pf_eqv (hcl_t* hcl, hcl_ooi_t nargs)
{
	hcl_oop_t a0, a1, rv;

	a0 = HCL_STACK_GETARG(hcl, nargs, 0);
	a1 = HCL_STACK_GETARG(hcl, nargs, 1);

	rv = (a0 == a1? hcl->_true: hcl->_false);

	HCL_STACK_SETRET (hcl, nargs, rv);
	return HCL_PF_SUCCESS;
}

static hcl_pfrc_t pf_eql (hcl_t* hcl, hcl_ooi_t nargs)
{
	int n;
	n = hcl_equalobjs(hcl, HCL_STACK_GETARG(hcl, nargs, 0), HCL_STACK_GETARG(hcl, nargs, 1));
	if (n <= -1) return HCL_PF_FAILURE;

	HCL_STACK_SETRET (hcl, nargs, (n? hcl->_true: hcl->_false));
	return HCL_PF_SUCCESS;
}

static hcl_pfrc_t pf_eqk (hcl_t* hcl, hcl_ooi_t nargs)
{
	/* equal kind? */
	hcl_oop_t a0, a1, rv;

	a0 = HCL_STACK_GETARG(hcl, nargs, 0);
	a1 = HCL_STACK_GETARG(hcl, nargs, 1);

	rv = (HCL_BRANDOF(hcl, a0) == HCL_BRANDOF(hcl, a1)? hcl->_true: hcl->_false);

	HCL_STACK_SETRET (hcl, nargs, rv);
	return HCL_PF_SUCCESS;
}

static hcl_pfrc_t pf_not (hcl_t* hcl, hcl_ooi_t nargs)
{
	hcl_oop_t arg, rv;

	arg = HCL_STACK_GETARG(hcl, nargs, 0);
	if (arg == hcl->_true) rv = hcl->_false;
	else if (arg == hcl->_false) rv = hcl->_true;
	else
	{
		hcl_seterrbfmt (hcl, HCL_EINVAL, "boolean parameter expected - %O", arg);
		return HCL_PF_FAILURE;
	}

	HCL_STACK_SETRET (hcl, nargs, rv);
	return HCL_PF_SUCCESS;
}

static hcl_pfrc_t pf_and (hcl_t* hcl, hcl_ooi_t nargs)
{
	hcl_oop_t arg, rv;
	hcl_oow_t i;

	rv = hcl->_true;
	for (i = 1; i < nargs; i++)
	{
		arg = HCL_STACK_GETARG(hcl, nargs, i);
		if (arg == hcl->_true)
		{
			/* do nothing */
		}
		else if (arg == hcl->_false) 
		{
			rv = hcl->_false;
			break;
		}
		else
		{
			hcl_seterrbfmt (hcl, HCL_EINVAL, "boolean parameter expected - %O", arg);
			return HCL_PF_FAILURE;
		}
	}

	HCL_STACK_SETRET (hcl, nargs, rv);
	return HCL_PF_SUCCESS;
}

static hcl_pfrc_t pf_or (hcl_t* hcl, hcl_ooi_t nargs)
{
	hcl_oop_t arg, rv;
	hcl_oow_t i;

	rv = hcl->_false;
	for (i = 1; i < nargs; i++)
	{
			arg = HCL_STACK_GETARG(hcl, nargs, i);
		if (arg == hcl->_true)
		{
			rv = hcl->_true;
			break;
		}
		else if (arg == hcl->_false) 
		{
			/* do nothing */
		}
		else
		{
			hcl_seterrbfmt (hcl, HCL_EINVAL, "boolean parameter expected - %O", arg);
			return HCL_PF_FAILURE;
		}
	}

	HCL_STACK_SETRET (hcl, nargs, rv);
	return HCL_PF_SUCCESS;
}

/* ------------------------------------------------------------------------- */

static hcl_pfrc_t pf_integer_add (hcl_t* hcl, hcl_ooi_t nargs)
{
	hcl_oow_t i;
	hcl_oop_t arg, ret;

	ret = HCL_STACK_GETARG(hcl, nargs, 0);
	for (i = 1; i < nargs; i++)
	{
		arg = HCL_STACK_GETARG(hcl, nargs, i);
		ret = hcl_addints(hcl, ret, arg);
		if (!ret) return HCL_PF_FAILURE;
	}

	HCL_STACK_SETRET (hcl, nargs, ret);
	return HCL_PF_SUCCESS;
}

static hcl_pfrc_t pf_integer_sub (hcl_t* hcl, hcl_ooi_t nargs)
{
	hcl_oow_t i;
	hcl_oop_t arg, ret;

	ret = HCL_STACK_GETARG(hcl, nargs, 0);
	for (i = 1; i < nargs; i++)
	{
		arg = HCL_STACK_GETARG(hcl, nargs, i);
		ret = hcl_subints(hcl, ret, arg);
		if (!ret) return HCL_PF_FAILURE;
	}

	HCL_STACK_SETRET (hcl, nargs, ret);
	return HCL_PF_SUCCESS;
}

static hcl_pfrc_t pf_integer_mul (hcl_t* hcl, hcl_ooi_t nargs)
{
	hcl_oow_t i;
	hcl_oop_t arg, ret;

	ret = HCL_STACK_GETARG(hcl, nargs, 0);
	for (i = 1; i < nargs; i++)
	{
		arg = HCL_STACK_GETARG(hcl, nargs, i);
		ret = hcl_mulints(hcl, ret, arg);
		if (!ret) return HCL_PF_FAILURE;
	}

	HCL_STACK_SETRET (hcl, nargs, ret);
	return HCL_PF_SUCCESS;
}

static hcl_pfrc_t pf_integer_quo (hcl_t* hcl, hcl_ooi_t nargs)
{
	hcl_oow_t i;
	hcl_oop_t arg, ret;

	ret = HCL_STACK_GETARG(hcl, nargs, 0);
	for (i = 1; i < nargs; i++)
	{
		arg = HCL_STACK_GETARG(hcl, nargs, i);
		ret = hcl_divints(hcl, ret, arg, 0, HCL_NULL);
		if (!ret) return HCL_PF_FAILURE;
	}

	HCL_STACK_SETRET (hcl, nargs, ret);
	return HCL_PF_SUCCESS;
}

static hcl_pfrc_t pf_integer_rem (hcl_t* hcl, hcl_ooi_t nargs)
{
	hcl_oow_t i;
	hcl_oop_t arg, ret, rem;

	ret = HCL_STACK_GETARG(hcl, nargs, 0);
	for (i = 1; i < nargs; i++)
	{
		arg = HCL_STACK_GETARG(hcl, nargs, i);
		ret = hcl_divints(hcl, ret, arg, 0, &rem);
		if (!ret) return HCL_PF_FAILURE;
		ret = rem;
	}

	HCL_STACK_SETRET (hcl, nargs, ret);
	return HCL_PF_SUCCESS;
}

/* ------------------------------------------------------------------------- */



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


#define PUT_OOCH(c,n) do { \
	if (n > 0) { \
		int xx; \
		if ((xx = put_formatted_chars(hcl, data->mask, c, n)) <= -1) goto oops; \
		if (xx == 0) goto done; \
		data->count += n; \
	} \
} while (0)

#define PUT_OOCS(ptr,len) do { \
	if (len > 0) { \
		int xx; \
		if ((xx = put_formatted_string(hcl, data->mask, ptr, len)) <= -1) goto oops; \
		if (xx == 0) goto done; \
		data->count += len; \
	} \
} while (0)

#if 0
static HCL_INLINE int print_formatted (hcl_t* hcl, hcl_ooi_t nargs)
{
	hcl_oop_char_t fmtoop;
	hcl_ooi_t i;

	const fmtchar_t* percent;
	const fmtchar_t* checkpoint;
	hcl_bch_t nbuf[MAXNBUF], bch;
	const hcl_bch_t* nbufp;
	int n, base, neg, sign;
	hcl_ooi_t tmp, width, precision;
	hcl_ooch_t ch, padc;
	int lm_flag, lm_dflag, flagc, numlen;
	hcl_uintmax_t num = 0;
	int stop = 0;

#if 0
	hcl_bchbuf_t* fltfmt;
	hcl_oochbuf_t* fltout;
#endif
	hcl_bch_t* (*sprintn) (hcl_bch_t* nbuf, hcl_uintmax_t num, int base, hcl_ooi_t* lenp);

	fmtoop = (hcl_oop_char_t)HCL_STACK_GETARG(hcl, nargs, 0);
	HCL_ASSERT (hcl, HCL_IS_STRING(hcl, fmtoop));

	fmt = HCL_OBJ_GET_CHAR_SLOT(fmtoop);

	data->count = 0;

	while (1)
	{
	#if defined(FMTCHAR_IS_OOCH)
		checkpoint = fmt;
		while ((ch = *fmt++) != '%' || stop) 
		{
			if (ch == '\0') 
			{
				PUT_OOCS (checkpoint, fmt - checkpoint - 1);
				goto done;
			}
		}
		PUT_OOCS (checkpoint, fmt - checkpoint - 1);
	#else

		while ((ch = *fmt++) != '%' || stop) 
		{
			if (ch == '\0') goto done;
			PUT_OOCH (ch, 1);
		}
	#endif
		percent = fmt - 1;


		padc = ' '; 
		width = 0; precision = 0;
		neg = 0; sign = 0;

		lm_flag = 0; lm_dflag = 0; flagc = 0; 
		sprintn = sprintn_lower;

reswitch:
		switch (ch = *fmt++) 
		{
		case '%': /* %% */
			bch = ch;
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
				if (lm_tab[ch - 'a'].dflag && lm_flag == lm_tab[ch - 'a'].flag)
				{
					lm_flag &= ~lm_tab[ch - 'a'].flag;
					lm_flag |= lm_tab[ch - 'a'].dflag;
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
				lm_flag |= lm_tab[ch - 'a'].flag;
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
				*(va_arg(ap, hcl_intmax_t*)) = data->count;
			else if (lm_flag & LF_Z) /* z */
				*(va_arg(ap, hcl_ooi_t*)) = data->count;
		#if (HCL_SIZEOF_LONG_LONG > 0)
			else if (lm_flag & LF_Q) /* ll */
				*(va_arg(ap, long long int*)) = data->count;
		#endif
			else if (lm_flag & LF_L) /* l */
				*(va_arg(ap, long int*)) = data->count;
			else if (lm_flag & LF_H) /* h */
				*(va_arg(ap, short int*)) = data->count;
			else if (lm_flag & LF_C) /* hh */
				*(va_arg(ap, char*)) = data->count;
			else if (flagc & FLAGC_LENMOD) 
				goto invalid_format;
			else
				*(va_arg(ap, int*)) = data->count;
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
			if (!(flagc & FLAGC_LEFTADJ) && width > 0) PUT_OOCH (padc, width);
			PUT_OOCH (bch, 1);
			if ((flagc & FLAGC_LEFTADJ) && width > 0) PUT_OOCH (padc, width);
			break;
		}

		case 'C':
		{
			hcl_uch_t ooch;

			/* zeropad must not take effect for 'C' */
			if (flagc & FLAGC_ZEROPAD) padc = ' ';
			if (lm_flag & LF_H) goto lowercase_c;
		#if defined(HCL_OOCH_IS_BCH)
			if (lm_flag & LF_J) goto lowercase_c;
		#endif
		uppercase_c:
			ooch = HCL_SIZEOF(hcl_uch_t) < HCL_SIZEOF(int)? va_arg(ap, int): va_arg(ap, hcl_uch_t);

			/* precision 0 doesn't kill the letter */
			width--;
			if (!(flagc & FLAGC_LEFTADJ) && width > 0) PUT_OOCH (padc, width);
			PUT_OOCH (ooch, 1);
			if ((flagc & FLAGC_LEFTADJ) && width > 0) PUT_OOCH (padc, width);
			break;
		}

		case 's':
		{
			const hcl_bch_t* bsp;
			hcl_oow_t bslen, slen;

			/* zeropad must not take effect for 'S' */
			if (flagc & FLAGC_ZEROPAD) padc = ' ';
			if (lm_flag & LF_L) goto uppercase_s;
		#if defined(HCL_OOCH_IS_UCH)
			if (lm_flag & LF_J) goto uppercase_s;
		#endif
		lowercase_s:

			bsp = va_arg (ap, hcl_bch_t*);
			if (bsp == HCL_NULL) bsp = bch_nullstr;

		#if defined(HCL_OOCH_IS_UCH)
			/* get the length */
			for (bslen = 0; bsp[bslen]; bslen++);

			if (hcl_convbtooochars(hcl, bsp, &bslen, HCL_NULL, &slen) <= -1) goto oops;

			/* slen holds the length after conversion */
			n = slen;
			if ((flagc & FLAGC_DOT) && precision < slen) n = precision;
			width -= n;

			if (!(flagc & FLAGC_LEFTADJ) && width > 0) PUT_OOCH (padc, width);

			{
				hcl_ooch_t conv_buf[32]; 
				hcl_oow_t conv_len, src_len, tot_len = 0;
				while (n > 0)
				{
					HCL_ASSERT (hcl, bslen > tot_len);

					src_len = bslen - tot_len;
					conv_len = HCL_COUNTOF(conv_buf);

					/* this must not fail since the dry-run above was successful */
					hcl_convbtooochars (hcl, &bsp[tot_len], &src_len, conv_buf, &conv_len);
					tot_len += src_len;

					if (conv_len > n) conv_len = n;
					PUT_OOCS (conv_buf, conv_len);

					n -= conv_len;
				}
			}
			
			if ((flagc & FLAGC_LEFTADJ) && width > 0) PUT_OOCH (padc, width);
		#else
			if (flagc & FLAGC_DOT)
			{
				for (n = 0; n < precision && bsp[n]; n++);
			}
			else
			{
				for (n = 0; bsp[n]; n++);
			}

			width -= n;

			if (!(flagc & FLAGC_LEFTADJ) && width > 0) PUT_OOCH (padc, width);
			PUT_OOCS (bsp, n);
			if ((flagc & FLAGC_LEFTADJ) && width > 0) PUT_OOCH (padc, width);
		#endif
			break;
		}

		case 'S':
		{
			const hcl_uch_t* usp;
			hcl_oow_t uslen, slen;

			/* zeropad must not take effect for 's' */
			if (flagc & FLAGC_ZEROPAD) padc = ' ';
			if (lm_flag & LF_H) goto lowercase_s;
		#if defined(HCL_OOCH_IS_UCH)
			if (lm_flag & LF_J) goto lowercase_s;
		#endif
		uppercase_s:
			usp = va_arg (ap, hcl_uch_t*);
			if (usp == HCL_NULL) usp = uch_nullstr;

		#if defined(HCL_OOCH_IS_BCH)
			/* get the length */
			for (uslen = 0; usp[uslen]; uslen++);

			if (hcl_convutooochars(hcl, usp, &uslen, HCL_NULL, &slen) <= -1) goto oops;

			/* slen holds the length after conversion */
			n = slen;
			if ((flagc & FLAGC_DOT) && precision < slen) n = precision;
			width -= n;

			if (!(flagc & FLAGC_LEFTADJ) && width > 0) PUT_OOCH (padc, width);
			{
				hcl_ooch_t conv_buf[32]; 
				hcl_oow_t conv_len, src_len, tot_len = 0;
				while (n > 0)
				{
					HCL_ASSERT (hcl, uslen > tot_len);

					src_len = uslen - tot_len;
					conv_len = HCL_COUNTOF(conv_buf);

					/* this must not fail since the dry-run above was successful */
					hcl_convutooochars (hcl, &usp[tot_len], &src_len, conv_buf, &conv_len);
					tot_len += src_len;

					if (conv_len > n) conv_len = n;
					PUT_OOCS (conv_buf, conv_len);

					n -= conv_len;
				}
			}
			if ((flagc & FLAGC_LEFTADJ) && width > 0) PUT_OOCH (padc, width);
		#else
			if (flagc & FLAGC_DOT)
			{
				for (n = 0; n < precision && usp[n]; n++);
			}
			else
			{
				for (n = 0; usp[n]; n++);
			}

			width -= n;

			if (!(flagc & FLAGC_LEFTADJ) && width > 0) PUT_OOCH (padc, width);
			PUT_OOCS (usp, n);
			if ((flagc & FLAGC_LEFTADJ) && width > 0) PUT_OOCH (padc, width);
		#endif
			break;
		}

		case 'O': /* object - ignore precision, width, adjustment */
			if (hcl_outfmtobj(hcl, data->mask, va_arg(ap, hcl_oop_t), outbfmt) <= -1) goto oops;
			break;

#if 0
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
		#if (HCL_SIZEOF___FLOAT128 > 0) && defined(HAVE_QUADMATH_SNPRINTF)
			__float128 v_qd;
		#endif
			long double v_ld;
			double v_d;
			int dtype = 0;
			hcl_oow_t newcapa;

			if (lm_flag & LF_J)
			{
			#if (HCL_SIZEOF___FLOAT128 > 0) && defined(HAVE_QUADMATH_SNPRINTF) && (HCL_SIZEOF_FLTMAX_T == HCL_SIZEOF___FLOAT128)
				v_qd = va_arg (ap, hcl_fltmax_t);
				dtype = LF_QD;
			#elif HCL_SIZEOF_FLTMAX_T == HCL_SIZEOF_DOUBLE
				v_d = va_arg (ap, hcl_fltmax_t);
			#elif HCL_SIZEOF_FLTMAX_T == HCL_SIZEOF_LONG_DOUBLE
				v_ld = va_arg (ap, hcl_fltmax_t);
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
				v_d = va_arg (ap, hcl_flt_t);
			#elif HCL_SIZEOF_FLT_T == HCL_SIZEOF_LONG_DOUBLE
				v_ld = va_arg (ap, hcl_flt_t);
				dtype = LF_LD;
			#else
				#error Unsupported hcl_flt_t
			#endif
			}
			else if (lm_flag & (LF_LD | LF_L))
			{
				v_ld = va_arg (ap, long double);
				dtype = LF_LD;
			}
		#if (HCL_SIZEOF___FLOAT128 > 0) && defined(HAVE_QUADMATH_SNPRINTF)
			else if (lm_flag & (LF_QD | LF_Q))
			{
				v_qd = va_arg (ap, __float128);
				dtype = LF_QD;
			}
		#endif
			else if (flagc & FLAGC_LENMOD)
			{
				goto invalid_format;
			}
			else
			{
				v_d = va_arg (ap, double);
			}

			fmtlen = fmt - percent;
			if (fmtlen > fltfmt->capa)
			{
				if (fltfmt->ptr == fltfmt->buf)
				{
					fltfmt->ptr = HCL_MMGR_ALLOC (HCL_MMGR_GETDFL(), HCL_SIZEOF(*fltfmt->ptr) * (fmtlen + 1));
					if (fltfmt->ptr == HCL_NULL) goto oops;
				}
				else
				{
					hcl_mchar_t* tmpptr;

					tmpptr = HCL_MMGR_REALLOC (HCL_MMGR_GETDFL(), fltfmt->ptr, HCL_SIZEOF(*fltfmt->ptr) * (fmtlen + 1));
					if (tmpptr == HCL_NULL) goto oops;
					fltfmt->ptr = tmpptr;
				}

				fltfmt->capa = fmtlen;
			}

			/* compose back the format specifier */
			fmtlen = 0;
			fltfmt->ptr[fmtlen++] = '%';
			if (flagc & FLAGC_SPACE) fltfmt->ptr[fmtlen++] = ' ';
			if (flagc & FLAGC_SHARP) fltfmt->ptr[fmtlen++] = '#';
			if (flagc & FLAGC_SIGN) fltfmt->ptr[fmtlen++] = '+';
			if (flagc & FLAGC_LEFTADJ) fltfmt->ptr[fmtlen++] = '-';
			if (flagc & FLAGC_ZEROPAD) fltfmt->ptr[fmtlen++] = '0';

			if (flagc & FLAGC_STAR1) fltfmt->ptr[fmtlen++] = '*';
			else if (flagc & FLAGC_WIDTH) 
			{
				fmtlen += hcl_fmtuintmaxtombs (
					&fltfmt->ptr[fmtlen], fltfmt->capa - fmtlen, 
					width, 10, -1, '\0', HCL_NULL);
			}
			if (flagc & FLAGC_DOT) fltfmt->ptr[fmtlen++] = '.';
			if (flagc & FLAGC_STAR2) fltfmt->ptr[fmtlen++] = '*';
			else if (flagc & FLAGC_PRECISION) 
			{
				fmtlen += hcl_fmtuintmaxtombs (
					&fltfmt->ptr[fmtlen], fltfmt->capa - fmtlen, 
					precision, 10, -1, '\0', HCL_NULL);
			}

			if (dtype == LF_LD)
				fltfmt->ptr[fmtlen++] = 'L';
		#if (HCL_SIZEOF___FLOAT128 > 0)
			else if (dtype == LF_QD)
				fltfmt->ptr[fmtlen++] = 'Q';
		#endif

			fltfmt->ptr[fmtlen++] = ch;
			fltfmt->ptr[fmtlen] = '\0';

		#if defined(HAVE_SNPRINTF)
			/* nothing special here */
		#else
			/* best effort to avoid buffer overflow when no snprintf is available. 
			 * i really can't do much if it happens. */
			newcapa = precision + width + 32;
			if (fltout->capa < newcapa)
			{
				HCL_ASSERT (hcl, fltout->ptr == fltout->buf);

				fltout->ptr = HCL_MMGR_ALLOC (HCL_MMGR_GETDFL(), HCL_SIZEOF(char_t) * (newcapa + 1));
				if (fltout->ptr == HCL_NULL) goto oops;
				fltout->capa = newcapa;
			}
		#endif

			while (1)
			{

				if (dtype == LF_LD)
				{
				#if defined(HAVE_SNPRINTF)
					q = snprintf ((hcl_mchar_t*)fltout->ptr, fltout->capa + 1, fltfmt->ptr, v_ld);
				#else
					q = sprintf ((hcl_mchar_t*)fltout->ptr, fltfmt->ptr, v_ld);
				#endif
				}
			#if (HCL_SIZEOF___FLOAT128 > 0) && defined(HAVE_QUADMATH_SNPRINTF)
				else if (dtype == LF_QD)
				{
					q = quadmath_snprintf ((hcl_mchar_t*)fltout->ptr, fltout->capa + 1, fltfmt->ptr, v_qd);
				}
			#endif
				else
				{
				#if defined(HAVE_SNPRINTF)
					q = snprintf ((hcl_mchar_t*)fltout->ptr, fltout->capa + 1, fltfmt->ptr, v_d);
				#else
					q = sprintf ((hcl_mchar_t*)fltout->ptr, fltfmt->ptr, v_d);
				#endif
				}
				if (q <= -1) goto oops;
				if (q <= fltout->capa) break;

				newcapa = fltout->capa * 2;
				if (newcapa < q) newcapa = q;

				if (fltout->ptr == fltout->sbuf)
				{
					fltout->ptr = HCL_MMGR_ALLOC (HCL_MMGR_GETDFL(), HCL_SIZEOF(char_t) * (newcapa + 1));
					if (fltout->ptr == HCL_NULL) goto oops;
				}
				else
				{
					char_t* tmpptr;

					tmpptr = HCL_MMGR_REALLOC (HCL_MMGR_GETDFL(), fltout->ptr, HCL_SIZEOF(char_t) * (newcapa + 1));
					if (tmpptr == HCL_NULL) goto oops;
					fltout->ptr = tmpptr;
				}
				fltout->capa = newcapa;
			}

			if (HCL_SIZEOF(char_t) != HCL_SIZEOF(hcl_mchar_t))
			{
				fltout->ptr[q] = '\0';
				while (q > 0)
				{
					q--;
					fltout->ptr[q] = ((hcl_mchar_t*)fltout->ptr)[q];
				}
			}

			sp = fltout->ptr;
			flagc &= ~FLAGC_DOT;
			width = 0;
			precision = 0;
			goto print_lowercase_s;
		}
#endif


handle_nosign:
			sign = 0;
			if (lm_flag & LF_J)
			{
			#if defined(__GNUC__) && \
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
					num |= (hcl_uintmax_t)x << (shift * 8);
				#endif
				}
			#else
				num = va_arg (ap, hcl_uintmax_t);
			#endif
			}
#if 0
			else if (lm_flag & LF_T)
				num = va_arg (ap, hcl_ptrdiff_t);
#endif
			else if (lm_flag & LF_Z)
				num = va_arg (ap, hcl_oow_t);
			#if (HCL_SIZEOF_LONG_LONG > 0)
			else if (lm_flag & LF_Q)
				num = va_arg (ap, unsigned long long int);
			#endif
			else if (lm_flag & (LF_L | LF_LD))
				num = va_arg (ap, unsigned long int);
			else if (lm_flag & LF_H)
				num = (unsigned short int)va_arg (ap, int);
			else if (lm_flag & LF_C)
				num = (unsigned char)va_arg (ap, int);
			else
				num = va_arg (ap, unsigned int);
			goto number;

handle_sign:
			if (lm_flag & LF_J)
			{
			#if defined(__GNUC__) && \
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
					num |= (hcl_uintmax_t)x << (shift * 8);
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

			nbufp = sprintn (nbuf, num, base, &tmp);
			if ((flagc & FLAGC_SHARP) && num != 0) 
			{
				if (base == 8) tmp++;
				else if (base == 16) tmp += 2;
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
				PUT_OOCH (padc, width);
				width = 0;
			}

			if (neg) PUT_OOCH ('-', 1);
			else if (flagc & FLAGC_SIGN) PUT_OOCH ('+', 1);
			else if (flagc & FLAGC_SPACE) PUT_OOCH (' ', 1);

			if ((flagc & FLAGC_SHARP) && num != 0) 
			{
				if (base == 2) 
				{
					PUT_OOCH ('0', 1);
					PUT_OOCH ('b', 1);
				}
				if (base == 8) 
				{
					PUT_OOCH ('0', 1);
				} 
				else if (base == 16) 
				{
					PUT_OOCH ('0', 1);
					PUT_OOCH ('x', 1);
				}
			}

			if ((flagc & FLAGC_DOT) && precision > numlen)
			{
				/* extra zeros for precision specified */
				PUT_OOCH ('0', precision - numlen);
			}

			if (!(flagc & FLAGC_LEFTADJ) && width > 0 && (width -= tmp) > 0)
			{
				PUT_OOCH (padc, width);
			}

			while (*nbufp) PUT_OOCH (*nbufp--, 1); /* output actual digits */

			if ((flagc & FLAGC_LEFTADJ) && width > 0 && (width -= tmp) > 0)
			{
				PUT_OOCH (padc, width);
			}
			break;

invalid_format:
		#if defined(FMTCHAR_IS_OOCH)
			PUT_OOCS (percent, fmt - percent);
		#else
			while (percent < fmt) PUT_OOCH (*percent++, 1);
		#endif
			break;

		default:
		#if defined(FMTCHAR_IS_OOCH)
			PUT_OOCS (percent, fmt - percent);
		#else
			while (percent < fmt) PUT_OOCH (*percent++, 1);
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
#endif

static hcl_pfrc_t pf_printf (hcl_t* hcl, hcl_ooi_t nargs)
{
	hcl_oop_char_t fmt;

	fmt = (hcl_oop_char_t)HCL_STACK_GETARG(hcl, nargs, 0);
	if (!HCL_IS_STRING(hcl, fmt))
	{
		/* if the first argument is not a string, it just prints the 
		 * argument and ignore the remaining arguments */
		if (hcl_print(hcl, (hcl_oop_t)fmt) <= -1)
			HCL_STACK_SETRETTOERRNUM (hcl, nargs);
		else
			HCL_STACK_SETRET (hcl, nargs, hcl->_nil);
		return HCL_PF_SUCCESS;
	}

//	print_formatted (hcl, nargs);

	HCL_STACK_SETRET (hcl, nargs, hcl->_nil);
	return HCL_PF_SUCCESS;
}

/* ------------------------------------------------------------------------- */

static pf_t builtin_prims[] =
{
	{ 0, HCL_TYPE_MAX(hcl_oow_t), pf_log,           3,  { 'l','o','g' } },
	{ 0, 0,                       pf_gc,            2,  { 'g','c' } },

	{ 1, 1,                       pf_not,           3,  { 'n','o','t' } }, 
	{ 2, HCL_TYPE_MAX(hcl_oow_t), pf_and,           3,  { 'a','n','d' } },
	{ 2, HCL_TYPE_MAX(hcl_oow_t), pf_or,            2,  { 'o','r' } },

	{ 2, 2,                       pf_eqv,           4,  { 'e','q','v','?' } },
	{ 2, 2,                       pf_eql,           4,  { 'e','q','l','?' } },
	{ 2, 2,                       pf_eqk,           4,  { 'e','q','k','?' } },

	/*
	{ 2, 2,                       pf_gt,            1,  { '>' } },
	{ 2, 2,                       pf_ge,            2,  { '>','=' } },
	{ 2, 2,                       pf_lt,            1,  { '<' } },
	{ 2, 2,                       pf_le,            2,  { '<','=' } },
	{ 2, 2,                       pf_eq,            1,  { '=' } },
	{ 2, 2,                       pf_ne,            2,  { '/','=' } },

	{ 2, 2,                       pf_max,           3,  { 'm','a','x' } },
	{ 2, 2,                       pf_min,           3,  { 'm','i','n' } },
	*/

	{ 1, HCL_TYPE_MAX(hcl_oow_t), pf_integer_add,   1,  { '+' } },
	{ 1, HCL_TYPE_MAX(hcl_oow_t), pf_integer_sub,   1,  { '-' } },
	{ 1, HCL_TYPE_MAX(hcl_oow_t), pf_integer_mul,   1,  { '*' } },
	{ 1, HCL_TYPE_MAX(hcl_oow_t), pf_integer_quo,   1,  { '/' } },
	{ 2, HCL_TYPE_MAX(hcl_oow_t), pf_integer_rem,   3,  { 'm','o','d' } },

	{ 1, HCL_TYPE_MAX(hcl_oow_t), pf_printf,        6,  { 'p','r','i','n','t','f' } },
};


int hcl_addbuiltinprims (hcl_t* hcl)
{
	hcl_oow_t i;
	hcl_oop_t prim, name;
	hcl_oop_cons_t cons;

	for (i = 0; i < HCL_COUNTOF(builtin_prims); i++)
	{
		prim = hcl_makeprim(hcl, builtin_prims[i].impl, builtin_prims[i].minargs, builtin_prims[i].maxargs);
		if (!prim) return -1;

		hcl_pushtmp (hcl, &prim);
		name = hcl_makesymbol(hcl, builtin_prims[i].name, builtin_prims[i].namelen);
		hcl_poptmp (hcl);
		if (!name) return -1;

		hcl_pushtmp (hcl, &name);
		cons = hcl_putatsysdic(hcl, name, prim);
		hcl_poptmp (hcl);
		if (!cons) return -1;

		/* turn on the kernel bit in the symbol associated with a primitive 
		 * function. 'set' prevents this symbol from being used as a variable
		 * name */ 
		HCL_OBJ_SET_FLAGS_KERNEL (name, 1);
	}

	return 0;
}

