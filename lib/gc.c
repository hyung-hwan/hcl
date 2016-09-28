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

static struct 
{
	hcl_oow_t  len;
	hcl_ooch_t ptr[10];
	int syncode;
	hcl_oow_t  offset;
} syminfo[] =
{
	{  6, { 'b', 'e', 'g', 'i', 'n' },         HCL_SYNCODE_BEGIN,   HCL_OFFSETOF(hcl_t,_begin) },
	{  5, { 'd', 'e', 'f', 'u', 'n' },         HCL_SYNCODE_DEFUN,   HCL_OFFSETOF(hcl_t,_defun) },
	{  2, { 'i', 'f'                     },    HCL_SYNCODE_IF,      HCL_OFFSETOF(hcl_t,_if)     },
	{  6, { 'l', 'a', 'm', 'b', 'd', 'a' },    HCL_SYNCODE_LAMBDA,  HCL_OFFSETOF(hcl_t,_lambda) },
	{  5, { 'q', 'u', 'o', 't', 'e' },         HCL_SYNCODE_QUOTE,   HCL_OFFSETOF(hcl_t,_quote)  },
	{  3, { 's', 'e', 't' },                   HCL_SYNCODE_SET,     HCL_OFFSETOF(hcl_t,_set)    }
};

/* ========================================================================= */

static void compact_symbol_table (hcl_t* hcl, hcl_oop_t _nil)
{
	hcl_oop_char_t symbol;
	hcl_oow_t i, x, y, z;
	hcl_oow_t bucket_size, index;
	hcl_ooi_t tally;

#if defined(HCL_SUPPORT_GC_DURING_IGNITION)
	if (!hcl->symtab) return; /* symbol table has not been created */
#endif

	/* the symbol table doesn't allow more data items than HCL_SMOOI_MAX.
	 * so hcl->symtab->tally must always be a small integer */
	HCL_ASSERT (HCL_OOP_IS_SMOOI(hcl->symtab->tally));
	tally = HCL_OOP_TO_SMOOI(hcl->symtab->tally);
	HCL_ASSERT (tally >= 0); /* it must not be less than 0 */
	if (tally <= 0) return;

	/* NOTE: in theory, the bucket size can be greater than HCL_SMOOI_MAX
	 * as it is an internal header field and is of an unsigned type */
	bucket_size = HCL_OBJ_GET_SIZE(hcl->symtab->bucket);

	for (index = 0; index < bucket_size; )
	{
		if (HCL_OBJ_GET_FLAGS_MOVED(hcl->symtab->bucket->slot[index]))
		{
			index++;
			continue;
		}

		HCL_ASSERT (hcl->symtab->bucket->slot[index] != _nil);

		for (i = 0, x = index, y = index; i < bucket_size; i++)
		{
			y = (y + 1) % bucket_size;

			/* done if the slot at the current hash index is _nil */
			if (hcl->symtab->bucket->slot[y] == _nil) break;

			/* get the natural hash index for the data in the slot 
			 * at the current hash index */
			symbol = (hcl_oop_char_t)hcl->symtab->bucket->slot[y];

			HCL_ASSERT (HCL_BRANDOF(hcl,symbol) == HCL_BRAND_SYMBOL);

			z = hcl_hashchars(symbol->slot, HCL_OBJ_GET_SIZE(symbol)) % bucket_size;

			/* move an element if necessary */
			if ((y > x && (z <= x || z > y)) ||
			    (y < x && (z <= x && z > y)))
			{
				hcl->symtab->bucket->slot[x] = hcl->symtab->bucket->slot[y];
				x = y;
			}
		}

		hcl->symtab->bucket->slot[x] = _nil;
		tally--;
	}

	HCL_ASSERT (tally >= 0);
	HCL_ASSERT (tally <= HCL_SMOOI_MAX);
	hcl->symtab->tally = HCL_SMOOI_TO_OOP(tally);
}


