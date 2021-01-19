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

#if defined(HCL_PROFILE_VM)
#include <sys/time.h>
#include <sys/resource.h> /* getrusage */
#endif

static struct 
{
	hcl_oow_t  len;
	hcl_ooch_t ptr[20];
	int syncode;
	hcl_oow_t  offset;
} syminfo[] =
{
	{  3, { 'a','n','d' },                 HCL_SYNCODE_AND,     HCL_OFFSETOF(hcl_t,_and)  },
	{  5, { 'b','r','e','a','k' },         HCL_SYNCODE_BREAK,   HCL_OFFSETOF(hcl_t,_break)  },
	{  5, { 'd','e','f','u','n' },         HCL_SYNCODE_DEFUN,   HCL_OFFSETOF(hcl_t,_defun)  },
	{  2, { 'd','o' },                     HCL_SYNCODE_DO,      HCL_OFFSETOF(hcl_t,_do)  },
	{  4, { 'e','l','i','f' },             HCL_SYNCODE_ELIF,    HCL_OFFSETOF(hcl_t,_elif)   },
	{  4, { 'e','l','s','e' },             HCL_SYNCODE_ELSE,    HCL_OFFSETOF(hcl_t,_else)   },
	{  2, { 'i','f' },                     HCL_SYNCODE_IF,      HCL_OFFSETOF(hcl_t,_if)     },
	{  6, { 'l','a','m','b','d','a' },     HCL_SYNCODE_LAMBDA,  HCL_OFFSETOF(hcl_t,_lambda) },
	{  2, { 'o','r' },                     HCL_SYNCODE_OR,      HCL_OFFSETOF(hcl_t,_or)  },
	{  6, { 'r','e','t','u','r','n'},      HCL_SYNCODE_RETURN,  HCL_OFFSETOF(hcl_t,_return) },
	{ 16, { 'r','e','t','u','r','n','-','f','r','o','m','-','h','o','m','e'},
	                                       HCL_SYNCODE_RETURN_FROM_HOME,  HCL_OFFSETOF(hcl_t,_return_from_home) },
	{  3, { 's','e','t' },                 HCL_SYNCODE_SET,     HCL_OFFSETOF(hcl_t,_set)    },
	{  5, { 'u','n','t','i','l' },         HCL_SYNCODE_UNTIL,   HCL_OFFSETOF(hcl_t,_until)  },
	{  5, { 'w','h','i','l','e' },         HCL_SYNCODE_WHILE,   HCL_OFFSETOF(hcl_t,_while)  }
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
	HCL_ASSERT (hcl, HCL_OOP_IS_SMOOI(hcl->symtab->tally));
	tally = HCL_OOP_TO_SMOOI(hcl->symtab->tally);
	HCL_ASSERT (hcl, tally >= 0); /* it must not be less than 0 */
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

		HCL_ASSERT (hcl, hcl->symtab->bucket->slot[index] != _nil);
	
		for (i = 0, x = index, y = index; i < bucket_size; i++)
		{
			y = (y + 1) % bucket_size;

			/* done if the slot at the current hash index is _nil */
			if (hcl->symtab->bucket->slot[y] == _nil) break;

			/* get the natural hash index for the data in the slot 
			 * at the current hash index */
			symbol = (hcl_oop_char_t)hcl->symtab->bucket->slot[y];

			HCL_ASSERT (hcl, HCL_IS_SYMBOL(hcl, symbol));

			z = hcl_hash_oochars(symbol->slot, HCL_OBJ_GET_SIZE(symbol)) % bucket_size;

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

	HCL_ASSERT (hcl, tally >= 0);
	HCL_ASSERT (hcl, tally <= HCL_SMOOI_MAX);
	hcl->symtab->tally = HCL_SMOOI_TO_OOP(tally);
}

hcl_oow_t hcl_getobjpayloadbytes (hcl_t* hcl, hcl_oop_t oop)
{
	hcl_oow_t nbytes_aligned;

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
		HCL_ASSERT (hcl, HCL_OBJ_GET_FLAGS_TYPE(oop) == HCL_OBJ_TYPE_OOP);
		HCL_ASSERT (hcl, HCL_OBJ_GET_FLAGS_UNIT(oop) == HCL_SIZEOF(hcl_oow_t));
		HCL_ASSERT (hcl, HCL_OBJ_GET_FLAGS_EXTRA(oop) == 0); /* no 'extra' for an OOP object */

		nbytes = HCL_OBJ_BYTESOF(oop) + HCL_SIZEOF(hcl_oow_t) + HCL_OBJ_GET_TRAILER_SIZE(oop);
		nbytes_aligned = HCL_ALIGN(nbytes, HCL_SIZEOF(hcl_oop_t));
	}
	else
	{
		/* calculate the payload size in bytes */
		nbytes_aligned = HCL_ALIGN(HCL_OBJ_BYTESOF(oop), HCL_SIZEOF(hcl_oop_t));
	}

	return nbytes_aligned;
}


