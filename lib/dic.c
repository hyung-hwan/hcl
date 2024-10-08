/*
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

/* The dictionary functions in this file are used for storing
 * a dictionary object enclosed in {}. So putting a non-symbol
 * key is allowed like { 1 2 3 4 } where 1 and 3 are keys.
 * so SYMBOL_ONLY_KEY must not be defined */
/*#define SYMBOL_ONLY_KEY*/

static hcl_oop_oop_t expand_bucket (hcl_t* hcl, hcl_oop_oop_t oldbuc)
{
	hcl_oop_oop_t newbuc;
	hcl_oow_t oldsz, newsz, index;
	hcl_oop_cons_t ass;

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
	newbuc = (hcl_oop_oop_t)hcl_makearray(hcl, newsz);
	hcl_popvolat (hcl);
	if (!newbuc) return HCL_NULL;

	while (oldsz > 0)
	{
		ass = (hcl_oop_cons_t)oldbuc->slot[--oldsz];
		if ((hcl_oop_t)ass != hcl->_nil)
		{
		#if defined(SYMBOL_ONLY_KEY)
			hcl_oop_char_t key;
			HCL_ASSERT (hcl, HCL_IS_CONS(hcl,ass));
			key = (hcl_oop_char_t)ass->car;
			HCL_ASSERT (hcl, HCL_IS_SYMBOL(hcl,key));
			index = hcl_hash_oochars(key->slot, HCL_OBJ_GET_SIZE(key)) % newsz;
		#else
			int n;
			HCL_ASSERT (hcl, HCL_IS_CONS(hcl,ass));
			n = hcl_hashobj(hcl, ass->car, &index);
			HCL_ASSERT (hcl, n == 0); /* since it's expanding, the existing one in the bucket should always be hashable */
			index %= newsz;
		#endif
			while (newbuc->slot[index] != hcl->_nil) index = (index + 1) % newsz;
			newbuc->slot[index] = (hcl_oop_t)ass;
		}
	}

	return newbuc;
}

