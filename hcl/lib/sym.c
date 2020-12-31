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

static hcl_oop_oop_t expand_bucket (hcl_t* hcl, hcl_oop_oop_t oldbuc)
{
	hcl_oop_oop_t newbuc;
	hcl_oow_t oldsz, newsz, index;
	hcl_oop_char_t symbol;

	oldsz = HCL_OBJ_GET_SIZE(oldbuc);

/* TODO: better growth policy? */
	if (oldsz < 5000) newsz = oldsz + oldsz;
	else if (oldsz < 50000) newsz = oldsz + (oldsz / 2);
	else if (oldsz < 100000) newsz = oldsz + (oldsz / 4);
	else if (oldsz < 200000) newsz = oldsz + (oldsz / 8);
	else if (oldsz < 400000) newsz = oldsz + (oldsz / 16);
	else if (oldsz < 800000) newsz = oldsz + (oldsz / 32);
	else if (oldsz < 1600000) newsz = oldsz + (oldsz / 64);
	else 
	{
		hcl_oow_t inc, inc_max;

		inc = oldsz / 128;
		inc_max = HCL_OBJ_SIZE_MAX - oldsz;
		if (inc > inc_max) 
		{
			if (inc_max > 0) inc = inc_max;
			else
			{
				hcl_seterrnum (hcl, HCL_EOOMEM);
				return HCL_NULL;
			}
		}
		newsz = oldsz + inc;
	}

	hcl_pushvolat (hcl, (hcl_oop_t*)&oldbuc);
	newbuc = (hcl_oop_oop_t)hcl_makearray(hcl, newsz, 0);
	hcl_popvolat (hcl);
	if (!newbuc) return HCL_NULL;

	while (oldsz > 0)
	{
		symbol = (hcl_oop_char_t)oldbuc->slot[--oldsz];
		if ((hcl_oop_t)symbol != hcl->_nil)
		{
			HCL_ASSERT (hcl, HCL_IS_SYMBOL(hcl, symbol));
			/*HCL_ASSERT (hcl, sym->size > 0);*/

			index = hcl_hash_oochars(symbol->slot, HCL_OBJ_GET_SIZE(symbol)) % newsz;
			while (newbuc->slot[index] != hcl->_nil) index = (index + 1) % newsz;
			newbuc->slot[index] = (hcl_oop_t)symbol;
		}
	}

	return newbuc;
}

static hcl_oop_t find_or_make_symbol (hcl_t* hcl, const hcl_ooch_t* ptr, hcl_oow_t len, int create)
{
	hcl_ooi_t tally;
	hcl_oow_t index;
	hcl_oop_char_t symbol;

	HCL_ASSERT (hcl, len > 0);
	if (len <= 0) 
	{
		/* i don't allow an empty symbol name */
		hcl_seterrnum (hcl, HCL_EINVAL);
		return HCL_NULL;
	}

	HCL_ASSERT (hcl, HCL_IS_ARRAY(hcl, hcl->symtab->bucket));
	index = hcl_hash_oochars(ptr, len) % HCL_OBJ_GET_SIZE(hcl->symtab->bucket);

	/* find a matching symbol in the open-addressed symbol table */
	while (hcl->symtab->bucket->slot[index] != hcl->_nil) 
	{
		symbol = (hcl_oop_char_t)hcl->symtab->bucket->slot[index];
		HCL_ASSERT (hcl, HCL_IS_SYMBOL(hcl, symbol));

		if (len == HCL_OBJ_GET_SIZE(symbol) &&
		    hcl_equal_oochars (ptr, symbol->slot, len))
		{
			return (hcl_oop_t)symbol;
		}

		index = (index + 1) % HCL_OBJ_GET_SIZE(hcl->symtab->bucket);
	}

	if (!create) 
	{
		hcl_seterrnum (hcl, HCL_ENOENT);
		return HCL_NULL;
	}

	/* make a new symbol and insert it */
	HCL_ASSERT (hcl, HCL_OOP_IS_SMOOI(hcl->symtab->tally));
	tally = HCL_OOP_TO_SMOOI(hcl->symtab->tally);
	if (tally >= HCL_SMOOI_MAX)
	{
		/* this built-in table is not allowed to hold more than 
		 * HCL_SMOOI_MAX items for efficiency sake */
		hcl_seterrnum (hcl, HCL_EDFULL);
		return HCL_NULL;
	}

	/* no conversion to hcl_oow_t is necessary for tally + 1.
	 * the maximum value of tally is checked to be HCL_SMOOI_MAX - 1.
	 * tally + 1 can produce at most HCL_SMOOI_MAX. above all, 
	 * HCL_SMOOI_MAX is way smaller than HCL_TYPE_MAX(hcl_ooi_t). */
	if (tally + 1 >= HCL_OBJ_GET_SIZE(hcl->symtab->bucket))
	{
		hcl_oop_oop_t bucket;

		/* TODO: make the growth policy configurable instead of growing
		         it just before it gets full. The polcy can be grow it
		         if it's 70% full */

		/* enlarge the symbol table before it gets full to
		 * make sure that it has at least one free slot left
		 * after having added a new symbol. this is to help
		 * traversal end at a _nil slot if no entry is found. */
		bucket = expand_bucket(hcl, hcl->symtab->bucket);
		if (!bucket) return HCL_NULL;

		hcl->symtab->bucket = bucket;

		/* recalculate the index for the expanded bucket */
		index = hcl_hash_oochars(ptr, len) % HCL_OBJ_GET_SIZE(hcl->symtab->bucket);

		while (hcl->symtab->bucket->slot[index] != hcl->_nil) 
			index = (index + 1) % HCL_OBJ_GET_SIZE(hcl->symtab->bucket);
	}

	/* create a new symbol since it isn't found in the symbol table */
	symbol = (hcl_oop_char_t)hcl_alloccharobj (hcl, HCL_BRAND_SYMBOL, ptr, len);
	if (symbol)
	{
		HCL_ASSERT (hcl, tally < HCL_SMOOI_MAX);
		hcl->symtab->tally = HCL_SMOOI_TO_OOP(tally + 1);
		hcl->symtab->bucket->slot[index] = (hcl_oop_t)symbol;
	}

	return (hcl_oop_t)symbol;
}

hcl_oop_t hcl_makesymbol (hcl_t* hcl, const hcl_ooch_t* ptr, hcl_oow_t len)
{
	return find_or_make_symbol (hcl, ptr, len, 1);
}

hcl_oop_t hcl_findsymbol (hcl_t* hcl, const hcl_ooch_t* ptr, hcl_oow_t len)
{
	return find_or_make_symbol (hcl, ptr, len, 0);
}
