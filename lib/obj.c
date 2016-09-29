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

void* hcl_allocbytes (hcl_t* hcl, hcl_oow_t size)
{
	hcl_uint8_t* ptr;

#if defined(HCL_DEBUG_GC)
	if (!(hcl->option.trait & HCL_NOGC)) hcl_gc (hcl);
#endif

	ptr = hcl_allocheapmem (hcl, hcl->curheap, size);
	if (!ptr && !(hcl->option.trait & HCL_NOGC))
	{
		hcl_gc (hcl);
		ptr = hcl_allocheapmem (hcl, hcl->curheap, size);

/* TODO: grow heap if ptr is still null. */
	}

	return ptr;
}

hcl_oop_t hcl_allocoopobj (hcl_t* hcl, hcl_oow_t size)
{
	hcl_oop_oop_t hdr;
	hcl_oow_t nbytes, nbytes_aligned;

	nbytes = size * HCL_SIZEOF(hcl_oop_t);

	/* this isn't really necessary since nbytes must be 
	 * aligned already. */
	nbytes_aligned = HCL_ALIGN(nbytes, HCL_SIZEOF(hcl_oop_t)); 

	/* making the number of bytes to allocate a multiple of
	 * HCL_SIZEOF(hcl_oop_t) will guarantee the starting address
	 * of the allocated space to be an even number. 
	 * see HCL_OOP_IS_NUMERIC() and HCL_OOP_IS_POINTER() */
	hdr = hcl_allocbytes (hcl, HCL_SIZEOF(hcl_obj_t) + nbytes_aligned);
	if (!hdr) return HCL_NULL;

	hdr->_flags = HCL_OBJ_MAKE_FLAGS(HCL_OBJ_TYPE_OOP, HCL_SIZEOF(hcl_oop_t), 0, 0, 0, 0, 0, 0);
	HCL_OBJ_SET_SIZE (hdr, size);
	HCL_OBJ_SET_CLASS (hdr, hcl->_nil);

	while (size > 0) hdr->slot[--size] = hcl->_nil;

	return (hcl_oop_t)hdr;
}

#if defined(HCL_USE_OBJECT_TRAILER)
hcl_oop_t hcl_allocoopobjwithtrailer (hcl_t* hcl, hcl_oow_t size, const hcl_oob_t* bptr, hcl_oow_t blen)
{
	hcl_oop_oop_t hdr;
	hcl_oow_t nbytes, nbytes_aligned;
	hcl_oow_t i;

	/* +1 for the trailer size of the hcl_oow_t type */
	nbytes = (size + 1) * HCL_SIZEOF(hcl_oop_t) + blen;
	nbytes_aligned = HCL_ALIGN(nbytes, HCL_SIZEOF(hcl_oop_t)); 

	hdr = hcl_allocbytes (hcl, HCL_SIZEOF(hcl_obj_t) + nbytes_aligned);
	if (!hdr) return HCL_NULL;

	hdr->_flags = HCL_OBJ_MAKE_FLAGS(HCL_OBJ_TYPE_OOP, HCL_SIZEOF(hcl_oop_t), 0, 0, 0, 0, 1, 0);
	HCL_OBJ_SET_SIZE (hdr, size);
	HCL_OBJ_SET_CLASS (hdr, hcl->_nil);

	for (i = 0; i < size; i++) hdr->slot[i] = hcl->_nil;

	/* [NOTE] this is not converted to a SMOOI object */
	hdr->slot[size] = (hcl_oop_t)blen; 

	if (bptr)
	{
		HCL_MEMCPY (&hdr->slot[size + 1], bptr, blen);
	}
	else
	{
		HCL_MEMSET (&hdr->slot[size + 1], 0, blen);
	}

	return (hcl_oop_t)hdr;
}
#endif

