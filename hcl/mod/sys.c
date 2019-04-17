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


#include "_sys.h"
#include <stdlib.h>

static hcl_pfrc_t pf_sys_time (hcl_t* hcl, hcl_mod_t* mod, hcl_ooi_t nargs)
{
	hcl_ntime_t now;
	hcl_oop_t tv;
	hcl->vmprim.gettime(hcl, &now);
	tv = hcl_oowtoint(hcl, now.sec);
	if (!tv) return HCL_PF_FAILURE;
	HCL_STACK_SETRET (hcl, nargs, tv);
	return HCL_PF_SUCCESS;
}

static hcl_pfrc_t pf_sys_srandom (hcl_t* hcl, hcl_mod_t* mod, hcl_ooi_t nargs)
{
	hcl_oop_t seed;
	hcl_oow_t seedw;

	seed = HCL_STACK_GETARG(hcl, nargs, 0);
	if (hcl_inttooow(hcl, seed, &seedw) == 0)
	{
		const hcl_ooch_t* orgmsg = hcl_backuperrmsg(hcl);
		hcl_seterrbfmt (hcl, HCL_EINVAL, "unacceptiable seed - %O - %js", seed, orgmsg);
		return HCL_PF_FAILURE;
	}

	srandom (seedw);
	HCL_STACK_SETRET (hcl, nargs, hcl->_nil);
	return HCL_PF_SUCCESS;
}

static hcl_pfrc_t pf_sys_random (hcl_t* hcl, hcl_mod_t* mod, hcl_ooi_t nargs)
{
	long int r = random();
	hcl_ooi_t rv = (hcl_ooi_t)(r % HCL_SMOOI_MAX);
	HCL_STACK_SETRET (hcl, nargs, HCL_SMOOI_TO_OOP(rv));
	return HCL_PF_SUCCESS;
}

static hcl_pfinfo_t pfinfos[] =
{
	/*{ { 'V','A','R','\0' },                  { HCL_PFBASE_VAR,  HCL_NULL,           0,  0 } },*/
	{ { 'r','a','n','d','o','m','\0' },      { HCL_PFBASE_FUNC,  pf_sys_random,       0,  0 } },
	{ { 's','r','a','n','d','o','m','\0' },  { HCL_PFBASE_FUNC,  pf_sys_srandom,      1,  1 } },
	{ { 't','i','m','e','\0' },              { HCL_PFBASE_FUNC,  pf_sys_time,         0,  0 } }
};

/* ------------------------------------------------------------------------ */

static hcl_pfbase_t* query (hcl_t* hcl, hcl_mod_t* mod, const hcl_ooch_t* name, hcl_oow_t namelen)
{
	return hcl_findpfbase(hcl, pfinfos, HCL_COUNTOF(pfinfos), name, namelen);
}

static void unload (hcl_t* hcl, hcl_mod_t* mod)
{
}

int hcl_mod_sys (hcl_t* hcl, hcl_mod_t* mod)
{
	mod->query = query;
	mod->unload = unload; 
	mod->ctx = HCL_NULL;
	return 0;
}