static HCL_INLINE hcl_oow_t get_payload_bytes (hcl_t* hcl, hcl_oop_t oop)
{
	hcl_oow_t nbytes_aligned;

#if defined(HCL_USE_OBJECT_TRAILER)
	if (HCL_OBJ_GET_FLAGS_TRAILER(oop))
	{
		hcl_oow_t nbytes;

		/* only an OOP object can have the trailer. 
		 *
		 * | _flags    |
		 * | _size     |  <-- if it's 3
		 * | _class    |
		 * |   X       |
		 * |   X       |
		 * |   X       |
		 * |   Y       | <-- it may exist if EXTRA is set in _flags.
		 * |   Z       | <-- if TRAILER is set, it is the number of bytes in the trailer
		 * |  |  |  |  | 
		 */
		HCL_ASSERT (HCL_OBJ_GET_FLAGS_TYPE(oop) == HCL_OBJ_TYPE_OOP);
		HCL_ASSERT (HCL_OBJ_GET_FLAGS_UNIT(oop) == HCL_SIZEOF(hcl_oow_t));
		HCL_ASSERT (HCL_OBJ_GET_FLAGS_EXTRA(oop) == 0); /* no 'extra' for an OOP object */

		nbytes = HCL_OBJ_BYTESOF(oop) + HCL_SIZEOF(hcl_oow_t) + \
		         (hcl_oow_t)((hcl_oop_oop_t)oop)->slot[HCL_OBJ_GET_SIZE(oop)];
		nbytes_aligned = HCL_ALIGN (nbytes, HCL_SIZEOF(hcl_oop_t));
	}
	else
	{
#endif
		/* calculate the payload size in bytes */
		nbytes_aligned = HCL_ALIGN (HCL_OBJ_BYTESOF(oop), HCL_SIZEOF(hcl_oop_t));
#if defined(HCL_USE_OBJECT_TRAILER)
	}
#endif

	return nbytes_aligned;
}

hcl_oop_t hcl_moveoop (hcl_t* hcl, hcl_oop_t oop)
{
#if defined(HCL_SUPPORT_GC_DURING_IGNITION)
	if (!oop) return oop;
#endif

	if (!HCL_OOP_IS_POINTER(oop)) return oop;
	if (HCL_OBJ_GET_FLAGS_NGC(oop)) return oop; /* non-GC object */

	if (HCL_OBJ_GET_FLAGS_MOVED(oop))
	{
		/* this object has migrated to the new heap. 
		 * the class field has been updated to the new object
		 * in the 'else' block below. i can simply return it
		 * without further migration. */
		return HCL_OBJ_GET_CLASS(oop);
	}
	else
	{
		hcl_oow_t nbytes_aligned;
		hcl_oop_t tmp;

		nbytes_aligned = get_payload_bytes (hcl, oop);

		/* allocate space in the new heap */
		tmp = hcl_allocheapmem (hcl, hcl->newheap, HCL_SIZEOF(hcl_obj_t) + nbytes_aligned);

		/* allocation here must not fail because
		 * i'm allocating the new space in a new heap for 
		 * moving an existing object in the current heap. 
		 *
		 * assuming the new heap is as large as the old heap,
		 * and garbage collection doesn't allocate more objects
		 * than in the old heap, it must not fail. */
		HCL_ASSERT (tmp != HCL_NULL); 

		/* copy the payload to the new object */
		HCL_MEMCPY (tmp, oop, HCL_SIZEOF(hcl_obj_t) + nbytes_aligned);

		/* mark the old object that it has migrated to the new heap */
		HCL_OBJ_SET_FLAGS_MOVED(oop, 1);

		/* let the class field of the old object point to the new 
		 * object allocated in the new heap. it is returned in 
		 * the 'if' block at the top of this function. */
		HCL_OBJ_SET_CLASS (oop, tmp);

		/* return the new object */
		return tmp;
	}
}

