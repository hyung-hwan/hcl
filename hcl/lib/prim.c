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
	hcl_ooch_t name[32];
};
typedef struct pf_t pf_t;

/* ------------------------------------------------------------------------- */

hcl_oop_t hcl_makeprim (hcl_t* hcl, hcl_pfimpl_t primimpl, hcl_oow_t minargs, hcl_oow_t maxargs, hcl_mod_t* mod)
{
	hcl_oop_word_t obj;

	obj = (hcl_oop_word_t)hcl_allocwordobj (hcl, HCL_BRAND_PRIM, HCL_NULL, 4);
	if (obj)
	{
		obj->slot[0] = (hcl_oow_t)primimpl;
		obj->slot[1] = minargs;
		obj->slot[2] = maxargs;
		obj->slot[3] = (hcl_oow_t)mod;
	}

	return (hcl_oop_t)obj;
}

/* ------------------------------------------------------------------------- */

static void log_char_object (hcl_t* hcl, hcl_bitmask_t mask, hcl_oop_char_t msg)
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

static hcl_pfrc_t pf_log (hcl_t* hcl, hcl_mod_t* mod, hcl_ooi_t nargs)
{
/* TODO: accept log level */
	hcl_oop_t msg;
	hcl_bitmask_t mask;
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

static hcl_pfrc_t pf_logf (hcl_t* hcl, hcl_mod_t* mod, hcl_ooi_t nargs)
{
	if (hcl_logfmtcallstack(hcl, nargs) <= -1)
	{
		HCL_STACK_SETRETTOERRNUM (hcl, nargs);
	}
	else
	{
/* TODO: better return code? */
		HCL_STACK_SETRET (hcl, nargs, hcl->_nil);
	}

	return HCL_PF_SUCCESS;
}

static hcl_pfrc_t pf_printf (hcl_t* hcl, hcl_mod_t* mod, hcl_ooi_t nargs)
{
	if (hcl_prfmtcallstack(hcl, nargs) <= -1)
	{
		HCL_STACK_SETRETTOERRNUM (hcl, nargs);
	}
	else
	{
/* TODO: better return code? */
		HCL_STACK_SETRET (hcl, nargs, hcl->_nil);
	}

	return HCL_PF_SUCCESS;
}

static hcl_pfrc_t pf_sprintf (hcl_t* hcl, hcl_mod_t* mod, hcl_ooi_t nargs)
{
	if (hcl_strfmtcallstack(hcl, nargs) <= -1)
	{
		HCL_STACK_SETRETTOERRNUM (hcl, nargs);
	}
	else
	{
		hcl_oop_t str;
		str = hcl_makestring (hcl, hcl->sprintf.xbuf.ptr, hcl->sprintf.xbuf.len, 0);
		if (!str) return HCL_PF_FAILURE;

		HCL_STACK_SETRET (hcl, nargs, str);
	}

	return HCL_PF_SUCCESS;
}



static hcl_pfrc_t pf_gc (hcl_t* hcl, hcl_mod_t* mod, hcl_ooi_t nargs)
{
	hcl_gc (hcl, 1);
	HCL_STACK_SETRET (hcl, nargs, hcl->_nil);
	return HCL_PF_SUCCESS;
}

/* ------------------------------------------------------------------------- */

static hcl_pfrc_t pf_eqv (hcl_t* hcl, hcl_mod_t* mod, hcl_ooi_t nargs)
{
	hcl_oop_t a0, a1, rv;

	a0 = HCL_STACK_GETARG(hcl, nargs, 0);
	a1 = HCL_STACK_GETARG(hcl, nargs, 1);

	rv = (a0 == a1? hcl->_true: hcl->_false);

	HCL_STACK_SETRET (hcl, nargs, rv);
	return HCL_PF_SUCCESS;
}

static hcl_pfrc_t pf_eql (hcl_t* hcl, hcl_mod_t* mod, hcl_ooi_t nargs)
{
	int n;
	n = hcl_equalobjs(hcl, HCL_STACK_GETARG(hcl, nargs, 0), HCL_STACK_GETARG(hcl, nargs, 1));
	if (n <= -1) return HCL_PF_FAILURE;

	HCL_STACK_SETRET (hcl, nargs, (n? hcl->_true: hcl->_false));
	return HCL_PF_SUCCESS;
}

static hcl_pfrc_t pf_eqk (hcl_t* hcl, hcl_mod_t* mod, hcl_ooi_t nargs)
{
	/* equal kind? */
	hcl_oop_t a0, a1, rv;

	a0 = HCL_STACK_GETARG(hcl, nargs, 0);
	a1 = HCL_STACK_GETARG(hcl, nargs, 1);

	rv = (HCL_BRANDOF(hcl, a0) == HCL_BRANDOF(hcl, a1)? hcl->_true: hcl->_false);

	HCL_STACK_SETRET (hcl, nargs, rv);
	return HCL_PF_SUCCESS;
}

static hcl_pfrc_t pf_nqv (hcl_t* hcl, hcl_mod_t* mod, hcl_ooi_t nargs)
{
	hcl_oop_t a0, a1, rv;

	a0 = HCL_STACK_GETARG(hcl, nargs, 0);
	a1 = HCL_STACK_GETARG(hcl, nargs, 1);

	rv = (a0 != a1? hcl->_true: hcl->_false);

	HCL_STACK_SETRET (hcl, nargs, rv);
	return HCL_PF_SUCCESS;
}

static hcl_pfrc_t pf_nql (hcl_t* hcl, hcl_mod_t* mod, hcl_ooi_t nargs)
{
	int n;
	n = hcl_equalobjs(hcl, HCL_STACK_GETARG(hcl, nargs, 0), HCL_STACK_GETARG(hcl, nargs, 1));
	if (n <= -1) return HCL_PF_FAILURE;

	HCL_STACK_SETRET (hcl, nargs, (!n? hcl->_true: hcl->_false));
	return HCL_PF_SUCCESS;
}

static hcl_pfrc_t pf_nqk (hcl_t* hcl, hcl_mod_t* mod, hcl_ooi_t nargs)
{
	/* not equal kind? */
	hcl_oop_t a0, a1, rv;

	a0 = HCL_STACK_GETARG(hcl, nargs, 0);
	a1 = HCL_STACK_GETARG(hcl, nargs, 1);

	rv = (HCL_BRANDOF(hcl, a0) != HCL_BRANDOF(hcl, a1)? hcl->_true: hcl->_false);

	HCL_STACK_SETRET (hcl, nargs, rv);
	return HCL_PF_SUCCESS;
}

/* ------------------------------------------------------------------------- */

static hcl_pfrc_t pf_is_null (hcl_t* hcl, hcl_mod_t* mod, hcl_ooi_t nargs)
{
	hcl_oop_t rv;
	rv = (HCL_STACK_GETARG(hcl, nargs, 0) == hcl->_nil)? hcl->_true: hcl->_false;
	HCL_STACK_SETRET (hcl, nargs, rv);
	return HCL_PF_SUCCESS;
}


static hcl_pfrc_t pf_is_boolean (hcl_t* hcl, hcl_mod_t* mod, hcl_ooi_t nargs)
{
	hcl_oop_t rv, x;
	x = HCL_STACK_GETARG(hcl, nargs, 0);
	rv = (HCL_IS_TRUE(hcl, x) || HCL_IS_FALSE(hcl, x))? hcl->_true: hcl->_false;
	HCL_STACK_SETRET (hcl, nargs, rv);
	return HCL_PF_SUCCESS;
}

static hcl_pfrc_t pf_is_character (hcl_t* hcl, hcl_mod_t* mod, hcl_ooi_t nargs)
{
	hcl_oop_t rv, x;
	x = HCL_STACK_GETARG(hcl, nargs, 0);
	rv = (HCL_OOP_IS_CHAR(x))? hcl->_true: hcl->_false;
	HCL_STACK_SETRET (hcl, nargs, rv);
	return HCL_PF_SUCCESS;
}

static hcl_pfrc_t pf_is_error (hcl_t* hcl, hcl_mod_t* mod, hcl_ooi_t nargs)
{
	hcl_oop_t rv, x;
	x = HCL_STACK_GETARG(hcl, nargs, 0);
	rv = (HCL_OOP_IS_ERROR(x))? hcl->_true: hcl->_false;
	HCL_STACK_SETRET (hcl, nargs, rv);
	return HCL_PF_SUCCESS;
}

static hcl_pfrc_t pf_is_smptr (hcl_t* hcl, hcl_mod_t* mod, hcl_ooi_t nargs)
{
	hcl_oop_t rv, x;
	x = HCL_STACK_GETARG(hcl, nargs, 0);
	rv = (HCL_OOP_IS_SMPTR(x))? hcl->_true: hcl->_false;
	HCL_STACK_SETRET (hcl, nargs, rv);
	return HCL_PF_SUCCESS;
}

static hcl_pfrc_t pf_is_integer (hcl_t* hcl, hcl_mod_t* mod, hcl_ooi_t nargs)
{
	hcl_oop_t rv, x;
	x = HCL_STACK_GETARG(hcl, nargs, 0);
	rv = (hcl_isint(hcl, x))? hcl->_true: hcl->_false;
	HCL_STACK_SETRET (hcl, nargs, rv);
	return HCL_PF_SUCCESS;
}

static hcl_pfrc_t pf_is_numeric(hcl_t* hcl, hcl_mod_t* mod, hcl_ooi_t nargs)
{
	hcl_oop_t rv, x;
	x = HCL_STACK_GETARG(hcl, nargs, 0);
	rv = (hcl_isint(hcl, x) || HCL_IS_FPDEC(hcl, x))? hcl->_true: hcl->_false;
	HCL_STACK_SETRET (hcl, nargs, rv);
	return HCL_PF_SUCCESS;
}

static hcl_pfrc_t pf_is_string (hcl_t* hcl, hcl_mod_t* mod, hcl_ooi_t nargs)
{
	hcl_oop_t rv, x;
	x = HCL_STACK_GETARG(hcl, nargs, 0);
	rv = (HCL_IS_STRING(hcl, x))? hcl->_true: hcl->_false;
	HCL_STACK_SETRET (hcl, nargs, rv);
	return HCL_PF_SUCCESS;
}

static hcl_pfrc_t pf_is_array (hcl_t* hcl, hcl_mod_t* mod, hcl_ooi_t nargs)
{
	hcl_oop_t rv, x;
	x = HCL_STACK_GETARG(hcl, nargs, 0);
	rv = (HCL_IS_ARRAY(hcl, x))? hcl->_true: hcl->_false;
	HCL_STACK_SETRET (hcl, nargs, rv);
	return HCL_PF_SUCCESS;
}

static hcl_pfrc_t pf_is_bytearray (hcl_t* hcl, hcl_mod_t* mod, hcl_ooi_t nargs)
{
	hcl_oop_t rv, x;
	x = HCL_STACK_GETARG(hcl, nargs, 0);
	rv = (HCL_IS_BYTEARRAY(hcl, x))? hcl->_true: hcl->_false;
	HCL_STACK_SETRET (hcl, nargs, rv);
	return HCL_PF_SUCCESS;
}

static hcl_pfrc_t pf_is_dictionary (hcl_t* hcl, hcl_mod_t* mod, hcl_ooi_t nargs)
{
	hcl_oop_t rv, x;
	x = HCL_STACK_GETARG(hcl, nargs, 0);
	rv = (HCL_IS_DIC(hcl, x))? hcl->_true: hcl->_false;
	HCL_STACK_SETRET (hcl, nargs, rv);
	return HCL_PF_SUCCESS;
}

static hcl_pfrc_t pf_is_lambda (hcl_t* hcl, hcl_mod_t* mod, hcl_ooi_t nargs)
{
	hcl_oop_t rv, x;
	x = HCL_STACK_GETARG(hcl, nargs, 0);
	rv = (HCL_IS_CONTEXT(hcl, x))? hcl->_true: hcl->_false;
	HCL_STACK_SETRET (hcl, nargs, rv);
	return HCL_PF_SUCCESS;
}

/* ------------------------------------------------------------------------- */

static hcl_pfrc_t pf_not (hcl_t* hcl, hcl_mod_t* mod, hcl_ooi_t nargs)
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

static hcl_pfrc_t pf_and (hcl_t* hcl, hcl_mod_t* mod, hcl_ooi_t nargs)
{
	hcl_oop_t arg, rv;
	hcl_ooi_t i;

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

static hcl_pfrc_t pf_or (hcl_t* hcl, hcl_mod_t* mod, hcl_ooi_t nargs)
{
	hcl_oop_t arg, rv;
	hcl_ooi_t i;

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

static hcl_pfrc_t pf_number_add (hcl_t* hcl, hcl_mod_t* mod, hcl_ooi_t nargs)
{
	hcl_ooi_t i;
	hcl_oop_t arg, ret;

	ret = HCL_STACK_GETARG(hcl, nargs, 0);
	for (i = 1; i < nargs; i++)
	{
		arg = HCL_STACK_GETARG(hcl, nargs, i);
		/*ret = hcl_addints(hcl, ret, arg);*/
		ret = hcl_addnums(hcl, ret, arg);
		if (!ret) return HCL_PF_FAILURE;
	}

	HCL_STACK_SETRET (hcl, nargs, ret);
	return HCL_PF_SUCCESS;
}

static hcl_pfrc_t pf_number_sub (hcl_t* hcl, hcl_mod_t* mod, hcl_ooi_t nargs)
{
	hcl_ooi_t i;
	hcl_oop_t arg, ret;

	ret = HCL_STACK_GETARG(hcl, nargs, 0);
	for (i = 1; i < nargs; i++)
	{
		arg = HCL_STACK_GETARG(hcl, nargs, i);
		/*ret = hcl_subints(hcl, ret, arg);*/
		ret = hcl_subnums(hcl, ret, arg);
		if (!ret) return HCL_PF_FAILURE;
	}

	HCL_STACK_SETRET (hcl, nargs, ret);
	return HCL_PF_SUCCESS;
}

static hcl_pfrc_t pf_number_mul (hcl_t* hcl, hcl_mod_t* mod, hcl_ooi_t nargs)
{
	hcl_ooi_t i;
	hcl_oop_t arg, ret;

	ret = HCL_STACK_GETARG(hcl, nargs, 0);
	for (i = 1; i < nargs; i++)
	{
		arg = HCL_STACK_GETARG(hcl, nargs, i);
		/*ret = hcl_mulints(hcl, ret, arg);*/
		ret = hcl_mulnums(hcl, ret, arg);
		if (!ret) return HCL_PF_FAILURE;
	}

	HCL_STACK_SETRET (hcl, nargs, ret);
	return HCL_PF_SUCCESS;
}

static hcl_pfrc_t pf_number_mlt (hcl_t* hcl, hcl_mod_t* mod, hcl_ooi_t nargs)
{
	hcl_ooi_t i;
	hcl_oop_t arg, ret;

	ret = HCL_STACK_GETARG(hcl, nargs, 0);
	for (i = 1; i < nargs; i++)
	{
		arg = HCL_STACK_GETARG(hcl, nargs, i);
		ret = hcl_mltnums(hcl, ret, arg);
		if (!ret) return HCL_PF_FAILURE;
	}

	HCL_STACK_SETRET (hcl, nargs, ret);
	return HCL_PF_SUCCESS;
}

static hcl_pfrc_t pf_number_div (hcl_t* hcl, hcl_mod_t* mod, hcl_ooi_t nargs)
{
	hcl_ooi_t i;
	hcl_oop_t arg, ret;

	ret = HCL_STACK_GETARG(hcl, nargs, 0);
	for (i = 1; i < nargs; i++)
	{
		arg = HCL_STACK_GETARG(hcl, nargs, i);
		ret = hcl_divnums(hcl, ret, arg);
		if (!ret) return HCL_PF_FAILURE;
	}

	HCL_STACK_SETRET (hcl, nargs, ret);
	return HCL_PF_SUCCESS;
}

static hcl_pfrc_t pf_integer_quo (hcl_t* hcl, hcl_mod_t* mod, hcl_ooi_t nargs)
{
	hcl_ooi_t i;
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

static hcl_pfrc_t pf_integer_rem (hcl_t* hcl, hcl_mod_t* mod, hcl_ooi_t nargs)
{
	hcl_ooi_t i;
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

static hcl_pfrc_t pf_integer_mquo (hcl_t* hcl, hcl_mod_t* mod, hcl_ooi_t nargs)
{
	hcl_ooi_t i;
	hcl_oop_t arg, ret;

	ret = HCL_STACK_GETARG(hcl, nargs, 0);
	for (i = 1; i < nargs; i++)
	{
		arg = HCL_STACK_GETARG(hcl, nargs, i);
		ret = hcl_divints(hcl, ret, arg, 1, HCL_NULL);
		if (!ret) return HCL_PF_FAILURE;
	}

	HCL_STACK_SETRET (hcl, nargs, ret);
	return HCL_PF_SUCCESS;
}

static hcl_pfrc_t pf_integer_mod (hcl_t* hcl, hcl_mod_t* mod, hcl_ooi_t nargs)
{
	hcl_ooi_t i;
	hcl_oop_t arg, ret, rem;

	ret = HCL_STACK_GETARG(hcl, nargs, 0);
	for (i = 1; i < nargs; i++)
	{
		arg = HCL_STACK_GETARG(hcl, nargs, i);
		ret = hcl_divints(hcl, ret, arg, 1, &rem);
		if (!ret) return HCL_PF_FAILURE;
		ret = rem;
	}

	HCL_STACK_SETRET (hcl, nargs, ret);
	return HCL_PF_SUCCESS;
}

static hcl_pfrc_t pf_number_sqrt (hcl_t* hcl, hcl_mod_t* mod, hcl_ooi_t nargs)
{
	hcl_oop_t ret;
	ret = hcl_sqrtnum(hcl, HCL_STACK_GETARG(hcl, nargs, 0));
	if (!ret) return HCL_PF_FAILURE;

	HCL_STACK_SETRET (hcl, nargs, ret);
	return HCL_PF_SUCCESS;
}

static hcl_pfrc_t pf_number_abs (hcl_t* hcl, hcl_mod_t* mod, hcl_ooi_t nargs)
{
	hcl_oop_t ret;
	ret = hcl_absnum(hcl, HCL_STACK_GETARG(hcl, nargs, 0)); 
	if (!ret) return HCL_PF_FAILURE;

	HCL_STACK_SETRET (hcl, nargs, ret);
	return HCL_PF_SUCCESS;
}

static hcl_pfrc_t pf_number_gt (hcl_t* hcl, hcl_mod_t* mod, hcl_ooi_t nargs)
{
	hcl_oop_t ret;
	ret = hcl_gtnums(hcl, HCL_STACK_GETARG(hcl, nargs, 0), HCL_STACK_GETARG(hcl, nargs, 1));
	if (!ret) return HCL_PF_FAILURE;

	HCL_STACK_SETRET (hcl, nargs, ret);
	return HCL_PF_SUCCESS;
}


static hcl_pfrc_t pf_number_ge (hcl_t* hcl, hcl_mod_t* mod, hcl_ooi_t nargs)
{
	hcl_oop_t ret;
	ret = hcl_genums(hcl, HCL_STACK_GETARG(hcl, nargs, 0), HCL_STACK_GETARG(hcl, nargs, 1));
	if (!ret) return HCL_PF_FAILURE;

	HCL_STACK_SETRET (hcl, nargs, ret);
	return HCL_PF_SUCCESS;
}

static hcl_pfrc_t pf_number_lt (hcl_t* hcl, hcl_mod_t* mod, hcl_ooi_t nargs)
{
	hcl_oop_t ret;
	ret = hcl_ltnums(hcl, HCL_STACK_GETARG(hcl, nargs, 0), HCL_STACK_GETARG(hcl, nargs, 1));
	if (!ret) return HCL_PF_FAILURE;

	HCL_STACK_SETRET (hcl, nargs, ret);
	return HCL_PF_SUCCESS;
}
static hcl_pfrc_t pf_number_le (hcl_t* hcl, hcl_mod_t* mod, hcl_ooi_t nargs)
{
	hcl_oop_t ret;
	ret = hcl_lenums(hcl, HCL_STACK_GETARG(hcl, nargs, 0), HCL_STACK_GETARG(hcl, nargs, 1));
	if (!ret) return HCL_PF_FAILURE;

	HCL_STACK_SETRET (hcl, nargs, ret);
	return HCL_PF_SUCCESS;
}
static hcl_pfrc_t pf_number_eq (hcl_t* hcl, hcl_mod_t* mod, hcl_ooi_t nargs)
{
	hcl_oop_t ret;
	ret = hcl_eqnums(hcl, HCL_STACK_GETARG(hcl, nargs, 0), HCL_STACK_GETARG(hcl, nargs, 1));
	if (!ret) return HCL_PF_FAILURE;

	HCL_STACK_SETRET (hcl, nargs, ret);
	return HCL_PF_SUCCESS;
}
static hcl_pfrc_t pf_number_ne (hcl_t* hcl, hcl_mod_t* mod, hcl_ooi_t nargs)
{
	hcl_oop_t ret;
	ret = hcl_nenums(hcl, HCL_STACK_GETARG(hcl, nargs, 0), HCL_STACK_GETARG(hcl, nargs, 1));
	if (!ret) return HCL_PF_FAILURE;

	HCL_STACK_SETRET (hcl, nargs, ret);
	return HCL_PF_SUCCESS;
}



static hcl_pfrc_t pf_integer_band (hcl_t* hcl, hcl_mod_t* mod, hcl_ooi_t nargs)
{
	hcl_oop_t ret;
	ret = hcl_bitandints(hcl, HCL_STACK_GETARG(hcl, nargs, 0), HCL_STACK_GETARG(hcl, nargs, 1));
	if (!ret) return HCL_PF_FAILURE;

	HCL_STACK_SETRET (hcl, nargs, ret);
	return HCL_PF_SUCCESS;
}

static hcl_pfrc_t pf_integer_bor (hcl_t* hcl, hcl_mod_t* mod, hcl_ooi_t nargs)
{
	hcl_oop_t ret;
	ret = hcl_bitorints(hcl, HCL_STACK_GETARG(hcl, nargs, 0), HCL_STACK_GETARG(hcl, nargs, 1));
	if (!ret) return HCL_PF_FAILURE;

	HCL_STACK_SETRET (hcl, nargs, ret);
	return HCL_PF_SUCCESS;
}
static hcl_pfrc_t pf_integer_bxor (hcl_t* hcl, hcl_mod_t* mod, hcl_ooi_t nargs)
{
	hcl_oop_t ret;
	ret = hcl_bitxorints(hcl, HCL_STACK_GETARG(hcl, nargs, 0), HCL_STACK_GETARG(hcl, nargs, 1));
	if (!ret) return HCL_PF_FAILURE;

	HCL_STACK_SETRET (hcl, nargs, ret);
	return HCL_PF_SUCCESS;
}
static hcl_pfrc_t pf_integer_bnot (hcl_t* hcl, hcl_mod_t* mod, hcl_ooi_t nargs)
{
	hcl_oop_t ret;
	ret = hcl_bitinvint(hcl, HCL_STACK_GETARG(hcl, nargs, 0));
	if (!ret) return HCL_PF_FAILURE;

	HCL_STACK_SETRET (hcl, nargs, ret);
	return HCL_PF_SUCCESS;
}
static hcl_pfrc_t pf_integer_bshift (hcl_t* hcl, hcl_mod_t* mod, hcl_ooi_t nargs)
{
	hcl_oop_t ret;
	ret = hcl_bitshiftint(hcl, HCL_STACK_GETARG(hcl, nargs, 0), HCL_STACK_GETARG(hcl, nargs, 1));
	if (!ret) return HCL_PF_FAILURE;

	HCL_STACK_SETRET (hcl, nargs, ret);
	return HCL_PF_SUCCESS;
}

/* ------------------------------------------------------------------------- */

static pf_t builtin_prims[] =
{
	{ 0, HCL_TYPE_MAX(hcl_oow_t), pf_log,             3,  { 'l','o','g' } },
	{ 1, HCL_TYPE_MAX(hcl_oow_t), pf_logf,            4,  { 'l','o','g','f' } },
	{ 1, HCL_TYPE_MAX(hcl_oow_t), pf_printf,          6,  { 'p','r','i','n','t','f' } },
	{ 1, HCL_TYPE_MAX(hcl_oow_t), pf_sprintf,         7,  { 's','p','r','i','n','t','f' } },

	{ 0, 0,                       pf_gc,              2,  { 'g','c' } },

	{ 1, 1,                       pf_not,             3,  { 'n','o','t' } }, 
	/* this is a long-circuit logical and the short-curcuit 'and' is treated as a special form */
	{ 2, HCL_TYPE_MAX(hcl_oow_t), pf_and,             4,  { '_','a','n','d' } },
	/* this is a long-cirtuit logical or. the short-circuit 'or' is treated as a special form */
	{ 2, HCL_TYPE_MAX(hcl_oow_t), pf_or,              3,  { '_','o','r' } },

	{ 2, 2,                       pf_eqv,             4,  { 'e','q','v','?' } },
	{ 2, 2,                       pf_eql,             4,  { 'e','q','l','?' } },
	{ 2, 2,                       pf_eqk,             4,  { 'e','q','k','?' } },
	{ 2, 2,                       pf_nqv,             4,  { 'n','q','v','?' } },
	{ 2, 2,                       pf_nql,             4,  { 'n','q','l','?' } },
	{ 2, 2,                       pf_nqk,             4,  { 'n','q','k','?' } },

	{ 1, 1,                       pf_is_null,         5,  { 'n','u','l','l','?' } },
	{ 1, 1,                       pf_is_boolean,      8,  { 'b','o','o','l','e','a','n','?' } },
	{ 1, 1,                       pf_is_character,   10,  { 'c','h','a','r','a','c','t','e','r','?' } },
	{ 1, 1,                       pf_is_error,        6,  { 'e','r','r','o','r','?' } },
	{ 1, 1,                       pf_is_smptr,        6,  { 's','m','p','t','r','?' } },
	{ 1, 1,                       pf_is_integer,      8,  { 'i','n','t','e','g','e','r','?' } },
	{ 1, 1,                       pf_is_numeric,      8,  { 'n','u','m','e','r','i','c','?' } },
	{ 1, 1,                       pf_is_string,       7,  { 's','t','r','i','n','g','?' } },
	{ 1, 1,                       pf_is_array,        6,  { 'a','r','r','a','y','?' } },
	{ 1, 1,                       pf_is_bytearray,   10,  { 'b','y','t','e','a','r','r','a','y','?' } },
	{ 1, 1,                       pf_is_dictionary,  11,  { 'd','i','c','t','i','o','n','a','r','y','?' } },
	{ 1, 1,                       pf_is_lambda,       7,  { 'l','a','m','b','d','a','?' } },


	{ 1, HCL_TYPE_MAX(hcl_oow_t), pf_number_add,      1,  { '+' } },
	{ 1, HCL_TYPE_MAX(hcl_oow_t), pf_number_sub,      1,  { '-' } },
	{ 1, HCL_TYPE_MAX(hcl_oow_t), pf_number_mul,      1,  { '*' } },
	{ 1, HCL_TYPE_MAX(hcl_oow_t), pf_number_mlt,      3,  { 'm','l','t' } },
	{ 1, HCL_TYPE_MAX(hcl_oow_t), pf_number_div,      1,  { '/' } },
	{ 1, 1,                       pf_number_sqrt,     4,  { 's','q','r','t' } },
	{ 1, 1,                       pf_number_abs,      3,  { 'a','b','s' } },

	{ 2, 2,                       pf_number_gt,       1,  { '>' } },
	{ 2, 2,                       pf_number_ge,       2,  { '>','=' } },
	{ 2, 2,                       pf_number_lt,       1,  { '<' } },
	{ 2, 2,                       pf_number_le,       2,  { '<','=' } },
	{ 2, 2,                       pf_number_eq,       1,  { '=' } },
	{ 2, 2,                       pf_number_ne,       2,  { '/','=' } },

	/* bitwise operations are supported for integers only */
	{ 2, 2,                       pf_integer_band,    7,  { 'b','i','t','-','a','n','d' } },
	{ 2, 2,                       pf_integer_bor,     6,  { 'b','i','t','-','o','r' } },
	{ 2, 2,                       pf_integer_bxor,    7,  { 'b','i','t','-','x','o','r' } },
	{ 1, 1,                       pf_integer_bnot,    7,  { 'b','i','t','-','n','o','t' } },
	{ 2, 2,                       pf_integer_bshift,  9,  { 'b','i','t','-','s','h','i','f','t'  } },
	
	{ 1, HCL_TYPE_MAX(hcl_oow_t), pf_integer_quo,      3,  { 'd','i','v' } },
	{ 2, HCL_TYPE_MAX(hcl_oow_t), pf_integer_rem,      3,  { 'r','e','m' } },
	{ 1, HCL_TYPE_MAX(hcl_oow_t), pf_integer_mquo,     4,  { 'm','d','i','v' } },
	{ 2, HCL_TYPE_MAX(hcl_oow_t), pf_integer_mod,      3,  { 'm','o','d' } },

	{ 1, 1,                       hcl_pf_process_fork,    4,  { 'f','o','r','k'} },
	{ 1, 1,                       hcl_pf_process_resume,  7,  { 'r','e','s','u','m','e' } },
	{ 0, 1,                       hcl_pf_process_suspend, 8,  { 's','u','s','p','e','n','d' } },
	{ 0, 0,                       hcl_pf_process_yield,   5,  { 'y','i','e','l','d'} }
};


int hcl_addbuiltinprims (hcl_t* hcl)
{
	hcl_oow_t i;
	hcl_oop_t prim, name;
	hcl_oop_cons_t cons;

	for (i = 0; i < HCL_COUNTOF(builtin_prims); i++)
	{
		prim = hcl_makeprim(hcl, builtin_prims[i].impl, builtin_prims[i].minargs, builtin_prims[i].maxargs, HCL_NULL);
		if (HCL_UNLIKELY(!prim)) return -1;

		hcl_pushvolat (hcl, &prim);
		name = hcl_makesymbol(hcl, builtin_prims[i].name, builtin_prims[i].namelen);
		hcl_popvolat (hcl);
		if (HCL_UNLIKELY(!name)) return -1;

		hcl_pushvolat (hcl, &name);
		cons = hcl_putatsysdic(hcl, name, prim);
		hcl_popvolat (hcl);
		if (HCL_UNLIKELY(!cons)) return -1;

		/* turn on the kernel bit in the symbol associated with a primitive 
		 * function. 'set' prevents this symbol from being used as a variable
		 * name */ 
		HCL_OBJ_SET_FLAGS_KERNEL (name, 2);
	}

	return 0;
}

