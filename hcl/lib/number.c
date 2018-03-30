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

hcl_oop_t hcl_mulnums (hcl_t* hcl, hcl_oop_t x, hcl_oop_t y)
{
	hcl_ooi_t xs, ys, scale;
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

	scale = xs + ys;
	if (scale > HCL_SMOOI_MAX)
	{
		/* TODO: limit scale */
	}

	return hcl_makefpdec(hcl, nv, scale);
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

	hcl_pushtmp (hcl, &y);
	for (i = 0; i < ys; i++)
	{
		nv = hcl_mulints(hcl, nv, HCL_SMOOI_TO_OOP(10));
		if (!nv) 
		{
			hcl_poptmp (hcl);
			return HCL_NULL;
		}
	}

	nv = hcl_divints(hcl, nv, y, 0, HCL_NULL);
	hcl_poptmp (hcl);
	if (!nv) return HCL_NULL;

	return hcl_makefpdec(hcl, nv, xs);
}