/* ----------------------------------------------------------------------- */

#if 0
static HCL_INLINE void gc_ms_mark (hcl_t* hcl, hcl_oop_t oop)
{
	hcl_oow_t i, sz;

#if defined(HCL_SUPPORT_GC_DURING_IGNITION)
	if (!oop) return;
#endif

	if (!HCL_OOP_IS_POINTER(oop)) return;
	if (HCL_OBJ_GET_FLAGS_MOVED(oop)) return; /* already marked */

	HCL_OBJ_SET_FLAGS_MOVED(oop, 1); /* mark */

	/*gc_ms_mark (hcl, (hcl_oop_t)HCL_OBJ_GET_CLASS(oop));*/ /* TODO: remove recursion */

	if (HCL_OBJ_GET_FLAGS_TYPE(oop) == HCL_OBJ_TYPE_OOP)
	{
		hcl_oow_t size, i;

		/* is it really better to use a flag bit in the header to
		 * determine that it is an instance of process? */
		if (HCL_UNLIKELY(HCL_OBJ_GET_FLAGS_PROC(oop)))
		{
			/* the stack in a process object doesn't need to be 
			 * scanned in full. the slots above the stack pointer 
			 * are garbages. */
			size = HCL_PROCESS_NAMED_INSTVARS + HCL_OOP_TO_SMOOI(((hcl_oop_process_t)oop)->sp) + 1;
			HCL_ASSERT (hcl, size <= HCL_OBJ_GET_SIZE(oop));
		}
		else
		{
			size = HCL_OBJ_GET_SIZE(oop);
		}

		for (i = 0; i < size; i++)
		{
			hcl_oop_t tmp = HCL_OBJ_GET_OOP_VAL(oop, i);
			if (HCL_OOP_IS_POINTER(tmp)) gc_ms_mark (hcl, tmp);  /* TODO: no resursion */
		}
	}
}
#else
static HCL_INLINE void gc_ms_mark_object (hcl_t* hcl, hcl_oop_t oop)
{
#if defined(HCL_SUPPORT_GC_DURING_IGNITION)
	if (!oop) return;
#endif
	if (!HCL_OOP_IS_POINTER(oop) || HCL_OBJ_GET_FLAGS_MOVED(oop)) return; /* non-pointer or already marked */

	HCL_OBJ_SET_FLAGS_MOVED(oop, 1); /* mark */
HCL_ASSERT (hcl, hcl->gci.stack.len < hcl->gci.stack.capa);
	hcl->gci.stack.ptr[hcl->gci.stack.len++] = oop; /* push */
if (hcl->gci.stack.len > hcl->gci.stack.max) hcl->gci.stack.max = hcl->gci.stack.len;
}

static HCL_INLINE void gc_ms_scan_stack (hcl_t* hcl)
{
	hcl_oop_t oop;

	while (hcl->gci.stack.len > 0)
	{
		oop = hcl->gci.stack.ptr[--hcl->gci.stack.len];

		/*gc_ms_mark_object (hcl, (hcl_oop_t)HCL_OBJ_GET_CLASS(oop));*/

		if (HCL_OBJ_GET_FLAGS_TYPE(oop) == HCL_OBJ_TYPE_OOP)
		{
			hcl_oow_t size, i;

			/* is it really better to use a flag bit in the header to
			 * determine that it is an instance of process? */
			/* if (HCL_UNLIKELY(HCL_OBJ_GET_FLAGS_PROC(oop))) */
			if (HCL_OBJ_GET_FLAGS_BRAND(oop) == HCL_BRAND_PROCESS)
			{
				/* the stack in a process object doesn't need to be 
				 * scanned in full. the slots above the stack pointer 
				 * are garbages. */
				size = HCL_PROCESS_NAMED_INSTVARS + HCL_OOP_TO_SMOOI(((hcl_oop_process_t)oop)->sp) + 1;
				HCL_ASSERT (hcl, size <= HCL_OBJ_GET_SIZE(oop));
			}
			else
			{
				size = HCL_OBJ_GET_SIZE(oop);
			}

			for (i = 0; i < size; i++)
			{
				gc_ms_mark_object (hcl, HCL_OBJ_GET_OOP_VAL(oop, i));
			}
		}
	}
}

