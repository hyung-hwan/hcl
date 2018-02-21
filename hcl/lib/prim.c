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

static hcl_pfrc_t pf_printf (hcl_t* hcl, hcl_ooi_t nargs)
{
/* TODO: */
	HCL_STACK_SETRET (hcl, nargs, hcl->_false);
	return HCL_PF_SUCCESS;
}

/* ------------------------------------------------------------------------- */

static pf_t builtin_prims[] =
{
	{ 0, HCL_TYPE_MAX(hcl_oow_t), pf_log,           3,  { 'l','o','g' } },
	{ 0, 0,                       pf_gc,            2,  { 'g','c' } },

	{ 1, 1,                       pf_not,   3,  { 'n','o','t' } }, 
	{ 2, HCL_TYPE_MAX(hcl_oow_t), pf_and,   3,  { 'a','n','d' } },
	{ 2, HCL_TYPE_MAX(hcl_oow_t), pf_or,    2,  { 'o','r' } },

	{ 2, 2,                       pf_eqv,   4,  { 'e','q','v','?' } },
	{ 2, 2,                       pf_eql,   4,  { 'e','q','l','?' } },

	/*
	{ 2, 2,                       pf_gt,    1,  { '>' } },
	{ 2, 2,                       pf_ge,    2,  { '>','=' } },
	{ 2, 2,                       pf_lt,    1,  { '<' } },
	{ 2, 2,                       pf_le,    2,  { '<','=' } },
	{ 2, 2,                       pf_eq,    1,  { '=' } },
	{ 2, 2,                       pf_ne,    2,  { '/','=' } },

	{ 2, 2,                       pf_max,   3,  { 'm','a','x' } },
	{ 2, 2,                       pf_min,   3,  { 'm','i','n' } },
	*/

	{ 1, HCL_TYPE_MAX(hcl_oow_t), pf_integer_add,   1,  { '+' } },
	{ 1, HCL_TYPE_MAX(hcl_oow_t), pf_integer_sub,   1,  { '-' } },
	{ 1, HCL_TYPE_MAX(hcl_oow_t), pf_integer_mul,   1,  { '*' } },
	{ 1, HCL_TYPE_MAX(hcl_oow_t), pf_integer_quo,   1,  { '/' } },
	{ 2, HCL_TYPE_MAX(hcl_oow_t), pf_integer_rem,   3,  { 'm','o','d' } },

	
	{ 0, HCL_TYPE_MAX(hcl_oow_t), pf_printf,        6,  { 'p','r','i','n','t','f' } },
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