static hcl_oop_cons_t find_or_upsert (hcl_t* hcl, hcl_oop_dic_t dic, hcl_oop_t key, hcl_oop_t value, int is_method)
{
	hcl_ooi_t tally;
	hcl_oow_t index;
	hcl_oop_cons_t ass;
	hcl_oow_t tmp_count = 0;

	/* the system dictionary is not a generic dictionary.
	 * it accepts only a symbol as a key. */
#if defined(SYMBOL_ONLY_KEY)
	HCL_ASSERT (hcl, HCL_IS_SYMBOL(hcl,key));
#endif
	HCL_ASSERT (hcl, HCL_OOP_IS_SMOOI(dic->tally));
	HCL_ASSERT (hcl, HCL_IS_ARRAY(hcl,dic->bucket));

#if defined(SYMBOL_ONLY_KEY)
	index = hcl_hash_oochars(HCL_OBJ_GET_CHAR_SLOT(key), HCL_OBJ_GET_SIZE(key)) % HCL_OBJ_GET_SIZE(dic->bucket);
#else
	if (hcl_hashobj(hcl, key, &index) <= -1) return HCL_NULL;
	index %= HCL_OBJ_GET_SIZE(dic->bucket);
#endif

	/* find */
	while (dic->bucket->slot[index] != hcl->_nil)
	{
#if defined(SYMBOL_ONLY_KEY)
		/* nothing */
#else
		int n;
#endif

		ass = (hcl_oop_cons_t)dic->bucket->slot[index];
		HCL_ASSERT (hcl, HCL_IS_CONS(hcl,ass));

#if defined(SYMBOL_ONLY_KEY)
		HCL_ASSERT (hcl, HCL_IS_SYMBOL(hcl,ass->car));
		if (HCL_OBJ_GET_SIZE(key) == HCL_OBJ_GET_SIZE(ass->car) &&
		    hcl_equal_oochars(HCL_OBJ_GET_CHAR_SLOT(key), HCL_OBJ_GET_CHAR_SLOT(ass->car), HCL_OBJ_GET_SIZE(key)))
#else
		n = hcl_equalobjs(hcl, key, ass->car);
		if (n <= -1) return HCL_NULL;
		if (n >= 1)
#endif
		{
			/* the value of HCL_NULL indicates no insertion or update. */
			if (value)
			{
				if (is_method)
				{
					hcl_oop_cons_t pair;
					pair = (hcl_oop_cons_t)ass->cdr; /* once found, this must be a pair of method pointers  */
					HCL_ASSERT (hcl, HCL_IS_CONS(hcl, pair));
					HCL_ASSERT (hcl, HCL_IS_COMPILED_BLOCK(hcl, value));
					if (is_method & 1) pair->car = value; /* class method */
					if (is_method & 2) pair->cdr = value; /* instance method */
					/* the class instantiation method goes to both cells.
					 * you can't define a class method or an instance method with the name of
					 * a class instantiation method */
				}
				else ass->cdr = value; /* normal update */
			}
			return ass;
		}

		index = (index + 1) % HCL_OBJ_GET_SIZE(dic->bucket);
	}

	if (!value)
	{
		/* when value is HCL_NULL, perform no insertion.
		 * the value of HCL_NULL indicates no insertion or update. */
		hcl_seterrbfmt (hcl, HCL_ENOENT, "key not found - %O", key);
		return HCL_NULL;
	}

	/* the key is not found. insert it. */
	HCL_ASSERT (hcl, HCL_OOP_IS_SMOOI(dic->tally));
	tally = HCL_OOP_TO_SMOOI(dic->tally);
	if (tally >= HCL_SMOOI_MAX)
	{
		/* this built-in dictionary is not allowed to hold more than
		 * HCL_SMOOI_MAX items for efficiency sake */
		hcl_seterrnum (hcl, HCL_EDFULL);
		return HCL_NULL;
	}

	hcl_pushvolat (hcl, (hcl_oop_t*)&dic); tmp_count++;
	hcl_pushvolat (hcl, (hcl_oop_t*)&key); tmp_count++;
	hcl_pushvolat (hcl, &value); tmp_count++;

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
		bucket = expand_bucket(hcl, dic->bucket);
		if (!bucket) goto oops;

		dic->bucket = bucket;

#if defined(SYMBOL_ONLY_KEY)
		/* recalculate the index for the expanded bucket */
		index = hcl_hash_oochars(HCL_OBJ_GET_CHAR_SLOT(key), HCL_OBJ_GET_SIZE(key)) % HCL_OBJ_GET_SIZE(dic->bucket);
#else
		hcl_hashobj(hcl, key, &index); /* this must succeed as i know 'key' is hashable */
		index %= HCL_OBJ_GET_SIZE(dic->bucket);
#endif
		while (dic->bucket->slot[index] != hcl->_nil)
			index = (index + 1) % HCL_OBJ_GET_SIZE(dic->bucket);
	}

	if (is_method)
	{
		/* create a new pair that holds a class method at the first cell and an instance method at the second cell */
		hcl_oop_t pair;
		HCL_ASSERT (hcl, HCL_IS_COMPILED_BLOCK(hcl, value));
		hcl_pushvolat (hcl, &key);
		pair = hcl_makecons(hcl, (is_method & 1? value: hcl->_nil), (is_method & 2? value: hcl->_nil));
		hcl_popvolat (hcl);
		if (HCL_UNLIKELY(!pair)) goto oops;
		value = pair;
	}

	/* create a new assocation of a key and a value since
	 * the key isn't found in the root dictionary */
	ass = (hcl_oop_cons_t)hcl_makecons(hcl, (hcl_oop_t)key, value);
	if (HCL_UNLIKELY(!ass)) goto oops;

	/* the current tally must be less than the maximum value. otherwise,
	 * it overflows after increment below */
	HCL_ASSERT (hcl, tally < HCL_SMOOI_MAX);
	dic->tally = HCL_SMOOI_TO_OOP(tally + 1);
	dic->bucket->slot[index] = (hcl_oop_t)ass;

	hcl_popvolats (hcl, tmp_count);
	return ass;