static HCL_INLINE void gc_ms_mark (hcl_t* hcl, hcl_oop_t oop)
{
	gc_ms_mark_object (hcl, oop);
	gc_ms_scan_stack (hcl);
}
#endif

static HCL_INLINE void gc_ms_mark_roots (hcl_t* hcl)
{
	hcl_oow_t i;
#if defined(ENABLE_GCFIN)
	hcl_oow_t gcfin_count;
#endif
	hcl_cb_t* cb;
 
#if defined(HCL_PROFILE_VM)
	struct rusage ru;
	hcl_ntime_t rut;
	getrusage(RUSAGE_SELF, &ru);
	HCL_INIT_NTIME (&rut,  ru.ru_utime.tv_sec, HCL_USEC_TO_NSEC(ru.ru_utime.tv_usec));
#endif

	if (hcl->processor && hcl->processor->active)
	{
		HCL_ASSERT (hcl, (hcl_oop_t)hcl->processor != hcl->_nil);
		HCL_ASSERT (hcl, (hcl_oop_t)hcl->processor->active != hcl->_nil);

		/* commit the stack pointer to the active process because
		 * gc needs the correct stack pointer for a process object */
		hcl->processor->active->sp = HCL_SMOOI_TO_OOP(hcl->sp);
	}

	gc_ms_mark (hcl, hcl->_nil);
	gc_ms_mark (hcl, hcl->_true);
	gc_ms_mark (hcl, hcl->_false);

	for (i = 0; i < HCL_COUNTOF(syminfo); i++)
	{
		gc_ms_mark (hcl, *(hcl_oop_t*)((hcl_uint8_t*)hcl + syminfo[i].offset));
	}

	gc_ms_mark (hcl, (hcl_oop_t)hcl->sysdic);
	gc_ms_mark (hcl, (hcl_oop_t)hcl->processor);
	gc_ms_mark (hcl, (hcl_oop_t)hcl->nil_process);


	for (i = 0; i < hcl->code.lit.len; i++)
	{
		/* the literal array ia a NGC object. but the literal objects 
		 * pointed by the elements of this array must be gabage-collected. */
		gc_ms_mark (hcl, ((hcl_oop_oop_t)hcl->code.lit.arr)->slot[i]);
	}
	gc_ms_mark (hcl, hcl->p.e);

	for (i = 0; i < hcl->sem_list_count; i++)
	{
		gc_ms_mark (hcl, (hcl_oop_t)hcl->sem_list[i]);
	}

	for (i = 0; i < hcl->sem_heap_count; i++)
	{
		gc_ms_mark (hcl, (hcl_oop_t)hcl->sem_heap[i]);
	}

	for (i = 0; i < hcl->sem_io_tuple_count; i++)
	{
		if (hcl->sem_io_tuple[i].sem[HCL_SEMAPHORE_IO_TYPE_INPUT])
			gc_ms_mark (hcl, (hcl_oop_t)hcl->sem_io_tuple[i].sem[HCL_SEMAPHORE_IO_TYPE_INPUT]);
		if (hcl->sem_io_tuple[i].sem[HCL_SEMAPHORE_IO_TYPE_OUTPUT])
			gc_ms_mark (hcl, (hcl_oop_t)hcl->sem_io_tuple[i].sem[HCL_SEMAPHORE_IO_TYPE_OUTPUT]);
	}

#if defined(ENABLE_GCFIN)
	gc_ms_mark (hcl, (hcl_oop_t)hcl->sem_gcfin);
#endif

	for (i = 0; i < hcl->proc_map_capa; i++)
	{
		gc_ms_mark (hcl, hcl->proc_map[i]);
	}

	for (i = 0; i < hcl->volat_count; i++)
	{
		gc_ms_mark (hcl, *hcl->volat_stack[i]);
	}

	if (hcl->initial_context) gc_ms_mark (hcl, (hcl_oop_t)hcl->initial_context);
	if (hcl->active_context) gc_ms_mark (hcl, (hcl_oop_t)hcl->active_context);
	if (hcl->initial_function) gc_ms_mark (hcl, (hcl_oop_t)hcl->initial_function);
	if (hcl->active_function) gc_ms_mark (hcl, (hcl_oop_t)hcl->active_function);

	if (hcl->last_retv) gc_ms_mark (hcl, hcl->last_retv);

	/*hcl_rbt_walk (&hcl->modtab, call_module_gc, hcl); */

	for (cb = hcl->cblist; cb; cb = cb->next)
	{
		if (cb->gc) cb->gc (hcl);
	}

#if defined(ENABLE_GCFIN)
	gcfin_count = move_finalizable_objects(hcl); /* mark finalizable objects */
#endif

	if (hcl->symtab)
	{
		compact_symbol_table (hcl, hcl->_nil); /* delete symbol table entries that are not marked */
	#if 0
		gc_ms_mark (hcl, (hcl_oop_t)hcl->symtab); /* mark the symbol table */
	#else
		HCL_OBJ_SET_FLAGS_MOVED(hcl->symtab, 1); /* mark */
		HCL_OBJ_SET_FLAGS_MOVED(hcl->symtab->bucket, 1); /* mark */
	#endif
	}

#if defined(ENABLE_GCFIN)
	if (gcfin_count > 0) hcl->sem_gcfin_sigreq = 1;
#endif

	if (hcl->active_function) hcl->active_code = HCL_FUNCTION_GET_CODE_BYTE(hcl->active_function); /* update hcl->active_code */

#if defined(HCL_PROFILE_VM)
	getrusage(RUSAGE_SELF, &ru);
	HCL_SUB_NTIME_SNS (&rut, &rut, ru.ru_utime.tv_sec, HCL_USEC_TO_NSEC(ru.ru_utime.tv_usec));
	HCL_SUB_NTIME (&hcl->gci.stat.mark, &hcl->gci.stat.mark, &rut); /* do subtraction because rut is negative */
#endif
}