static hcl_uint8_t* scan_new_heap (hcl_t* hcl, hcl_uint8_t* ptr)
{
	while (ptr < hcl->newheap->ptr)
	{
		hcl_oow_t i;
		hcl_oow_t nbytes_aligned;
		hcl_oop_t oop;

		oop = (hcl_oop_t)ptr;

	#if defined(HCL_USE_OBJECT_TRAILER)
		if (HCL_OBJ_GET_FLAGS_TRAILER(oop))
		{
			hcl_oow_t nbytes;

			HCL_ASSERT (HCL_OBJ_GET_FLAGS_TYPE(oop) == HCL_OBJ_TYPE_OOP);
			HCL_ASSERT (HCL_OBJ_GET_FLAGS_UNIT(oop) == HCL_SIZEOF(hcl_oow_t));
			HCL_ASSERT (HCL_OBJ_GET_FLAGS_EXTRA(oop) == 0); /* no 'extra' for an OOP object */

			nbytes = HCL_OBJ_BYTESOF(oop) + HCL_SIZEOF(hcl_oow_t) + \
			         (hcl_oow_t)((hcl_oop_oop_t)oop)->slot[HCL_OBJ_GET_SIZE(oop)];
			nbytes_aligned = HCL_ALIGN (nbytes, HCL_SIZEOF(hcl_oop_t));
		}
		else
		{
	#endif
			nbytes_aligned = HCL_ALIGN (HCL_OBJ_BYTESOF(oop), HCL_SIZEOF(hcl_oop_t));
	#if defined(HCL_USE_OBJECT_TRAILER)
		}
	#endif

		HCL_OBJ_SET_CLASS (oop, hcl_moveoop(hcl, HCL_OBJ_GET_CLASS(oop)));
		if (HCL_OBJ_GET_FLAGS_TYPE(oop) == HCL_OBJ_TYPE_OOP)
		{
			hcl_oop_oop_t xtmp;
			hcl_oow_t size;

			if (HCL_OBJ_GET_FLAGS_BRAND(oop) == HCL_BRAND_PROCESS)
			{
				/* the stack in a process object doesn't need to be 
				 * scanned in full. the slots above the stack pointer 
				 * are garbages. */
				size = HCL_PROCESS_NAMED_INSTVARS +
				       HCL_OOP_TO_SMOOI(((hcl_oop_process_t)oop)->sp) + 1;
				HCL_ASSERT (size <= HCL_OBJ_GET_SIZE(oop));
			}
			else
			{
				size = HCL_OBJ_GET_SIZE(oop);
			}

			xtmp = (hcl_oop_oop_t)oop;
			for (i = 0; i < size; i++)
			{
				if (HCL_OOP_IS_POINTER(xtmp->slot[i]))
					xtmp->slot[i] = hcl_moveoop (hcl, xtmp->slot[i]);
			}
		}

		ptr = ptr + HCL_SIZEOF(hcl_obj_t) + nbytes_aligned;
	}

	/* return the pointer to the beginning of the free space in the heap */
	return ptr; 
}

