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


#include "_dic.h"

static hcl_pfrc_t pf_dic_get (hcl_t* hcl, hcl_mod_t* mod, hcl_ooi_t nargs)
{
	hcl_oop_t dic;
	hcl_oop_t key;
	hcl_oop_cons_t pair;

	dic = HCL_STACK_GETARG(hcl, nargs, 0);
	key = HCL_STACK_GETARG(hcl, nargs, 1);

	if (!HCL_IS_DIC(hcl,dic))
	{
		hcl_seterrbfmt (hcl, HCL_EINVAL, "parameter not an dictionary - %O", dic);
		return HCL_PF_FAILURE;
	}

	pair = hcl_getatdic(hcl, (hcl_oop_dic_t)dic, key);
	if (!pair)
	{
		HCL_STACK_SETRETTOERROR (hcl, nargs, HCL_ENOENT);
		return HCL_PF_SUCCESS;
	}

	HCL_STACK_SETRET (hcl, nargs, HCL_CONS_CDR(pair));
	return HCL_PF_SUCCESS;
}


static hcl_pfrc_t pf_dic_put (hcl_t* hcl, hcl_mod_t* mod, hcl_ooi_t nargs)
{
	hcl_oop_t dic;
	hcl_oop_t key, val;
	hcl_oop_cons_t pair;

	dic = HCL_STACK_GETARG(hcl, nargs, 0);
	key = HCL_STACK_GETARG(hcl, nargs, 1);
	val = HCL_STACK_GETARG(hcl, nargs, 2);

	if (!HCL_IS_DIC(hcl,dic))
	{
		hcl_seterrbfmt (hcl, HCL_EINVAL, "parameter not an dictionary - %O", dic);
		return HCL_PF_FAILURE;
	}

	pair = hcl_putatdic(hcl, (hcl_oop_dic_t)dic, key, val);
	if (!pair)
	{
		HCL_STACK_SETRETTOERRNUM (hcl, nargs);
		return HCL_PF_SUCCESS;
	}

	HCL_STACK_SETRET (hcl, nargs, HCL_CONS_CDR(pair));
	return HCL_PF_SUCCESS;
}


static int walker (hcl_t* hcl, hcl_oop_dic_t dic, hcl_oop_cons_t pair, void* ctx)
{
	HCL_DEBUG2 (hcl, "walker ===> %O  =====> %O\n", HCL_CONS_CAR(pair), HCL_CONS_CDR(pair));
	return 0;
}

static hcl_pfrc_t pf_dic_walk (hcl_t* hcl, hcl_mod_t* mod, hcl_ooi_t nargs)
{
/* TODO: write a proper function 
 * (dic.apply #{ ... } callable-or-lambda)
 */
	hcl_oop_t arg;

	arg = HCL_STACK_GETARG(hcl, nargs, 0);
	if (!HCL_IS_DIC(hcl,arg))
	{
		hcl_seterrbfmt (hcl, HCL_EINVAL, "parameter not a dictionary - %O", arg);
		return HCL_PF_FAILURE;
	}

	hcl_walkdic (hcl, (hcl_oop_dic_t)arg, walker, HCL_NULL);
	HCL_STACK_SETRET (hcl, nargs, hcl->_true);
	return HCL_PF_SUCCESS;
}


static hcl_pfinfo_t pfinfos[] =
{
	{ "get",   { HCL_PFBASE_FUNC, pf_dic_get,     2,  2 } },
/*	{ "make",  { HCL_PFBASE_FUNC, pf_dic_make,    1,  1 } }, */
	{ "put",   { HCL_PFBASE_FUNC, pf_dic_put,     3,  3 } },
	{ "walk",  { HCL_PFBASE_FUNC, pf_dic_walk,    2,  2 } },
};

/* ------------------------------------------------------------------------ */

static hcl_pfbase_t* query (hcl_t* hcl, hcl_mod_t* mod, const hcl_ooch_t* name, hcl_oow_t namelen)
{
	return hcl_findpfbase(hcl, pfinfos, HCL_COUNTOF(pfinfos), name, namelen);
}


static void unload (hcl_t* hcl, hcl_mod_t* mod)
{
}

int hcl_mod_dic (hcl_t* hcl, hcl_mod_t* mod)
{
	mod->query = query;
	mod->unload = unload; 
	mod->ctx = HCL_NULL;
	return 0;
}