void hcl_gc_ms_sweep_lazy (hcl_t* hcl, hcl_oow_t allocsize)
{
	hcl_gchdr_t* curr, * next, * prev;
	hcl_oop_t obj;
	hcl_oow_t freed_size;

#if defined(HCL_PROFILE_VM)
	struct rusage ru;
	hcl_ntime_t rut;
	getrusage(RUSAGE_SELF, &ru);
	HCL_INIT_NTIME (&rut,  ru.ru_utime.tv_sec, HCL_USEC_TO_NSEC(ru.ru_utime.tv_usec));
#endif

	if (!hcl->gci.ls.curr) goto done;

	freed_size = 0;

	prev = hcl->gci.ls.prev;
	curr = hcl->gci.ls.curr;

	while (curr)
	{
		next = curr->next;
		obj = (hcl_oop_t)(curr + 1);

		if (HCL_OBJ_GET_FLAGS_MOVED(obj)) /* if marked */
		{
			HCL_OBJ_SET_FLAGS_MOVED (obj, 0); /* unmark */
			prev = curr;
		}
		else
		{
			hcl_oow_t objsize;

			if (prev) prev->next = next;
			else hcl->gci.b = next;

			objsize = HCL_SIZEOF(hcl_obj_t) + hcl_getobjpayloadbytes(hcl, obj);
			freed_size += objsize;
			hcl->gci.bsz -= objsize;
			hcl_freeheapmem (hcl, hcl->heap, curr); /* destroy */

			/*if (freed_size > allocsize)*/  /* TODO: can it secure large enough space? */
			if (objsize == allocsize)
			{
				hcl->gci.ls.prev = prev;
				hcl->gci.ls.curr = next; /* let the next lazy sweeping begin at this point */
				goto done;
			}
		}

		curr = next;
	}

	hcl->gci.ls.curr = HCL_NULL;

done:
#if defined(HCL_PROFILE_VM)
	getrusage(RUSAGE_SELF, &ru);
	HCL_SUB_NTIME_SNS (&rut, &rut, ru.ru_utime.tv_sec, HCL_USEC_TO_NSEC(ru.ru_utime.tv_usec));
	HCL_SUB_NTIME (&hcl->gci.stat.sweep, &hcl->gci.stat.sweep, &rut); /* do subtraction because rut is negative */
#endif
	return;
}

