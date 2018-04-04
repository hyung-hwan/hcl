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


static hcl_ooi_t equalize_scale (hcl_t* hcl, hcl_oop_t* x, hcl_oop_t* y)
{
	hcl_ooi_t xs, ys;
	hcl_oop_t nv;
	hcl_oop_t xv, yv;

	/* this function assumes that x and y are protected by the caller */

	xs = 0;
	xv = *x;
	if (HCL_IS_FPDEC(hcl, xv))
	{
		xs = HCL_OOP_TO_SMOOI(((hcl_oop_fpdec_t)xv)->scale);
		xv = ((hcl_oop_fpdec_t)xv)->value;
	}
	else if (!hcl_isint(hcl, xv))
	{
		hcl_seterrbfmt (hcl, HCL_EINVAL, "parameter not numeric - %O", xv);
		return -1;
	}
	
	ys = 0;
	yv = *y;
	if (HCL_IS_FPDEC(hcl, *y))
	{
		ys = HCL_OOP_TO_SMOOI(((hcl_oop_fpdec_t)yv)->scale);
		yv = ((hcl_oop_fpdec_t)yv)->value;
	}
	else if (!hcl_isint(hcl, yv))
	{
		hcl_seterrbfmt (hcl, HCL_EINVAL, "parameter not numeric - %O", yv);
		return -1;
	}

	if (xs < ys)
	{
		nv = xv;
		while (xs < ys)
		{
			/* TODO: optmize this. less multiplications */
			nv = hcl_mulints(hcl, nv, HCL_SMOOI_TO_OOP(10));
			if (!nv) return -1;
			xs++;
		}

		nv = hcl_makefpdec(hcl, nv, xs);
		if (!nv) return -1;

		*x = nv;
	}
	else if (xs > ys)
	{
		nv = yv;
		while (ys < xs)
		{
			nv = hcl_mulints(hcl, nv, HCL_SMOOI_TO_OOP(10));
			if (!nv) return -1;
			ys++;
		}

		nv = hcl_makefpdec(hcl, nv, ys);
		if (!nv) return -1;

		*y = nv;
	}

	return xs;
}

hcl_oop_t hcl_truncfpdecval (hcl_t* hcl, hcl_oop_t iv, hcl_ooi_t cs, hcl_ooi_t ns)
{
	/* this function truncates an existing fixed-point decimal.
	 * it doesn't create a new object */

	if (cs > ns)
	{
		do
		{
			/* TODO: optimizatino... less divisions */
			iv = hcl_divints(hcl, iv, HCL_SMOOI_TO_OOP(10), 0, HCL_NULL);
			if (!iv) return HCL_NULL;
			cs--;
		}
		while (cs > ns);
	}

	return iv;
}

hcl_oop_t hcl_addnums (hcl_t* hcl, hcl_oop_t x, hcl_oop_t y)
{
	if (!HCL_IS_FPDEC(hcl, x) && !HCL_IS_FPDEC(hcl, y))
	{
		/* both are probably integers */
		return hcl_addints(hcl, x, y);
	}
	else
	{
		hcl_oop_t v;
		hcl_ooi_t scale;

		hcl_pushtmp (hcl, &x);
		hcl_pushtmp (hcl, &y);

		scale = equalize_scale(hcl, &x, &y);
		if (scale <= -1) 
		{
			hcl_poptmps (hcl, 2);
			return HCL_NULL;
		}
		v = hcl_addints(hcl, ((hcl_oop_fpdec_t)x)->value, ((hcl_oop_fpdec_t)y)->value);
		hcl_poptmps (hcl, 2);
		if (!v) return HCL_NULL;

		return hcl_makefpdec(hcl, v, scale);
	}
}