void hcl_gc (hcl_t* hcl)
{
	/* 
	 * move a referenced object to the new heap.
	 * inspect the fields of the moved object in the new heap.
	 * move objects pointed to by the fields to the new heap.
	 * finally perform some tricky symbol table clean-up.
	 */
	hcl_uint8_t* ptr;
	hcl_heap_t* tmp;
	hcl_oop_t old_nil;
	hcl_oow_t i;
	hcl_cb_t* cb;

	if (hcl->active_context)
	{
		/*HCL_ASSERT ((hcl_oop_t)hcl->processor != hcl->_nil);
		if ((hcl_oop_t)hcl->processor->active != hcl->_nil)*/
			hcl->processor->active->sp = HCL_SMOOI_TO_OOP(hcl->sp);

		/* store the instruction pointer to the active context */
		hcl->active_context->ip = HCL_SMOOI_TO_OOP(hcl->ip);
	}

	HCL_LOG4 (hcl, HCL_LOG_GC | HCL_LOG_INFO, 
		"Starting GC curheap base %p ptr %p newheap base %p ptr %p\n",
		hcl->curheap->base, hcl->curheap->ptr, hcl->newheap->base, hcl->newheap->ptr); 

	/* TODO: allocate common objects like _nil and the root dictionary 
	 *       in the permanant heap.  minimize moving around */
	old_nil = hcl->_nil;

	/* move _nil and the root object table */
	hcl->_nil               = hcl_moveoop (hcl, hcl->_nil);
	hcl->_true              = hcl_moveoop (hcl, hcl->_true);
	hcl->_false             = hcl_moveoop (hcl, hcl->_false);

	for (i = 0; i < HCL_COUNTOF(syminfo); i++)
	{
		hcl_oop_t tmp;
		tmp = *(hcl_oop_t*)((hcl_uint8_t*)hcl + syminfo[i].offset);
		tmp = hcl_moveoop (hcl, tmp);
		*(hcl_oop_t*)((hcl_uint8_t*)hcl + syminfo[i].offset) = tmp;
	}

	hcl->_class                  = hcl_moveoop (hcl, hcl->_class);
	hcl->_character              = hcl_moveoop (hcl, hcl->_character);
	hcl->_small_integer          = hcl_moveoop (hcl, hcl->_small_integer);
	hcl->_large_positive_integer = hcl_moveoop (hcl, hcl->_large_positive_integer);
	hcl->_large_negative_integer = hcl_moveoop (hcl, hcl->_large_negative_integer);

	hcl->sysdic = (hcl_oop_set_t) hcl_moveoop (hcl, (hcl_oop_t)hcl->sysdic);
	hcl->processor = (hcl_oop_process_scheduler_t) hcl_moveoop (hcl, (hcl_oop_t)hcl->processor);
	hcl->nil_process = (hcl_oop_process_t) hcl_moveoop (hcl, (hcl_oop_t)hcl->nil_process);

	for (i = 0; i < hcl->code.lit.len; i++)
	{
		((hcl_oop_oop_t)hcl->code.lit.arr)->slot[i] =
			hcl_moveoop (hcl, ((hcl_oop_oop_t)hcl->code.lit.arr)->slot[i]);
	}

	hcl->p.e = hcl_moveoop (hcl, hcl->p.e);

	for (i = 0; i < hcl->sem_list_count; i++)
	{
		hcl->sem_list[i] = (hcl_oop_semaphore_t)hcl_moveoop (hcl, (hcl_oop_t)hcl->sem_list[i]);
	}

	for (i = 0; i < hcl->sem_heap_count; i++)
	{
		hcl->sem_heap[i] = (hcl_oop_semaphore_t)hcl_moveoop (hcl, (hcl_oop_t)hcl->sem_heap[i]);
	}

	for (i = 0; i < hcl->tmp_count; i++)
	{
		*hcl->tmp_stack[i] = hcl_moveoop (hcl, *hcl->tmp_stack[i]);
	}

	if (hcl->active_context)
		hcl->active_context = (hcl_oop_context_t)hcl_moveoop (hcl, (hcl_oop_t)hcl->active_context);
	if (hcl->active_method)
		hcl->active_method = (hcl_oop_method_t)hcl_moveoop (hcl, (hcl_oop_t)hcl->active_method);

	for (cb = hcl->cblist; cb; cb = cb->next)
	{
		if (cb->gc) cb->gc (hcl);
	}

	/* scan the new heap to move referenced objects */
	ptr = (hcl_uint8_t*) HCL_ALIGN ((hcl_uintptr_t)hcl->newheap->base, HCL_SIZEOF(hcl_oop_t));
	ptr = scan_new_heap (hcl, ptr);

	/* traverse the symbol table for unreferenced symbols.
	 * if the symbol has not moved to the new heap, the symbol
	 * is not referenced by any other objects than the symbol 
	 * table itself */
	compact_symbol_table (hcl, old_nil);

	/* move the symbol table itself */
	hcl->symtab = (hcl_oop_set_t)hcl_moveoop (hcl, (hcl_oop_t)hcl->symtab);

	/* scan the new heap again from the end position of
	 * the previous scan to move referenced objects by 
	 * the symbol table. */
	ptr = scan_new_heap (hcl, ptr);

	/* the contents of the current heap is not needed any more.
	 * reset the upper bound to the base. don't forget to align the heap
	 * pointer to the OOP size. See hcl_makeheap() also */
	hcl->curheap->ptr = (hcl_uint8_t*)HCL_ALIGN(((hcl_uintptr_t)hcl->curheap->base), HCL_SIZEOF(hcl_oop_t));

	/* swap the current heap and old heap */
	tmp = hcl->curheap;
	hcl->curheap = hcl->newheap;
	hcl->newheap = tmp;

/*
{
hcl_oow_t index;
hcl_oop_oop_t buc;
printf ("=== SURVIVING SYMBOLS ===\n");
buc = (hcl_oop_oop_t) hcl->symtab->slot[HCL_SYMTAB_BUCKET];
for (index = 0; index < buc->size; index++)
{
	if ((hcl_oop_t)buc->slot[index] != hcl->_nil) 
	{
		const hcl_oop_char_t* p = ((hcl_oop_char_t)buc->slot[index])->slot;
		printf ("SYM [");
		while (*p) printf ("%c", *p++);
		printf ("]\n");
	}
}
printf ("===========================\n");
}
*/
	if (hcl->active_method) SET_ACTIVE_METHOD_CODE (hcl); /* update hcl->active_code */
}