static HCL_INLINE void gc_ms_sweep (hcl_t* hcl)
{
	hcl_gchdr_t* curr, * next, * prev;
	hcl_oop_t obj;

#if defined(HCL_PROFILE_VM)
	struct rusage ru;
	hcl_ntime_t rut;
	getrusage(RUSAGE_SELF, &ru);
	HCL_INIT_NTIME (&rut,  ru.ru_utime.tv_sec, HCL_USEC_TO_NSEC(ru.ru_utime.tv_usec));
#endif

	prev = HCL_NULL;
	curr = hcl->gci.b;
	while (curr)
	{
		next = curr->next;
		obj = (hcl_oop_t)(curr + 1);

		if (HCL_OBJ_GET_FLAGS_MOVED(obj)) /* if marked */
		{
			HCL_OBJ_SET_FLAGS_MOVED (obj, 0); 	/* unmark */
			prev = curr;
		}
		else
		{
			if (prev) prev->next = next;
			else hcl->gci.b = next;

			hcl->gci.bsz -= HCL_SIZEOF(hcl_obj_t) + hcl_getobjpayloadbytes(hcl, obj);
			hcl_freeheapmem (hcl, hcl->heap, curr); /* destroy */
		}

		curr = next;
	}

	hcl->gci.ls.curr = HCL_NULL;

#if defined(HCL_PROFILE_VM)
	getrusage(RUSAGE_SELF, &ru);
	HCL_SUB_NTIME_SNS (&rut, &rut, ru.ru_utime.tv_sec, HCL_USEC_TO_NSEC(ru.ru_utime.tv_usec));
	HCL_SUB_NTIME (&hcl->gci.stat.sweep, &hcl->gci.stat.sweep, &rut); /* do subtraction because rut is negative */
#endif
}

void hcl_gc (hcl_t* hcl, int full)
{
	if (hcl->gci.lazy_sweep) hcl_gc_ms_sweep_lazy (hcl, HCL_TYPE_MAX(hcl_oow_t));

	HCL_LOG1 (hcl, HCL_LOG_GC | HCL_LOG_INFO, "Starting GC (mark-sweep) - gci.bsz = %zu\n", hcl->gci.bsz);

	hcl->gci.stack.len = 0;
	/*hcl->gci.stack.max = 0;*/
	gc_ms_mark_roots (hcl);

	if (!full && hcl->gci.lazy_sweep)
	{
		/* set the lazy sweeping point to the head of the allocated blocks.
		 * hawk_allocbytes() updates hcl->gci.ls.prev if it is called while
		 * hcl->gci.ls.curr stays at hcl->gci.b */
		hcl->gci.ls.prev = HCL_NULL;
		hcl->gci.ls.curr = hcl->gci.b;
	}
	else
	{
	    gc_ms_sweep (hcl);
	}

	HCL_LOG2 (hcl, HCL_LOG_GC | HCL_LOG_INFO, "Finished GC (mark-sweep) - gci.bsz = %zu, gci.stack.max %zu\n", hcl->gci.bsz, hcl->gci.stack.max);
}

hcl_oop_t hcl_moveoop (hcl_t* hcl, hcl_oop_t oop)
{
	if (oop) gc_ms_mark (hcl, oop);
	return oop;
}

