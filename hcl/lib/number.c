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

static hcl_ooi_t equalize_scale (hcl_t* hcl, hcl_oop_fpdec_t x, hcl_oop_fpdec_t y)
{
	hcl_ooi_t xs, ys;
	
	xs = HCL_OOP_TO_SMOOI(x->scale);
	ys = HCL_OOP_TO_SMOOI(y->scale);

	if (xs < ys)
	{
	/* TODO: don't change x or y. create new objects */
		x->scale = y->scale;
		hcl_pushtmp(hcl, &x);
		while (xs < ys)
		{
			x->value = hcl_mulints(hcl, x->value, HCL_SMOOI_TO_OOP(10));
			xs++;
		}
		hcl_poptmp(hcl);
	}
	else if (xs > ys)
	{
		y->scale = x->scale;
		hcl_pushtmp(hcl, &y);
		while (ys < xs)
		{
			y->value = hcl_mulints(hcl, y->value, HCL_SMOOI_TO_OOP(10));
			ys++;
		}
		hcl_poptmp(hcl);
	}

	return xs;
}

hcl_oop_t hcl_addnums (hcl_t* hcl, hcl_oop_t x, hcl_oop_t y)
{
	if (HCL_IS_FPDEC(hcl, x))
	{
		if (HCL_IS_FPDEC(hcl, y))
		{
			hcl_oop_t v;
			hcl_ooi_t scale;

/* TODO: error handling */
			hcl_pushtmp (hcl, &x);
			hcl_pushtmp (hcl, &y);
			scale = equalize_scale (hcl, x, y);
			v = hcl_addints(hcl, ((hcl_oop_fpdec_t)x)->value, ((hcl_oop_fpdec_t)y)->value);
			hcl_poptmps (hcl, 2);
			return hcl_makefpdec(hcl, v, scale);
		}
		else
		{
		}
	}
	else
	{
		if (HCL_IS_FPDEC(hcl, y))
		{
		}
		else
		{
			return hcl_addints(hcl, x, y);
		}
	}
}

hcl_oop_t hcl_subnums (hcl_t* hcl, hcl_oop_t x, hcl_oop_t y)
{
	if (HCL_IS_FPDEC(hcl, x))
	{
		if (HCL_IS_FPDEC(hcl, y))
		{
			hcl_oop_t v;
			hcl_ooi_t scale;

			hcl_pushtmp (hcl, &x);
			hcl_pushtmp (hcl, &y);
			scale = equalize_scale (hcl, x, y);
			v = hcl_subints(hcl, ((hcl_oop_fpdec_t)x)->value, ((hcl_oop_fpdec_t)y)->value);
			hcl_poptmps (hcl, 2);
			return hcl_makefpdec(hcl, v, scale);
		}
		else
		{
		}
	}
	else
	{
		if (HCL_IS_FPDEC(hcl, y))
		{
		}
		else
		{
			return hcl_subints(hcl, x, y);
		}
	}
}