static HCL_INLINE hcl_oop_t alloc_numeric_array (hcl_t* hcl, const void* ptr, hcl_oow_t len, hcl_obj_type_t type, hcl_oow_t unit, int extra, int ngc)
{
	/* allocate a variable object */

	hcl_oop_t hdr;
	hcl_oow_t xbytes, nbytes, nbytes_aligned;

	xbytes = len * unit; 
	/* 'extra' indicates an extra unit to append at the end.
	 * it's useful to store a string with a terminating null */
	nbytes = extra? xbytes + len: xbytes; 
	nbytes_aligned = HCL_ALIGN(nbytes, HCL_SIZEOF(hcl_oop_t));
/* TODO: check overflow in size calculation*/

	/* making the number of bytes to allocate a multiple of
	 * HCL_SIZEOF(hcl_oop_t) will guarantee the starting address
	 * of the allocated space to be an even number. 
	 * see HCL_OOP_IS_NUMERIC() and HCL_OOP_IS_POINTER() */
	if (HCL_UNLIKELY(ngc))
		hdr = hcl_callocmem (hcl, HCL_SIZEOF(hcl_obj_t) + nbytes_aligned);
	else
		hdr = hcl_allocbytes (hcl, HCL_SIZEOF(hcl_obj_t) + nbytes_aligned);
	if (!hdr) return HCL_NULL;

	hdr->_flags = HCL_OBJ_MAKE_FLAGS(type, unit, extra, 0, 0, ngc, 0, 0);
	hdr->_size = len;
	HCL_OBJ_SET_SIZE (hdr, len);
	HCL_OBJ_SET_CLASS (hdr, hcl->_nil);

	if (ptr)
	{
		/* copy data */
		HCL_MEMCPY (hdr + 1, ptr, xbytes);
		HCL_MEMSET ((hcl_uint8_t*)(hdr + 1) + xbytes, 0, nbytes_aligned - xbytes);
	}
	else
	{
		/* initialize with zeros when the string pointer is not given */
		HCL_MEMSET ((hdr + 1), 0, nbytes_aligned);
	}

	return hdr;
}

hcl_oop_t hcl_alloccharobj (hcl_t* hcl, const hcl_ooch_t* ptr, hcl_oow_t len)
{
	return alloc_numeric_array (hcl, ptr, len, HCL_OBJ_TYPE_CHAR, HCL_SIZEOF(hcl_ooch_t), 1, 0);
}

hcl_oop_t hcl_allocbyteobj (hcl_t* hcl, const hcl_oob_t* ptr, hcl_oow_t len)
{
	return alloc_numeric_array (hcl, ptr, len, HCL_OBJ_TYPE_BYTE, HCL_SIZEOF(hcl_oob_t), 0, 0);
}

hcl_oop_t hcl_allochalfwordobj (hcl_t* hcl, const hcl_oohw_t* ptr, hcl_oow_t len)
{
	return alloc_numeric_array (hcl, ptr, len, HCL_OBJ_TYPE_HALFWORD, HCL_SIZEOF(hcl_oohw_t), 0, 0);
}

hcl_oop_t hcl_allocwordobj (hcl_t* hcl, const hcl_oow_t* ptr, hcl_oow_t len)
{
	return alloc_numeric_array (hcl, ptr, len, HCL_OBJ_TYPE_WORD, HCL_SIZEOF(hcl_oow_t), 0, 0);
}


static HCL_INLINE int decode_spec (hcl_t* hcl, hcl_oop_t _class, hcl_oow_t vlen, hcl_obj_type_t* type, hcl_oow_t* outlen)
{
	hcl_oow_t spec;
	hcl_oow_t named_instvar;
	hcl_obj_type_t indexed_type;

	HCL_ASSERT (HCL_OOP_IS_POINTER(_class));
	HCL_ASSERT (HCL_CLASSOF(hcl, _class) == hcl->_class);

	HCL_ASSERT (HCL_OOP_IS_SMOOI(((hcl_oop_class_t)_class)->spec));
	spec = HCL_OOP_TO_SMOOI(((hcl_oop_class_t)_class)->spec);

	named_instvar = HCL_CLASS_SPEC_NAMED_INSTVAR(spec); /* size of the named_instvar part */

	if (HCL_CLASS_SPEC_IS_INDEXED(spec)) 
	{
		indexed_type = HCL_CLASS_SPEC_INDEXED_TYPE(spec);

		if (indexed_type == HCL_OBJ_TYPE_OOP)
		{
			if (named_instvar > HCL_MAX_NAMED_INSTVARS ||
			    vlen > HCL_MAX_INDEXED_INSTVARS(named_instvar))
			{
				return -1;
			}

			HCL_ASSERT (named_instvar + vlen <= HCL_OBJ_SIZE_MAX);
		}
		else
		{
			/* a non-pointer indexed class can't have named instance variables */
			if (named_instvar > 0) return -1;
			if (vlen > HCL_OBJ_SIZE_MAX) return -1;
		}
	}
	else
	{
		/* named instance variables only. treat it as if it is an
		 * indexable class with no variable data */
		indexed_type = HCL_OBJ_TYPE_OOP;
		vlen = 0; /* vlen is not used */

		if (named_instvar > HCL_MAX_NAMED_INSTVARS) return -1;
		HCL_ASSERT (named_instvar <= HCL_OBJ_SIZE_MAX);
	}

	*type = indexed_type;
	*outlen = named_instvar + vlen;
	return 0; 
}