#if 0
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
		HCL_ASSERT (hcl, (hcl_oop_t)hcl->processor != hcl->_nil);
		HCL_ASSERT (hcl, (hcl_oop_t)hcl->processor->active != hcl->_nil);
		/* store the stack pointer to the active process */
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
	hcl->_nil = hcl_moveoop(hcl, hcl->_nil);
	hcl->_true = hcl_moveoop(hcl, hcl->_true);
	hcl->_false = hcl_moveoop(hcl, hcl->_false);

	for (i = 0; i < HCL_COUNTOF(syminfo); i++)
	{
		hcl_oop_t tmp;
		tmp = *(hcl_oop_t*)((hcl_uint8_t*)hcl + syminfo[i].offset);
		tmp = hcl_moveoop(hcl, tmp);
		*(hcl_oop_t*)((hcl_uint8_t*)hcl + syminfo[i].offset) = tmp;
	}

	hcl->sysdic = (hcl_oop_dic_t)hcl_moveoop(hcl, (hcl_oop_t)hcl->sysdic);
	hcl->processor = (hcl_oop_process_scheduler_t)hcl_moveoop(hcl, (hcl_oop_t)hcl->processor);
	hcl->nil_process = (hcl_oop_process_t)hcl_moveoop(hcl, (hcl_oop_t)hcl->nil_process);

	for (i = 0; i < hcl->code.lit.len; i++)
	{
		/* the literal array ia a NGC object. but the literal objects 
		 * pointed by the elements of this array must be gabage-collected. */
		((hcl_oop_oop_t)hcl->code.lit.arr)->slot[i] =
			hcl_moveoop(hcl, ((hcl_oop_oop_t)hcl->code.lit.arr)->slot[i]);
	}

	hcl->p.e = hcl_moveoop(hcl, hcl->p.e);

	for (i = 0; i < hcl->sem_list_count; i++)
	{
		hcl->sem_list[i] = (hcl_oop_semaphore_t)hcl_moveoop(hcl, (hcl_oop_t)hcl->sem_list[i]);
	}

	for (i = 0; i < hcl->sem_heap_count; i++)
	{
		hcl->sem_heap[i] = (hcl_oop_semaphore_t)hcl_moveoop(hcl, (hcl_oop_t)hcl->sem_heap[i]);
	}

	for (i = 0; i < hcl->sem_io_tuple_count; i++)
	{
		if (hcl->sem_io_tuple[i].sem[HCL_SEMAPHORE_IO_TYPE_INPUT])
			hcl->sem_io_tuple[i].sem[HCL_SEMAPHORE_IO_TYPE_INPUT] = (hcl_oop_semaphore_t)hcl_moveoop(hcl, (hcl_oop_t)hcl->sem_io_tuple[i].sem[HCL_SEMAPHORE_IO_TYPE_INPUT]);
		if (hcl->sem_io_tuple[i].sem[HCL_SEMAPHORE_IO_TYPE_OUTPUT])
			hcl->sem_io_tuple[i].sem[HCL_SEMAPHORE_IO_TYPE_OUTPUT] = (hcl_oop_semaphore_t)hcl_moveoop(hcl, (hcl_oop_t)hcl->sem_io_tuple[i].sem[HCL_SEMAPHORE_IO_TYPE_OUTPUT]);
	}

#if defined(ENABLE_GCFIN)
	hcl->sem_gcfin = (hcl_oop_semaphore_t)hcl_moveoop(hcl, (hcl_oop_t)hcl->sem_gcfin);
#endif

	for (i = 0; i < hcl->proc_map_capa; i++)
	{
		hcl->proc_map[i] = hcl_moveoop(hcl, hcl->proc_map[i]);
	}

	for (i = 0; i < hcl->volat_count; i++)
	{
		*hcl->volat_stack[i] = hcl_moveoop(hcl, *hcl->volat_stack[i]);
	}

	if (hcl->initial_context)
		hcl->initial_context = (hcl_oop_context_t)hcl_moveoop(hcl, (hcl_oop_t)hcl->initial_context);
	if (hcl->active_context)
		hcl->active_context = (hcl_oop_context_t)hcl_moveoop(hcl, (hcl_oop_t)hcl->active_context);
	if (hcl->initial_function)
		hcl->initial_function = (hcl_oop_function_t)hcl_moveoop(hcl, (hcl_oop_t)hcl->initial_function);
	if (hcl->active_function)
		hcl->active_function = (hcl_oop_function_t)hcl_moveoop(hcl, (hcl_oop_t)hcl->active_function);

	if (hcl->last_retv) hcl->last_retv = hcl_moveoop(hcl, hcl->last_retv);

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
	hcl->symtab = (hcl_oop_dic_t)hcl_moveoop(hcl, (hcl_oop_t)hcl->symtab);

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
	if (hcl->symtab && HCL_LOG_ENABLED(hcl, HCL_LOG_GC | HCL_LOG_DEBUG))
	{
		hcl_oow_t index;
		hcl_oop_oop_t buc;
		HCL_LOG0 (hcl, HCL_LOG_GC | HCL_LOG_DEBUG, "--------- SURVIVING SYMBOLS IN GC ----------\n");
		buc = (hcl_oop_oop_t) hcl->symtab->bucket;
		for (index = 0; index < HCL_OBJ_GET_SIZE(buc); index++)
		{
			if ((hcl_oop_t)buc->slot[index] != hcl->_nil) 
			{
				HCL_LOG1 (hcl, HCL_LOG_GC | HCL_LOG_DEBUG, "\t%O\n", buc->slot[index]);
			}
		}
		HCL_LOG0 (hcl, HCL_LOG_GC | HCL_LOG_DEBUG, "--------------------------------------------\n");
	}