void hcl_pushtmp (hcl_t* hcl, hcl_oop_t* oop_ptr)
{
	/* if you have too many temporaries pushed, something must be wrong.
	 * change your code not to exceede the stack limit */
	HCL_ASSERT (hcl->tmp_count < HCL_COUNTOF(hcl->tmp_stack));
	hcl->tmp_stack[hcl->tmp_count++] = oop_ptr;
}

void hcl_poptmp (hcl_t* hcl)
{
	HCL_ASSERT (hcl->tmp_count > 0);
	hcl->tmp_count--;
}

void hcl_poptmps (hcl_t* hcl, hcl_oow_t count)
{
	HCL_ASSERT (hcl->tmp_count >= count);
	hcl->tmp_count -= count;
}


hcl_oop_t hcl_shallowcopy (hcl_t* hcl, hcl_oop_t oop)
{
	if (HCL_OOP_IS_POINTER(oop) && HCL_OBJ_GET_FLAGS_BRAND(oop) != HCL_BRAND_SYMBOL)
	{
		hcl_oop_t z;
		hcl_oow_t total_bytes;

		total_bytes = HCL_SIZEOF(hcl_obj_t) + get_payload_bytes(hcl, oop);

		hcl_pushtmp (hcl, &oop);
		z = hcl_allocbytes (hcl, total_bytes);
		hcl_poptmp(hcl);

		HCL_MEMCPY (z, oop, total_bytes);
		return z;
	}

	return oop;
}

/* ========================================================================= */


int hcl_ignite (hcl_t* hcl)
{
	hcl_oow_t i;

	if (!hcl->_nil) 
	{
		hcl->_nil = hcl_makenil (hcl);
		if (!hcl->_nil) return -1;
	}
	if (!hcl->_true) 
	{
		hcl->_true = hcl_maketrue (hcl);
		if (!hcl->_true) return -1;
	}
	if (!hcl->_false)
	{
		hcl->_false = hcl_makefalse (hcl);
		if (!hcl->_false) return -1;
	}

	if (!hcl->symtab) 
	{
		hcl->symtab = (hcl_oop_set_t)hcl_makeset (hcl, hcl->option.dfl_symtab_size);
		if (!hcl->symtab) return -1;
	}

	if (!hcl->sysdic)
	{
		hcl->sysdic = (hcl_oop_set_t)hcl_makeset (hcl, hcl->option.dfl_sysdic_size);
		if (!hcl->sysdic) return -1;
	}

	/* symbol table available now. symbols can be created */
	for (i = 0; i < HCL_COUNTOF(syminfo); i++)
	{
		hcl_oop_t tmp;

		tmp = hcl_makesymbol (hcl, syminfo[i].ptr, syminfo[i].len);
		if (!tmp) return -1;

		HCL_OBJ_SET_FLAGS_SYNCODE (tmp, syminfo[i].syncode);
		*(hcl_oop_t*)((hcl_uint8_t*)hcl + syminfo[i].offset) = tmp;
	}

	if (!hcl->code.bc.arr)
	{
		hcl->code.bc.arr = hcl_makengcbytearray (hcl, HCL_NULL, 20000); /* TODO: set a proper intial size */
		if (!hcl->code.bc.arr) return -1;
	}

	if (!hcl->code.lit.arr)
	{
		hcl->code.lit.arr = hcl_makengcarray (hcl, 20000); /* TOOD: set a proper initial size */
		if (!hcl->code.lit.arr) return -1;
	}

	hcl->p.e = hcl->_nil;
	return 0;
}
