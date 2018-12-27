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
    IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WAfRRANTIES
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

#if (HCL_LIW_BITS == HCL_OOW_BITS)
	/* nothing special */
#elif (HCL_LIW_BITS == HCL_OOHW_BITS)
#	define MAKE_WORD(hw1,hw2) ((hcl_oow_t)(hw1) | (hcl_oow_t)(hw2) << HCL_LIW_BITS)
#else
#	error UNSUPPORTED LIW BIT SIZE
#endif

/*#define IS_POWER_OF_2(ui) (((ui) > 0) && (((ui) & (~(ui)+ 1)) == (ui)))*/
#define IS_POWER_OF_2(ui) (((ui) > 0) && ((ui) & ((ui) - 1)) == 0)

#define IS_SIGN_DIFF(x,y) (((x) ^ (y)) < 0)

/* digit character array */
static char _digitc_upper[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
static char _digitc_lower[] = "0123456789abcdefghijklmnopqrstuvwxyz";

/* exponent table */
static hcl_uint8_t _exp_tab[] = 
{
	0, 0, 1, 0, 2, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 4, 
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 5, 0
};


#if (HCL_SIZEOF_OOW_T == HCL_SIZEOF_INT) && defined(HCL_HAVE_BUILTIN_UADD_OVERFLOW)
#	define oow_add_overflow(a,b,c)  __builtin_uadd_overflow(a,b,c)
#elif (HCL_SIZEOF_OOW_T == HCL_SIZEOF_LONG) && defined(HCL_HAVE_BUILTIN_UADDL_OVERFLOW)
#	define oow_add_overflow(a,b,c)  __builtin_uaddl_overflow(a,b,c)
#elif (HCL_SIZEOF_OOW_T == HCL_SIZEOF_LONG_LONG) && defined(HCL_HAVE_BUILTIN_UADDLL_OVERFLOW)
#	define oow_add_overflow(a,b,c)  __builtin_uaddll_overflow(a,b,c)
#else
static HCL_INLINE int oow_add_overflow (hcl_oow_t a, hcl_oow_t b, hcl_oow_t* c)
{
	*c = a + b;
	return b > HCL_TYPE_MAX(hcl_oow_t) - a;
}
#endif


#if (HCL_SIZEOF_OOW_T == HCL_SIZEOF_INT) && defined(HCL_HAVE_BUILTIN_UMUL_OVERFLOW)
#	define oow_mul_overflow(a,b,c)  __builtin_umul_overflow(a,b,c)
#elif (HCL_SIZEOF_OOW_T == HCL_SIZEOF_LONG) && defined(HCL_HAVE_BUILTIN_UMULL_OVERFLOW)
#	define oow_mul_overflow(a,b,c)  __builtin_umull_overflow(a,b,c)
#elif (HCL_SIZEOF_OOW_T == HCL_SIZEOF_LONG_LONG) && defined(HCL_HAVE_BUILTIN_UMULLL_OVERFLOW)
#	define oow_mul_overflow(a,b,c)  __builtin_umulll_overflow(a,b,c)
#else
static HCL_INLINE int oow_mul_overflow (hcl_oow_t a, hcl_oow_t b, hcl_oow_t* c)
{
#if (HCL_SIZEOF_UINTMAX_T > HCL_SIZEOF_OOW_T)
	hcl_uintmax_t k;
	k = (hcl_uintmax_t)a * (hcl_uintmax_t)b;
	*c = (hcl_oow_t)k;
	return (k >> HCL_OOW_BITS) > 0;
	/*return k > HCL_TYPE_MAX(hcl_oow_t);*/
#else
	*c = a * b;
	return b != 0 && a > HCL_TYPE_MAX(hcl_oow_t) / b; /* works for unsigned types only */
#endif
}
#endif

#if (HCL_SIZEOF_OOI_T == HCL_SIZEOF_INT) && defined(HCL_HAVE_BUILTIN_SMUL_OVERFLOW)
#	define shcli_mul_overflow(hcl,a,b,c)  __builtin_smul_overflow(a,b,c)
#elif (HCL_SIZEOF_OOI_T == HCL_SIZEOF_LONG) && defined(HCL_HAVE_BUILTIN_SMULL_OVERFLOW)
#	define shcli_mul_overflow(hcl,a,b,c)  __builtin_smull_overflow(a,b,c)
#elif (HCL_SIZEOF_OOI_T == HCL_SIZEOF_LONG_LONG) && defined(HCL_HAVE_BUILTIN_SMULLL_OVERFLOW)
#	define shcli_mul_overflow(hcl,a,b,c)  __builtin_smulll_overflow(a,b,c)
#else
static HCL_INLINE int shcli_mul_overflow (hcl_t* hcl, hcl_ooi_t a, hcl_ooi_t b, hcl_ooi_t* c)
{
	/* take note that this function is not supposed to handle
	 * the whole hcl_ooi_t range. it handles the shcli subrange */

#if (HCL_SIZEOF_UINTMAX_T > HCL_SIZEOF_OOI_T)
	hcl_intmax_t k;

	HCL_ASSERT (hcl, HCL_IN_SMOOI_RANGE(a));
	HCL_ASSERT (hcl, HCL_IN_SMOOI_RANGE(b));

	k = (hcl_intmax_t)a * (hcl_intmax_t)b;
	*c = (hcl_ooi_t)k;

	return k > HCL_TYPE_MAX(hcl_ooi_t) || k < HCL_TYPE_MIN(hcl_ooi_t);
#else

	hcl_ooi_t ua, ub;

	HCL_ASSERT (hcl, HCL_IN_SMOOI_RANGE(a));
	HCL_ASSERT (hcl, HCL_IN_SMOOI_RANGE(b));

	*c = a * b;

	ub = (b >= 0)? b: -b;
	ua = (a >= 0)? a: -a;

	/* though this fomula basically works for unsigned types in principle, 
	 * the values used here are all absolute values and they fall in
	 * a safe range to apply this fomula. the safe range is guaranteed because
	 * the sources are supposed to be shclis. */
	return ub != 0 && ua > HCL_TYPE_MAX(hcl_ooi_t) / ub; 
#endif
}
#endif

#if (HCL_SIZEOF_LIW_T == HCL_SIZEOF_INT) && defined(HCL_HAVE_BUILTIN_UADD_OVERFLOW)
#	define liw_add_overflow(a,b,c)  __builtin_uadd_overflow(a,b,c)
#elif (HCL_SIZEOF_LIW_T == HCL_SIZEOF_LONG) && defined(HCL_HAVE_BUILTIN_UADDL_OVERFLOW)
#	define liw_add_overflow(a,b,c)  __builtin_uaddl_overflow(a,b,c)
#elif (HCL_SIZEOF_LIW_T == HCL_SIZEOF_LONG_LONG) && defined(HCL_HAVE_BUILTIN_UADDLL_OVERFLOW)
#	define liw_add_overflow(a,b,c)  __builtin_uaddll_overflow(a,b,c)
#else
static HCL_INLINE int liw_add_overflow (hcl_liw_t a, hcl_liw_t b, hcl_liw_t* c)
{
	*c = a + b;
	return b > HCL_TYPE_MAX(hcl_liw_t) - a;
}
#endif

#if (HCL_SIZEOF_LIW_T == HCL_SIZEOF_INT) && defined(HCL_HAVE_BUILTIN_UMUL_OVERFLOW)
#	define liw_mul_overflow(a,b,c)  __builtin_umul_overflow(a,b,c)
#elif (HCL_SIZEOF_LIW_T == HCL_SIZEOF_LONG) && defined(HCL_HAVE_BUILTIN_UMULL_OVERFLOW)
#	define liw_mul_overflow(a,b,c)  __builtin_uaddl_overflow(a,b,c)
#elif (HCL_SIZEOF_LIW_T == HCL_SIZEOF_LONG_LONG) && defined(HCL_HAVE_BUILTIN_UMULLL_OVERFLOW)
#	define liw_mul_overflow(a,b,c)  __builtin_uaddll_overflow(a,b,c)
#else
static HCL_INLINE int liw_mul_overflow (hcl_liw_t a, hcl_liw_t b, hcl_liw_t* c)
{
#if (HCL_SIZEOF_UINTMAX_T > HCL_SIZEOF_LIW_T)
	hcl_uintmax_t k;
	k = (hcl_uintmax_t)a * (hcl_uintmax_t)b;
	*c = (hcl_liw_t)k;
	return (k >> HCL_LIW_BITS) > 0;
	/*return k > HCL_TYPE_MAX(hcl_liw_t);*/
#else
	*c = a * b;
	return b != 0 && a > HCL_TYPE_MAX(hcl_liw_t) / b; /* works for unsigned types only */
#endif
}
#endif

static int is_normalized_integer (hcl_t* hcl, hcl_oop_t oop)
{
	if (HCL_OOP_IS_SMOOI(oop)) return 1;
	if (HCL_IS_BIGINT(hcl,oop))
	{
		hcl_oow_t sz;
		sz = HCL_OBJ_GET_SIZE(oop);
		HCL_ASSERT (hcl, sz >= 1);
		return ((hcl_oop_liword_t)oop)->slot[sz - 1] != 0;
	}

	return 0;
}

HCL_INLINE static int is_bigint (hcl_t* hcl, hcl_oop_t x)
{
	return HCL_IS_BIGINT(hcl, x);
}

HCL_INLINE int hcl_isint (hcl_t* hcl, hcl_oop_t x)
{
	return HCL_OOP_IS_SMOOI(x) || HCL_IS_BIGINT(hcl, x);
}

static HCL_INLINE int bigint_to_oow (hcl_t* hcl, hcl_oop_t num, hcl_oow_t* w)
{
	HCL_ASSERT (hcl, HCL_IS_BIGINT(hcl,num));

#if (HCL_LIW_BITS == HCL_OOW_BITS)
	HCL_ASSERT (hcl, HCL_OBJ_GET_SIZE(num) >= 1);
	if (HCL_OBJ_GET_SIZE(num) == 1)
	{
		*w = ((hcl_oop_word_t)num)->slot[0];
		return HCL_IS_NBIGINT(hcl,num)? -1: 1;
	}

#elif (HCL_LIW_BITS == HCL_OOHW_BITS)
	/* this function must be called with a real large integer.
	 * a real large integer is at least 2 half-word long.
	 * you must not call this function with an unnormalized
	 * large integer. */

	HCL_ASSERT (hcl, HCL_OBJ_GET_SIZE(num) >= 2);
	if (HCL_OBJ_GET_SIZE(num) == 2)
	{
		*w = MAKE_WORD (((hcl_oop_halfword_t)num)->slot[0], ((hcl_oop_halfword_t)num)->slot[1]);
		return HCL_IS_NBIGINT(hcl,num)? -1: 1;
	}
#else
#	error UNSUPPORTED LIW BIT SIZE
#endif

	hcl_seterrnum (hcl, HCL_ERANGE);
	return 0; /* not convertable */
}

static HCL_INLINE int integer_to_oow (hcl_t* hcl, hcl_oop_t x, hcl_oow_t* w)
{
	/* return value 
	 *   1 - a positive number including 0 that can fit into hcl_oow_t
	 *  -1 - a negative number whose absolute value can fit into hcl_oow_t
	 *   0 - number too large or too small
	 */

	if (HCL_OOP_IS_SMOOI(x))
	{
		hcl_ooi_t v;

		v = HCL_OOP_TO_SMOOI(x);
		if (v < 0)
		{
			*w = -v;
			return -1;
		}
		else
		{
			*w = v;
			return 1;
		}
	}

	HCL_ASSERT (hcl, is_bigint(hcl, x));
	return bigint_to_oow(hcl, x, w);
}

int hcl_inttooow (hcl_t* hcl, hcl_oop_t x, hcl_oow_t* w)
{
	if (HCL_OOP_IS_SMOOI(x))
	{
		hcl_ooi_t v;

		v = HCL_OOP_TO_SMOOI(x);
		if (v < 0)
		{
			*w = -v;
			return -1; /* negative number negated - kind of an error */
		}
		else
		{
			*w = v;
			return 1; /* zero or positive number */
		}
	}

	if (is_bigint(hcl, x)) return bigint_to_oow (hcl, x, w);

	hcl_seterrbfmt (hcl, HCL_EINVAL, "parameter not integer - %O", x);
	return 0; /* not convertable - too big, too small, or not integer */
}

int hcl_inttoooi (hcl_t* hcl, hcl_oop_t x, hcl_ooi_t* i)
{
	hcl_oow_t w;
	int n;

	n = hcl_inttooow(hcl, x, &w);
	if (n < 0) 
	{
		HCL_ASSERT (hcl, HCL_TYPE_MAX(hcl_ooi_t) + HCL_TYPE_MIN(hcl_ooi_t) == -1); /* assume 2's complement */
		if (w > (hcl_oow_t)HCL_TYPE_MAX(hcl_ooi_t) + 1)
		{
			hcl_seterrnum (hcl, HCL_ERANGE); /* not convertable. number too small */
			return 0;
		}
		*i = -w;
	}
	else if (n > 0) 
	{
		if (w > HCL_TYPE_MAX(hcl_ooi_t)) 
		{
			hcl_seterrnum (hcl, HCL_ERANGE); /* not convertable. number too big */
			return 0;
		}
		*i = w;
	}

	return n;
}

static HCL_INLINE hcl_oop_t make_bigint_with_oow (hcl_t* hcl, hcl_oow_t w)
{
#if (HCL_LIW_BITS == HCL_OOW_BITS)
	HCL_ASSERT (hcl, HCL_SIZEOF(hcl_oow_t) == HCL_SIZEOF(hcl_liw_t));
	return hcl_makebigint(hcl, HCL_BRAND_PBIGINT, &w, 1);
#elif (HCL_LIW_BITS == HCL_OOHW_BITS)
	hcl_liw_t hw[2];
	hw[0] = w /*& HCL_LBMASK(hcl_oow_t,HCL_LIW_BITS)*/;
	hw[1] = w >> HCL_LIW_BITS;
	return hcl_makebigint(hcl, HCL_BRAND_PBIGINT,  &hw, (hw[1] > 0? 2: 1));
#else
#	error UNSUPPORTED LIW BIT SIZE
#endif
}

static HCL_INLINE hcl_oop_t make_bigint_with_ooi (hcl_t* hcl, hcl_ooi_t i)
{
#if (HCL_LIW_BITS == HCL_OOW_BITS)
	hcl_oow_t w;

	HCL_ASSERT (hcl, HCL_SIZEOF(hcl_oow_t) == HCL_SIZEOF(hcl_liw_t));
	if (i >= 0)
	{
		w = i;
		return hcl_makebigint(hcl, HCL_BRAND_PBIGINT, &w, 1);
	}
	else
	{
		/* The caller must ensure that i is greater than the smallest value
		 * that hcl_ooi_t can represent. otherwise, the absolute value 
		 * cannot be held in hcl_ooi_t. */
		HCL_ASSERT (hcl, i > HCL_TYPE_MIN(hcl_ooi_t));
		w = -i;
		return hcl_makebigint(hcl, HCL_BRAND_NBIGINT, &w, 1);
	}
#elif (HCL_LIW_BITS == HCL_OOHW_BITS)
	hcl_liw_t hw[2];
	hcl_oow_t w;

	if (i >= 0)
	{
		w = i;
		hw[0] = w /*& HCL_LBMASK(hcl_oow_t,HCL_LIW_BITS)*/;
		hw[1] = w >> HCL_LIW_BITS;
		return hcl_makebigint(hcl, HCL_BRAND_PBIGINT, &hw, (hw[1] > 0? 2: 1));
	}
	else
	{
		HCL_ASSERT (hcl, i > HCL_TYPE_MIN(hcl_ooi_t));
		w = -i;
		hw[0] = w /*& HCL_LBMASK(hcl_oow_t,HCL_LIW_BITS)*/;
		hw[1] = w >> HCL_LIW_BITS;
		return hcl_makebigint(hcl, HCL_BRAND_NBIGINT, &hw, (hw[1] > 0? 2: 1));
	}
#else
#	error UNSUPPORTED LIW BIT SIZE
#endif
}

static HCL_INLINE hcl_oop_t make_bloated_bigint_with_ooi (hcl_t* hcl, hcl_ooi_t i, hcl_oow_t extra)
{
#if (HCL_LIW_BITS == HCL_OOW_BITS)
	hcl_oow_t w;
	hcl_oop_t z;

	HCL_ASSERT (hcl, extra <= HCL_OBJ_SIZE_MAX - 1); 
	HCL_ASSERT (hcl, HCL_SIZEOF(hcl_oow_t) == HCL_SIZEOF(hcl_liw_t));
	if (i >= 0)
	{
		w = i;
		z = hcl_makebigint(hcl, HCL_BRAND_PBIGINT, HCL_NULL, 1 + extra);
	}
	else
	{
		HCL_ASSERT (hcl, i > HCL_TYPE_MIN(hcl_ooi_t));
		w = -i;
		z = hcl_makebigint(hcl, HCL_BRAND_NBIGINT, HCL_NULL, 1 + extra);
	}

	if (!z) return HCL_NULL;
	((hcl_oop_liword_t)z)->slot[0] = w;
	return z;

#elif (HCL_LIW_BITS == HCL_OOHW_BITS)
	hcl_liw_t hw[2];
	hcl_oow_t w;
	hcl_oop_t z;

	HCL_ASSERT (hcl, extra <= HCL_OBJ_SIZE_MAX - 2); 
	if (i >= 0)
	{
		w = i;
		hw[0] = w /*& HCL_LBMASK(hcl_oow_t,HCL_LIW_BITS)*/;
		hw[1] = w >> HCL_LIW_BITS;
		z = hcl_makebigint(hcl, HCL_BRAND_PBIGINT, HCL_NULL, (hw[1] > 0? 2: 1) + extra);
	}
	else
	{
		HCL_ASSERT (hcl, i > HCL_TYPE_MIN(hcl_ooi_t));
		w = -i;
		hw[0] = w /*& HCL_LBMASK(hcl_oow_t,HCL_LIW_BITS)*/;
		hw[1] = w >> HCL_LIW_BITS;
		z = hcl_makebigint(hcl, HCL_BRAND_NBIGINT, HCL_NULL, (hw[1] > 0? 2: 1) + extra);
	}

	if (!z) return HCL_NULL;
	((hcl_oop_liword_t)z)->slot[0] = hw[0];
	if (hw[1] > 0) ((hcl_oop_liword_t)z)->slot[1] = hw[1];
	return z;
#else
#	error UNSUPPORTED LIW BIT SIZE
#endif
}

static HCL_INLINE hcl_oop_t make_bigint_with_intmax (hcl_t* hcl, hcl_intmax_t v)
{
	hcl_oow_t len;
	hcl_liw_t buf[HCL_SIZEOF_INTMAX_T / HCL_SIZEOF_LIW_T];
	hcl_uintmax_t ui;

	/* this is not a generic function. it can't handle v 
	 * if it's HCL_TYPE_MIN(hcl_intmax_t) */
	HCL_ASSERT (hcl, v > HCL_TYPE_MIN(hcl_intmax_t));

	ui = (v >= 0)? v: -v;
	len = 0;
	do
	{
		buf[len++] = (hcl_liw_t)ui;
		ui = ui >> HCL_LIW_BITS;
	}
	while (ui > 0);

	return hcl_makebigint(hcl, ((v >= 0)? HCL_BRAND_PBIGINT: HCL_BRAND_NBIGINT), buf, len);
}

hcl_oop_t hcl_oowtoint (hcl_t* hcl, hcl_oow_t w)
{
	HCL_ASSERT (hcl, HCL_TYPE_IS_UNSIGNED(hcl_oow_t));
	/*if (HCL_IN_SMOOI_RANGE(w))*/
	if (w <= HCL_SMOOI_MAX)
	{
		return HCL_SMOOI_TO_OOP(w);
	}
	else
	{
		return make_bigint_with_oow(hcl, w);
	}
}

hcl_oop_t hcl_ooitoint (hcl_t* hcl, hcl_ooi_t i)
{
	if (HCL_IN_SMOOI_RANGE(i))
	{
		return HCL_SMOOI_TO_OOP(i);
	}
	else
	{
		return make_bigint_with_ooi (hcl, i);
	}
}

static HCL_INLINE hcl_oop_t expand_bigint (hcl_t* hcl, hcl_oop_t oop, hcl_oow_t inc)
{
	hcl_oop_t z;
	hcl_oow_t i;
	hcl_oow_t count;

	HCL_ASSERT (hcl, HCL_OOP_IS_POINTER(oop));
	count = HCL_OBJ_GET_SIZE(oop);

	if (inc > HCL_OBJ_SIZE_MAX - count)
	{
		hcl_seterrnum (hcl, HCL_EOOMEM); /* TODO: is it a soft failure or a hard failure? is this error code proper? */
		return HCL_NULL;
	}

	hcl_pushtmp (hcl, &oop);
	z = hcl_makebigint(hcl, HCL_OBJ_GET_FLAGS_BRAND(oop), HCL_NULL, count + inc);
	hcl_poptmp (hcl);
	if (!z) return HCL_NULL;

	for (i = 0; i < count; i++)
	{
		((hcl_oop_liword_t)z)->slot[i] = ((hcl_oop_liword_t)oop)->slot[i];
	}
	return z;
}

static HCL_INLINE hcl_oop_t _clone_bigint (hcl_t* hcl, hcl_oop_t oop, hcl_oow_t count, int brand)
{
	hcl_oop_t z;
	hcl_oow_t i;

	HCL_ASSERT (hcl, HCL_OOP_IS_POINTER(oop));
	if (count <= 0) count = HCL_OBJ_GET_SIZE(oop);

	hcl_pushtmp (hcl, &oop);
	z = hcl_makebigint(hcl, brand, HCL_NULL, count);
	hcl_poptmp (hcl);
	if (!z) return HCL_NULL;

	for (i = 0; i < count; i++)
	{
		((hcl_oop_liword_t)z)->slot[i] = ((hcl_oop_liword_t)oop)->slot[i];
	}
	return z;
}

static HCL_INLINE hcl_oop_t clone_bigint (hcl_t* hcl, hcl_oop_t oop, hcl_oow_t count)
{
	return _clone_bigint (hcl, oop, count, HCL_OBJ_GET_FLAGS_BRAND(oop));
}

static HCL_INLINE hcl_oop_t clone_bigint_negated (hcl_t* hcl, hcl_oop_t oop, hcl_oow_t count)
{
	int brand;

	HCL_ASSERT (hcl, HCL_IS_BIGINT(hcl,oop));
	
	if (HCL_IS_PBIGINT(hcl, oop))
	{
		brand = HCL_BRAND_NBIGINT;
	}
	else
	{
		HCL_ASSERT (hcl, HCL_IS_NBIGINT(hcl, oop));
		brand = HCL_BRAND_PBIGINT;
	}

	return _clone_bigint (hcl, oop, count, brand);
}

static HCL_INLINE hcl_oop_t clone_bigint_to_positive (hcl_t* hcl, hcl_oop_t oop, hcl_oow_t count)
{
	return _clone_bigint (hcl, oop, count, HCL_BRAND_PBIGINT);
}

static HCL_INLINE hcl_oow_t count_effective (hcl_liw_t* x, hcl_oow_t xs)
{
#if 0
	while (xs > 1 && x[xs - 1] == 0) xs--;
	return xs;
#else
	while (xs > 1) { if (x[--xs]) return xs + 1; }
	return 1;
#endif
}

static HCL_INLINE hcl_oow_t count_effective_digits (hcl_oop_t oop)
{
	hcl_oow_t i;

	for (i = HCL_OBJ_GET_SIZE(oop); i > 1; )
	{
		--i;
		if (((hcl_oop_liword_t)oop)->slot[i]) return i + 1;
	}

	return 1;
}

static hcl_oop_t normalize_bigint (hcl_t* hcl, hcl_oop_t oop)
{
	hcl_oow_t count;

	HCL_ASSERT (hcl, HCL_OOP_IS_POINTER(oop));
	count = count_effective_digits (oop);

#if (HCL_LIW_BITS == HCL_OOW_BITS)
	if (count == 1) /* 1 word */
	{
		hcl_oow_t w;

		w = ((hcl_oop_liword_t)oop)->slot[0];
		if (HCL_IS_PBIGINT(hcl, oop))
		{
			if (w <= HCL_SMOOI_MAX) return HCL_SMOOI_TO_OOP(w);
		}
		else
		{
			HCL_ASSERT (hcl, -HCL_SMOOI_MAX  == HCL_SMOOI_MIN);
			HCL_ASSERT (hcl, HCL_IS_NBIGINT(hcl, oop));
			if (w <= HCL_SMOOI_MAX) return HCL_SMOOI_TO_OOP(-(hcl_ooi_t)w);
		}
	}
#elif (HCL_LIW_BITS == HCL_OOHW_BITS)

	if (count == 1) /* 1 half-word */
	{
		if (HCL_IS_PBIGINT(hcl, oop))
		{
			return HCL_SMOOI_TO_OOP(((hcl_oop_liword_t)oop)->slot[0]);
		}
		else
		{
			HCL_ASSERT (hcl, HCL_IS_NBIGINT(hcl, oop));
			return HCL_SMOOI_TO_OOP(-(hcl_ooi_t)((hcl_oop_liword_t)oop)->slot[0]);
		}
	}
	else if (count == 2) /* 2 half-words */
	{
		hcl_oow_t w;

		w = MAKE_WORD(((hcl_oop_liword_t)oop)->slot[0], ((hcl_oop_liword_t)oop)->slot[1]);
		if (HCL_IS_PBIGINT(hcl, oop))
		{
			if (w <= HCL_SMOOI_MAX) return HCL_SMOOI_TO_OOP(w);
		}
		else
		{
			HCL_ASSERT (hcl, -HCL_SMOOI_MAX  == HCL_SMOOI_MIN);
			HCL_ASSERT (hcl, HCL_IS_NBIGINT(hcl, oop));
			if (w <= HCL_SMOOI_MAX) return HCL_SMOOI_TO_OOP(-(hcl_ooi_t)w);
		}
	}
#else
#	error UNSUPPORTED LIW BIT SIZE
#endif
	if (HCL_OBJ_GET_SIZE(oop) == count)
	{
		/* no compaction is needed. return it as it is */
		return oop;
	}

	return clone_bigint (hcl, oop, count);
}

static HCL_INLINE int is_less_unsigned_array (const hcl_liw_t* x, hcl_oow_t xs, const hcl_liw_t* y, hcl_oow_t ys)
{
	hcl_oow_t i;

	if (xs != ys) return xs < ys;
	for (i = xs; i > 0; )
	{
		--i;
		if (x[i] != y[i]) return x[i] < y[i];
	}

	return 0;
}

static HCL_INLINE int is_less_unsigned (hcl_oop_t x, hcl_oop_t y)
{
	return is_less_unsigned_array (
		((hcl_oop_liword_t)x)->slot, HCL_OBJ_GET_SIZE(x), 
		((hcl_oop_liword_t)y)->slot, HCL_OBJ_GET_SIZE(y));
}

static HCL_INLINE int is_less (hcl_t* hcl, hcl_oop_t x, hcl_oop_t y)
{
	if (HCL_OBJ_GET_FLAGS_BRAND(x) != HCL_OBJ_GET_FLAGS_BRAND(y))
	{
		return HCL_IS_NBIGINT(hcl, x);
	}

	if (HCL_IS_PBIGINT(hcl, x))
	{
		return is_less_unsigned (x, y);
	}
	else
	{
		return is_less_unsigned (y, x);
	}
}

static HCL_INLINE int is_greater_unsigned_array (const hcl_liw_t* x, hcl_oow_t xs, const hcl_liw_t* y, hcl_oow_t ys)
{
	hcl_oow_t i;

	if (xs != ys) return xs > ys;
	for (i = xs; i > 0; )
	{
		--i;
		if (x[i] != y[i]) return x[i] > y[i];
	}

	return 0;
}

static HCL_INLINE int is_greater_unsigned (hcl_oop_t x, hcl_oop_t y)
{
	return is_greater_unsigned_array (
		((hcl_oop_liword_t)x)->slot, HCL_OBJ_GET_SIZE(x), 
		((hcl_oop_liword_t)y)->slot, HCL_OBJ_GET_SIZE(y));
}

static HCL_INLINE int is_greater (hcl_t* hcl, hcl_oop_t x, hcl_oop_t y)
{
	if (HCL_OBJ_GET_FLAGS_BRAND(x) != HCL_OBJ_GET_FLAGS_BRAND(y))
	{
		return HCL_IS_NBIGINT(hcl, y);
	}

	if (HCL_IS_PBIGINT(hcl, x))
	{
		return is_greater_unsigned (x, y);
	}
	else
	{
		return is_greater_unsigned (y, x);
	}
}

static HCL_INLINE int is_equal (hcl_t* hcl, hcl_oop_t x, hcl_oop_t y)
{
	/* check if two large integers are equal to each other */
	return HCL_OBJ_GET_FLAGS_BRAND(x) == HCL_OBJ_GET_FLAGS_BRAND(y) && HCL_OBJ_GET_SIZE(x) == HCL_OBJ_GET_SIZE(y) &&
	       HCL_MEMCMP(((hcl_oop_liword_t)x)->slot,  ((hcl_oop_liword_t)y)->slot, HCL_OBJ_GET_SIZE(x) * HCL_SIZEOF(hcl_liw_t)) == 0;
}

static void complement2_unsigned_array (hcl_t* hcl, const hcl_liw_t* x, hcl_oow_t xs, hcl_liw_t* z)
{
	hcl_oow_t i;
	hcl_lidw_t w;
	hcl_lidw_t carry;

	/* get 2's complement (~x + 1) */

	carry = 1; 
	for (i = 0; i < xs; i++)
	{
		w = (hcl_lidw_t)(~x[i]) + carry;
		/*w = (hcl_lidw_t)(x[i] ^ (~(hcl_liw_t)0)) + carry;*/
		carry = w >> HCL_LIW_BITS;
		z[i] = w;
	}

	/* if the array pointed to by x contains all zeros, carry will be
	 * 1 here and it actually requires 1 more slot. Let't take this 8-bit
	 * zero for instance:
	 *    2r00000000  -> 2r11111111 + 1 => 2r0000000100000000
	 * 
	 * this function is not designed to handle such a case.
	 * in fact, 0 is a small integer and it must not stand a change 
	 * to be given to this function */
	HCL_ASSERT (hcl, carry == 0);
}

static HCL_INLINE hcl_oow_t add_unsigned_array (const hcl_liw_t* x, hcl_oow_t xs, const hcl_liw_t* y, hcl_oow_t ys, hcl_liw_t* z)
{
#if 1
	register hcl_oow_t i;
	hcl_lidw_t w;

	if (xs < ys)
	{
		/* swap x and y */
		i = xs;
		xs = ys;
		ys = i;

		i = (hcl_oow_t)x;
		x = y;
		y = (hcl_liw_t*)i;
	}

	w = 0;
	i = 0;
	while (i < ys)
	{
		w += (hcl_lidw_t)x[i] + (hcl_lidw_t)y[i];
		z[i++] = w & HCL_LBMASK(hcl_lidw_t, HCL_LIW_BITS);
		w >>= HCL_LIW_BITS;
	}

	while (w && i < xs)
	{
		w += x[i];
		z[i++] = w & HCL_LBMASK(hcl_lidw_t, HCL_LIW_BITS);
		w >>= HCL_LIW_BITS;
	}

	while (i < xs)
	{
		z[i] = x[i];
		i++;
	}
	if (w) z[i++] = w & HCL_LBMASK(hcl_lidw_t, HCL_LIW_BITS);
	return i;

#else
	register hcl_oow_t i;
	hcl_lidw_t w;
	hcl_liw_t carry = 0;

	if (xs < ys)
	{
		/* swap x and y */
		i = xs;
		xs = ys;
		ys = i;

		i = (hcl_oow_t)x;
		x = y;
		y = (hcl_liw_t*)i;
	}


	for (i = 0; i < ys; i++)
	{
		w = (hcl_lidw_t)x[i] + (hcl_lidw_t)y[i] + carry;
		carry = w >> HCL_LIW_BITS;
		z[i] = w /*& HCL_LBMASK(hcl_lidw_t, HCL_LIW_BITS) */;
	}

	if (x == z)
	{
		for (; carry && i < xs; i++)
		{
			w = (hcl_lidw_t)x[i] + carry;
			carry = w >> HCL_LIW_BITS;
			z[i] = w /*& HCL_LBMASK(hcl_lidw_t, HCL_LIW_BITS) */;
		}
		i = xs;
	}
	else
	{
		for (; i < xs; i++)
		{
			w = (hcl_lidw_t)x[i] + carry;
			carry = w >> HCL_LIW_BITS;
			z[i] = w /*& HCL_LBMASK(hcl_lidw_t, HCL_LIW_BITS)*/;
		}
	}

	if (carry) z[i++] = carry;
	return i; /* the number of effective digits in the result */
#endif
}

static HCL_INLINE hcl_oow_t subtract_unsigned_array (hcl_t* hcl, const hcl_liw_t* x, hcl_oow_t xs, const hcl_liw_t* y, hcl_oow_t ys, hcl_liw_t* z)
{
#if 1
	hcl_oow_t i;
	hcl_lidi_t w = 0;

	if (x == y)
	{
		HCL_ASSERT (hcl, xs == ys);
		z[0] = 0;
		return 1;
	}

	HCL_ASSERT (hcl, !is_less_unsigned_array(x, xs, y, ys));

	for (i = 0; i < ys; i++)
	{
		w += (hcl_lidi_t)x[i] - (hcl_lidi_t)y[i];
		z[i] = w & HCL_LBMASK(hcl_lidw_t, HCL_LIW_BITS);
		w >>= HCL_LIW_BITS;
	}

	while (w && i < xs)
	{
		w += x[i];
		z[i++] = w & HCL_LBMASK(hcl_lidw_t, HCL_LIW_BITS);
		w >>= HCL_LIW_BITS;
	}

	while (i < xs)
	{
		z[i] = x[i];
		i++;
	}

	while (i > 1 && z[i - 1] == 0) i--;
	return i;

#else
	hcl_oow_t i;
	hcl_lidw_t w;
	hcl_lidw_t borrow = 0;
	hcl_lidw_t borrowed_word;

	if (x == y)
	{
		HCL_ASSERT (hcl, xs == ys);
		z[0] = 0;
		return 1;
	}

	HCL_ASSERT (hcl, !is_less_unsigned_array(x, xs, y, ys));

	borrowed_word = (hcl_lidw_t)1 << HCL_LIW_BITS;
	for (i = 0; i < ys; i++)
	{
		w = (hcl_lidw_t)y[i] + borrow;
		if ((hcl_lidw_t)x[i] >= w)
		{
			z[i] = x[i] - w;
			borrow = 0;
		}
		else
		{
			z[i] = (borrowed_word + (hcl_lidw_t)x[i]) - w; 
			borrow = 1;
		}
	}

	for (; i < xs; i++)
	{
		if (x[i] >= borrow) 
		{
			z[i] = x[i] - (hcl_liw_t)borrow;
			borrow = 0;
		}
		else
		{
			z[i] = (borrowed_word + (hcl_lidw_t)x[i]) - borrow;
			borrow = 1;
		}
	}

	HCL_ASSERT (hcl, borrow == 0);

	while (i > 1 && z[i - 1] == 0) i--;
	return i; /* the number of effective digits in the result */
#endif
}

static HCL_INLINE void multiply_unsigned_array (const hcl_liw_t* x, hcl_oow_t xs, const hcl_liw_t* y, hcl_oow_t ys, hcl_liw_t* z)
{
	hcl_lidw_t v;
	hcl_oow_t pa;

	if (xs < ys)
	{
		hcl_oow_t i;

		/* swap x and y */
		i = xs;
		xs = ys;
		ys = i;

		i = (hcl_oow_t)x;
		x = y;
		y = (hcl_liw_t*)i;
	}

	pa = xs;
	if (pa <= ((hcl_oow_t)1 << (HCL_LIDW_BITS - (HCL_LIW_BITS * 2))))
	{
		/* Comba(column-array) multiplication */

		/* when the input length is too long, v may overflow. if it
		 * happens, comba's method doesn't work as carry propagation is
		 * affected badly. so we need to use this method only if
		 * the input is short enough. */

		hcl_oow_t pa, ix, iy, iz, tx, ty;

		pa = xs + ys;
		v = 0;
		for (ix = 0; ix < pa; ix++)
		{
			ty = (ix < ys - 1)? ix: (ys - 1);
			tx = ix - ty;
			iy = (ty + 1 < xs - tx)? (ty + 1): (xs - tx);

			for (iz = 0; iz < iy; iz++)
			{
				v = v + (hcl_lidw_t)x[tx + iz] * (hcl_lidw_t)y[ty - iz];
			}

			z[ix] = (hcl_liw_t)v;
			v = v >> HCL_LIW_BITS;
		}
	}
	else
	{
		hcl_oow_t i, j;
		hcl_liw_t carry;

		for (i = 0; i < xs; i++)
		{
			if (x[i] == 0)
			{
				z[i + ys] = 0;
			}
			else
			{
				carry = 0;

				for (j = 0; j < ys; j++)
				{
					v = (hcl_lidw_t)x[i] * (hcl_lidw_t)y[j] + (hcl_lidw_t)carry + (hcl_lidw_t)z[i + j];
					z[i + j] = (hcl_liw_t)v;
					carry = (hcl_liw_t)(v >> HCL_LIW_BITS);
				}

				z[i + j] = carry;
			}
		}
	}
}

/* KARATSUBA MULTIPLICATION
 * 
 * c = |a| * |b|
 *
 * Let B represent the radix(2^DIGIT_BITS)
 * Let n represent half the number of digits
 *
 * a = a1 * B^n + a0
 * b = b1 * B^n + b0
 * a * b => a1b1 * B^2n + ((a1 + a0)(b1 + b0) - (a0b0 + a1b1)) * B^n + a0b0
 *
 * --------------------------------------------------------------------
 * For example, for 2 number 0xFAC2 and 0xABCD => A848635A
 *   DIGIT_BITS = 8 (1 byte, each digit is 1 byte long)
 *   B = 2^8 = 0x100
 *   n = 1 (half the digits of 2 digit numbers)
 *   B^n = 0x100 ^ 1 = 0x100
 *   B^2n = 0x100 ^ 2 = 0x10000
 *   0xFAC2 = 0xFA * 0x100 + 0xC2
 *   0xABCD = 0xAB * 0x100 + 0xCD
 *   a1 = 0xFA, a0 = 0xC2
 *   b1 = 0xAB, b0 = 0xCD
 *   a1b1 = 0xFA * 0xAB = 0xA6FE
 *   a0b0 = 0xC2 * 0xCD = 0x9B5A
 *   a1 + a0 = 0xFA + 0xC2 = 0x1BC
 *   b1 + b0 = 0xAB + 0xCD = 0x178
 * --------------------------------------------------------------------
 *   (A6FE * 10000) + (((1BC * 178) - (985A + A6FE)) * 100) + 9B5A =
 *   (A6FE << (8 * 2)) + (((1BC * 178) - (985A + A6FE)) << (8 * 1)) =
 *   A6FE0000 + 14CC800 + 9B5A = 9848635A 
 * --------------------------------------------------------------------
 *
 * 0xABCD9876 * 0xEFEFABAB => 0xA105C97C9755A8D2
 * B = 2^8 = 0x100
 * n = 2
 * B^n = 0x100 ^ 2 = 0x10000
 * B^2n = 0x100 ^ 4 = 0x100000000
 * 0xABCD9876 = 0xABCD * 0x10000 + 0x9876
 * 0xEFEFABAB = 0xEFEF * 0x10000 + 0xABAB
 * a1 = 0xABCD, a0 = 0x9876
 * b1 - 0xEFEF, b0 = 0xABAB
 * a1b1 = 0xA104C763
 * a0b0 = 0x663CA8D2
 * a1 + a0 = 0x14443
 * b1 + b0 = 0x19B9A
 * --------------------------------------------------------------------
 * (A104C763 * 100000000) + (((14443 * 19B9A) - (663CA8D2 + A104C763)) * 10000) + 663CA8D2 =
 * (A104C763 << (8 * 4)) + (((14443 * 19B9A) - (663CA8D2 + A104C763)) << (8 * 2)) + 663CA8D2 = A105C97C9755A8D2
 * --------------------------------------------------------------------
 *
 *  Multiplying by B is t same as shifting by DIGIT_BITS.
 *  DIGIT_BITS in this implementation is HCL_LIW_BITS
 *  B => 2^HCL_LIW_BITS
 *  X * B^n => X << (HCL_LIW_BITS * n)
 *  X * B^2n => X << (HCL_LIW_BITS * n * 2)
 * --------------------------------------------------------------------
 */
 
#if defined(HCL_BUILD_DEBUG)
#define CANNOT_KARATSUBA(hcl,xs,ys) \
	((xs) < (hcl)->option.karatsuba_cutoff || (ys) < (hcl)->option.karatsuba_cutoff || \
	((xs) > (ys) && (ys) <= (((xs) + 1) / 2)) || \
	((xs) < (ys) && (xs) <= (((ys) + 1) / 2)))
#else
#define CANNOT_KARATSUBA(hcl,xs,ys) \
	((xs) < HCL_KARATSUBA_CUTOFF || (ys) < HCL_KARATSUBA_CUTOFF || \
	((xs) > (ys) && (ys) <= (((xs) + 1) / 2)) || \
	((xs) < (ys) && (xs) <= (((ys) + 1) / 2)))
#endif

static HCL_INLINE hcl_oow_t multiply_unsigned_array_karatsuba (hcl_t* hcl, const hcl_liw_t* x, hcl_oow_t xs, const hcl_liw_t* y, hcl_oow_t ys, hcl_liw_t* z)
{
#if 1
	hcl_lidw_t nshifts;
	hcl_lidw_t ndigits_xh, ndigits_xl;
	hcl_lidw_t ndigits_yh, ndigits_yl;
	hcl_liw_t* tmp[2] = { HCL_NULL, HCL_NULL};
	hcl_liw_t* zsp; 
	hcl_oow_t tmplen[2];
	hcl_oow_t xlen, zcapa;

	zcapa = xs + ys; /* the caller ensures this capacity for z at the minimum*/

	if (xs < ys)
	{
		hcl_oow_t i;

		/* swap x and y */
		i = xs;
		xs = ys;
		ys = i;

		i = (hcl_oow_t)x;
		x = y;
		y = (hcl_liw_t*)i;
	}

	/* calculate value of nshifts, that is 2^(HCL_LIW_BITS*nshifts) */
	nshifts = (xs + 1) / 2;

	ndigits_xl = nshifts; /* ndigits of lower part of x */
	ndigits_xh = xs - nshifts; /* ndigits of upper part of x */
	ndigits_yl = nshifts; /* ndigits of lower part of y */
	ndigits_yh = ys - nshifts; /* ndigits of uppoer part of y */

	HCL_ASSERT (hcl, ndigits_xl >= ndigits_xh);
	HCL_ASSERT (hcl, ndigits_yl >= ndigits_yh);

	/* make a temporary buffer for (b0 + b1) and (a1 * b1) */
	tmplen[0] = ndigits_xh + ndigits_yh;
	tmplen[1] = ndigits_yl + ndigits_yh + 1; 
	if (tmplen[1] < tmplen[0]) tmplen[1] = tmplen[0];
	tmp[1] = (hcl_liw_t*)hcl_callocmem(hcl, HCL_SIZEOF(hcl_liw_t) * tmplen[1]); /* TODO: should i use the object memory? */
	if (!tmp[1]) goto oops;

	/* make a temporary for (a0 + a1) and (a0 * b0) */
	tmplen[0] = ndigits_xl + ndigits_yl + 1;
	tmp[0] = (hcl_liw_t*)hcl_callocmem(hcl, HCL_SIZEOF(hcl_liw_t) * tmplen[0]);
	if (!tmp[0]) goto oops;

	/* tmp[0] = a0 + a1 */
	tmplen[0] = add_unsigned_array (x, ndigits_xl, x + nshifts, ndigits_xh, tmp[0]);

	/* tmp[1] = b0 + b1 */
	tmplen[1] = add_unsigned_array (y, ndigits_yl, y + nshifts, ndigits_yh, tmp[1]);

	/*HCL_DEBUG6 (hcl, "karatsuba t %p u %p ndigits_xl %d ndigits_xh %d ndigits_yl %d ndigits_yh %d\n", tmp[0], tmp[1], (int)ndigits_xl, (int)ndigits_xh, (int)ndigits_yl, (int)ndigits_yh);*/
	/*HCL_DEBUG5 (hcl, "zcapa %d, tmplen[0] %d tmplen[1] %d nshifts %d total %d\n", (int)zcapa, (int)tmplen[0], (int)tmplen[1], (int)nshifts, (int)(tmplen[0] + tmplen[1] + nshifts));*/

	/* place (a0 + a1) * (b0 + b1) at the shifted position */
	zsp = z + nshifts;
	if (CANNOT_KARATSUBA(hcl, tmplen[0], tmplen[1]))
	{
		multiply_unsigned_array (tmp[0], tmplen[0], tmp[1], tmplen[1], zsp);
		xlen = count_effective (zsp, tmplen[0] + tmplen[1]);
	}
	else 
	{
		xlen = multiply_unsigned_array_karatsuba(hcl, tmp[0], tmplen[0], tmp[1], tmplen[1], zsp);
		if (xlen == 0) goto oops;
	}

	/* tmp[0] = a0 * b0 */
	tmplen[0] = ndigits_xl + ndigits_yl;
	HCL_MEMSET (tmp[0], 0, sizeof(hcl_liw_t) * tmplen[0]);
	if (CANNOT_KARATSUBA(hcl, ndigits_xl, ndigits_yl))
	{
		multiply_unsigned_array (x, ndigits_xl, y, ndigits_yl, tmp[0]);
		tmplen[0] = count_effective(tmp[0], tmplen[0]);
	}
	else
	{
		tmplen[0] = multiply_unsigned_array_karatsuba (hcl, x, ndigits_xl, y, ndigits_yl, tmp[0]);
		if (tmplen[0] <= 0) goto oops;
	}

	/* tmp[1] = a1 * b1 */
	tmplen[1] = ndigits_xh + ndigits_yh;
	HCL_MEMSET (tmp[1], 0, sizeof(hcl_liw_t) * tmplen[1]);
	if (CANNOT_KARATSUBA(hcl, ndigits_xh, ndigits_yh))
	{
		multiply_unsigned_array (x + nshifts, ndigits_xh, y + nshifts, ndigits_yh, tmp[1]);
		tmplen[1] = count_effective (tmp[1], tmplen[1]);
	}
	else
	{
		tmplen[1] = multiply_unsigned_array_karatsuba (hcl, x + nshifts, ndigits_xh, y + nshifts, ndigits_yh, tmp[1]);
		if (tmplen[1] <= 0) goto oops;
	}

	/* (a0+a1)*(b0+b1) -(a0*b0) */
	xlen = subtract_unsigned_array(hcl, zsp, xlen, tmp[0], tmplen[0], zsp);

	/* (a0+a1)*(b0+b1) - (a0*b0) - (a1*b1) */
	xlen = subtract_unsigned_array(hcl, zsp, xlen, tmp[1], tmplen[1], zsp);
	/* a1b1 is in tmp[1]. add (a1b1 * B^2n) to the high part of 'z' */
	zsp = z + (nshifts * 2); /* emulate shifting for "* B^2n". */
	xlen = zcapa - (nshifts * 2);
	xlen = add_unsigned_array (zsp, xlen, tmp[1], tmplen[1], zsp);

	/* z = z + a0b0. a0b0 is in tmp[0] */
	xlen = add_unsigned_array(z, zcapa, tmp[0], tmplen[0], z);

	hcl_freemem (hcl, tmp[1]);
	hcl_freemem (hcl, tmp[0]);
	return count_effective (z, xlen);

oops:
	if (tmp[1]) hcl_freemem (hcl, tmp[1]);
	if (tmp[0]) hcl_freemem (hcl, tmp[0]);
	return 0;

#else
	hcl_lidw_t nshifts;
	hcl_lidw_t ndigits_xh, ndigits_xl;
	hcl_lidw_t ndigits_yh, ndigits_yl;
	hcl_liw_t* tmp[3] = { HCL_NULL, HCL_NULL, HCL_NULL };
	hcl_liw_t* zsp; 
	hcl_oow_t tmplen[3];
	hcl_oow_t xlen, zcapa;

	zcapa = xs + ys; /* the caller ensures this capacity for z at the minimum*/

	if (xs < ys)
	{
		hcl_oow_t i;

		/* swap x and y */
		i = xs;
		xs = ys;
		ys = i;

		i = (hcl_oow_t)x;
		x = y;
		y = (hcl_liw_t*)i;
	}

	/* calculate value of nshifts, that is 2^(HCL_LIW_BITS*nshifts) */
	nshifts = (xs + 1) / 2;

	ndigits_xl = nshifts; /* ndigits of lower part of x */
	ndigits_xh = xs - nshifts; /* ndigits of upper part of x */
	ndigits_yl = nshifts; /* ndigits of lower part of y */
	ndigits_yh = ys - nshifts; /* ndigits of uppoer part of y */

	HCL_ASSERT (hcl, ndigits_xl >= ndigits_xh);
	HCL_ASSERT (hcl, ndigits_yl >= ndigits_yh);

	/* make a temporary buffer for (b0 + b1) and (a1 * b1) */
	tmplen[0] = ndigits_yl + ndigits_yh + 1; 
	tmplen[1] = ndigits_xh + ndigits_yh;
	if (tmplen[1] < tmplen[0]) tmplen[1] = tmplen[0];
	tmp[1] = hcl_callocmem (hcl, HCL_SIZEOF(hcl_liw_t) * tmplen[1]);
	if (!tmp[1]) goto oops;

	/* make a temporary for (a0 + a1) and (a0 * b0) */
	tmplen[0] = ndigits_xl + ndigits_yl;
	tmp[0] = hcl_callocmem (hcl, HCL_SIZEOF(hcl_liw_t) * tmplen[0]);
	if (!tmp[0]) goto oops;

	/* tmp[0] = a0 + a1 */
	tmplen[0] = add_unsigned_array (x, ndigits_xl, x + nshifts, ndigits_xh, tmp[0]);

	/* tmp[1] = b0 + b1 */
	tmplen[1] = add_unsigned_array (y, ndigits_yl, y + nshifts, ndigits_yh, tmp[1]);

	/* tmp[2] = (a0 + a1) * (b0 + b1) */
	tmplen[2] = tmplen[0] + tmplen[1]; 
	tmp[2] = hcl_callocmem (hcl, HCL_SIZEOF(hcl_liw_t) * tmplen[2]);
	if (!tmp[2]) goto oops;
	if (CANNOT_KARATSUBA(hcl, tmplen[0], tmplen[1]))
	{
		multiply_unsigned_array (tmp[0], tmplen[0], tmp[1], tmplen[1], tmp[2]);
		xlen = count_effective (tmp[2], tmplen[2]);
	}
	else 
	{
		xlen = multiply_unsigned_array_karatsuba(hcl, tmp[0], tmplen[0], tmp[1], tmplen[1], tmp[2]);
		if (xlen == 0) goto oops;
	}

	/* tmp[0] = a0 * b0 */
	tmplen[0] = ndigits_xl + ndigits_yl;
	HCL_MEMSET (tmp[0], 0, sizeof(hcl_liw_t) * tmplen[0]);
	if (CANNOT_KARATSUBA(hcl, ndigits_xl, ndigits_yl))
	{
		multiply_unsigned_array (x, ndigits_xl, y, ndigits_yl, tmp[0]);
		tmplen[0] = count_effective(tmp[0], tmplen[0]);
	}
	else
	{
		tmplen[0] = multiply_unsigned_array_karatsuba (hcl, x, ndigits_xl, y, ndigits_yl, tmp[0]);
		if (tmplen[0] <= 0) goto oops;
	}

	/* tmp[1] = a1 * b1 */
	tmplen[1] = ndigits_xh + ndigits_yh;
	HCL_MEMSET (tmp[1], 0, sizeof(hcl_liw_t) * tmplen[1]);
	if (CANNOT_KARATSUBA(hcl, ndigits_xh, ndigits_yh))
	{
		multiply_unsigned_array (x + nshifts, ndigits_xh, y + nshifts, ndigits_yh, tmp[1]);
		tmplen[1] = count_effective (tmp[1], tmplen[1]);
	}
	else
	{
		tmplen[1] = multiply_unsigned_array_karatsuba (hcl, x + nshifts, ndigits_xh, y + nshifts, ndigits_yh, tmp[1]);
		if (tmplen[1] <= 0) goto oops;
	}

	/* w = w - tmp[0] */
	xlen = subtract_unsigned_array(hcl, tmp[2], xlen, tmp[0], tmplen[0], tmp[2]);

	/* r = w - tmp[1] */
	zsp = z + nshifts; /* emulate shifting for "* B^n" */
	xlen = subtract_unsigned_array(hcl, tmp[2], xlen, tmp[1], tmplen[1], zsp);

	/* a1b1 is in tmp[1]. add (a1b1 * B^2n) to the high part of 'z' */
	zsp = z + (nshifts * 2); /* emulate shifting for "* B^2n". */
	xlen = zcapa - (nshifts * 2);
	xlen = add_unsigned_array (zsp, xlen, tmp[1], tmplen[1], zsp);

	/* z = z + a0b0. a0b0 is in tmp[0] */
	xlen = add_unsigned_array(z, zcapa, tmp[0], tmplen[0], z);

	hcl_freemem (hcl, tmp[2]);
	hcl_freemem (hcl, tmp[1]);
	hcl_freemem (hcl, tmp[0]);

	return count_effective (z, xlen);

oops:
	if (tmp[2]) hcl_freemem (hcl, tmp[2]);
	if (tmp[1]) hcl_freemem (hcl, tmp[1]);
	if (tmp[0]) hcl_freemem (hcl, tmp[0]);
	return 0;
#endif
}

static HCL_INLINE void lshift_unsigned_array (hcl_liw_t* x, hcl_oow_t xs, hcl_oow_t bits)
{
	/* this function doesn't grow/shrink the array. Shifting is performed
	 * over the given array */

	hcl_oow_t word_shifts, bit_shifts, bit_shifts_right;
	hcl_oow_t si, di;

	/* get how many words to shift */
	word_shifts = bits / HCL_LIW_BITS;
	if (word_shifts >= xs)
	{
		HCL_MEMSET (x, 0, xs * HCL_SIZEOF(hcl_liw_t));
		return;
	}

	/* get how many remaining bits to shift */
	bit_shifts = bits % HCL_LIW_BITS;
	bit_shifts_right = HCL_LIW_BITS - bit_shifts;

	/* shift words and bits */
	di = xs - 1;
	si = di - word_shifts;
	x[di] = x[si] << bit_shifts;
	while (di > word_shifts)
	{
		x[di] = x[di] | (x[--si] >> bit_shifts_right);
		x[--di] = x[si] << bit_shifts;
	}

	/* fill the remaining part with zeros */
	if (word_shifts > 0)
		HCL_MEMSET (x, 0, word_shifts * HCL_SIZEOF(hcl_liw_t));
}

static HCL_INLINE void rshift_unsigned_array (hcl_liw_t* x, hcl_oow_t xs, hcl_oow_t bits)
{
	/* this function doesn't grow/shrink the array. Shifting is performed
	 * over the given array */

	hcl_oow_t word_shifts, bit_shifts, bit_shifts_left;
	hcl_oow_t si, di, bound;

	/* get how many words to shift */
	word_shifts = bits / HCL_LIW_BITS;
	if (word_shifts >= xs)
	{
		HCL_MEMSET (x, 0, xs * HCL_SIZEOF(hcl_liw_t));
		return;
	}

	/* get how many remaining bits to shift */
	bit_shifts = bits % HCL_LIW_BITS;
	bit_shifts_left = HCL_LIW_BITS - bit_shifts;

/* TODO: verify this function */
	/* shift words and bits */
	di = 0;
	si = word_shifts;
	x[di] = x[si] >> bit_shifts;
	bound = xs - word_shifts - 1;
	while (di < bound)
	{
		x[di] = x[di] | (x[++si] << bit_shifts_left);
		x[++di] = x[si] >> bit_shifts;
	}

	/* fill the remaining part with zeros */
	if (word_shifts > 0)
		HCL_MEMSET (&x[xs - word_shifts], 0, word_shifts * HCL_SIZEOF(hcl_liw_t));
}

static void divide_unsigned_array (hcl_t* hcl, const hcl_liw_t* x, hcl_oow_t xs, const hcl_liw_t* y, hcl_oow_t ys, hcl_liw_t* q, hcl_liw_t* r)
{
/* TODO: this function needs to be rewritten for performance improvement. 
 *       the binary long division is extremely slow for a big number */

	/* Perform binary long division.
	 * http://en.wikipedia.org/wiki/Division_algorithm
	 * ---------------------------------------------------------------------
	 * Q := 0                 initialize quotient and remainder to zero
	 * R := 0                     
	 * for i = n-1...0 do     where n is number of bits in N
	 *   R := R << 1          left-shift R by 1 bit    
	 *   R(0) := X(i)         set the least-significant bit of R equal to bit i of the numerator
	 *   if R >= Y then
	 *     R = R - Y               
	 *     Q(i) := 1
	 *   end
	 * end 
	 */

	hcl_oow_t rs, i , j;

	HCL_ASSERT (hcl, xs >= ys);
	HCL_MEMSET (q, 0, HCL_SIZEOF(*q) * xs);
	HCL_MEMSET (r, 0, HCL_SIZEOF(*q) * xs);

	for (i = xs; i > 0; )
	{
		--i;
		for (j = HCL_LIW_BITS; j > 0;)
		{
			--j;

			lshift_unsigned_array (r, xs, 1);
			HCL_SETBITS (hcl_liw_t, r[0], 0, 1, HCL_GETBITS(hcl_liw_t, x[i], j, 1));

			rs = count_effective (r, xs);
			if (!is_less_unsigned_array (r, rs, y, ys))
			{
				subtract_unsigned_array (hcl, r, rs, y, ys, r);
				HCL_SETBITS (hcl_liw_t, q[i], j, 1, 1);
			}
		}
	}
}

static hcl_oop_t add_unsigned_integers (hcl_t* hcl, hcl_oop_t x, hcl_oop_t y)
{
	hcl_oow_t as, bs, zs;
	hcl_oop_t z;

	as = HCL_OBJ_GET_SIZE(x);
	bs = HCL_OBJ_GET_SIZE(y);
	zs = (as >= bs? as: bs);

	if (zs >= HCL_OBJ_SIZE_MAX)
	{
		hcl_seterrnum (hcl, HCL_EOOMEM); /* TOOD: is it a soft failure or hard failure? */
		return HCL_NULL;
	}
	zs++;

	hcl_pushtmp (hcl, &x);
	hcl_pushtmp (hcl, &y);
	z = hcl_makebigint(hcl, HCL_OBJ_GET_FLAGS_BRAND(x), HCL_NULL, zs);
	hcl_poptmps (hcl, 2);
	if (!z) return HCL_NULL;

	add_unsigned_array (
		((hcl_oop_liword_t)x)->slot, as,
		((hcl_oop_liword_t)y)->slot, bs,
		((hcl_oop_liword_t)z)->slot
	);

	return z;
}

static hcl_oop_t subtract_unsigned_integers (hcl_t* hcl, hcl_oop_t x, hcl_oop_t y)
{
	hcl_oop_t z;

	HCL_ASSERT (hcl, !is_less_unsigned(x, y));

	hcl_pushtmp (hcl, &x);
	hcl_pushtmp (hcl, &y);
	z = hcl_makebigint(hcl, HCL_BRAND_PBIGINT, HCL_NULL, HCL_OBJ_GET_SIZE(x));
	hcl_poptmps (hcl, 2);
	if (!z) return HCL_NULL;

	subtract_unsigned_array (hcl, 
		((hcl_oop_liword_t)x)->slot, HCL_OBJ_GET_SIZE(x),
		((hcl_oop_liword_t)y)->slot, HCL_OBJ_GET_SIZE(y),
		((hcl_oop_liword_t)z)->slot);
	return z;
}

static hcl_oop_t multiply_unsigned_integers (hcl_t* hcl, hcl_oop_t x, hcl_oop_t y)
{
	hcl_oop_t z;
	hcl_oow_t xs, ys;

	xs = HCL_OBJ_GET_SIZE(x);
	ys = HCL_OBJ_GET_SIZE(y);

	if (ys > HCL_OBJ_SIZE_MAX - xs)
	{
		hcl_seterrnum (hcl, HCL_EOOMEM); /* TOOD: is it a soft failure or hard failure? */
		return HCL_NULL;
	}

	hcl_pushtmp (hcl, &x);
	hcl_pushtmp (hcl, &y);
	z = hcl_makebigint(hcl, HCL_BRAND_PBIGINT, HCL_NULL, xs + ys);
	hcl_poptmps (hcl, 2);
	if (!z) return HCL_NULL;

#if defined(HCL_ENABLE_KARATSUBA)
	if (CANNOT_KARATSUBA(hcl,xs, ys))
	{
#endif
		multiply_unsigned_array (
			((hcl_oop_liword_t)x)->slot, HCL_OBJ_GET_SIZE(x),
			((hcl_oop_liword_t)y)->slot, HCL_OBJ_GET_SIZE(y),
			((hcl_oop_liword_t)z)->slot);
#if defined(HCL_ENABLE_KARATSUBA)
	}
	else
	{
		if (multiply_unsigned_array_karatsuba (
			hcl,
			((hcl_oop_liword_t)x)->slot, HCL_OBJ_GET_SIZE(x),
			((hcl_oop_liword_t)y)->slot, HCL_OBJ_GET_SIZE(y),
			((hcl_oop_liword_t)z)->slot) == 0) return HCL_NULL;
	}
#endif
	return z;
}

static hcl_oop_t divide_unsigned_integers (hcl_t* hcl, hcl_oop_t x, hcl_oop_t y, hcl_oop_t* r)
{
	hcl_oop_t qq, rr;

	/* the caller must ensure that x >= y */
	HCL_ASSERT (hcl, !is_less_unsigned (x, y)); 
	hcl_pushtmp (hcl, &x);
	hcl_pushtmp (hcl, &y);
	qq = hcl_makebigint(hcl, HCL_BRAND_PBIGINT, HCL_NULL, HCL_OBJ_GET_SIZE(x));
	if (!qq) 
	{
		hcl_poptmps (hcl, 2);
		return HCL_NULL;
	}

	hcl_pushtmp (hcl, &qq);
	rr = hcl_makebigint(hcl, HCL_BRAND_PBIGINT, HCL_NULL, HCL_OBJ_GET_SIZE(x));
	hcl_poptmps (hcl, 3);
	if (!rr) return HCL_NULL;

	divide_unsigned_array (hcl,
		((hcl_oop_liword_t)x)->slot, HCL_OBJ_GET_SIZE(x),
		((hcl_oop_liword_t)y)->slot, HCL_OBJ_GET_SIZE(y),
		((hcl_oop_liword_t)qq)->slot, ((hcl_oop_liword_t)rr)->slot);

	*r = rr;
	return qq;
}

hcl_oop_t hcl_addints (hcl_t* hcl, hcl_oop_t x, hcl_oop_t y)
{
	hcl_oop_t z;

	if (HCL_OOP_IS_SMOOI(x) && HCL_OOP_IS_SMOOI(y))
	{
		hcl_ooi_t i;

		/* no integer overflow/underflow must occur as the possible integer
		 * range is narrowed by the tag bits used */
		HCL_ASSERT (hcl, HCL_SMOOI_MAX + HCL_SMOOI_MAX < HCL_TYPE_MAX(hcl_ooi_t));
		HCL_ASSERT (hcl, HCL_SMOOI_MIN + HCL_SMOOI_MIN > HCL_TYPE_MIN(hcl_ooi_t));

		i = HCL_OOP_TO_SMOOI(x) + HCL_OOP_TO_SMOOI(y);
		if (HCL_IN_SMOOI_RANGE(i)) return HCL_SMOOI_TO_OOP(i);

		return make_bigint_with_ooi (hcl, i);
	}
	else
	{
		hcl_ooi_t v;

		if (HCL_OOP_IS_SMOOI(x))
		{
			if (!is_bigint(hcl,y)) goto oops_einval;

			v = HCL_OOP_TO_SMOOI(x);
			if (v == 0) return clone_bigint (hcl, y, HCL_OBJ_GET_SIZE(y));

			hcl_pushtmp (hcl, &y);
			x = make_bigint_with_ooi (hcl, v);
			hcl_poptmp (hcl);
			if (!x) return HCL_NULL;
		}
		else if (HCL_OOP_IS_SMOOI(y))
		{
			if (!is_bigint(hcl,x)) goto oops_einval;

			v = HCL_OOP_TO_SMOOI(y);
			if (v == 0) return clone_bigint (hcl, x, HCL_OBJ_GET_SIZE(x));

			hcl_pushtmp (hcl, &x);
			y = make_bigint_with_ooi (hcl, v);
			hcl_poptmp (hcl);
			if (!y) return HCL_NULL;
		}
		else
		{
			if (!is_bigint(hcl,x)) goto oops_einval;
			if (!is_bigint(hcl,y)) goto oops_einval;
		}

		if (HCL_OBJ_GET_FLAGS_BRAND(x) != HCL_OBJ_GET_FLAGS_BRAND(y))
		{
			if (HCL_IS_NBIGINT(hcl, x))
			{
				/* x is negative, y is positive */
				if (is_less_unsigned (x, y))
				{
					z = subtract_unsigned_integers (hcl, y, x);
					if (!z) return HCL_NULL;
				}
				else
				{
					z = subtract_unsigned_integers (hcl, x, y);
					if (!z) return HCL_NULL;
					HCL_OBJ_SET_FLAGS_BRAND (z, HCL_BRAND_NBIGINT);
				}
			}
			else
			{
				/* x is positive, y is negative */
				if (is_less_unsigned (x, y))
				{
					z = subtract_unsigned_integers (hcl, y, x);
					if (!z) return HCL_NULL;
					HCL_OBJ_SET_FLAGS_BRAND (z, HCL_BRAND_NBIGINT);
				}
				else
				{
					z = subtract_unsigned_integers (hcl, x, y);
					if (!z) return HCL_NULL;
				}
			}
		}
		else
		{
			int neg;
			/* both are positive or negative */
			neg = HCL_IS_NBIGINT(hcl, x);
			z = add_unsigned_integers (hcl, x, y);
			if (!z) return HCL_NULL;
			if (neg) HCL_OBJ_SET_FLAGS_BRAND (z, HCL_BRAND_NBIGINT);
		}
	}

	return normalize_bigint (hcl, z);

oops_einval:
	hcl_seterrbfmt (hcl, HCL_EINVAL, "parameter not integer - %O, %O", x, y);
	return HCL_NULL;
}

hcl_oop_t hcl_subints (hcl_t* hcl, hcl_oop_t x, hcl_oop_t y)
{
	hcl_oop_t z;

	if (HCL_OOP_IS_SMOOI(x) && HCL_OOP_IS_SMOOI(y))
	{
		hcl_ooi_t i;

		/* no integer overflow/underflow must occur as the possible integer
		 * range is narrowed by the tag bits used */
		HCL_ASSERT (hcl, HCL_SMOOI_MAX - HCL_SMOOI_MIN < HCL_TYPE_MAX(hcl_ooi_t));
		HCL_ASSERT (hcl, HCL_SMOOI_MIN - HCL_SMOOI_MAX > HCL_TYPE_MIN(hcl_ooi_t));

		i = HCL_OOP_TO_SMOOI(x) - HCL_OOP_TO_SMOOI(y);
		if (HCL_IN_SMOOI_RANGE(i)) return HCL_SMOOI_TO_OOP(i);

		return make_bigint_with_ooi (hcl, i);
	}
	else
	{
		hcl_ooi_t v;
		int neg;

		if (HCL_OOP_IS_SMOOI(x))
		{
			if (!is_bigint(hcl,y)) goto oops_einval;

			v = HCL_OOP_TO_SMOOI(x);
			if (v == 0) 
			{
				/* switch the sign to the opposite and return it */
				return clone_bigint_negated (hcl, y, HCL_OBJ_GET_SIZE(y));
			}

			hcl_pushtmp (hcl, &y);
			x = make_bigint_with_ooi (hcl, v);
			hcl_poptmp (hcl);
			if (!x) return HCL_NULL;
		}
		else if (HCL_OOP_IS_SMOOI(y))
		{
			if (!is_bigint(hcl,x)) goto oops_einval;

			v = HCL_OOP_TO_SMOOI(y);
			if (v == 0) return clone_bigint (hcl, x, HCL_OBJ_GET_SIZE(x));

			hcl_pushtmp (hcl, &x);
			y = make_bigint_with_ooi (hcl, v);
			hcl_poptmp (hcl);
			if (!y) return HCL_NULL;
		}
		else
		{
			if (!is_bigint(hcl,x)) goto oops_einval;
			if (!is_bigint(hcl,y)) goto oops_einval;
		}

		if (HCL_OBJ_GET_FLAGS_BRAND(x) != HCL_OBJ_GET_FLAGS_BRAND(y))
		{
			neg = HCL_IS_NBIGINT(hcl, x);
			z = add_unsigned_integers (hcl, x, y);
			if (!z) return HCL_NULL;
			if (neg) HCL_OBJ_SET_FLAGS_BRAND (z, HCL_BRAND_NBIGINT);
		}
		else
		{
			/* both are positive or negative */
			if (is_less_unsigned (x, y))
			{
				neg = HCL_IS_NBIGINT(hcl, x);
				z = subtract_unsigned_integers (hcl, y, x);
				if (!z) return HCL_NULL;
				if (!neg) HCL_OBJ_SET_FLAGS_BRAND (z, HCL_BRAND_NBIGINT);
			}
			else
			{
				neg = HCL_IS_NBIGINT(hcl, x);
				z = subtract_unsigned_integers (hcl, x, y); /* take x's sign */
				if (!z) return HCL_NULL;
				if (neg) HCL_OBJ_SET_FLAGS_BRAND (z, HCL_BRAND_NBIGINT);
			}
		}
	}

	return normalize_bigint (hcl, z);

oops_einval:
	hcl_seterrbfmt (hcl, HCL_EINVAL, "parameter not integer - %O, %O", x, y);
	return HCL_NULL;
}

hcl_oop_t hcl_mulints (hcl_t* hcl, hcl_oop_t x, hcl_oop_t y)
{
	hcl_oop_t z;

	if (HCL_OOP_IS_SMOOI(x) && HCL_OOP_IS_SMOOI(y))
	{
	#if HCL_SIZEOF_INTMAX_T > HCL_SIZEOF_OOI_T
		hcl_intmax_t i;
		i = (hcl_intmax_t)HCL_OOP_TO_SMOOI(x) * (hcl_intmax_t)HCL_OOP_TO_SMOOI(y);
		if (HCL_IN_SMOOI_RANGE(i)) return HCL_SMOOI_TO_OOP((hcl_ooi_t)i);
		return make_bigint_with_intmax (hcl, i);
	#else
		hcl_ooi_t i;
		hcl_ooi_t xv, yv;

		xv = HCL_OOP_TO_SMOOI(x);
		yv = HCL_OOP_TO_SMOOI(y);
		if (shcli_mul_overflow (hcl, xv, yv, &i))
		{
			/* overflowed - convert x and y normal objects and carry on */

			/* no need to call hcl_pushtmp before creating x because
			 * xv and yv contains actual values needed */
			x = make_bigint_with_ooi (hcl, xv);
			if (!x) return HCL_NULL;

			hcl_pushtmp (hcl, &x); /* protect x made above */
			y = make_bigint_with_ooi (hcl, yv);
			hcl_poptmp (hcl);
			if (!y) return HCL_NULL;

			goto normal;
		}
		else
		{
			if (HCL_IN_SMOOI_RANGE(i)) return HCL_SMOOI_TO_OOP(i);
			return make_bigint_with_ooi (hcl, i);
		}
	#endif
	}
	else
	{
		hcl_ooi_t v;

		if (HCL_OOP_IS_SMOOI(x))
		{
			if (!is_bigint(hcl,y)) goto oops_einval;

			v = HCL_OOP_TO_SMOOI(x);
			switch (v)
			{
				case 0:
					return HCL_SMOOI_TO_OOP(0);
				case 1:
					return clone_bigint (hcl, y, HCL_OBJ_GET_SIZE(y));
				case -1:
					return clone_bigint_negated (hcl, y, HCL_OBJ_GET_SIZE(y));
			}

			hcl_pushtmp (hcl, &y);
			x = make_bigint_with_ooi (hcl, v);
			hcl_poptmp (hcl);
			if (!x) return HCL_NULL;
		}
		else if (HCL_OOP_IS_SMOOI(y))
		{
			if (!is_bigint(hcl,x)) goto oops_einval;

			v = HCL_OOP_TO_SMOOI(y);
			switch (v)
			{
				case 0:
					return HCL_SMOOI_TO_OOP(0);
				case 1:
					return clone_bigint (hcl, x, HCL_OBJ_GET_SIZE(x));
				case -1:
					return clone_bigint_negated (hcl, x, HCL_OBJ_GET_SIZE(x));
			}

			hcl_pushtmp (hcl, &x);
			y = make_bigint_with_ooi (hcl, v);
			hcl_poptmp (hcl);
			if (!y) return HCL_NULL;
		}
		else
		{
			if (!is_bigint(hcl,x)) goto oops_einval;
			if (!is_bigint(hcl,y)) goto oops_einval;
		}

	normal:
		hcl_pushtmp (hcl, &x);
		hcl_pushtmp (hcl, &y);
		z = multiply_unsigned_integers (hcl, x, y);
		hcl_poptmps (hcl, 2);
		if (!z) return HCL_NULL;
		if (HCL_OBJ_GET_FLAGS_BRAND(x) != HCL_OBJ_GET_FLAGS_BRAND(y))
			HCL_OBJ_SET_FLAGS_BRAND (z, HCL_BRAND_NBIGINT);
	}

	return normalize_bigint (hcl, z);

oops_einval:
	hcl_seterrbfmt (hcl, HCL_EINVAL, "parameter not integer - %O, %O", x, y);
	return HCL_NULL;
}

hcl_oop_t hcl_divints (hcl_t* hcl, hcl_oop_t x, hcl_oop_t y, int modulo, hcl_oop_t* rem)
{
	hcl_oop_t z, r;
	int x_neg, y_neg;

	if (HCL_OOP_IS_SMOOI(x) && HCL_OOP_IS_SMOOI(y))
	{
		hcl_ooi_t xv, yv, q, r;

		xv = HCL_OOP_TO_SMOOI(x);
		yv = HCL_OOP_TO_SMOOI(y);

		if (yv == 0)
		{
			hcl_seterrnum (hcl, HCL_EDIVBY0);
			return HCL_NULL;
		}

		if (xv == 0)
		{
			if (rem) *rem = HCL_SMOOI_TO_OOP(0);
			return HCL_SMOOI_TO_OOP(0);
		}

		/* In C89, integer division with a negative number  is
		 * implementation dependent. In C99, it truncates towards zero.
		 * 
		 * http://python-history.blogspot.kr/2010/08/why-pythons-integer-division-floors.html
		 *   The integer division operation (//) and its sibling, 
		 *   the modulo operation (%), go together and satisfy a nice
		 *   mathematical relationship (all variables are integers):
		 *      a/b = q with remainder r
		 *   such that
		 *      b*q + r = a and 0 <= r < b (assuming- a and b are >= 0).
		 * 
		 *   If you want the relationship to extend for negative a
		 *   (keeping b positive), you have two choices: if you truncate q
		 *   towards zero, r will become negative, so that the invariant
		 *   changes to 0 <= abs(r) < abs(b). otherwise, you can floor q
		 *   towards negative infinity, and the invariant remains 0 <= r < b. 
		 */

		q = xv / yv;
		HCL_ASSERT (hcl, HCL_IN_SMOOI_RANGE(q));

		r = xv - yv * q; /* xv % yv; */
		if (r)
		{
			if (modulo)
			{
				/* modulo */
				/*
					xv      yv      q       r
					-------------------------
					 7       3      2       1
					-7       3     -3       2
					 7      -3     -3      -2
					-7      -3      2      -1
				 */

				/* r must be floored. that is, it rounds away from zero 
				 * and towards negative infinity */
				if (IS_SIGN_DIFF(yv, r))
				{
					/* if the divisor has a different sign from r,
					 * change the sign of r to the divisor's sign */
					r += yv;
					--q;
					HCL_ASSERT (hcl, r && !IS_SIGN_DIFF(yv, r));
				}
			}
			else
			{
				/* remainder */
				/*
					xv      yv      q       r
					-------------------------
					 7       3      2       1
					-7       3     -2      -1
					 7      -3     -2       1
					-7      -3      2      -1
				 */
				if (xv && IS_SIGN_DIFF(xv, r)) 
				{
					/* if the dividend has a different sign from r,
					 * change the sign of r to the dividend's sign.
					 * all the compilers i've worked with produced
					 * the quotient and the remainder that don't need
					 * any adjustment. however, there may be an esoteric
					 * architecture. */
					r -= yv;
					++q;
					HCL_ASSERT (hcl, xv && !IS_SIGN_DIFF(xv, r));
				}
			}
		}

		if (rem)
		{
			HCL_ASSERT (hcl, HCL_IN_SMOOI_RANGE(r));
			*rem = HCL_SMOOI_TO_OOP(r);
		}

		return HCL_SMOOI_TO_OOP((hcl_ooi_t)q);
	}
	else 
	{
		if (HCL_OOP_IS_SMOOI(x))
		{
			hcl_ooi_t v;

			if (!is_bigint(hcl,y)) goto oops_einval;

			v = HCL_OOP_TO_SMOOI(x);
			if (v == 0)
			{
				if (rem) *rem = HCL_SMOOI_TO_OOP(0);
				return HCL_SMOOI_TO_OOP(0);
			}

			hcl_pushtmp (hcl, &y);
			x = make_bigint_with_ooi (hcl, v);
			hcl_poptmp (hcl);
			if (!x) return HCL_NULL;
		}
		else if (HCL_OOP_IS_SMOOI(y))
		{
			hcl_ooi_t v;

			if (!is_bigint(hcl,x)) goto oops_einval;

			v = HCL_OOP_TO_SMOOI(y);
			switch (v)
			{
				case 0:
					hcl_seterrnum (hcl, HCL_EDIVBY0);
					return HCL_NULL;

				case 1:
					z = clone_bigint (hcl, x, HCL_OBJ_GET_SIZE(x));
					if (!z) return HCL_NULL;
					if (rem) *rem = HCL_SMOOI_TO_OOP(0);
					return z;


				case -1:
					z = clone_bigint_negated (hcl, x, HCL_OBJ_GET_SIZE(x));
					if (!z) return HCL_NULL;
					if (rem) *rem = HCL_SMOOI_TO_OOP(0);
					return z;

	/*
				default:
					if (IS_POWER_OF_2(v))
					{
	TODO:
	DO SHIFTING. how to get remainder..
	if v is powerof2, do shifting???

						z = clone_bigint_negated (hcl, x, HCL_OBJ_GET_SIZE(x));
						rshift_unsigned_array (z, HCL_OBJ_GET_SIZE(z), 10);
					}
	*/
			}

			hcl_pushtmp (hcl, &x);
			y = make_bigint_with_ooi (hcl, v);
			hcl_poptmp (hcl);
			if (!y) return HCL_NULL;
		}
		else
		{
			if (!is_bigint(hcl,x)) goto oops_einval;
			if (!is_bigint(hcl,y)) goto oops_einval;
		}
	}

	x_neg = HCL_IS_NBIGINT(hcl, x);
	y_neg = HCL_IS_NBIGINT(hcl, y);

	hcl_pushtmp (hcl, &x);
	hcl_pushtmp (hcl, &y);
	z = divide_unsigned_integers (hcl, x, y, &r);
	hcl_poptmps (hcl, 2);
	if (!z) return HCL_NULL;

	if (x_neg) 
	{
		/* the class on r must be set before normalize_bigint() 
		 * because it can get changed to a small integer */
		HCL_OBJ_SET_FLAGS_BRAND (r, HCL_BRAND_NBIGINT);
	}

	if (x_neg != y_neg)
	{
		HCL_OBJ_SET_FLAGS_BRAND (z, HCL_BRAND_NBIGINT);

		hcl_pushtmp (hcl, &z);
		hcl_pushtmp (hcl, &y);
		r = normalize_bigint (hcl, r);
		hcl_poptmps (hcl, 2);
		if (!r) return HCL_NULL;

		if (r != HCL_SMOOI_TO_OOP(0) && modulo)
		{
			if (rem)
			{
				hcl_pushtmp (hcl, &z);
				hcl_pushtmp (hcl, &y);
				r = hcl_addints (hcl, r, y);
				hcl_poptmps (hcl, 2);
				if (!r) return HCL_NULL;

				hcl_pushtmp (hcl, &r);
				z = normalize_bigint (hcl, z);
				hcl_poptmp (hcl);
				if (!z) return HCL_NULL;

				hcl_pushtmp (hcl, &r);
				z = hcl_subints (hcl, z, HCL_SMOOI_TO_OOP(1));
				hcl_poptmp (hcl);
				if (!z) return HCL_NULL;

				*rem = r;
				return z;
			}
			else
			{
				/* remainder is not needed at all */
/* TODO: subtract 1 without normalization??? */
				z = normalize_bigint (hcl, z);
				if (!z) return HCL_NULL;
				return hcl_subints (hcl, z, HCL_SMOOI_TO_OOP(1));
			}
		}
	}
	else
	{
		hcl_pushtmp (hcl, &z);
		r = normalize_bigint (hcl, r);
		hcl_poptmp (hcl);
		if (!r) return HCL_NULL;
	}

	if (rem) *rem = r;
	return normalize_bigint (hcl, z);

oops_einval:
	hcl_seterrbfmt (hcl, HCL_EINVAL, "parameter not integer - %O, %O", x, y);
	return HCL_NULL;
}


hcl_oop_t hcl_negateint (hcl_t* hcl, hcl_oop_t x)
{
	if (HCL_OOP_IS_SMOOI(x))
	{
		hcl_ooi_t v;
		v = HCL_OOP_TO_SMOOI(x);
		return HCL_SMOOI_TO_OOP(-v);
	}
	else
	{
		if (!is_bigint(hcl, x)) goto oops_einval;
		return clone_bigint_negated (hcl, x, HCL_OBJ_GET_SIZE(x));
	}

oops_einval:
	hcl_seterrbfmt (hcl, HCL_EINVAL, "parameter not integer - %O", x);
	return HCL_NULL;
}

hcl_oop_t hcl_bitatint (hcl_t* hcl, hcl_oop_t x, hcl_oop_t y)
{
	/* y is 0-based */

	if (HCL_OOP_IS_SMOOI(x) && HCL_OOP_IS_SMOOI(y))
	{
		hcl_ooi_t v1, v2, v3;

		v1 = HCL_OOP_TO_SMOOI(x);
		v2 = HCL_OOP_TO_SMOOI(y);

		if (v2 < 0) return HCL_SMOOI_TO_OOP(0);
		if (v1 >= 0)
		{
			/* the absolute value may be composed of up to 
			 * HCL_SMOOI_BITS - 1 bits as there is a sign bit.*/
			if (v2 >= HCL_SMOOI_BITS - 1) return HCL_SMOOI_TO_OOP(0);
			v3 = ((hcl_oow_t)v1 >> v2) & 1;
		}
		else
		{
			if (v2 >= HCL_SMOOI_BITS - 1) return HCL_SMOOI_TO_OOP(1);
			v3 = ((~(hcl_oow_t)-v1 + 1) >> v2) & 1;
		}
		return HCL_SMOOI_TO_OOP(v3);
	}
	else if (HCL_OOP_IS_SMOOI(x))
	{
		if (!is_bigint(hcl, y)) goto oops_einval;

		if (HCL_IS_NBIGINT(hcl, y)) return HCL_SMOOI_TO_OOP(0);

		/* y is definitely >= HCL_SMOOI_BITS */
		if (HCL_OOP_TO_SMOOI(x) >= 0) 
			return HCL_SMOOI_TO_OOP(0);
		else 
			return HCL_SMOOI_TO_OOP(1);
	}
	else if (HCL_OOP_IS_SMOOI(y))
	{
		hcl_ooi_t v;
		hcl_oow_t wp, bp, xs;

		if (!is_bigint(hcl, x)) goto oops_einval;
		v = HCL_OOP_TO_SMOOI(y);

		if (v < 0) return HCL_SMOOI_TO_OOP(0);
		wp = v / HCL_LIW_BITS;
		bp = v - (wp * HCL_LIW_BITS);

		xs = HCL_OBJ_GET_SIZE(x);
		if (HCL_IS_PBIGINT(hcl, x))
		{
			if (wp >= xs) return HCL_SMOOI_TO_OOP(0);
			v = (((hcl_oop_liword_t)x)->slot[wp] >> bp) & 1;
		}
		else
		{
			hcl_lidw_t w, carry;
			hcl_oow_t i;

			if (wp >= xs) return HCL_SMOOI_TO_OOP(1);

			carry = 1;
			for (i = 0; i <= wp; i++)
			{
				w = (hcl_lidw_t)((hcl_liw_t)~((hcl_oop_liword_t)x)->slot[i]) + carry;
				carry = w >> HCL_LIW_BITS;
			}
			v = ((hcl_oow_t)w >> bp) & 1;
		}

		return HCL_SMOOI_TO_OOP(v);
	}
	else
	{
	#if defined(HCL_LIMIT_OBJ_SIZE)
		/* nothing */
	#else
		hcl_oow_t w, wp, bp, xs;
		hcl_ooi_t v;
		int sign;
	#endif

		if (!is_bigint(hcl, x) || !is_bigint(hcl, y)) goto oops_einval;

	#if defined(HCL_LIMIT_OBJ_SIZE)
		if (HCL_IS_NBIGINT(hcl, y)) return HCL_SMOOI_TO_OOP(0);

		HCL_ASSERT (hcl, HCL_OBJ_SIZE_BITS_MAX <= HCL_TYPE_MAX(hcl_oow_t));
		if (HCL_IS_PBIGINT(hcl, x))
		{
			return HCL_SMOOI_TO_OOP(0);
		}
		else
		{
			return HCL_SMOOI_TO_OOP(1);
		}
	#else
		xs = HCL_OBJ_GET_SIZE(x);

		if (HCL_IS_NBIGINT(hcl, y)) return HCL_SMOOI_TO_OOP(0);

		sign = bigint_to_oow (hcl, y, &w);
		HCL_ASSERT (hcl, sign >= 0);
		if (sign >= 1)
		{
			wp = w / HCL_LIW_BITS;
			bp = w - (wp * HCL_LIW_BITS);
		}
		else
		{
			hcl_oop_t quo, rem;

			HCL_ASSERT (hcl, sign == 0);

			hcl_pushtmp (hcl, &x);
			quo = hcl_divints (hcl, y, HCL_SMOOI_TO_OOP(HCL_LIW_BITS), 0, &rem);
			hcl_poptmp (hcl);
			if (!quo) return HCL_NULL;

			sign = integer_to_oow (hcl, quo, &wp);
			HCL_ASSERT (hcl, sign >= 0);
			if (sign == 0)
			{
				/* too large. set it to xs so that it gets out of
				 * the valid range */
				wp = xs;
			}

			HCL_ASSERT (hcl, HCL_OOP_IS_SMOOI(rem));
			bp = HCL_OOP_TO_SMOOI(rem);
			HCL_ASSERT (hcl, bp >= 0 && bp < HCL_LIW_BITS);
		}

		if (HCL_IS_PBIGINT(hcl, x))
		{
			if (wp >= xs) return HCL_SMOOI_TO_OOP(0);
			v = (((hcl_oop_liword_t)x)->slot[wp] >> bp) & 1;
		}
		else
		{
			hcl_lidw_t w, carry;
			hcl_oow_t i;

			if (wp >= xs) return HCL_SMOOI_TO_OOP(1);

			carry = 1;
			for (i = 0; i <= wp; i++)
			{
				w = (hcl_lidw_t)((hcl_liw_t)~((hcl_oop_liword_t)x)->slot[i]) + carry;
				carry = w >> HCL_LIW_BITS;
			}
			v = ((hcl_oow_t)w >> bp) & 1;
		}

		return HCL_SMOOI_TO_OOP(v);
	#endif
	}

oops_einval:
	hcl_seterrbfmt (hcl, HCL_EINVAL, "parameter not integer - %O, %O", x, y);
	return HCL_NULL;
}

hcl_oop_t hcl_bitandints (hcl_t* hcl, hcl_oop_t x, hcl_oop_t y)
{
	if (HCL_OOP_IS_SMOOI(x) && HCL_OOP_IS_SMOOI(y))
	{
		hcl_ooi_t v1, v2, v3;

		v1 = HCL_OOP_TO_SMOOI(x);
		v2 = HCL_OOP_TO_SMOOI(y);
		v3 = v1 & v2;

		if (HCL_IN_SMOOI_RANGE(v3)) return HCL_SMOOI_TO_OOP(v3);
		return make_bigint_with_ooi (hcl, v3);
	}
	else if (HCL_OOP_IS_SMOOI(x))
	{
		hcl_ooi_t v;

		if (!is_bigint(hcl, y)) goto oops_einval;

		v = HCL_OOP_TO_SMOOI(x);
		if (v == 0) return HCL_SMOOI_TO_OOP(0);

		hcl_pushtmp (hcl, &y);
		x = make_bigint_with_ooi (hcl, v);
		hcl_poptmp (hcl);
		if (!x) return HCL_NULL;

		goto bigint_and_bigint;
	}
	else if (HCL_OOP_IS_SMOOI(y))
	{
		hcl_ooi_t v;

		if (!is_bigint(hcl, x)) goto oops_einval;

		v = HCL_OOP_TO_SMOOI(y);
		if (v == 0) return HCL_SMOOI_TO_OOP(0);

		hcl_pushtmp (hcl, &x);
		y = make_bigint_with_ooi (hcl, v);
		hcl_poptmp (hcl);
		if (!x) return HCL_NULL;

		goto bigint_and_bigint;
	}
	else
	{
		hcl_oop_t z;
		hcl_oow_t i, xs, ys, zs, zalloc;
		int negx, negy;

		if (!is_bigint(hcl,x) || !is_bigint(hcl, y)) goto oops_einval;

	bigint_and_bigint:
		xs = HCL_OBJ_GET_SIZE(x);
		ys = HCL_OBJ_GET_SIZE(y);

		if (xs < ys)
		{
			/* make sure that x is greater than or equal to y */
			z = x;
			x = y;
			y = z;
			zs = ys;
			ys = xs;
			xs = zs;
		}

		negx = HCL_IS_NBIGINT(hcl, x);
		negy = HCL_IS_NBIGINT(hcl, y);

		if (negx && negy)
		{
			zalloc = xs + 1;
			zs = xs;
		}
		else if (negx)
		{
			zalloc = ys;
			zs = ys;
		}
		else if (negy)
		{
			zalloc = xs;
			zs = xs;
		}
		else
		{
			zalloc = ys;
			zs = ys;
		}

		hcl_pushtmp (hcl, &x);
		hcl_pushtmp (hcl, &y);
		z = hcl_makebigint(hcl, HCL_BRAND_PBIGINT, HCL_NULL, zalloc);
		hcl_poptmps (hcl, 2);
		if (!z) return HCL_NULL;

		if (negx && negy)
		{
			/* both are negative */
			hcl_lidw_t w[2];
			hcl_lidw_t carry[2];

			carry[0] = 1;
			carry[1] = 1;
			/* 2's complement on both x and y and perform bitwise-and */
			for (i = 0; i < ys; i++)
			{
				w[0] = (hcl_lidw_t)((hcl_liw_t)~((hcl_oop_liword_t)x)->slot[i]) + carry[0];
				carry[0] = w[0] >> HCL_LIW_BITS;

				w[1] = (hcl_lidw_t)((hcl_liw_t)~((hcl_oop_liword_t)y)->slot[i]) + carry[1];
				carry[1] = w[1] >> HCL_LIW_BITS;

				((hcl_oop_liword_t)z)->slot[i] = (hcl_liw_t)w[0] & (hcl_liw_t)w[1];
			}
			HCL_ASSERT (hcl, carry[1] == 0);

			/* 2's complement on the remaining part of x. the lacking part
			 * in y is treated as if they are all 1s. */
			for (; i < xs; i++)
			{
				w[0] = (hcl_lidw_t)((hcl_liw_t)~((hcl_oop_liword_t)x)->slot[i]) + carry[0];
				carry[0] = w[0] >> HCL_LIW_BITS;
				((hcl_oop_liword_t)z)->slot[i] = (hcl_liw_t)w[0];
			}
			HCL_ASSERT (hcl, carry[0] == 0);

			/* 2's complement on the final result */
			((hcl_oop_liword_t)z)->slot[zs] = ~(hcl_liw_t)0;
			carry[0] = 1;
			for (i = 0; i <= zs; i++)
			{
				w[0] = (hcl_lidw_t)((hcl_liw_t)~((hcl_oop_liword_t)z)->slot[i]) + carry[0];
				carry[0] = w[0] >> HCL_LIW_BITS;
				((hcl_oop_liword_t)z)->slot[i] = (hcl_liw_t)w[0];
			}
			HCL_ASSERT (hcl, carry[0] == 0);

			HCL_OBJ_SET_FLAGS_BRAND (z, HCL_BRAND_NBIGINT);
		}
		else if (negx)
		{
			/* x is negative, y is positive */
			hcl_lidw_t w, carry;

			carry = 1;
			for (i = 0; i < ys; i++)
			{
				w = (hcl_lidw_t)((hcl_liw_t)~((hcl_oop_liword_t)x)->slot[i]) + carry;
				carry = w >> HCL_LIW_BITS;
				((hcl_oop_liword_t)z)->slot[i] = (hcl_liw_t)w & ((hcl_oop_liword_t)y)->slot[i];
			}

			/* the lacking part in y is all 0's. the remaining part in x is
			 * just masked out when bitwise-anded with 0. so nothing is done
			 * to handle the remaining part in x */
		}
		else if (negy)
		{
			/* x is positive, y is negative  */
			hcl_lidw_t w, carry;

			/* x & 2's complement on y up to ys */
			carry = 1;
			for (i = 0; i < ys; i++)
			{
				w = (hcl_lidw_t)((hcl_liw_t)~((hcl_oop_liword_t)y)->slot[i]) + carry;
				carry = w >> HCL_LIW_BITS;
				((hcl_oop_liword_t)z)->slot[i] = ((hcl_oop_liword_t)x)->slot[i] & (hcl_liw_t)w;
			}
			HCL_ASSERT (hcl, carry == 0);

			/* handle the longer part in x than y
			 * 
			 * For example,
			 *  x => + 1010 1100
			 *  y => -      0011
			 * 
			 * If y is extended to the same length as x, 
			 * it is a negative 0000 0001. 
			 * 2's complement is performed on this imaginary extension.
			 * the result is '1111 1101' (1111 1100 + 1).
			 * 
			 * when y is shorter and negative, the lacking part can be
			 * treated as all 1s in the 2's complement format.
			 * 
			 * the remaining part in x can be just copied to the 
			 * final result 'z'.
			 */
			for (; i < xs; i++)
			{
				((hcl_oop_liword_t)z)->slot[i] = ((hcl_oop_liword_t)x)->slot[i];
			}
		}
		else
		{
			/* both are positive */
			for (i = 0; i < ys; i++)
			{
				((hcl_oop_liword_t)z)->slot[i] = ((hcl_oop_liword_t)x)->slot[i] & ((hcl_oop_liword_t)y)->slot[i];
			}
		}

		return normalize_bigint(hcl, z);
	}

oops_einval:
	hcl_seterrbfmt (hcl, HCL_EINVAL, "parameter not integer - %O, %O", x, y);
	return HCL_NULL;
}

hcl_oop_t hcl_bitorints (hcl_t* hcl, hcl_oop_t x, hcl_oop_t y)
{
	if (HCL_OOP_IS_SMOOI(x) && HCL_OOP_IS_SMOOI(y))
	{
		hcl_ooi_t v1, v2, v3;

		v1 = HCL_OOP_TO_SMOOI(x);
		v2 = HCL_OOP_TO_SMOOI(y);
		v3 = v1 | v2;

		if (HCL_IN_SMOOI_RANGE(v3)) return HCL_SMOOI_TO_OOP(v3);
		return make_bigint_with_ooi (hcl, v3);
	}
	else if (HCL_OOP_IS_SMOOI(x))
	{
		hcl_ooi_t v;

		if (!is_bigint(hcl, y)) goto oops_einval;

		v = HCL_OOP_TO_SMOOI(x);
		if (v == 0) return clone_bigint(hcl, y, HCL_OBJ_GET_SIZE(y));

		hcl_pushtmp (hcl, &y);
		x = make_bigint_with_ooi (hcl, v);
		hcl_poptmp (hcl);
		if (!x) return HCL_NULL;

		goto bigint_and_bigint;
	}
	else if (HCL_OOP_IS_SMOOI(y))
	{
		hcl_ooi_t v;

		if (!is_bigint(hcl, x)) goto oops_einval;

		v = HCL_OOP_TO_SMOOI(y);
		if (v == 0) return clone_bigint(hcl, x, HCL_OBJ_GET_SIZE(x));

		hcl_pushtmp (hcl, &x);
		y = make_bigint_with_ooi (hcl, v);
		hcl_poptmp (hcl);
		if (!x) return HCL_NULL;

		goto bigint_and_bigint;
	}
	else
	{
		hcl_oop_t z;
		hcl_oow_t i, xs, ys, zs, zalloc;
		int negx, negy;

		if (!is_bigint(hcl,x) || !is_bigint(hcl, y)) goto oops_einval;

	bigint_and_bigint:
		xs = HCL_OBJ_GET_SIZE(x);
		ys = HCL_OBJ_GET_SIZE(y);

		if (xs < ys)
		{
			/* make sure that x is greater than or equal to y */
			z = x;
			x = y;
			y = z;
			zs = ys;
			ys = xs;
			xs = zs;
		}

		negx = HCL_IS_NBIGINT(hcl, x);
		negy = HCL_IS_NBIGINT(hcl, y);

		if (negx && negy)
		{
			zalloc = ys + 1;
			zs = ys;
		}
		else if (negx)
		{
			zalloc = xs + 1;
			zs = xs;
		}
		else if (negy)
		{
			zalloc = ys + 1;
			zs = ys;
		}
		else
		{
			zalloc = xs;
			zs = xs;
		}

		if (zalloc < zs)
		{
			/* overflow in zalloc calculation above */
			hcl_seterrnum (hcl, HCL_EOOMEM); /* TODO: is it a soft failure or hard failure? */
			return HCL_NULL;
		}

		hcl_pushtmp (hcl, &x);
		hcl_pushtmp (hcl, &y);
		z = hcl_makebigint(hcl, HCL_BRAND_PBIGINT, HCL_NULL, zalloc);
		hcl_poptmps (hcl, 2);
		if (!z) return HCL_NULL;

		if (negx && negy)
		{
			/* both are negative */
			hcl_lidw_t w[2];
			hcl_lidw_t carry[2];

			carry[0] = 1;
			carry[1] = 1;
			/* 2's complement on both x and y and perform bitwise-and */
			for (i = 0; i < ys; i++)
			{
				w[0] = (hcl_lidw_t)((hcl_liw_t)~((hcl_oop_liword_t)x)->slot[i]) + carry[0];
				carry[0] = w[0] >> HCL_LIW_BITS;

				w[1] = (hcl_lidw_t)((hcl_liw_t)~((hcl_oop_liword_t)y)->slot[i]) + carry[1];
				carry[1] = w[1] >> HCL_LIW_BITS;

				((hcl_oop_liword_t)z)->slot[i] = (hcl_liw_t)w[0] | (hcl_liw_t)w[1];
			}
			HCL_ASSERT (hcl, carry[1] == 0);

			/* do nothing about the extra part in x and the lacking part
			 * in y for the reason shown in [NOTE] in the 'else if' block
			 * further down. */

		adjust_to_negative:
			/* 2's complement on the final result */
			((hcl_oop_liword_t)z)->slot[zs] = ~(hcl_liw_t)0;
			carry[0] = 1;
			for (i = 0; i <= zs; i++)
			{
				w[0] = (hcl_lidw_t)((hcl_liw_t)~((hcl_oop_liword_t)z)->slot[i]) + carry[0];
				carry[0] = w[0] >> HCL_LIW_BITS;
				((hcl_oop_liword_t)z)->slot[i] = (hcl_liw_t)w[0];
			}
			HCL_ASSERT (hcl, carry[0] == 0);

			HCL_OBJ_SET_FLAGS_BRAND (z, HCL_BRAND_NBIGINT);
		}
		else if (negx)
		{
			/* x is negative, y is positive */
			hcl_lidw_t w, carry;

			carry = 1;
			for (i = 0; i < ys; i++)
			{
				w = (hcl_lidw_t)((hcl_liw_t)~((hcl_oop_liword_t)x)->slot[i]) + carry;
				carry = w >> HCL_LIW_BITS;
				((hcl_oop_liword_t)z)->slot[i] = (hcl_liw_t)w | ((hcl_oop_liword_t)y)->slot[i];
			}

			for (; i < xs; i++)
			{
				w = (hcl_lidw_t)((hcl_liw_t)~((hcl_oop_liword_t)x)->slot[i]) + carry;
				carry = w >> HCL_LIW_BITS;
				((hcl_oop_liword_t)z)->slot[i] = (hcl_liw_t)w;
			}

			HCL_ASSERT (hcl, carry == 0);
			goto adjust_to_negative;
		}
		else if (negy)
		{
			/* x is positive, y is negative  */
			hcl_lidw_t w, carry;

			/* x & 2's complement on y up to ys */
			carry = 1;
			for (i = 0; i < ys; i++)
			{
				w = (hcl_lidw_t)((hcl_liw_t)~((hcl_oop_liword_t)y)->slot[i]) + carry;
				carry = w >> HCL_LIW_BITS;
				((hcl_oop_liword_t)z)->slot[i] = ((hcl_oop_liword_t)x)->slot[i] | (hcl_liw_t)w;
			}
			HCL_ASSERT (hcl, carry == 0);

			/* [NOTE]
			 *  in theory, the lacking part in ys is all 1s when y is
			 *  extended to the width of x. but those 1s are inverted to
			 *  0s when another 2's complement is performed over the final
			 *  result after the jump to 'adjust_to_negative'.
			 *  setting zs to 'xs + 1' and performing the following loop is 
			 *  redundant.
			for (; i < xs; i++)
			{
				((hcl_oop_liword_t)z)->slot[i] = ~(hcl_liw_t)0;
			}
			*/
			goto adjust_to_negative;
		}
		else
		{
			/* both are positive */
			for (i = 0; i < ys; i++)
			{
				((hcl_oop_liword_t)z)->slot[i] = ((hcl_oop_liword_t)x)->slot[i] | ((hcl_oop_liword_t)y)->slot[i];
			}

			for (; i < xs; i++)
			{
				((hcl_oop_liword_t)z)->slot[i] = ((hcl_oop_liword_t)x)->slot[i];
			}
		}

		return normalize_bigint(hcl, z);
	}

oops_einval:
	hcl_seterrbfmt (hcl, HCL_EINVAL, "parameter not integer - %O, %O", x, y);
	return HCL_NULL;
}

hcl_oop_t hcl_bitxorints (hcl_t* hcl, hcl_oop_t x, hcl_oop_t y)
{
	if (HCL_OOP_IS_SMOOI(x) && HCL_OOP_IS_SMOOI(y))
	{
		hcl_ooi_t v1, v2, v3;

		v1 = HCL_OOP_TO_SMOOI(x);
		v2 = HCL_OOP_TO_SMOOI(y);
		v3 = v1 ^ v2;

		if (HCL_IN_SMOOI_RANGE(v3)) return HCL_SMOOI_TO_OOP(v3);
		return make_bigint_with_ooi (hcl, v3);
	}
	else if (HCL_OOP_IS_SMOOI(x))
	{
		hcl_ooi_t v;

		if (!is_bigint(hcl, y)) goto oops_einval;

		v = HCL_OOP_TO_SMOOI(x);
		if (v == 0) return clone_bigint(hcl, y, HCL_OBJ_GET_SIZE(y));

		hcl_pushtmp (hcl, &y);
		x = make_bigint_with_ooi (hcl, v);
		hcl_poptmp (hcl);
		if (!x) return HCL_NULL;

		goto bigint_and_bigint;
	}
	else if (HCL_OOP_IS_SMOOI(y))
	{
		hcl_ooi_t v;

		if (!is_bigint(hcl, x)) goto oops_einval;

		v = HCL_OOP_TO_SMOOI(y);
		if (v == 0) return clone_bigint(hcl, x, HCL_OBJ_GET_SIZE(x));

		hcl_pushtmp (hcl, &x);
		y = make_bigint_with_ooi (hcl, v);
		hcl_poptmp (hcl);
		if (!x) return HCL_NULL;

		goto bigint_and_bigint;
	}
	else
	{
		hcl_oop_t z;
		hcl_oow_t i, xs, ys, zs, zalloc;
		int negx, negy;

		if (!is_bigint(hcl,x) || !is_bigint(hcl, y)) goto oops_einval;

	bigint_and_bigint:
		xs = HCL_OBJ_GET_SIZE(x);
		ys = HCL_OBJ_GET_SIZE(y);

		if (xs < ys)
		{
			/* make sure that x is greater than or equal to y */
			z = x;
			x = y;
			y = z;
			zs = ys;
			ys = xs;
			xs = zs;
		}

		negx = HCL_IS_NBIGINT(hcl, x);
		negy = HCL_IS_NBIGINT(hcl, y);

		if (negx && negy)
		{
			zalloc = xs;
			zs = xs;
		}
		else if (negx)
		{
			zalloc = xs + 1;
			zs = xs;
		}
		else if (negy)
		{
			zalloc = xs + 1;
			zs = xs;
		}
		else
		{
			zalloc = xs;
			zs = xs;
		}

		if (zalloc < zs)
		{
			/* overflow in zalloc calculation above */
			hcl_seterrnum (hcl, HCL_EOOMEM); /* TODO: is it a soft failure or hard failure? */
			return HCL_NULL;
		}

		hcl_pushtmp (hcl, &x);
		hcl_pushtmp (hcl, &y);
		z = hcl_makebigint(hcl, HCL_BRAND_PBIGINT, HCL_NULL, zalloc);
		hcl_poptmps (hcl, 2);
		if (!z) return HCL_NULL;

		if (negx && negy)
		{
			/* both are negative */
			hcl_lidw_t w[2];
			hcl_lidw_t carry[2];

			carry[0] = 1;
			carry[1] = 1;
			/* 2's complement on both x and y and perform bitwise-and */
			for (i = 0; i < ys; i++)
			{
				w[0] = (hcl_lidw_t)((hcl_liw_t)~((hcl_oop_liword_t)x)->slot[i]) + carry[0];
				carry[0] = w[0] >> HCL_LIW_BITS;

				w[1] = (hcl_lidw_t)((hcl_liw_t)~((hcl_oop_liword_t)y)->slot[i]) + carry[1];
				carry[1] = w[1] >> HCL_LIW_BITS;

				((hcl_oop_liword_t)z)->slot[i] = (hcl_liw_t)w[0] ^ (hcl_liw_t)w[1];
			}
			HCL_ASSERT (hcl, carry[1] == 0);

			/* treat the lacking part in y as all 1s */
			for (; i < xs; i++)
			{
				w[0] = (hcl_lidw_t)((hcl_liw_t)~((hcl_oop_liword_t)x)->slot[i]) + carry[0];
				carry[0] = w[0] >> HCL_LIW_BITS;
				((hcl_oop_liword_t)z)->slot[i] = (hcl_liw_t)w[0] ^ (~(hcl_liw_t)0);
			}
			HCL_ASSERT (hcl, carry[0] == 0);
		}
		else if (negx)
		{
			/* x is negative, y is positive */
			hcl_lidw_t w, carry;

			carry = 1;
			for (i = 0; i < ys; i++)
			{
				w = (hcl_lidw_t)((hcl_liw_t)~((hcl_oop_liword_t)x)->slot[i]) + carry;
				carry = w >> HCL_LIW_BITS;
				((hcl_oop_liword_t)z)->slot[i] = (hcl_liw_t)w ^ ((hcl_oop_liword_t)y)->slot[i];
			}

			/* treat the lacking part in y as all 0s */
			for (; i < xs; i++)
			{
				w = (hcl_lidw_t)((hcl_liw_t)~((hcl_oop_liword_t)x)->slot[i]) + carry;
				carry = w >> HCL_LIW_BITS;
				((hcl_oop_liword_t)z)->slot[i] = (hcl_liw_t)w;
			}
			HCL_ASSERT (hcl, carry == 0);

		adjust_to_negative:
			/* 2's complement on the final result */
			((hcl_oop_liword_t)z)->slot[zs] = ~(hcl_liw_t)0;
			carry = 1;
			for (i = 0; i <= zs; i++)
			{
				w = (hcl_lidw_t)((hcl_liw_t)~((hcl_oop_liword_t)z)->slot[i]) + carry;
				carry = w >> HCL_LIW_BITS;
				((hcl_oop_liword_t)z)->slot[i] = (hcl_liw_t)w;
			}
			HCL_ASSERT (hcl, carry == 0);

			HCL_OBJ_SET_FLAGS_BRAND (z, HCL_BRAND_NBIGINT);
		}
		else if (negy)
		{
			/* x is positive, y is negative  */
			hcl_lidw_t w, carry;

			/* x & 2's complement on y up to ys */
			carry = 1;
			for (i = 0; i < ys; i++)
			{
				w = (hcl_lidw_t)((hcl_liw_t)~((hcl_oop_liword_t)y)->slot[i]) + carry;
				carry = w >> HCL_LIW_BITS;
				((hcl_oop_liword_t)z)->slot[i] = ((hcl_oop_liword_t)x)->slot[i] ^ (hcl_liw_t)w;
			}
			HCL_ASSERT (hcl, carry == 0);

			/* treat the lacking part in y as all 1s */
			for (; i < xs; i++)
			{
				((hcl_oop_liword_t)z)->slot[i] = ((hcl_oop_liword_t)x)->slot[i] ^ (~(hcl_liw_t)0);
			}

			goto adjust_to_negative;
		}
		else
		{
			/* both are positive */
			for (i = 0; i < ys; i++)
			{
				((hcl_oop_liword_t)z)->slot[i] = ((hcl_oop_liword_t)x)->slot[i] ^ ((hcl_oop_liword_t)y)->slot[i];
			}

			/* treat the lacking part in y as all 0s */
			for (; i < xs; i++)
			{
				((hcl_oop_liword_t)z)->slot[i] = ((hcl_oop_liword_t)x)->slot[i];
			}
		}

		return normalize_bigint(hcl, z);
	}

oops_einval:
	hcl_seterrbfmt (hcl, HCL_EINVAL, "parameter not integer - %O, %O", x, y);
	return HCL_NULL;
}

hcl_oop_t hcl_bitinvint (hcl_t* hcl, hcl_oop_t x)
{
	if (HCL_OOP_IS_SMOOI(x))
	{
		hcl_ooi_t v;

		v = HCL_OOP_TO_SMOOI(x);
		v = ~v;

		if (HCL_IN_SMOOI_RANGE(v)) return HCL_SMOOI_TO_OOP(v);
		return make_bigint_with_ooi (hcl, v);
	}
	else
	{
		hcl_oop_t z;
		hcl_oow_t i, xs, zs, zalloc;
		int negx;

		if (!is_bigint(hcl,x)) goto oops_einval;

		xs = HCL_OBJ_GET_SIZE(x);
		negx = HCL_IS_NBIGINT(hcl, x);

		if (negx)
		{
			zalloc = xs;
			zs = xs;
		}
		else
		{
			zalloc = xs + 1;
			zs = xs;
		}

		if (zalloc < zs)
		{
			/* overflow in zalloc calculation above */
			hcl_seterrnum (hcl, HCL_EOOMEM); /* TODO: is it a soft failure or hard failure? */
			return HCL_NULL;
		}

		hcl_pushtmp (hcl, &x);
		z = hcl_makebigint(hcl, HCL_BRAND_PBIGINT, HCL_NULL, zalloc);
		hcl_poptmp (hcl);
		if (!z) return HCL_NULL;

		if (negx)
		{
			hcl_lidw_t w, carry;

			carry = 1;
			for (i = 0; i < xs; i++)
			{
				w = (hcl_lidw_t)((hcl_liw_t)~((hcl_oop_liword_t)x)->slot[i]) + carry;
				carry = w >> HCL_LIW_BITS;
				((hcl_oop_liword_t)z)->slot[i] = ~(hcl_liw_t)w;
			}
			HCL_ASSERT (hcl, carry == 0);
		}
		else
		{
			hcl_lidw_t w, carry;

		#if 0
			for (i = 0; i < xs; i++)
			{
				((hcl_oop_liword_t)z)->slot[i] = ~((hcl_oop_liword_t)x)->slot[i];
			}

			((hcl_oop_liword_t)z)->slot[zs] = ~(hcl_liw_t)0;
			carry = 1;
			for (i = 0; i <= zs; i++)
			{
				w = (hcl_lidw_t)((hcl_liw_t)~((hcl_oop_liword_t)z)->slot[i]) + carry;
				carry = w >> HCL_LIW_BITS;
				((hcl_oop_liword_t)z)->slot[i] = (hcl_liw_t)w;
			}
			HCL_ASSERT (hcl, carry == 0);
		#else
			carry = 1;
			for (i = 0; i < xs; i++)
			{
				w = (hcl_lidw_t)(((hcl_oop_liword_t)x)->slot[i]) + carry;
				carry = w >> HCL_LIW_BITS;
				((hcl_oop_liword_t)z)->slot[i] = (hcl_liw_t)w;
			}
			HCL_ASSERT (hcl, i == zs);
			((hcl_oop_liword_t)z)->slot[i] = (hcl_liw_t)carry;
			HCL_ASSERT (hcl, (carry >> HCL_LIW_BITS) == 0);
		#endif

			HCL_OBJ_SET_FLAGS_BRAND (z, HCL_BRAND_NBIGINT);
		}

		return normalize_bigint(hcl, z);
	}

oops_einval:
	hcl_seterrbfmt (hcl, HCL_EINVAL, "parameter not integer - %O", x);
	return HCL_NULL;
}

static HCL_INLINE hcl_oop_t rshift_negative_bigint (hcl_t* hcl, hcl_oop_t x, hcl_oow_t shift)
{
	hcl_oop_t z;
	hcl_lidw_t w;
	hcl_lidw_t carry;
	hcl_oow_t i, xs;

	HCL_ASSERT (hcl, HCL_IS_NBIGINT(hcl, x));
	xs = HCL_OBJ_GET_SIZE(x);

	hcl_pushtmp (hcl, &x);
	/* +1 for the second inversion below */
	z = hcl_makebigint(hcl, HCL_BRAND_NBIGINT, HCL_NULL, xs + 1);
	hcl_poptmp (hcl);
	if (!z) return HCL_NULL;

	/* the following lines roughly for 'z = hcl_bitinv (hcl, x)' */
	carry = 1;
	for (i = 0; i < xs; i++)
	{
		w = (hcl_lidw_t)((hcl_liw_t)~((hcl_oop_liword_t)x)->slot[i]) + carry;
		carry = w >> HCL_LIW_BITS;
		((hcl_oop_liword_t)z)->slot[i] = ~(hcl_liw_t)w;
	}
	HCL_ASSERT (hcl, carry == 0);

	/* shift to the right */
	rshift_unsigned_array (((hcl_oop_liword_t)z)->slot, xs, shift);

	/* the following lines roughly for 'z = hcl_bitinv (hcl, z)' */
#if 0
	for (i = 0; i < xs; i++)
	{
		((hcl_oop_liword_t)z)->slot[i] = ~((hcl_oop_liword_t)z)->slot[i];
	}
	((hcl_oop_liword_t)z)->slot[xs] = ~(hcl_liw_t)0;

	carry = 1;
	for (i = 0; i <= xs; i++)
	{
		w = (hcl_lidw_t)((hcl_liw_t)~((hcl_oop_liword_t)z)->slot[i]) + carry;
		carry = w >> HCL_LIW_BITS;
		((hcl_oop_liword_t)z)->slot[i] = (hcl_liw_t)w;
	}
	HCL_ASSERT (hcl, carry == 0);
#else
	carry = 1;
	for (i = 0; i < xs; i++)
	{
		w = (hcl_lidw_t)(((hcl_oop_liword_t)z)->slot[i]) + carry;
		carry = w >> HCL_LIW_BITS;
		((hcl_oop_liword_t)z)->slot[i] = (hcl_liw_t)w;
	}
	((hcl_oop_liword_t)z)->slot[i] = (hcl_liw_t)carry;
	HCL_ASSERT (hcl, (carry >> HCL_LIW_BITS) == 0);
#endif

	return z; /* z is not normalized */
}

#if defined(HCL_LIMIT_OBJ_SIZE)
	/* nothing */
#else

static HCL_INLINE hcl_oop_t rshift_negative_bigint_and_normalize (hcl_t* hcl, hcl_oop_t x, hcl_oop_t y)
{
	hcl_oop_t z;
	hcl_oow_t shift;
	int sign;

	HCL_ASSERT (hcl, HCL_IS_NBIGINT(hcl, x));
	HCL_ASSERT (hcl, HCL_IS_NBIGINT(hcl, y));

	/* for convenience in subtraction below. 
	 * it could be HCL_TYPE_MAX(hcl_oow_t) 
	 * if make_bigint_with_intmax() or something
	 * similar were used instead of HCL_SMOOI_TO_OOP().*/
	shift = HCL_SMOOI_MAX;
	do
	{
		hcl_pushtmp (hcl, &y);
		z = rshift_negative_bigint (hcl, x, shift);
		hcl_poptmp (hcl);
		if (!z) return HCL_NULL;

		/* y is a negative number. use hcl_addints() until it becomes 0 */
		hcl_pushtmp (hcl, &z);
		y = hcl_addints (hcl, y, HCL_SMOOI_TO_OOP(shift));
		hcl_poptmp (hcl);
		if (!y) return HCL_NULL;

		sign = integer_to_oow (hcl, y, &shift);
		if (sign == 0) shift = HCL_SMOOI_MAX;
		else 
		{
			if (shift == 0)
			{
				/* no more shift */
				return normalize_bigint (hcl, z);
			}
			HCL_ASSERT (hcl, sign <= -1);
		}

		hcl_pushtmp (hcl, &y);
		x = normalize_bigint (hcl, z);
		hcl_poptmp (hcl);
		if (!x) return HCL_NULL;

		if (HCL_OOP_IS_SMOOI(x))
		{
			/* for normaization above, x can become a small integer */
			hcl_ooi_t v;

			v = HCL_OOP_TO_SMOOI(x);
			HCL_ASSERT (hcl, v < 0);

			/* normal right shift of a small negative integer */
			if (shift >= HCL_OOI_BITS - 1) 
			{
				/* when y is still a large integer, this condition is met
				 * met as HCL_SMOOI_MAX > HCL_OOI_BITS. so i can simly 
				 * terminate the loop after this */
				return HCL_SMOOI_TO_OOP(-1);
			}
			else 
			{
				v = (hcl_ooi_t)(((hcl_oow_t)v >> shift) | HCL_HBMASK(hcl_oow_t, shift));
				if (HCL_IN_SMOOI_RANGE(v)) 
					return HCL_SMOOI_TO_OOP(v);
				else 
					return make_bigint_with_ooi (hcl, v);
			}
		}
	}
	while (1);

	/* this part must not be reached */
	HCL_ASSERT (hcl, !"internal error - must not happen");
	hcl_seterrnum (hcl, HCL_EINTERN);
	return HCL_NULL;
}

static HCL_INLINE hcl_oop_t rshift_positive_bigint_and_normalize (hcl_t* hcl, hcl_oop_t x, hcl_oop_t y)
{
	hcl_oop_t z;
	hcl_oow_t zs, shift;
	int sign;

	HCL_ASSERT (hcl, HCL_IS_PBIGINT(hcl, x));
	HCL_ASSERT (hcl, HCL_IS_NBIGINT(hcl, y));

	zs = HCL_OBJ_GET_SIZE(x);

	hcl_pushtmp (hcl, &y);
	z = clone_bigint (hcl, x, zs);
	hcl_poptmp (hcl);
	if (!z) return HCL_NULL;

	/* for convenience in subtraction below. 
	 * it could be HCL_TYPE_MAX(hcl_oow_t) 
	 * if make_bigint_with_intmax() or something
	 * similar were used instead of HCL_SMOOI_TO_OOP().*/
	shift = HCL_SMOOI_MAX;
	do
	{
		rshift_unsigned_array (((hcl_oop_liword_t)z)->slot, zs, shift);
		if (count_effective (((hcl_oop_liword_t)z)->slot, zs) == 1 &&
		    ((hcl_oop_liword_t)z)->slot[0] == 0) 
		{
			/* if z is 0, i don't have to go on */
			break;
		}

		/* y is a negative number. use hcl_addints() until it becomes 0 */
		hcl_pushtmp (hcl, &z);
		y = hcl_addints (hcl, y, HCL_SMOOI_TO_OOP(shift));
		hcl_poptmp (hcl);
		if (!y) return HCL_NULL;

		sign = integer_to_oow (hcl, y, &shift);
		if (sign == 0) shift = HCL_SMOOI_MAX;
		else 
		{
			if (shift == 0) break;
			HCL_ASSERT (hcl, sign <= -1);
		}
	}
	while (1);

	return normalize_bigint(hcl, z);
}

static HCL_INLINE hcl_oop_t lshift_bigint_and_normalize (hcl_t* hcl, hcl_oop_t x, hcl_oop_t y)
{
	hcl_oop_t z;
	hcl_oow_t wshift, shift;
	int sign;

	HCL_ASSERT (hcl, HCL_IS_PBIGINT(hcl, y));

	/* this loop is very inefficient as shifting is repeated
	 * with lshift_unsigned_array(). however, this part of the
	 * code is not likey to be useful because the amount of
	 * memory available is certainly not enough to support 
	 * huge shifts greater than HCL_TYPE_MAX(hcl_oow_t) */
	shift = HCL_SMOOI_MAX;
	do
	{
		/* for convenience only in subtraction below. 
		 * should it be between HCL_SMOOI_MAX and HCL_TYPE_MAX(hcl_oow_t),
		 * the second parameter to hcl_subints() can't be composed
		 * using HCL_SMOOI_TO_OOP() */
		wshift = shift / HCL_LIW_BITS;
		if (shift > wshift * HCL_LIW_BITS) wshift++;

		hcl_pushtmp (hcl, &y);
		z = expand_bigint (hcl, x, wshift);
		hcl_poptmp (hcl);
		if (!z) return HCL_NULL;

		lshift_unsigned_array (((hcl_oop_liword_t)z)->slot, HCL_OBJ_GET_SIZE(z), shift);

		hcl_pushtmp (hcl, &y);
		x = normalize_bigint (hcl, z);
		hcl_poptmp (hcl);
		if (!x) return HCL_NULL;

		hcl_pushtmp (hcl, &x);
		y = hcl_subints (hcl, y, HCL_SMOOI_TO_OOP(shift));
		hcl_poptmp (hcl);
		if (!y) return HCL_NULL;

		sign = integer_to_oow (hcl, y, &shift);
		if (sign == 0) shift = HCL_SMOOI_MAX;
		else
		{
			if (shift == 0) 
			{
				HCL_ASSERT (hcl, is_normalized_integer (hcl, x));
				return x;
			}
			HCL_ASSERT (hcl, sign >= 1);
		}
	}
	while (1);

	/* this part must not be reached */
	HCL_ASSERT (hcl, !"internal error - must not happen");
	hcl_seterrnum (hcl, HCL_EINTERN);
	return HCL_NULL;
}

#endif

hcl_oop_t hcl_bitshiftint (hcl_t* hcl, hcl_oop_t x, hcl_oop_t y)
{
	/* left shift if y is positive,
	 * right shift if y is negative */

	if (HCL_OOP_IS_SMOOI(x) && HCL_OOP_IS_SMOOI(y))
	{
		hcl_ooi_t v1, v2;

		v1 = HCL_OOP_TO_SMOOI(x);
		v2 = HCL_OOP_TO_SMOOI(y);
		if (v1 == 0 || v2 == 0) 
		{
			/* return without cloning as x is a small integer */
			return x; 
		}

		if (v2 > 0)
		{
			/* left shift */
			hcl_oop_t z;
			hcl_oow_t wshift;

			wshift = v2 / HCL_LIW_BITS;
			if (v2 > wshift * HCL_LIW_BITS) wshift++;

			z = make_bloated_bigint_with_ooi (hcl, v1, wshift);
			if (!z) return HCL_NULL;

			lshift_unsigned_array (((hcl_oop_liword_t)z)->slot, HCL_OBJ_GET_SIZE(z), v2);
			return normalize_bigint (hcl, z);
		}
		else
		{
			/* right shift */
			hcl_ooi_t v;

			v2 = -v2;
			if (v1 < 0)
			{
				/* guarantee arithmetic shift preserving the sign bit
				 * regardless of compiler implementation.
				 *
				 *    binary    decimal   shifted by
				 *   -------------------------------
				 *   10000011    (-125)    0
				 *   11000001    (-63)     1
				 *   11100000    (-32)     2
				 *   11110000    (-16)     3
				 *   11111000    (-8)      4
				 *   11111100    (-4)      5
				 *   11111110    (-2)      6
				 *   11111111    (-1)      7
				 *   11111111    (-1)      8
				 */
				
				if (v2 >= HCL_OOI_BITS - 1) v = -1;
				else 
				{
					/* HCL_HBMASK_SAFE(hcl_oow_t, v2 + 1) could also be
					 * used as a mask. but the sign bit is shifted in. 
					 * so, masking up to 'v2' bits is sufficient */
					v = (hcl_ooi_t)(((hcl_oow_t)v1 >> v2) | HCL_HBMASK(hcl_oow_t, v2));
				}
			}
			else
			{
				if (v2 >= HCL_OOI_BITS) v = 0;
				else v = v1 >> v2;
			}
			if (HCL_IN_SMOOI_RANGE(v)) return HCL_SMOOI_TO_OOP(v);
			return make_bigint_with_ooi (hcl, v);
		}
	}
	else
	{
		int sign, negx, negy;
		hcl_oow_t shift;

		if (HCL_OOP_IS_SMOOI(x))
		{
			hcl_ooi_t v;

			if (!is_bigint(hcl,y)) goto oops_einval;

			v = HCL_OOP_TO_SMOOI(x);
			if (v == 0) return HCL_SMOOI_TO_OOP(0);

			if (HCL_IS_NBIGINT(hcl, y))
			{
				/* right shift - special case.
				 * x is a small integer. it is just a few bytes long.
				 * y is a large negative integer. its smallest absolute value
				 * is HCL_SMOOI_MAX. i know the final answer. */
				return (v < 0)? HCL_SMOOI_TO_OOP(-1): HCL_SMOOI_TO_OOP(0);
			}

			hcl_pushtmp (hcl, &y);
			x = make_bigint_with_ooi (hcl, v);
			hcl_poptmp (hcl);
			if (!x) return HCL_NULL;

			goto bigint_and_bigint;
		}
		else if (HCL_OOP_IS_SMOOI(y))
		{
			hcl_ooi_t v;

			if (!is_bigint(hcl,x)) goto oops_einval;

			v = HCL_OOP_TO_SMOOI(y);
			if (v == 0) return clone_bigint (hcl, x, HCL_OBJ_GET_SIZE(x));

			negx = HCL_IS_NBIGINT(hcl, x);
			if (v > 0)
			{
				sign = 1;
				negy = 0;
				shift = v;
				goto bigint_and_positive_oow;
			}
			else 
			{
				sign = -1;
				negy = 1;
				shift = -v;
				goto bigint_and_negative_oow;
			}
		}
		else
		{
			hcl_oop_t z;

			if (!is_bigint(hcl,x) || !is_bigint(hcl, y)) goto oops_einval;

		bigint_and_bigint:
			negx = HCL_IS_NBIGINT(hcl, x);
			negy = HCL_IS_NBIGINT(hcl, y);

			sign = bigint_to_oow (hcl, y, &shift);
			if (sign == 0)
			{
				/* y is too big or too small */
				if (negy)
				{
					/* right shift */
				#if defined(HCL_LIMIT_OBJ_SIZE)
					/* the maximum number of bit shifts are guaranteed to be
					 * small enough to fit into the hcl_oow_t type. so i can 
					 * easily assume that all bits are shifted out */
					HCL_ASSERT (hcl, HCL_OBJ_SIZE_BITS_MAX <= HCL_TYPE_MAX(hcl_oow_t));
					return (negx)? HCL_SMOOI_TO_OOP(-1): HCL_SMOOI_TO_OOP(0);
				#else
					if (negx)
						return rshift_negative_bigint_and_normalize (hcl, x, y);
					else
						return rshift_positive_bigint_and_normalize (hcl, x, y);
				#endif
				}
				else
				{
					/* left shift */
				#if defined(HCL_LIMIT_OBJ_SIZE)
					/* the maximum number of bit shifts are guaranteed to be
					 * small enough to fit into the hcl_oow_t type. so i can 
					 * simply return a failure here becuase it's surely too 
					 * large after shifting */
					HCL_ASSERT (hcl, HCL_TYPE_MAX(hcl_oow_t) >= HCL_OBJ_SIZE_BITS_MAX);
					hcl_seterrnum (hcl, HCL_EOOMEM); /* is it a soft failure or a hard failure? is this error code proper? */
					return HCL_NULL;
				#else
					return lshift_bigint_and_normalize (hcl, x, y);
				#endif
				}
			}
			else if (sign >= 1)
			{
				/* left shift */
				hcl_oow_t wshift;

			bigint_and_positive_oow:
				wshift = shift / HCL_LIW_BITS;
				if (shift > wshift * HCL_LIW_BITS) wshift++;

				z = expand_bigint (hcl, x, wshift);
				if (!z) return HCL_NULL;

				lshift_unsigned_array (((hcl_oop_liword_t)z)->slot, HCL_OBJ_GET_SIZE(z), shift);
			}
			else 
			{
				/* right shift */
			bigint_and_negative_oow:

				HCL_ASSERT (hcl, sign <= -1);

				if (negx)
				{
					z = rshift_negative_bigint (hcl, x, shift);
					if (!z) return HCL_NULL;
				}
				else
				{
					z = clone_bigint (hcl, x, HCL_OBJ_GET_SIZE(x));
					if (!z) return HCL_NULL;
					rshift_unsigned_array (((hcl_oop_liword_t)z)->slot, HCL_OBJ_GET_SIZE(z), shift);
				}
			}

			return normalize_bigint(hcl, z);
		}
	}

oops_einval:
	hcl_seterrbfmt (hcl, HCL_EINVAL, "parameter not integer - %O, %O", x, y);
	return HCL_NULL;
}

static hcl_uint8_t ooch_val_tab[] =
{
	99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99,
	99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99,
	99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99,
	 0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 99, 99, 99, 99, 99, 99,
	99, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24,
	25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 99, 99, 99, 99, 99,
	99, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24,
	25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 99, 99, 99, 99, 99
};

hcl_oop_t hcl_strtoint (hcl_t* hcl, const hcl_ooch_t* str, hcl_oow_t len, int radix)
{
	int sign = 1;
	const hcl_ooch_t* ptr, * start, * end;
	hcl_lidw_t w, v;
	hcl_liw_t hw[16], * hwp = HCL_NULL;
	hcl_oow_t hwlen, outlen;
	hcl_oop_t res;

	if (radix < 0) 
	{
		/* when radix is less than 0, it treats it as if '-' is preceeding */
		sign = -1;
		radix = -radix;
	}

	HCL_ASSERT (hcl, radix >= 2 && radix <= 36);

	ptr = str;
	end = str + len;

	if (ptr < end)
	{
		if (*ptr == '+') ptr++;
		else if (*ptr == '-') 
		{
			ptr++; 
			sign = -1;
		}
	}

	if (ptr >= end) goto oops_einval; /* no digits */

	while (ptr < end && *ptr == '0') 
	{
		/* skip leading zeros */
		ptr++;
	}
	if (ptr >= end)
	{
		/* all zeros */
		return HCL_SMOOI_TO_OOP(0);
	}

	hwlen = 0;
	start = ptr; /* this is the real start */

	if (IS_POWER_OF_2(radix))
	{
		unsigned int exp;
		unsigned int bitcnt;

		/* get log2(radix) in a fast way under the fact that
		 * radix is a power of 2. the exponent acquired is
		 * the number of bits that a digit of the given radix takes up */
	#if defined(HCL_HAVE_BUILTIN_CTZ)
		exp = __builtin_ctz(radix);

	#elif defined(__GNUC__) && (defined(__x86_64) || defined(__amd64) || defined(__i386) || defined(i386))
		/* use the Bit Scan Forward instruction */
		__asm__ volatile (
			"bsf %1,%0\n\t"
			: "=r"(exp) /* output */
			: "r"(radix) /* input */
		);

	#elif defined(USE_UGLY_CODE) && defined(__GNUC__) && defined(__arm__) && (defined(__ARM_ARCH_5__) || defined(__ARM_ARCH_6__) || defined(__ARM_ARCH_7__) || defined(__ARM_ARCH_8__))

		/* CLZ is available in ARMv5T and above. there is no instruction to
		 * count trailing zeros or something similar. using RBIT with CLZ
		 * would be good in ARMv6T2 and above to avoid further calculation
		 * afte CLZ */
		__asm__ volatile (
			"clz %0,%1\n\t"
			: "=r"(exp) /* output */
			: "r"(radix) /* input */
		);
		exp = (HCL_SIZEOF(exp) * 8) - exp - 1; 

		/* TODO: PPC - use cntlz, cntlzw, cntlzd, SPARC - use lzcnt, MIPS clz */
	#else
		exp = _exp_tab[radix];
	#endif

		/* bytes */
		outlen = ((hcl_oow_t)(end - str) * exp + 7) / 8; 
		/* number of hcl_liw_t */
		outlen = (outlen + HCL_SIZEOF(hw[0]) - 1) / HCL_SIZEOF(hw[0]);

		if (outlen > HCL_COUNTOF(hw)) 
		{
			hwp = (hcl_liw_t*)hcl_allocmem(hcl, outlen * HCL_SIZEOF(hw[0]));
			if (!hwp) return HCL_NULL;
		}
		else
		{
			hwp = hw;
		}

		w = 0;
		bitcnt = 0;
		ptr = end - 1;

		while (ptr >= start)
		{
			if (*ptr < 0 || *ptr >= HCL_COUNTOF(ooch_val_tab)) goto oops_einval;
			v = ooch_val_tab[*ptr];
			if (v >= radix) goto oops_einval;

			w |= (v << bitcnt);
			bitcnt += exp;
			if (bitcnt >= HCL_LIW_BITS)
			{
				bitcnt -= HCL_LIW_BITS;
				hwp[hwlen++] = w; /*(hcl_liw_t)(w & HCL_LBMASK(hcl_lidw_t, HCL_LIW_BITS));*/
				w >>= HCL_LIW_BITS;
			}

			ptr--;
		}

		HCL_ASSERT (hcl, w <= HCL_TYPE_MAX(hcl_liw_t));
		if (hwlen == 0 || w > 0) hwp[hwlen++] = w;
	}
	else
	{
		hcl_lidw_t r1, r2;
		hcl_liw_t multiplier;
		int dg, i, safe_ndigits;

		w = 0;
		ptr = start;

		safe_ndigits = hcl->bigint[radix].safe_ndigits;
		multiplier = (hcl_liw_t)hcl->bigint[radix].multiplier;

		outlen = (end - str) / safe_ndigits + 1;
		if (outlen > HCL_COUNTOF(hw)) 
		{
			hwp = (hcl_liw_t*)hcl_allocmem(hcl, outlen * HCL_SIZEOF(hcl_liw_t));
			if (!hwp) return HCL_NULL;
		}
		else
		{
			hwp = hw;
		}

		HCL_ASSERT (hcl, ptr < end);
		do
		{
			r1 = 0;
			for (dg = 0; dg < safe_ndigits; dg++)
			{
				if (ptr >= end) 
				{
					multiplier = 1;
					for (i = 0; i < dg; i++) multiplier *= radix;
					break;
				}

				if (*ptr < 0 || *ptr >= HCL_COUNTOF(ooch_val_tab)) goto oops_einval;
				v = ooch_val_tab[*ptr];
				if (v >= radix) goto oops_einval;

				r1 = r1 * radix + (hcl_liw_t)v;
				ptr++;
			}


			r2 = r1;
			for (i = 0; i < hwlen; i++)
			{
				hcl_liw_t high, low;

				v = (hcl_lidw_t)hwp[i] * multiplier;
				high = (hcl_liw_t)(v >> HCL_LIW_BITS);
				low = (hcl_liw_t)(v /*& HCL_LBMASK(hcl_oow_t, HCL_LIW_BITS)*/);

			#if defined(liw_add_overflow)
				/* use liw_add_overflow() only if it's compiler-builtin. */
				r2 = high + liw_add_overflow(low, r2, &low);
			#else
				/* don't use the fall-back version of liw_add_overflow() */
				low += r2;
				r2 = (hcl_lidw_t)high + (low < r2);
			#endif

				hwp[i] = low;
			}
			if (r2) hwp[hwlen++] = (hcl_liw_t)r2;
		}
		while (ptr < end);
	}

	HCL_ASSERT (hcl, hwlen >= 1);

#if (HCL_LIW_BITS == HCL_OOW_BITS)
	if (hwlen == 1) 
	{
		w = hwp[0];
		HCL_ASSERT (hcl, -HCL_SMOOI_MAX == HCL_SMOOI_MIN);
		if (w <= HCL_SMOOI_MAX) return HCL_SMOOI_TO_OOP((hcl_ooi_t)w * sign);
	}
#elif (HCL_LIW_BITS == HCL_OOHW_BITS)
	if (hwlen == 1) 
	{
		HCL_ASSERT (hcl, hwp[0] <= HCL_SMOOI_MAX);
		return HCL_SMOOI_TO_OOP((hcl_ooi_t)hwp[0] * sign);
	}
	else if (hwlen == 2)
	{
		w = MAKE_WORD(hwp[0], hwp[1]);
		HCL_ASSERT (hcl, -HCL_SMOOI_MAX == HCL_SMOOI_MIN);
		if (w <= HCL_SMOOI_MAX) return HCL_SMOOI_TO_OOP((hcl_ooi_t)w * sign);
	}
#else
#	error UNSUPPORTED LIW BIT SIZE
#endif

	res = hcl_makebigint(hcl, (sign < 0? HCL_BRAND_NBIGINT: HCL_BRAND_PBIGINT), hwp, hwlen);
	if (hwp && hw != hwp) hcl_freemem (hcl, hwp);

	return res;

oops_einval:
	if (hwp && hw != hwp) hcl_freemem (hcl, hwp);
	hcl_seterrnum (hcl, HCL_EINVAL);
	return HCL_NULL;
}

static hcl_oow_t oow_to_text (hcl_t* hcl, hcl_oow_t w, int radix, hcl_ooch_t* buf)
{
	hcl_ooch_t* ptr;
	const char* _digitc;

	if (radix < 0) 
	{

		_digitc = _digitc_lower;
		radix = -radix;
	}
	else
	{
		_digitc = _digitc_upper;
	}

	HCL_ASSERT (hcl, radix >= 2 && radix <= 36);

	ptr = buf;
	do
	{
		*ptr++ = _digitc[w % radix];
		w /= radix;
	}
	while (w > 0);

	return ptr - buf;
}

static void reverse_string (hcl_ooch_t* str, hcl_oow_t len)
{
	hcl_ooch_t ch;
	hcl_ooch_t* start = str;
	hcl_ooch_t* end = str + len - 1;

	while (start < end)
	{
		ch = *start;
		*start++ = *end;
		*end-- = ch;
	}
}

hcl_oop_t hcl_eqints (hcl_t* hcl, hcl_oop_t x, hcl_oop_t y)
{
	if (HCL_OOP_IS_SMOOI(x) && HCL_OOP_IS_SMOOI(y))
	{
		return (HCL_OOP_TO_SMOOI(x) == HCL_OOP_TO_SMOOI(y))? hcl->_true: hcl->_false;
	}
	else if (HCL_OOP_IS_SMOOI(x) || HCL_OOP_IS_SMOOI(y)) 
	{
		return hcl->_false;
	}
	else 
	{
		if (!is_bigint(hcl, x) || !is_bigint(hcl, y)) goto oops_einval;
		return is_equal(hcl, x, y)? hcl->_true: hcl->_false;
	}

oops_einval:
	hcl_seterrbfmt (hcl, HCL_EINVAL, "parameter not integer - %O, %O", x, y);
	return HCL_NULL;
}

hcl_oop_t hcl_neints (hcl_t* hcl, hcl_oop_t x, hcl_oop_t y)
{
	if (HCL_OOP_IS_SMOOI(x) && HCL_OOP_IS_SMOOI(y))
	{
		return (HCL_OOP_TO_SMOOI(x) != HCL_OOP_TO_SMOOI(y))? hcl->_true: hcl->_false;
	}
	else if (HCL_OOP_IS_SMOOI(x) || HCL_OOP_IS_SMOOI(y)) 
	{
		return hcl->_true;
	}
	else 
	{
		if (!is_bigint(hcl, x) || !is_bigint(hcl, y)) goto oops_einval;
		return !is_equal(hcl, x, y)? hcl->_true: hcl->_false;
	}

oops_einval:
	hcl_seterrbfmt (hcl, HCL_EINVAL, "parameter not integer - %O, %O", x, y);
	return HCL_NULL;
}

hcl_oop_t hcl_gtints (hcl_t* hcl, hcl_oop_t x, hcl_oop_t y)
{
	if (HCL_OOP_IS_SMOOI(x) && HCL_OOP_IS_SMOOI(y))
	{
		return (HCL_OOP_TO_SMOOI(x) > HCL_OOP_TO_SMOOI(y))? hcl->_true: hcl->_false;
	}
	else if (HCL_OOP_IS_SMOOI(x))
	{
		if (!is_bigint(hcl, y)) goto oops_einval;
		return (HCL_IS_NBIGINT(hcl, y))? hcl->_true: hcl->_false;
	}
	else if (HCL_OOP_IS_SMOOI(y)) 
	{
		if (!is_bigint(hcl, x)) goto oops_einval;
		return (HCL_IS_PBIGINT(hcl, x))? hcl->_true: hcl->_false;
	}
	else 
	{
		if (!is_bigint(hcl, x) || !is_bigint(hcl, y)) goto oops_einval;
		return is_greater(hcl, x, y)? hcl->_true: hcl->_false;
	}

oops_einval:
	hcl_seterrbfmt (hcl, HCL_EINVAL, "parameter not integer - %O, %O", x, y);
	return HCL_NULL;
}

hcl_oop_t hcl_geints (hcl_t* hcl, hcl_oop_t x, hcl_oop_t y)
{
	if (HCL_OOP_IS_SMOOI(x) && HCL_OOP_IS_SMOOI(y))
	{
		return (HCL_OOP_TO_SMOOI(x) >= HCL_OOP_TO_SMOOI(y))? hcl->_true: hcl->_false;
	}
	else if (HCL_OOP_IS_SMOOI(x))
	{
		if (!is_bigint(hcl, y)) goto oops_einval;
		return (HCL_IS_NBIGINT(hcl, y))? hcl->_true: hcl->_false;
	}
	else if (HCL_OOP_IS_SMOOI(y)) 
	{
		if (!is_bigint(hcl, x)) goto oops_einval;
		return (HCL_IS_PBIGINT(hcl, x))? hcl->_true: hcl->_false;
	}
	else 
	{
		if (!is_bigint(hcl, x) || !is_bigint(hcl, y)) goto oops_einval;
		return (is_greater(hcl, x, y) || is_equal(hcl, x, y))? hcl->_true: hcl->_false;
	}

oops_einval:
	hcl_seterrbfmt (hcl, HCL_EINVAL, "parameter not integer - %O, %O", x, y);
	return HCL_NULL;
}

hcl_oop_t hcl_ltints (hcl_t* hcl, hcl_oop_t x, hcl_oop_t y)
{
	if (HCL_OOP_IS_SMOOI(x) && HCL_OOP_IS_SMOOI(y))
	{
		return (HCL_OOP_TO_SMOOI(x) < HCL_OOP_TO_SMOOI(y))? hcl->_true: hcl->_false;
	}
	else if (HCL_OOP_IS_SMOOI(x))
	{
		if (!is_bigint(hcl, y)) goto oops_einval;
		return (HCL_IS_PBIGINT(hcl, y))? hcl->_true: hcl->_false;
	}
	else if (HCL_OOP_IS_SMOOI(y)) 
	{
		if (!is_bigint(hcl, x)) goto oops_einval;
		return (HCL_IS_NBIGINT(hcl, x))? hcl->_true: hcl->_false;
	}
	else 
	{
		if (!is_bigint(hcl, x) || !is_bigint(hcl, y)) goto oops_einval;
		return is_less(hcl, x, y)? hcl->_true: hcl->_false;
	}

oops_einval:
	hcl_seterrbfmt (hcl, HCL_EINVAL, "parameter not integer - %O, %O", x, y);
	return HCL_NULL;
}

hcl_oop_t hcl_leints (hcl_t* hcl, hcl_oop_t x, hcl_oop_t y)
{
	if (HCL_OOP_IS_SMOOI(x) && HCL_OOP_IS_SMOOI(y))
	{
		return (HCL_OOP_TO_SMOOI(x) <= HCL_OOP_TO_SMOOI(y))? hcl->_true: hcl->_false;
	}
	else if (HCL_OOP_IS_SMOOI(x))
	{
		if (!is_bigint(hcl, y)) goto oops_einval;
		return (HCL_IS_PBIGINT(hcl, y))? hcl->_true: hcl->_false;
	}
	else if (HCL_OOP_IS_SMOOI(y)) 
	{
		if (!is_bigint(hcl, x)) goto oops_einval;
		return (HCL_IS_NBIGINT(hcl, x))? hcl->_true: hcl->_false;
	}
	else 
	{
		if (!is_bigint(hcl, x) || !is_bigint(hcl, y)) goto oops_einval;
		return (is_less(hcl, x, y) || is_equal(hcl, x, y))? hcl->_true: hcl->_false;
	}

oops_einval:
	hcl_seterrbfmt (hcl, HCL_EINVAL, "parameter not integer - %O, %O", x, y);
	return HCL_NULL;
}

hcl_oop_t hcl_sqrtint (hcl_t* hcl, hcl_oop_t x)
{
	/* TODO: find a faster and more efficient algorithm??? */
	hcl_oop_t a, b, m, m2, t;
	int neg;

	if (!hcl_isint(hcl, x)) 
	{
		hcl_seterrbfmt (hcl, HCL_EINVAL, "parameter not integer - %O", x);
		return HCL_NULL;
	}

	a = hcl->_nil;
	b = hcl->_nil;
	m = hcl->_nil;
	m2 = hcl->_nil;

	hcl_pushtmp (hcl, &x);
	hcl_pushtmp (hcl, &a);
	hcl_pushtmp (hcl, &b);
	hcl_pushtmp (hcl, &m);
	hcl_pushtmp (hcl, &m2);

	a = hcl_ltints(hcl, x, HCL_SMOOI_TO_OOP(0));
	if (!a) goto oops;
	if (a == hcl->_true)
	{
		/* the given number is a negative number.
		 * i will arrange the return value to be negative. */
		x = hcl_negateint(hcl, x);
		if (!x) goto oops;
		neg = 1;
	}
	else neg = 0;

	a = HCL_SMOOI_TO_OOP(1);
	b = hcl_bitshiftint(hcl, x, HCL_SMOOI_TO_OOP(-5));
	if (!b) goto oops;
	b = hcl_addints(hcl, b, HCL_SMOOI_TO_OOP(8));
	if (!b) goto oops;

	while (1)
	{
		t = hcl_geints(hcl, b, a);
		if (!t) return HCL_NULL;
		if (t == hcl->_false) break;

		m = hcl_addints(hcl, a, b);
		if (!m) goto oops;
		m = hcl_bitshiftint(hcl, m, HCL_SMOOI_TO_OOP(-1));
		if (!m) goto oops;
		m2 = hcl_mulints(hcl, m, m);
		if (!m2) goto oops;
		t = hcl_gtints(hcl, m2, x);
		if (!t) return HCL_NULL;
		if (t == hcl->_true)
		{
			b = hcl_subints(hcl, m, HCL_SMOOI_TO_OOP(1));
			if (!b) goto oops;
		}
		else
		{
			a = hcl_addints(hcl, m, HCL_SMOOI_TO_OOP(1));
			if (!a) goto oops;
		}
	}

	hcl_poptmps (hcl, 5);
	x = hcl_subints(hcl, a, HCL_SMOOI_TO_OOP(1));
	if (!x) return HCL_NULL;

	if (neg) x = hcl_negateint(hcl, x);
	return x;

oops:
	hcl_poptmps (hcl, 5);
	return HCL_NULL;
}


hcl_oop_t hcl_absint (hcl_t* hcl, hcl_oop_t x)
{
	if (HCL_OOP_IS_SMOOI(x))
	{
		hcl_ooi_t v;
		v = HCL_OOP_TO_SMOOI(x);
		if (v < 0) 
		{
			v = -v;
			x = HCL_SMOOI_TO_OOP(v);
		}
	}
	else if (HCL_IS_NBIGINT(hcl, x))
	{
		x = _clone_bigint(hcl, x, HCL_OBJ_GET_SIZE(x), HCL_BRAND_PBIGINT);
	}
	else if (HCL_IS_PBIGINT(hcl, x))
	{
		/* do nothing. return x without change.
		 * [THINK] but do i need to clone a positive bigint? */
	}
	else
	{
		hcl_seterrbfmt (hcl, HCL_EINVAL, "parameter not integer - %O", x);
		return HCL_NULL;
	}

	return x;
}

hcl_oop_t hcl_inttostr (hcl_t* hcl, hcl_oop_t num, int radix, int ngc)
{
	hcl_ooi_t v = 0;
	hcl_oow_t w;
	hcl_oow_t as, bs, rs;
#if (HCL_LIW_BITS == HCL_OOW_BITS)
	hcl_liw_t b[1];
#elif (HCL_LIW_BITS == HCL_OOHW_BITS)
	hcl_liw_t b[2];
#else
#	error UNSUPPORTED LIW BIT SIZE
#endif
	hcl_liw_t* a, * q, * r;
	hcl_liw_t* t = HCL_NULL;
	hcl_ooch_t* xbuf = HCL_NULL;
	hcl_oow_t xlen = 0, seglen, reqcapa;

	const char* _digitc;
	int orgradix = radix;

	if (radix < 0) 
	{
		_digitc = _digitc_lower;
		radix = -radix;
	}
	else
	{
		_digitc = _digitc_upper;
	}
	HCL_ASSERT (hcl, radix >= 2 && radix <= 36);

	if (!hcl_isint(hcl,num)) 
	{
		hcl_seterrbfmt (hcl, HCL_EINVAL, "parameter not integer - %O", num);
		return HCL_NULL;
	}
	v = integer_to_oow(hcl, num, &w);

	if (v)
	{
		/* The largest buffer is required for radix 2.
		 * For a binary conversion(radix 2), the number of bits is
		 * the maximum number of digits that can be produced. +1 is
		 * needed for the sign. */

		reqcapa = HCL_OOW_BITS + 1;
		if (hcl->inttostr.xbuf.capa < reqcapa)
		{
			xbuf = (hcl_ooch_t*)hcl_reallocmem(hcl, hcl->inttostr.xbuf.ptr, reqcapa * HCL_SIZEOF(*xbuf));
			if (!xbuf) return HCL_NULL;
			hcl->inttostr.xbuf.capa = reqcapa;
			hcl->inttostr.xbuf.ptr = xbuf;
		}
		else
		{
			xbuf = hcl->inttostr.xbuf.ptr;
		}

		xlen = oow_to_text(hcl, w, orgradix, xbuf);
		if (v < 0) xbuf[xlen++] = '-';

		reverse_string (xbuf, xlen);
		if (ngc < 0)
		{
			/* special case. don't create a new object.
			 * the caller can use the data left in hcl->inttostr.xbuf */
			hcl->inttostr.xbuf.len = xlen;
			return hcl->_nil;
		}
		return hcl_makestring(hcl, xbuf, xlen, ngc);
	}

	as = HCL_OBJ_GET_SIZE(num);

	if (IS_POWER_OF_2(radix))
	{
		unsigned int exp, accbits;
		hcl_lidw_t acc;
		hcl_oow_t xpos;

		exp = _exp_tab[radix];
		xlen = as * ((HCL_LIW_BITS + exp) / exp) + 1;
		xpos = xlen;

		reqcapa = xlen; 
		if (hcl->inttostr.xbuf.capa < reqcapa)
		{
			xbuf = (hcl_ooch_t*)hcl_reallocmem(hcl, hcl->inttostr.xbuf.ptr, reqcapa * HCL_SIZEOF(*xbuf));
			if (!xbuf) return HCL_NULL;
			hcl->inttostr.xbuf.capa = reqcapa;
			hcl->inttostr.xbuf.ptr = xbuf;
		}
		else
		{
			xbuf = hcl->inttostr.xbuf.ptr;
		}

		acc = 0;
		accbits = 0;

		w = 0;
		while (w < as)
		{
			acc |= (hcl_lidw_t)((hcl_oop_liword_t)num)->slot[w] << accbits;
			accbits += HCL_LIW_BITS;

			w++;
			do
			{
				xbuf[--xpos] = _digitc[acc & (radix - 1)]; /* acc % radix */
				accbits -= exp;
				acc >>= exp;
				if (w < as)
				{
					if (accbits < exp) break;
				}
				else
				{
					if (acc <= 0) break;
				}
			}
			while (1);
		}

		HCL_ASSERT (hcl, xpos >= 1);
		if (HCL_IS_NBIGINT(hcl, num)) xbuf[--xpos] = '-';

		if (ngc < 0)
		{
			/* special case. don't create a new object.
			 * the caller can use the data left in hcl->inttostr.xbuf */
			HCL_MEMMOVE (&xbuf[0], &xbuf[xpos], HCL_SIZEOF(*xbuf) * (xlen - xpos));
			hcl->inttostr.xbuf.len = xlen - xpos;
			return hcl->_nil;
		}

		return hcl_makestring(hcl, &xbuf[xpos], xlen - xpos, ngc);
	}

	/* Do it in a hard way for other cases */
/* TODO: find an optimial buffer size */
	reqcapa = as * HCL_LIW_BITS + 1; 
	if (hcl->inttostr.xbuf.capa < reqcapa)
	{
		xbuf = (hcl_ooch_t*)hcl_reallocmem(hcl, hcl->inttostr.xbuf.ptr, reqcapa * HCL_SIZEOF(*xbuf));
		if (!xbuf) return HCL_NULL;
		hcl->inttostr.xbuf.capa = reqcapa;
		hcl->inttostr.xbuf.ptr = xbuf;
	}
	else
	{
		xbuf = hcl->inttostr.xbuf.ptr;
	}

	reqcapa = as * 3;
	if (hcl->inttostr.t.capa < reqcapa)
	{
		t = (hcl_liw_t*)hcl_reallocmem(hcl, hcl->inttostr.t.ptr, reqcapa * HCL_SIZEOF(*t));
		if (!t) return HCL_NULL;
		hcl->inttostr.t.capa = reqcapa;
		hcl->inttostr.t.ptr = t;
	}
	else 
	{
		t = hcl->inttostr.t.ptr;
	}

#if (HCL_LIW_BITS == HCL_OOW_BITS)
	b[0] = hcl->bigint[radix].multiplier; /* block divisor */
	bs = 1;
#elif (HCL_LIW_BITS == HCL_OOHW_BITS)
	b[0] = hcl->bigint[radix].multiplier /*& HCL_LBMASK(hcl_oow_t, HCL_LIW_BITS)*/;
	b[1] = hcl->bigint[radix].multiplier >> HCL_LIW_BITS;
	bs = (b[1] > 0)? 2: 1;
#else
#	error UNSUPPORTED LIW BIT SIZE
#endif

	a = &t[0];
	q = &t[as];
	r = &t[as * 2];

	HCL_MEMCPY (a, ((hcl_oop_liword_t)num)->slot, HCL_SIZEOF(*a) * as);

	do
	{
		if (is_less_unsigned_array (b, bs, a, as))
		{
			hcl_liw_t* tmp;

			divide_unsigned_array (hcl, a, as, b, bs, q, r);

			/* get 'rs' before 'as' gets changed */
			rs = count_effective (r, as); 
 
			/* swap a and q for later division */
			tmp = a;
			a = q;
			q = tmp;

			as = count_effective (a, as);
		}
		else
		{
			/* it is the last block */
			r = a;
			rs = as;
		}

	#if (HCL_LIW_BITS == HCL_OOW_BITS)
		HCL_ASSERT (hcl, rs == 1);
		w = r[0];
	#elif (HCL_LIW_BITS == HCL_OOHW_BITS)
		if (rs == 1) w = r[0];
		else 
		{
			HCL_ASSERT (hcl, rs == 2);
			w = MAKE_WORD (r[0], r[1]);
		}
	#else
	#	error UNSUPPORTED LIW BIT SIZE
	#endif
		seglen = oow_to_text (hcl, w, orgradix, &xbuf[xlen]);
		xlen += seglen;
		if (r == a) break; /* reached the last block */

		/* fill unfilled leading digits with zeros as it's not 
		 * the last block */
		while (seglen < hcl->bigint[radix].safe_ndigits)
		{
			xbuf[xlen++] = '0';
			seglen++;
		}
	}
	while (1);

	if (HCL_IS_NBIGINT(hcl, num)) xbuf[xlen++] = '-';
	reverse_string (xbuf, xlen);
	if (ngc < 0)
	{
		/* special case. don't create a new object.
		 * the caller can use the data left in hcl->inttostr.xbuf */
		hcl->inttostr.xbuf.len = xlen;
		return hcl->_nil;
	}

	return hcl_makestring(hcl, xbuf, xlen, ngc);
}