hcl_oop_t hcl_instantiate (hcl_t* hcl, hcl_oop_t _class, const void* vptr, hcl_oow_t vlen)
{
	hcl_oop_t oop;
	hcl_obj_type_t type;
	hcl_oow_t alloclen;
	hcl_oow_t tmp_count = 0;

	HCL_ASSERT (hcl->_nil != HCL_NULL);

	if (decode_spec (hcl, _class, vlen, &type, &alloclen) <= -1) 
	{
		hcl->errnum = HCL_EINVAL;
		return HCL_NULL;
	}

	hcl_pushtmp (hcl, &_class); tmp_count++;

	switch (type)
	{
		case HCL_OBJ_TYPE_OOP:
			/* both the fixed part(named instance variables) and 
			 * the variable part(indexed instance variables) are allowed. */
			oop = hcl_allocoopobj (hcl, alloclen);

			HCL_ASSERT (vptr == HCL_NULL);
			/*
			This function is not GC-safe. so i don't want to initialize
			the payload of a pointer object. The caller can call this
			function and initialize payloads then.
			if (oop && vptr && vlen > 0)
			{
				hcl_oop_oop_t hdr = (hcl_oop_oop_t)oop;
				HCL_MEMCPY (&hdr->slot[named_instvar], vptr, vlen * HCL_SIZEOF(hcl_oop_t));
			}

			For the above code to work, it should protect the elements of 
			the vptr array with hcl_pushtmp(). So it might be better 
			to disallow a non-NULL vptr when indexed_type is OOP. See
			the assertion above this comment block.
			*/
			break;

		case HCL_OBJ_TYPE_CHAR:
			oop = hcl_alloccharobj (hcl, vptr, alloclen);
			break;

		case HCL_OBJ_TYPE_BYTE:
			oop = hcl_allocbyteobj (hcl, vptr, alloclen);
			break;

		case HCL_OBJ_TYPE_HALFWORD:
			oop = hcl_allochalfwordobj (hcl, vptr, alloclen);
			break;

		case HCL_OBJ_TYPE_WORD:
			oop = hcl_allocwordobj (hcl, vptr, alloclen);
			break;

		default:
			hcl->errnum = HCL_EINTERN;
			oop = HCL_NULL;
			break;
	}

	if (oop) HCL_OBJ_SET_CLASS (oop, _class);
	hcl_poptmps (hcl, tmp_count);
	return oop;
}

#if defined(HCL_USE_OBJECT_TRAILER)

hcl_oop_t hcl_instantiatewithtrailer (hcl_t* hcl, hcl_oop_t _class, hcl_oow_t vlen, const hcl_oob_t* tptr, hcl_oow_t tlen)
{
	hcl_oop_t oop;
	hcl_obj_type_t type;
	hcl_oow_t alloclen;
	hcl_oow_t tmp_count = 0;

	HCL_ASSERT (hcl->_nil != HCL_NULL);

	if (decode_spec (hcl, _class, vlen, &type, &alloclen) <= -1) 
	{
		hcl->errnum = HCL_EINVAL;
		return HCL_NULL;
	}

	hcl_pushtmp (hcl, &_class); tmp_count++;

	switch (type)
	{
		case HCL_OBJ_TYPE_OOP:
			/* NOTE: vptr is not used for GC unsafety */
			oop = hcl_allocoopobjwithtrailer(hcl, alloclen, tptr, tlen);
			break;

		default:
			hcl->errnum = HCL_EINTERN;
			oop = HCL_NULL;
			break;
	}

	if (oop) HCL_OBJ_SET_CLASS (oop, _class);
	hcl_poptmps (hcl, tmp_count);
	return oop;
}
#endif


/* ------------------------------------------------------------------------ *
 * COMMON OBJECTS
 * ------------------------------------------------------------------------ */


hcl_oop_t hcl_makenil (hcl_t* hcl)
{
	hcl_oop_t obj;

	obj = hcl_allocoopobj (hcl, 0);
	if (obj)
	{
		HCL_OBJ_SET_FLAGS_BRAND (obj, HCL_BRAND_NIL);
	}

	return obj;
}

