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

struct prim_t
{
	hcl_oow_t minargs;
	hcl_oow_t maxargs;
	hcl_prim_impl_t impl;

	hcl_oow_t namelen;
	hcl_ooch_t name[10];
	
};
typedef struct prim_t prim_t;

/* ------------------------------------------------------------------------- */

hcl_oop_t hcl_makeprim (hcl_t* hcl, hcl_prim_impl_t primimpl, hcl_oow_t minargs, hcl_oow_t maxargs)
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

static void log_char_object (hcl_t* hcl, hcl_oow_t mask, hcl_oop_char_t msg)
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
			n = hcl_logbfmt (hcl, mask, "%C", *ptr);
			HCL_ASSERT (hcl, n == 1);
			rem -= n;
			ptr += n;
			goto start_over;
		}

		n = hcl_logbfmt (hcl, mask, "%.*S", rem, ptr);
		if (n <= -1) break;
		if (n == 0) 
		{
			/* to skip the unprinted character. 
			 * actually, this check is not needed because of '\0' skipping
			 * at the beginning  of the loop */
			n = hcl_logbfmt (hcl, mask, "%C", *ptr);
			HCL_ASSERT (hcl, n == 1);
		}
		rem -= n;
		ptr += n;
	}
}

static int prim_log (hcl_t* hcl, hcl_ooi_t nargs)
{
/* TODO: accept log level */
	hcl_oop_t msg, level;
	hcl_oow_t mask;
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
		else if (HCL_OOP_IS_POINTER(msg))
		{
			if (HCL_OBJ_GET_FLAGS_TYPE(msg) == HCL_OBJ_TYPE_CHAR)
			{
				log_char_object (hcl, mask, (hcl_oop_char_t)msg);
			}
			else if (HCL_OBJ_GET_FLAGS_TYPE(msg) == HCL_OBJ_TYPE_OOP)
			{
				/* visit only 1-level down into an array-like object */
				hcl_oop_t inner, _class;
				hcl_oow_t i, spec;

				_class = HCL_CLASSOF(hcl, msg);

				spec = HCL_OOP_TO_SMOOI(((hcl_oop_class_t)_class)->spec);
				if (HCL_CLASS_SPEC_NAMED_INSTVAR(spec) > 0 || !HCL_CLASS_SPEC_IS_INDEXED(spec)) goto dump_object;

				for (i = 0; i < HCL_OBJ_GET_SIZE(msg); i++)
				{
					inner = ((hcl_oop_oop_t)msg)->slot[i];

					if (i > 0) hcl_logbfmt (hcl, mask, " ");
					if (HCL_OOP_IS_POINTER(inner) &&
					    HCL_OBJ_GET_FLAGS_TYPE(inner) == HCL_OBJ_TYPE_CHAR)
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
	return 0;
}

/* ------------------------------------------------------------------------- */

static int oop_to_ooi (hcl_t* hcl, hcl_oop_t iv, hcl_ooi_t* ov)
{
	if (HCL_OOP_IS_SMOOI(iv))
	{
		*ov = HCL_OOP_TO_SMOOI(iv);
		return 0;
	}
	else if (HCL_IS_INTEGER(hcl, iv))
	{
		*ov = (hcl_ooi_t)((hcl_oop_word_t)iv)->slot[0];
		return 0;
	}
	else
	{
		/* TODO: set error number or something...to indicate primitive failure... */
		return -1;
	}
}

static int prim_plus (hcl_t* hcl, hcl_ooi_t nargs)
{
	hcl_ooi_t x = 0;
	hcl_oow_t i;
	hcl_oop_t arg, ret;

	for (i = 0; i < nargs; i++)
	{
		hcl_ooi_t v;

		arg = HCL_STACK_GETARG(hcl, nargs, i);
		if (oop_to_ooi(hcl, arg, &v) <= -1) return -1;
		x += v;
	}

	ret = hcl_makeinteger (hcl, x);
	if (!ret) return -1;

	HCL_STACK_SETRET (hcl, nargs, ret);
	return 0;
}

static int prim_minus (hcl_t* hcl, hcl_ooi_t nargs)
{
	hcl_ooi_t x = 0;
	hcl_oow_t i;
	hcl_oop_t arg, ret;

	if (nargs > 0)
	{
		arg = HCL_STACK_GETARG(hcl, nargs, 0);
		if (oop_to_ooi(hcl, arg, &x) <= -1) return -1;
		for (i = 1; i < nargs; i++)
		{
			hcl_ooi_t v;
			arg = HCL_STACK_GETARG(hcl, nargs, i);
			if (oop_to_ooi(hcl, arg, &v) <= -1) return -1;
			x -= v;
		}
	}

	ret = hcl_makeinteger (hcl, x);
	if (!ret) return -1;

	HCL_STACK_SETRET (hcl, nargs, ret);
	return 0;
}

static int prim_printf (hcl_t* hcl, hcl_ooi_t nargs)
{
	hcl_ooi_t x = 0;
	hcl_oow_t i;
	hcl_oop_t arg, ret;

	if (nargs > 0)
	{
		arg = HCL_STACK_GETARG(hcl, nargs, 0);
		if (oop_to_ooi(hcl, arg, &x) <= -1) return -1;
		for (i = 1; i < nargs; i++)
		{
			hcl_ooi_t v;
			arg = HCL_STACK_GETARG(hcl, nargs, i);
			if (oop_to_ooi(hcl, arg, &v) <= -1) return -1;
			x -= v;
		}
	}

	ret = hcl_makeinteger (hcl, x);
	if (!ret) return -1;

	HCL_STACK_SETRET (hcl, nargs, ret);
	return 0;
}
/* ------------------------------------------------------------------------- */

static prim_t builtin_prims[] =
{
	{ 0, HCL_TYPE_MAX(hcl_oow_t), prim_log,   3,  { 'l','o','g' } },

	/*
	{ 2, 2,                       prim_gt,    1,  { '>' } },
	{ 2, 2,                       prim_ge,    2,  { '>','=' } },
	{ 2, 2,                       prim_lt,    1,  { '<' } },
	{ 2, 2,                       prim_le,    2,  { '<','=' } },
	{ 2, 2,                       prim_eq,    1,  { '=' } },
	{ 2, 2,                       prim_ne,    2,  { '/','=' } },

	{ 2, 2,                       prim_eql,   3,  { 'e','q','l' } },
	{ 2, 2,                       prim_max,   3,  { 'm','a','x' } },
	{ 2, 2,                       prim_min,   3,  { 'm','i','n' } },

	{ 2, 2,                       prim_and,   3,  { 'a','n','d' } },
	{ 2, 2,                       prim_or,    2,  { 'o','r' } },
	{ 1, 1,                       prim_not,   3,  { 'n','o','t' } }, */

	{ 0, HCL_TYPE_MAX(hcl_oow_t), prim_plus,   1,  { '+' } },
	{ 0, HCL_TYPE_MAX(hcl_oow_t), prim_minus,  1,  { '-' } },

	{ 0, HCL_TYPE_MAX(hcl_oow_t), prim_printf, 6, { 'p','r','i','n','t','f' } },
};


int hcl_addbuiltinprims (hcl_t* hcl)
{
	hcl_oow_t i;
	hcl_oop_t prim, name;

	for (i = 0; i < HCL_COUNTOF(builtin_prims); i++)
	{
		prim = hcl_makeprim (hcl, builtin_prims[i].impl, builtin_prims[i].minargs, builtin_prims[i].maxargs);
		if (!prim) return -1;

		hcl_pushtmp (hcl, &prim);
		name = hcl_makesymbol (hcl, builtin_prims[i].name, builtin_prims[i].namelen);
		hcl_poptmp (hcl);
		if (!name) return -1;

		if (!hcl_putatsysdic (hcl, name, prim)) return -1;
	}

	return 0;
}
