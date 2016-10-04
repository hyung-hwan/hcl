/*
 * $Id$
 *
    Copyright (c) 2014-2016 Chung, Hyung-Hwan. All rights reserved.

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
	hcl_oop_cons_t ass;
	hcl_oop_char_t key;

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
				hcl->errnum = HCL_EOOMEM;
				return HCL_NULL;
			}
		}
		newsz = oldsz + inc;
	}

	hcl_pushtmp (hcl, (hcl_oop_t*)&oldbuc);
	newbuc = (hcl_oop_oop_t)hcl_makearray (hcl, newsz); 
	hcl_poptmp (hcl);
	if (!newbuc) return HCL_NULL;

	while (oldsz > 0)
	{
		ass = (hcl_oop_cons_t)oldbuc->slot[--oldsz];
		if ((hcl_oop_t)ass != hcl->_nil)
		{
			HCL_ASSERT (HCL_BRANDOF(hcl,ass) == HCL_BRAND_CONS);

			key = (hcl_oop_char_t)ass->car;
			HCL_ASSERT (HCL_BRANDOF(hcl,key) == HCL_BRAND_SYMBOL);

			index = hcl_hashchars(key->slot, HCL_OBJ_GET_SIZE(key)) % newsz;
			while (newbuc->slot[index] != hcl->_nil) index = (index + 1) % newsz;
			newbuc->slot[index] = (hcl_oop_t)ass;
		}
	}

	return newbuc;
}

static hcl_oop_cons_t find_or_upsert (hcl_t* hcl, hcl_oop_set_t dic, hcl_oop_char_t key, hcl_oop_t value)
{
	hcl_ooi_t tally;
	hcl_oow_t index;
	hcl_oop_cons_t ass;
	hcl_oow_t tmp_count = 0;

	/* the system dictionary is not a generic dictionary.
	 * it accepts only a symbol as a key. */
	HCL_ASSERT (HCL_IS_SYMBOL(hcl,key));
	HCL_ASSERT (HCL_OOP_IS_SMOOI(dic->tally));
	HCL_ASSERT (HCL_IS_ARRAY(hcl,dic->bucket));

	index = hcl_hashchars(key->slot, HCL_OBJ_GET_SIZE(key)) % HCL_OBJ_GET_SIZE(dic->bucket);

	/* find */
	while (dic->bucket->slot[index] != hcl->_nil) 
	{
		ass = (hcl_oop_cons_t)dic->bucket->slot[index];

		HCL_ASSERT (HCL_BRANDOF(hcl,ass) == HCL_BRAND_CONS);
		HCL_ASSERT (HCL_BRANDOF(hcl,ass->car) == HCL_BRAND_SYMBOL);

		if (HCL_OBJ_GET_SIZE(key) == HCL_OBJ_GET_SIZE(ass->car) &&
		    hcl_equalchars (key->slot, ((hcl_oop_char_t)ass->car)->slot, HCL_OBJ_GET_SIZE(key))) 
		{
			/* the value of HCL_NULL indicates no insertion or update. */
			if (value) ass->cdr = value; /* update */
			return ass;
		}

		index = (index + 1) % HCL_OBJ_GET_SIZE(dic->bucket);
	}

	if (!value)
	{
		/* when value is HCL_NULL, perform no insertion.
		 * the value of HCL_NULL indicates no insertion or update. */
		hcl->errnum = HCL_ENOENT;
		return HCL_NULL;
	}

	/* the key is not found. insert it. */
	HCL_ASSERT (HCL_OOP_IS_SMOOI(dic->tally));
	tally = HCL_OOP_TO_SMOOI(dic->tally);
	if (tally >= HCL_SMOOI_MAX)
	{
		/* this built-in dictionary is not allowed to hold more than 
		 * HCL_SMOOI_MAX items for efficiency sake */
		hcl->errnum = HCL_EDFULL;
		return HCL_NULL;
	}

	hcl_pushtmp (hcl, (hcl_oop_t*)&dic); tmp_count++;
	hcl_pushtmp (hcl, (hcl_oop_t*)&key); tmp_count++;
	hcl_pushtmp (hcl, &value); tmp_count++;

	/* no conversion to hcl_oow_t is necessary for tally + 1.
	 * the maximum value of tally is checked to be HCL_SMOOI_MAX - 1.
	 * tally + 1 can produce at most HCL_SMOOI_MAX. above all, 
	 * HCL_SMOOI_MAX is way smaller than HCL_TYPE_MAX(hcl_ooi_t). */
	if (tally + 1 >= HCL_OBJ_GET_SIZE(dic->bucket))
	{
		hcl_oop_oop_t bucket;

		/* TODO: make the growth policy configurable instead of growing
			     it just before it gets full. The polcy can be grow it
			     if it's 70% full */

		/* enlarge the bucket before it gets full to
		 * make sure that it has at least one free slot left
		 * after having added a new symbol. this is to help
		 * traversal end at a _nil slot if no entry is found. */
		bucket = expand_bucket (hcl, dic->bucket);
		if (!bucket) goto oops;

		dic->bucket = bucket;

		/* recalculate the index for the expanded bucket */
		index = hcl_hashchars(key->slot, HCL_OBJ_GET_SIZE(key)) % HCL_OBJ_GET_SIZE(dic->bucket);

		while (dic->bucket->slot[index] != hcl->_nil) 
			index = (index + 1) % HCL_OBJ_GET_SIZE(dic->bucket);
	}

	/* create a new assocation of a key and a value since 
	 * the key isn't found in the root dictionary */
	ass = (hcl_oop_cons_t)hcl_makecons (hcl, (hcl_oop_t)key, (hcl_oop_t)value);
	if (!ass) goto oops;

	/* the current tally must be less than the maximum value. otherwise,
	 * it overflows after increment below */
	HCL_ASSERT (tally < HCL_SMOOI_MAX);
	dic->tally = HCL_SMOOI_TO_OOP(tally + 1);
	dic->bucket->slot[index] = (hcl_oop_t)ass;

	hcl_poptmps (hcl, tmp_count);
	return ass;