hcl_oop_t hcl_subnums (hcl_t* hcl, hcl_oop_t x, hcl_oop_t y)
{
	if (!HCL_IS_FPDEC(hcl, x) && !HCL_IS_FPDEC(hcl, y))
	{
		/* both are probably integers */
		return hcl_subints(hcl, x, y);
	}
	else
	{
		hcl_oop_t v;
		hcl_ooi_t scale;

		hcl_pushtmp (hcl, &x);
		hcl_pushtmp (hcl, &y);

		scale = equalize_scale(hcl, &x, &y);
		if (scale <= -1) 
		{
			hcl_poptmps (hcl, 2);
			return HCL_NULL;
		}
		v = hcl_subints(hcl, ((hcl_oop_fpdec_t)x)->value, ((hcl_oop_fpdec_t)y)->value);
		hcl_poptmps (hcl, 2);
		if (!v) return HCL_NULL;

		return hcl_makefpdec(hcl, v, scale);
	}
}

static hcl_oop_t mul_nums (hcl_t* hcl, hcl_oop_t x, hcl_oop_t y, int mult)
{
	hcl_ooi_t xs, ys, cs, ns;
	hcl_oop_t nv;
	hcl_oop_t xv, yv;

	xs = 0;
	xv = x;
	if (HCL_IS_FPDEC(hcl, xv))
	{
		xs = HCL_OOP_TO_SMOOI(((hcl_oop_fpdec_t)xv)->scale);
		xv = ((hcl_oop_fpdec_t)xv)->value;
	}
	else if (!hcl_isint(hcl, xv))
	{
		hcl_seterrbfmt (hcl, HCL_EINVAL, "parameter not numeric - %O", xv);
		return HCL_NULL;
	}
	
	ys = 0;
	yv = y;
	if (HCL_IS_FPDEC(hcl, y))
	{
		ys = HCL_OOP_TO_SMOOI(((hcl_oop_fpdec_t)yv)->scale);
		yv = ((hcl_oop_fpdec_t)yv)->value;
	}
	else if (!hcl_isint(hcl, yv))
	{
		hcl_seterrbfmt (hcl, HCL_EINVAL, "parameter not numeric - %O", yv);
		return HCL_NULL;
	}

	nv = hcl_mulints(hcl, xv, yv);
	if (!nv) return HCL_NULL;

	cs = xs + ys; 
	if (cs <= 0) return nv; /* the result must be an integer */

	ns = (mult || xs > ys)? xs: ys;

	/* cs may be larger than HCL_SMOOI_MAX. but ns is guaranteed to be
	 * equal to or less than HCL_SMOOI_MAX */
	HCL_ASSERT (hcl, ns <= HCL_SMOOI_MAX);

	nv = hcl_truncfpdecval(hcl, nv, cs, ns);
	if (!nv) return HCL_NULL;

	return (ns <= 0)? nv: hcl_makefpdec(hcl, nv, ns);
}

hcl_oop_t hcl_mulnums (hcl_t* hcl, hcl_oop_t x, hcl_oop_t y)
{
	/* (* 1.00 12.123) => 12.123 */
	return mul_nums(hcl, x, y, 0);
}

hcl_oop_t hcl_mltnums (hcl_t* hcl, hcl_oop_t x, hcl_oop_t y)
{
	/* (mlt 1.00 12.123) =>  12.12 */
	return mul_nums(hcl, x, y, 1);
}

