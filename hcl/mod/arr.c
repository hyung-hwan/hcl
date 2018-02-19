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


#include "_arr.h"

static hcl_pfrc_t pf_arr_get (hcl_t* hcl, hcl_ooi_t nargs)
{
	hcl_oop_oop_t arr;
	hcl_oop_t idx;
	hcl_oow_t index;

	arr = (hcl_oop_oop_t)HCL_STACK_GETARG(hcl, nargs, 0);
	idx = HCL_STACK_GETARG(hcl, nargs, 1);

	if (!HCL_IS_ARRAY(hcl,arr))
	{
		hcl_seterrbfmt (hcl, HCL_EINVAL, "parameter not an array - %O", arr);
		return HCL_PF_FAILURE;
	}

	if (hcl_inttooow(hcl, idx, &index) == 0) return HCL_PF_FAILURE;

	if (index >= HCL_OBJ_GET_SIZE(arr))
	{
		hcl_seterrbfmt (hcl, HCL_EINVAL, "array index(%zu) out of bounds(0-%zu)", index, HCL_OBJ_GET_SIZE(arr) - 1);
		return HCL_PF_FAILURE;
	}

	HCL_STACK_SETRET (hcl, nargs, arr->slot[index]);
	return HCL_PF_SUCCESS;
}

static hcl_pfrc_t pf_arr_put (hcl_t* hcl, hcl_ooi_t nargs)
{
	hcl_oop_oop_t arr;
	hcl_oop_t idx, val;
	hcl_oow_t index;

	arr = (hcl_oop_oop_t)HCL_STACK_GETARG(hcl, nargs, 0);
	idx = HCL_STACK_GETARG(hcl, nargs, 1);
	val = HCL_STACK_GETARG(hcl, nargs, 2);

	if (!HCL_IS_ARRAY(hcl,arr))
	{
		hcl_seterrbfmt (hcl, HCL_EINVAL, "parameter not an array - %O", arr);
		return HCL_PF_FAILURE;
	}

	if (hcl_inttooow(hcl, idx, &index) == 0) return HCL_PF_FAILURE;

	if (index >= HCL_OBJ_GET_SIZE(arr))
	{
		hcl_seterrbfmt (hcl, HCL_EINVAL, "array index(%zu) out of bounds(0-%zu)", index, HCL_OBJ_GET_SIZE(arr) - 1);
		return HCL_PF_FAILURE;
	}

	arr->slot[index] = val;
	HCL_STACK_SETRET (hcl, nargs, val);
	return HCL_PF_SUCCESS;
}

static hcl_pfinfo_t pfinfos[] =
{
	{ { 'g','e','t','\0' },      0, { pf_arr_get,     2,  2 } },
/*	{ { 'm','a','k','e','\0' },  0, { pf_arr_make,    1,  1 } },*/
	{ { 'p','u','t','\0' },      0, { pf_arr_put,     3,  3 } }
};

/* ------------------------------------------------------------------------ */

static hcl_pfbase_t* query (hcl_t* hcl, hcl_mod_t* mod, const hcl_ooch_t* name, hcl_oow_t namelen)
{
	return hcl_findpfbase(hcl, pfinfos, HCL_COUNTOF(pfinfos), name, namelen);
}


static void unload (hcl_t* hcl, hcl_mod_t* mod)
{
}

int hcl_mod_arr (hcl_t* hcl, hcl_mod_t* mod)
{
	mod->query = query;
	mod->unload = unload; 
	mod->ctx = HCL_NULL;
	return 0;
}