oops:
	hcl_popvolats (hcl, tmp_count);
	return HCL_NULL;
}

static hcl_oop_cons_t lookupdic_noseterr (hcl_t* hcl, hcl_oop_dic_t dic, const hcl_oocs_t* name)
{
	/* this is special version of hcl_getatsysdic() that performs
	 * lookup using a plain symbol specified */

	hcl_oow_t index;
	hcl_oop_cons_t ass;

	HCL_ASSERT (hcl, HCL_OOP_IS_SMOOI(dic->tally));
	HCL_ASSERT (hcl, HCL_IS_ARRAY(hcl,dic->bucket));

	index = hcl_hash_oochars(name->ptr, name->len) % HCL_OBJ_GET_SIZE(dic->bucket);

	while ((hcl_oop_t)(ass = (hcl_oop_cons_t)HCL_OBJ_GET_OOP_VAL(dic->bucket, index)) != hcl->_nil)
	{
		HCL_ASSERT (hcl, HCL_IS_CONS(hcl,ass));
		if (HCL_IS_SYMBOL(hcl, ass->car))
		{
			if (name->len == HCL_OBJ_GET_SIZE(ass->car) &&
			    hcl_equal_oochars(name->ptr, HCL_OBJ_GET_CHAR_SLOT(ass->car), name->len))
			{
				return ass;
			}
		}

		index = (index + 1) % HCL_OBJ_GET_SIZE(dic->bucket);
	}


	/* when value is HCL_NULL, perform no insertion */

	/* hcl_seterrXXX() is not called here. the dictionary lookup is very frequent
	 * and so is lookup failure. for instance, hcl_findmethod() calls this over
	 * a class chain. there might be a failure at each class level. it's waste to
	 * set the error information whenever the failure occurs.
	 * the caller of this function must set the error information upon failure */
	return HCL_NULL;
}

static HCL_INLINE hcl_oop_cons_t lookupdic (hcl_t* hcl, hcl_oop_dic_t dic, const hcl_oocs_t* name)
{
	hcl_oop_cons_t ass = lookupdic_noseterr(hcl, dic, name);
	if (!ass) hcl_seterrbfmt(hcl, HCL_ENOENT, "unable to find %.*js in a dictionary", name->len, name->ptr);
	return ass;
}

hcl_oop_cons_t hcl_lookupdicforsymbol_noseterr (hcl_t* hcl, hcl_oop_dic_t dic, const hcl_oocs_t* name)
{
	return lookupdic_noseterr(hcl, dic, name);
}

hcl_oop_cons_t hcl_lookupdicforsymbol (hcl_t* hcl, hcl_oop_dic_t dic, const hcl_oocs_t* name)
{
	return lookupdic(hcl, dic, name);
}


hcl_oop_cons_t hcl_putatsysdic (hcl_t* hcl, hcl_oop_t key, hcl_oop_t value)
{
#if defined(SYMBOL_ONLY_KEY)
	HCL_ASSERT (hcl, HCL_IS_SYMBOL(hcl,key));
#endif
	return find_or_upsert(hcl, hcl->sysdic, key, value, 0);
}

hcl_oop_cons_t hcl_getatsysdic (hcl_t* hcl, hcl_oop_t key)
{
#if defined(SYMBOL_ONLY_KEY)
	HCL_ASSERT (hcl, HCL_IS_SYMBOL(hcl,key));
#endif
	return find_or_upsert(hcl, hcl->sysdic, key, HCL_NULL, 0);
}

hcl_oop_cons_t hcl_lookupsysdicforsymbol_noseterr (hcl_t* hcl, const hcl_oocs_t* name)
{
	return lookupdic_noseterr(hcl, hcl->sysdic, name);
}