hcl_oop_t hcl_maketrue (hcl_t* hcl)
{
	hcl_oop_t obj;

	obj = hcl_allocoopobj (hcl, 0);
	if (obj)
	{
		HCL_OBJ_SET_FLAGS_BRAND (obj, HCL_BRAND_TRUE);
	}

	return obj;
}

hcl_oop_t hcl_makefalse (hcl_t* hcl)
{
	hcl_oop_t obj;

	obj = hcl_allocoopobj (hcl, 0);
	if (obj)
	{
		HCL_OBJ_SET_FLAGS_BRAND (obj, HCL_BRAND_FALSE);
	}

	return obj;
}

hcl_oop_t hcl_makeinteger (hcl_t* hcl, hcl_ooi_t v)
{
	hcl_oop_t obj;

	if (HCL_IN_SMOOI_RANGE(v)) return HCL_SMOOI_TO_OOP(v);

	obj = hcl_allocwordobj (hcl, (hcl_oow_t*)&v, 1);
	if (obj)
	{
		HCL_OBJ_SET_FLAGS_BRAND (obj, HCL_BRAND_INTEGER);
	}

	return obj;
}

hcl_oop_t hcl_makecons (hcl_t* hcl, hcl_oop_t car, hcl_oop_t cdr)
{
	hcl_oop_cons_t cons;

	hcl_pushtmp (hcl, &car);
	hcl_pushtmp (hcl, &cdr);

	cons = (hcl_oop_cons_t)hcl_allocoopobj (hcl, 2);
	if (cons)
	{
		cons->car = car;
		cons->cdr = cdr;
		HCL_OBJ_SET_FLAGS_BRAND (cons, HCL_BRAND_CONS);
	}

	hcl_poptmps (hcl, 2);

	return (hcl_oop_t)cons;
}

hcl_oop_t hcl_makearray (hcl_t* hcl, hcl_oow_t size)
{
	hcl_oop_t obj;

	obj = hcl_allocoopobj (hcl, size);
	if (obj)
	{
		HCL_OBJ_SET_FLAGS_BRAND (obj, HCL_BRAND_ARRAY);
	}

	return obj;
}

hcl_oop_t hcl_makebytearray (hcl_t* hcl, const hcl_oob_t* ptr, hcl_oow_t size)
{
	hcl_oop_t obj;

	obj = hcl_allocbyteobj (hcl, ptr, size);
	if (obj)
	{
		HCL_OBJ_SET_FLAGS_BRAND (obj, HCL_BRAND_BYTE_ARRAY);
	}

	return obj;
}

hcl_oop_t hcl_makestring (hcl_t* hcl, const hcl_ooch_t* ptr, hcl_oow_t len)
{
	hcl_oop_t obj;

	obj = hcl_alloccharobj (hcl, ptr, len);
	if (obj)
	{
		HCL_OBJ_SET_FLAGS_BRAND (obj, HCL_BRAND_STRING);
	}

	return obj;
}

hcl_oop_t hcl_makeset (hcl_t* hcl, hcl_oow_t inisize)
{
	hcl_oop_set_t obj;

	obj = (hcl_oop_set_t)hcl_allocoopobj (hcl, 2);
	if (obj)
	{
		hcl_oop_oop_t bucket;

		HCL_OBJ_SET_FLAGS_BRAND (obj, HCL_BRAND_SET);
		obj->tally = HCL_SMOOI_TO_OOP(0);
		
		hcl_pushtmp (hcl, (hcl_oop_t*)&obj);
		bucket = (hcl_oop_oop_t)hcl_makearray (hcl, inisize);
		hcl_poptmp (hcl);

		if (!bucket) obj = HCL_NULL;
		else obj->bucket = bucket;
	}

	return (hcl_oop_t)obj;
}


void hcl_freengcobj (hcl_t* hcl, hcl_oop_t obj)
{
	if (HCL_OOP_IS_POINTER(obj) && HCL_OBJ_GET_FLAGS_NGC(obj)) hcl_freemem (hcl, obj);
}

hcl_oop_t hcl_makengcbytearray (hcl_t* hcl, const hcl_oob_t* ptr, hcl_oow_t len)
{
	hcl_oop_t obj;

	obj = alloc_numeric_array (hcl, ptr, len, HCL_OBJ_TYPE_BYTE, HCL_SIZEOF(hcl_oob_t), 0, 1);
	if (obj)
	{
		HCL_OBJ_SET_FLAGS_BRAND (obj, HCL_BRAND_BYTE_ARRAY);
	}

	return obj;
}