*/

	if (hcl->active_function) hcl->active_code = HCL_FUNCTION_GET_CODE_BYTE(hcl->active_function);  /* update hcl->active_code */

/* TODO: include some gc statstics like number of live objects, gc performance, etc */
	HCL_LOG4 (hcl, HCL_LOG_GC | HCL_LOG_INFO, 
		"Finished GC curheap base %p ptr %p newheap base %p ptr %p\n",
		hcl->curheap->base, hcl->curheap->ptr, hcl->newheap->base, hcl->newheap->ptr); 
}
#endif

void hcl_pushvolat (hcl_t* hcl, hcl_oop_t* oop_ptr)
{
	/* if you have too many temporaries pushed, something must be wrong.
	 * change your code not to exceede the stack limit */
	HCL_ASSERT (hcl, hcl->volat_count < HCL_COUNTOF(hcl->volat_stack));
	hcl->volat_stack[hcl->volat_count++] = oop_ptr;
}

void hcl_popvolat (hcl_t* hcl)
{
	HCL_ASSERT (hcl, hcl->volat_count > 0);
	hcl->volat_count--;
}

void hcl_popvolats (hcl_t* hcl, hcl_oow_t count)
{
	HCL_ASSERT (hcl, hcl->volat_count >= count);
	hcl->volat_count -= count;
}