hcl_oop_cons_t hcl_lookupsysdicforsymbol (hcl_t* hcl, const hcl_oocs_t* name)
{
	return lookupdic(hcl, hcl->sysdic, name);
}

int hcl_zapatsysdic (hcl_t* hcl, hcl_oop_t key)
{
#if defined(SYMBOL_ONLY_KEY)
	HCL_ASSERT (hcl, HCL_IS_SYMBOL(hcl,key));
#endif
	return hcl_zapatdic(hcl, hcl->sysdic, key);
}

hcl_oop_cons_t hcl_putatdic (hcl_t* hcl, hcl_oop_dic_t dic, hcl_oop_t key, hcl_oop_t value)
{
#if defined(SYMBOL_ONLY_KEY)
	HCL_ASSERT (hcl, HCL_IS_SYMBOL(hcl,key));
#endif
	return find_or_upsert(hcl, dic, key, value, 0);
}

hcl_oop_cons_t hcl_putatdic_method (hcl_t* hcl, hcl_oop_dic_t dic, hcl_oop_t key, hcl_oop_t value, int mtype)
{
#if defined(SYMBOL_ONLY_KEY)
	HCL_ASSERT (hcl, HCL_IS_SYMBOL(hcl,key));
#endif
	return find_or_upsert(hcl, dic, key, value, mtype);
}

hcl_oop_cons_t hcl_getatdic (hcl_t* hcl, hcl_oop_dic_t dic, hcl_oop_t key)
{
#if defined(SYMBOL_ONLY_KEY)
	HCL_ASSERT (hcl, HCL_IS_SYMBOL(hcl,key));
#endif
	return find_or_upsert(hcl, dic, key, HCL_NULL, 0);
}

int hcl_zapatdic (hcl_t* hcl, hcl_oop_dic_t dic, hcl_oop_t key)
{
	hcl_ooi_t tally;
	hcl_oow_t index, bs, i, x, y, z;
	hcl_oop_cons_t ass;

	tally = HCL_OOP_TO_SMOOI(dic->tally);
	bs = HCL_OBJ_GET_SIZE(dic->bucket);

	/* the system dictionary is not a generic dictionary.
	 * it accepts only a symbol as a key. */
#if defined(SYMBOL_ONLY_KEY)
	HCL_ASSERT (hcl, HCL_IS_SYMBOL(hcl,key));
#endif
	HCL_ASSERT (hcl, HCL_OOP_IS_SMOOI(dic->tally));
	HCL_ASSERT (hcl, HCL_IS_ARRAY(hcl,dic->bucket));

#if defined(SYMBOL_ONLY_KEY)
	index = hcl_hash_oochars(HCL_OBJ_GET_CHAR_SLOT(key), HCL_OBJ_GET_SIZE(key)) % bs;
#else
	if (hcl_hashobj(hcl, key, &index) <= -1) return -1;
	index %= bs;
#endif

	/* find */
	while (dic->bucket->slot[index] != hcl->_nil)
	{
	#if defined(SYMBOL_ONLY_KEY)
		ass = (hcl_oop_cons_t)dic->bucket->slot[index];
		HCL_ASSERT (hcl, HCL_IS_CONS(hcl,ass));
		HCL_ASSERT (hcl, HCL_IS_SYMBOL(hcl,ass->car));

		if (HCL_OBJ_GET_SIZE(key) == HCL_OBJ_GET_SIZE(ass->car) &&
		    hcl_equal_oochars(HCL_OBJ_GET_CHAR_SLOT(key), HCL_OBJ_GET_CHAR_SLOT(ass->car), HCL_OBJ_GET_SIZE(key)))
		{
			/* the value of HCL_NULL indicates no insertion or update. */
			goto found;
		}
	#else
		int n;

		ass = (hcl_oop_cons_t)dic->bucket->slot[index];
		HCL_ASSERT (hcl, HCL_IS_CONS(hcl,ass));

		n = hcl_equalobjs(hcl, (hcl_oop_t)key, ass->car);
		if (n <= -1) return -1;
		if (n >= 1) goto found;
	#endif

		index = (index + 1) % bs;
	}

	hcl_seterrnum (hcl, HCL_ENOENT);
	return -1;

found:
	/* compact the cluster */
	for (i = 0, x = index, y = index; i < tally; i++)
	{
		y = (y + 1) % bs;

		/* done if the slot at the current index is empty */
		if (dic->bucket->slot[y] == hcl->_nil) break;

		ass = (hcl_oop_cons_t)dic->bucket->slot[y];
	#if defined(SYMBOL_ONLY_KEY)
		/* get the natural hash index for the data in the slot at
		 * the current hash index */
		z = hcl_hash_oochars(HCL_OBJ_GET_CHAR_SLOT(ass->car), HCL_OBJ_GET_SIZE(ass->car)) % bs;
	#else
		if (hcl_hashobj(hcl, ass->car, &z) <= -1) return -1;
		z %= bs;
	#endif

		/* move an element if necesary */
		if ((y > x && (z <= x || z > y)) ||
		    (y < x && (z <= x && z > y)))
		{
			dic->bucket->slot[x] = dic->bucket->slot[y];
			x = y;
		}
	}

	dic->bucket->slot[x] = hcl->_nil;

	tally--;
	dic->tally = HCL_SMOOI_TO_OOP(tally);

	return 0;
}