hcl_oop_t hcl_divnums (hcl_t* hcl, hcl_oop_t x, hcl_oop_t y)
{
	hcl_ooi_t xs, ys, i;
	hcl_oop_t nv;
	hcl_oop_t xv, yv;

	xs = 0;
	xv = x;
	if (HCL_IS_FPDEC(hcl, xv))
	{
		xs = HCL_OOP_TO_SMOOI(((hcl_oop_fpdec_t)xv)->scale);
		xv = ((hcl_oop_fpdec_t)xv)->value;
	}
	else if (!hcl_isint(hcl, xv))
	{
		hcl_seterrbfmt (hcl, HCL_EINVAL, "parameter not numeric - %O", xv);
		return HCL_NULL;
	}

	ys = 0;
	yv = y;
	if (HCL_IS_FPDEC(hcl, y))
	{
		ys = HCL_OOP_TO_SMOOI(((hcl_oop_fpdec_t)yv)->scale);
		yv = ((hcl_oop_fpdec_t)yv)->value;
	}
	else if (!hcl_isint(hcl, yv))
	{
		hcl_seterrbfmt (hcl, HCL_EINVAL, "parameter not numeric - %O", yv);
		return HCL_NULL;
	}

	nv = xv;

	hcl_pushtmp (hcl, &yv);
	for (i = 0; i < ys; i++)
	{
		nv = hcl_mulints(hcl, nv, HCL_SMOOI_TO_OOP(10));
		if (!nv) 
		{
			hcl_poptmp (hcl);
			return HCL_NULL;
		}
	}

	nv = hcl_divints(hcl, nv, yv, 0, HCL_NULL);
	hcl_poptmp (hcl);
	if (!nv) return HCL_NULL;

	return hcl_makefpdec(hcl, nv, xs);
}

static hcl_oop_t comp_nums (hcl_t* hcl, hcl_oop_t x, hcl_oop_t y, hcl_oop_t (*comper) (hcl_t*, hcl_oop_t, hcl_oop_t))
{
	if (!HCL_IS_FPDEC(hcl, x) && !HCL_IS_FPDEC(hcl, y))
	{
		/* both are probably integers */
		return comper(hcl, x, y);
	}
	else
	{
		hcl_oop_t v;
		hcl_ooi_t scale;

		hcl_pushtmp (hcl, &x);
		hcl_pushtmp (hcl, &y);

		scale = equalize_scale(hcl, &x, &y);
		if (scale <= -1) 
		{
			hcl_poptmps (hcl, 2);
			return HCL_NULL;
		}
		v = comper(hcl, ((hcl_oop_fpdec_t)x)->value, ((hcl_oop_fpdec_t)y)->value);
		hcl_poptmps (hcl, 2);
		return v;
	}
}


hcl_oop_t hcl_gtnums (hcl_t* hcl, hcl_oop_t x, hcl_oop_t y)
{
	return comp_nums(hcl, x, y, hcl_gtints);
}
hcl_oop_t hcl_genums (hcl_t* hcl, hcl_oop_t x, hcl_oop_t y)
{
	return comp_nums(hcl, x, y, hcl_geints);
}


hcl_oop_t hcl_ltnums (hcl_t* hcl, hcl_oop_t x, hcl_oop_t y)
{
	return comp_nums(hcl, x, y, hcl_ltints);
}
hcl_oop_t hcl_lenums (hcl_t* hcl, hcl_oop_t x, hcl_oop_t y)
{
	return comp_nums(hcl, x, y, hcl_leints);
}

hcl_oop_t hcl_eqnums (hcl_t* hcl, hcl_oop_t x, hcl_oop_t y)
{
	return comp_nums(hcl, x, y, hcl_eqints);
}
hcl_oop_t hcl_nenums (hcl_t* hcl, hcl_oop_t x, hcl_oop_t y)
{
	return comp_nums(hcl, x, y, hcl_neints);
}

hcl_oop_t hcl_sqrtnum (hcl_t* hcl, hcl_oop_t x)
{
	if (!HCL_IS_FPDEC(hcl, x))
	{
		return hcl_sqrtint(hcl, x);
	}
	else
	{
		hcl_oop_t v;
		hcl_ooi_t i, scale;

		scale = HCL_OOP_TO_SMOOI(((hcl_oop_fpdec_t)x)->scale);

		v = ((hcl_oop_fpdec_t)x)->value;
		for (i = 0; i < scale ; i++)
		{
			v = hcl_mulints(hcl, v, HCL_SMOOI_TO_OOP(10));
			if (!v)
			{
				hcl_poptmp (hcl);
				return HCL_NULL;
			}
		}

		v = hcl_sqrtint(hcl, v);
		if (!v) return HCL_NULL;

		return hcl_makefpdec(hcl, v, scale);
	}
}