hcl_oop_t hcl_shallowcopy (hcl_t* hcl, hcl_oop_t oop)
{
	if (HCL_OOP_IS_POINTER(oop) && HCL_OBJ_GET_FLAGS_BRAND(oop) != HCL_BRAND_SYMBOL)
	{
		hcl_oop_t z;
		hcl_oow_t total_bytes;

		total_bytes = HCL_SIZEOF(hcl_obj_t) + hcl_getobjpayloadbytes(hcl, oop);

		hcl_pushvolat (hcl, &oop);
		z = (hcl_oop_t)hcl_allocbytes (hcl, total_bytes);
		hcl_popvolat(hcl);

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
		hcl->_nil = hcl_makenil(hcl);
		if (HCL_UNLIKELY(!hcl->_nil)) return -1;
	}

	if (!hcl->_true) 
	{
		hcl->_true = hcl_maketrue(hcl);
		if (HCL_UNLIKELY(!hcl->_true)) return -1;
	}
	if (!hcl->_false)
	{
		hcl->_false = hcl_makefalse(hcl);
		if (HCL_UNLIKELY(!hcl->_false)) return -1;
	}


	if (!hcl->symtab) 
	{
		hcl->symtab = (hcl_oop_dic_t)hcl_makedic(hcl, hcl->option.dfl_symtab_size);
		if (HCL_UNLIKELY(!hcl->symtab)) return -1;
	}

	if (!hcl->sysdic)
	{
		hcl->sysdic = (hcl_oop_dic_t)hcl_makedic(hcl, hcl->option.dfl_sysdic_size);
		if (HCL_UNLIKELY(!hcl->sysdic)) return -1;
	}

	/* symbol table available now. symbols can be created */
	for (i = 0; i < HCL_COUNTOF(syminfo); i++)
	{
		hcl_oop_t tmp;

		tmp = hcl_makesymbol(hcl, syminfo[i].ptr, syminfo[i].len);
		if (HCL_UNLIKELY(!tmp)) return -1;

		HCL_OBJ_SET_FLAGS_SYNCODE (tmp, syminfo[i].syncode);
		*(hcl_oop_t*)((hcl_uint8_t*)hcl + syminfo[i].offset) = tmp;
	}

	if (!hcl->nil_process)
	{
		/* Create a nil process used to simplify nil check in GC.
		 * only accessible by VM. not exported via the global dictionary. */
		hcl->nil_process = (hcl_oop_process_t)hcl_allocoopobj(hcl, HCL_BRAND_PROCESS, HCL_PROCESS_NAMED_INSTVARS);
		if (HCL_UNLIKELY(!hcl->nil_process)) return -1;
		hcl->nil_process->sp = HCL_SMOOI_TO_OOP(-1);
	}

	if (!hcl->processor)
	{
		hcl->processor = (hcl_oop_process_scheduler_t)hcl_allocoopobj(hcl, HCL_BRAND_PROCESS_SCHEDULER, HCL_PROCESS_SCHEDULER_NAMED_INSTVARS);
		if (HCL_UNLIKELY(!hcl->processor)) return -1;
		hcl->processor->active = hcl->nil_process;
		hcl->processor->total_count = HCL_SMOOI_TO_OOP(0);
		hcl->processor->runnable.count = HCL_SMOOI_TO_OOP(0);
		hcl->processor->suspended.count = HCL_SMOOI_TO_OOP(0);

		/* commit the sp field of the initial active context to hcl->sp */
		hcl->sp = HCL_OOP_TO_SMOOI(hcl->processor->active->sp);
	}

	/* TODO: move code.bc.ptr creation to hcl_init? */
	if (!hcl->code.bc.ptr)
	{
		hcl->code.bc.ptr = (hcl_oob_t*)hcl_allocmem(hcl, HCL_SIZEOF(*hcl->code.bc.ptr) * HCL_BC_BUFFER_INIT); /* TODO: set a proper intial size */
		if (HCL_UNLIKELY(!hcl->code.bc.ptr)) return -1;
		HCL_ASSERT (hcl, hcl->code.bc.len == 0);
		hcl->code.bc.capa = HCL_BC_BUFFER_INIT;
	}

	if (!hcl->code.locptr)
	{
		hcl->code.locptr = (hcl_oow_t*)hcl_allocmem(hcl, HCL_SIZEOF(*hcl->code.locptr) * HCL_BC_BUFFER_INIT);
		if (HCL_UNLIKELY(!hcl->code.locptr)) 
		{
			/* bc.ptr and locptr go together. so free bc.ptr if locptr allocation fails */
			hcl_freemem (hcl, hcl->code.bc.ptr);
			hcl->code.bc.ptr = HCL_NULL;
			hcl->code.bc.capa = 0;
			return -1;
		}

		HCL_MEMSET (hcl->code.locptr, 0, HCL_SIZEOF(*hcl->code.locptr) * HCL_BC_BUFFER_INIT);
	}

	/* TODO: move code.lit.arr creation to hcl_init() after swithching to hcl_allocmem? */
	if (!hcl->code.lit.arr)
	{
		hcl->code.lit.arr = (hcl_oop_oop_t)hcl_makengcarray(hcl, HCL_LIT_BUFFER_INIT); /* TOOD: set a proper initial size */
		if (HCL_UNLIKELY(!hcl->code.lit.arr)) return -1;
		HCL_ASSERT (hcl, hcl->code.lit.len == 0);
	}

	hcl->p.e = hcl->_nil;
	return 0;
}



int hcl_getsyncodebyoocs_noseterr (hcl_t* hcl, const hcl_oocs_t* name)
{
	hcl_oow_t i;
	for (i = 0; i < HCL_COUNTOF(syminfo); i++)
	{
		if (hcl_comp_oochars(syminfo[i].ptr, syminfo[i].len, name->ptr, name->len) == 0) 
			return syminfo[i].syncode;
	}
	return 0; /* 0 indicates no syntax code found */
}

int hcl_getsyncode_noseterr (hcl_t* hcl, const hcl_ooch_t* ptr, const hcl_oow_t len)
{
	hcl_oow_t i;
	for (i = 0; i < HCL_COUNTOF(syminfo); i++)
	{
		if (hcl_comp_oochars(syminfo[i].ptr, syminfo[i].len, ptr, len) == 0) 
			return syminfo[i].syncode;
	}
	return 0; /* 0 indicates no syntax code found */
}

const hcl_ooch_t* hcl_getsyncodename_noseterr (hcl_t* hcl, hcl_syncode_t syncode)
{
	hcl_oow_t i;
	for (i = 0; i < HCL_COUNTOF(syminfo); i++)
	{
		if (syncode == syminfo[i].syncode) return syminfo[i].ptr;
	}
	return HCL_NULL;
}