oops:
	hcl_poptmps (hcl, tmp_count);
	return HCL_NULL;
}

static hcl_oop_cons_t lookup (hcl_t* hcl, hcl_oop_set_t dic, const hcl_oocs_t* name)
{
	/* this is special version of hcl_getatsysdic() that performs
	 * lookup using a plain string specified */

	hcl_oow_t index;
	hcl_oop_cons_t ass;

	HCL_ASSERT (HCL_OOP_IS_SMOOI(dic->tally));
	HCL_ASSERT (HCL_BRANDOF(hcl,dic->bucket) == HCL_BRAND_ARRAY);

	index = hcl_hashchars(name->ptr, name->len) % HCL_OBJ_GET_SIZE(dic->bucket);

	while (dic->bucket->slot[index] != hcl->_nil) 
	{
		ass = (hcl_oop_cons_t)dic->bucket->slot[index];

		HCL_ASSERT (HCL_BRANDOF(hcl,ass) == HCL_BRAND_CONS);
		HCL_ASSERT (HCL_BRANDOF(hcl,ass->car) == HCL_BRAND_SYMBOL);

		if (name->len == HCL_OBJ_GET_SIZE(ass->car) &&
		    hcl_equalchars(name->ptr, ((hcl_oop_char_t)ass->car)->slot, name->len)) 
		{
			return ass;
		}

		index = (index + 1) % HCL_OBJ_GET_SIZE(dic->bucket);
	}

	/* when value is HCL_NULL, perform no insertion */
	hcl->errnum = HCL_ENOENT;
	return HCL_NULL;
}

hcl_oop_cons_t hcl_putatsysdic (hcl_t* hcl, hcl_oop_t key, hcl_oop_t value)
{
	HCL_ASSERT (HCL_IS_SYMBOL(hcl,key));
	return find_or_upsert (hcl, hcl->sysdic, (hcl_oop_char_t)key, value);
}

hcl_oop_cons_t hcl_getatsysdic (hcl_t* hcl, hcl_oop_t key)
{
	HCL_ASSERT (HCL_IS_SYMBOL(hcl,key));
	return find_or_upsert (hcl, hcl->sysdic, (hcl_oop_char_t)key, HCL_NULL);
}

hcl_oop_cons_t hcl_lookupsysdic (hcl_t* hcl, const hcl_oocs_t* name)
{
	return lookup (hcl, hcl->sysdic, name);
}

hcl_oop_cons_t hcl_putatdic (hcl_t* hcl, hcl_oop_set_t dic, hcl_oop_t key, hcl_oop_t value)
{
	HCL_ASSERT (HCL_BRANDOF(hcl,key) == HCL_BRAND_SYMBOL);
	return find_or_upsert (hcl, dic, (hcl_oop_char_t)key, value);
}

hcl_oop_cons_t hcl_getatdic (hcl_t* hcl, hcl_oop_set_t dic, hcl_oop_t key)
{
	HCL_ASSERT (HCL_BRANDOF(hcl,key) == HCL_BRAND_SYMBOL);
	return find_or_upsert (hcl, dic, (hcl_oop_char_t)key, HCL_NULL);
}

hcl_oop_cons_t hcl_lookupdic (hcl_t* hcl, hcl_oop_set_t dic, const hcl_oocs_t* name)
{
	return lookup (hcl, dic, name);
}

hcl_oop_set_t hcl_makedic (hcl_t* hcl, hcl_oow_t size)
{
	return (hcl_oop_set_t)hcl_makeset (hcl, size);
}