hcl_oop_t hcl_remakengcbytearray (hcl_t* hcl, hcl_oop_t obj, hcl_oow_t newsize)
{
	hcl_oop_t tmp;

	HCL_ASSERT (!obj || (HCL_OOP_IS_POINTER(obj) && HCL_OBJ_GET_FLAGS_NGC(obj)));

	/* no hcl_pushtmp() is needed because 'obj' is a non-GC object. */
	/* TODO: improve this by using realloc */

	tmp = hcl_makengcbytearray (hcl, HCL_NULL, newsize);
	if (tmp)
	{
		if (obj)
		{
			hcl_oow_t cpsize;
			cpsize =  (newsize > HCL_OBJ_GET_SIZE(obj))? HCL_OBJ_GET_SIZE(obj): newsize;
			HCL_MEMCPY (((hcl_oop_byte_t)tmp)->slot, ((hcl_oop_byte_t)obj)->slot, cpsize * HCL_SIZEOF(hcl_oob_t));
		}
		hcl_freengcobj (hcl, obj);
	}
	return tmp;
}

hcl_oop_t hcl_makengcarray (hcl_t* hcl, hcl_oow_t len)
{
	hcl_oop_t obj;

	obj = alloc_numeric_array (hcl, HCL_NULL, len, HCL_OBJ_TYPE_OOP, HCL_SIZEOF(hcl_oop_t), 0, 1);
	if (obj)
	{
		HCL_OBJ_SET_FLAGS_BRAND (obj, HCL_BRAND_ARRAY);
	}

	return obj;
}

hcl_oop_t hcl_remakengcarray (hcl_t* hcl, hcl_oop_t obj, hcl_oow_t newsize)
{
	hcl_oop_t tmp;

	HCL_ASSERT (!obj || (HCL_OOP_IS_POINTER(obj) && HCL_OBJ_GET_FLAGS_NGC(obj)));

	/* no hcl_pushtmp() is needed because 'obj' is a non-GC object. */
	/* TODO: improve this by using realloc */

	tmp = hcl_makengcarray (hcl, newsize);
	if (tmp)
	{
		if (obj)
		{
			hcl_oow_t cpsize;
			cpsize =  (newsize > HCL_OBJ_GET_SIZE(obj))? HCL_OBJ_GET_SIZE(obj): newsize;
			HCL_MEMCPY (((hcl_oop_oop_t)tmp)->slot, ((hcl_oop_oop_t)obj)->slot, cpsize * HCL_SIZEOF(hcl_oop_t));
		}
		hcl_freengcobj (hcl, obj);
	}
	return tmp;
}


/* ------------------------------------------------------------------------ *
 * CONS
 * ------------------------------------------------------------------------ */
hcl_oow_t hcl_countcons (hcl_t* hcl, hcl_oop_t cons)
{
	/* this function ignores the last cdr */
	hcl_oow_t count = 1;

	HCL_ASSERT (HCL_BRANDOF(hcl, cons));
	do
	{
		cons = HCL_CONS_CDR(cons);
		if (HCL_BRANDOF(hcl, cons) != HCL_BRAND_CONS) break;
		count++;
	}
	while (1);

	return count;
}

hcl_oop_t hcl_getlastconscdr (hcl_t* hcl, hcl_oop_t cons)
{
	HCL_ASSERT (HCL_BRANDOF(hcl, cons));
	do
	{
		cons = HCL_CONS_CDR(cons);
		if (HCL_BRANDOF(hcl, cons) != HCL_BRAND_CONS) break;
	}
	while (1);

	return cons;
}

hcl_oop_t hcl_reversecons (hcl_t* hcl, hcl_oop_t cons)
{
	hcl_oop_t ptr, prev, next;

	/* Note: The non-nil cdr in the last cons cell gets lost.
	 *  e.g.) Reversing (1 2 3 . 4) results in (3 2 1) */

	HCL_ASSERT (HCL_BRANDOF(hcl, cons));

	prev = hcl->_nil;
	ptr = cons;

	do
	{
		next = HCL_CONS_CDR(ptr);
		HCL_CONS_CDR(ptr) = prev;
		prev = ptr;
		if (HCL_BRANDOF(hcl,next) != HCL_BRAND_CONS) break;
		ptr = next;
	}
	while (1);

	return ptr;
}