hcl_oop_t hcl_makedic (hcl_t* hcl, hcl_oow_t inisize)
{
#if 0
	hcl_oop_dic_t obj;

	obj = (hcl_oop_dic_t)hcl_allocoopobj(hcl, HCL_BRAND_DIC, 2);
	if (obj)
	{
		hcl_oop_oop_t bucket;

		obj->tally = HCL_SMOOI_TO_OOP(0);

		hcl_pushvolat (hcl, (hcl_oop_t*)&obj);
		bucket = (hcl_oop_oop_t)hcl_makearray(hcl, inisize);
		hcl_popvolat (hcl);

		if (!bucket) obj = HCL_NULL;
		else obj->bucket = bucket;
	}

	return (hcl_oop_t)obj;
#else
	hcl_oop_dic_t v;

	v = (hcl_oop_dic_t)hcl_instantiate(hcl, hcl->c_dictionary, HCL_NULL, 0);
	if (HCL_UNLIKELY(!v))
	{
		const hcl_ooch_t* orgmsg = hcl_backuperrmsg(hcl);
		hcl_seterrbfmt (hcl, HCL_ERRNUM(hcl),
			"unable to instantiate %O - %js", hcl->c_dictionary->name, orgmsg);
	}
	else
	{
		hcl_oop_oop_t bucket;

		v->tally = HCL_SMOOI_TO_OOP(0);

		hcl_pushvolat (hcl, (hcl_oop_t*)&v);
		bucket = (hcl_oop_oop_t)hcl_makearray(hcl, inisize);
		hcl_popvolat (hcl);

		if (HCL_UNLIKELY(!bucket))
		{
			/* TODO: can I remove the instanated object immediately above?
			 *       it must be ok as the dictionary object is never referenced.
			 *       some care must be taken not to screw up gc metadata...  */
			v = HCL_NULL;
		}
		else v->bucket = bucket;
	}

	return (hcl_oop_t)v;
#endif
}

int hcl_walkdic (hcl_t* hcl, hcl_oop_dic_t dic, hcl_dic_walker_t walker, void* ctx)
{
	hcl_oow_t i;

	hcl_pushvolat (hcl, (hcl_oop_t*)&dic);

	for (i = 0; i < HCL_OBJ_GET_SIZE(dic->bucket); i++)
	{
		hcl_oop_t tmp = dic->bucket->slot[i];
		if (HCL_IS_CONS(hcl, tmp) && walker(hcl, dic, (hcl_oop_cons_t)tmp, ctx) <= -1) return -1;
	}

	hcl_popvolat (hcl);
	return 0;
}

