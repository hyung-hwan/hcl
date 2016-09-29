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


hcl_t* hcl_open (hcl_mmgr_t* mmgr, hcl_oow_t xtnsize, hcl_oow_t heapsize, const hcl_vmprim_t* vmprim, hcl_errnum_t* errnum)
{
	hcl_t* hcl;

	/* if this assertion fails, correct the type definition in hcl.h */
	HCL_ASSERT (HCL_SIZEOF(hcl_oow_t) == HCL_SIZEOF(hcl_oop_t));

	hcl = HCL_MMGR_ALLOC (mmgr, HCL_SIZEOF(*hcl) + xtnsize);
	if (hcl)
	{
		if (hcl_init(hcl, mmgr, heapsize, vmprim) <= -1)
		{
			if (errnum) *errnum = hcl->errnum;
			HCL_MMGR_FREE (mmgr, hcl);
			hcl = HCL_NULL;
		}
		else HCL_MEMSET (hcl + 1, 0, xtnsize);
	}
	else if (errnum) *errnum = HCL_ESYSMEM;

	return hcl;
}

void hcl_close (hcl_t* hcl)
{
	hcl_fini (hcl);
	HCL_MMGR_FREE (hcl->mmgr, hcl);
}

static void fill_bigint_tables (hcl_t* hcl)
{
	int radix, safe_ndigits;
	hcl_oow_t ub, w;
	hcl_oow_t multiplier;

	for (radix = 2; radix <= 36; radix++)
	{
		w = 0;
		ub = (hcl_oow_t)HCL_TYPE_MAX(hcl_liw_t) / radix - (radix - 1);
		multiplier = 1;
		safe_ndigits = 0;

		while (w <= ub)
		{
			w = w * radix + (radix - 1);
			multiplier *= radix;
			safe_ndigits++;
		}

		/* safe_ndigits contains the number of digits that never
		 * cause overflow when computed normally with a native type. */
		hcl->bigint[radix].safe_ndigits = safe_ndigits;
		hcl->bigint[radix].multiplier = multiplier;
	}
}

int hcl_init (hcl_t* hcl, hcl_mmgr_t* mmgr, hcl_oow_t heapsz, const hcl_vmprim_t* vmprim)
{
	HCL_MEMSET (hcl, 0, HCL_SIZEOF(*hcl));
	hcl->mmgr = mmgr;
	hcl->vmprim = *vmprim;

	hcl->option.log_mask = ~0u;
	hcl->option.dfl_symtab_size = HCL_DFL_SYMTAB_SIZE;
	hcl->option.dfl_sysdic_size = HCL_DFL_SYSDIC_SIZE;
	hcl->option.dfl_procstk_size = HCL_DFL_PROCSTK_SIZE;

	/*hcl->permheap = hcl_makeheap (hcl, what is the best size???);
	if (!hcl->curheap) goto oops; */
	hcl->curheap = hcl_makeheap (hcl, heapsz);
	if (!hcl->curheap) goto oops;
	hcl->newheap = hcl_makeheap (hcl, heapsz);
	if (!hcl->newheap) goto oops;

	if (hcl_rbt_init (&hcl->pmtable, mmgr, HCL_SIZEOF(hcl_ooch_t), 1) <= -1) goto oops;
	hcl_rbt_setstyle (&hcl->pmtable, hcl_getrbtstyle(HCL_RBT_STYLE_INLINE_COPIERS));

	fill_bigint_tables (hcl);
	return 0;

oops:
	if (hcl->newheap) hcl_killheap (hcl, hcl->newheap);
	if (hcl->curheap) hcl_killheap (hcl, hcl->curheap);
	if (hcl->permheap) hcl_killheap (hcl, hcl->permheap);
	return -1;
}

static hcl_rbt_walk_t unload_primitive_module (hcl_rbt_t* rbt, hcl_rbt_pair_t* pair, void* ctx)
{
	hcl_t* hcl = (hcl_t*)ctx;
	hcl_prim_mod_data_t* md;

	md = HCL_RBT_VPTR(pair);
	if (md->mod.unload) md->mod.unload (hcl, &md->mod);
	if (md->handle) hcl->vmprim.mod_close (hcl, md->handle);

	return HCL_RBT_WALK_FORWARD;
}

void hcl_fini (hcl_t* hcl)
{
	hcl_cb_t* cb;

	if (hcl->sem_list)
	{
		hcl_freemem (hcl, hcl->sem_list);
		hcl->sem_list_capa = 0;
		hcl->sem_list_count = 0;
	}

	if (hcl->sem_heap)
	{
		hcl_freemem (hcl, hcl->sem_heap);
		hcl->sem_heap_capa = 0;
		hcl->sem_heap_count = 0;
	}

	for (cb = hcl->cblist; cb; cb = cb->next)
	{
		if (cb->fini) cb->fini (hcl);
	}

	hcl_rbt_walk (&hcl->pmtable, unload_primitive_module, hcl);
	hcl_rbt_fini (&hcl->pmtable);

	if (hcl->code.bc.arr)
	{
		hcl_freengcobj (hcl, hcl->code.bc.arr);
		hcl->code.bc.arr = HCL_NULL;
		hcl->code.bc.len = 0;
	}

	if (hcl->code.lit.arr)
	{
		hcl_freengcobj (hcl, hcl->code.lit.arr);
		hcl->code.lit.arr = HCL_NULL;
		hcl->code.lit.len = 0;
	}

	if (hcl->p.s.ptr)
	{
		hcl_freemem (hcl, hcl->p.s.ptr);
		hcl->p.s.ptr= HCL_NULL;
		hcl->p.s.capa = 0;
		hcl->p.s.size = 0;
	}

	hcl_killheap (hcl, hcl->newheap);
	hcl_killheap (hcl, hcl->curheap);
	hcl_killheap (hcl, hcl->permheap);

	/* deregister all callbacks */
	while (hcl->cblist) hcl_deregcb (hcl, hcl->cblist);

	if (hcl->log.ptr) 
	{
		/* make sure to flush your log message */
/* TODO: flush unwritten message */

		hcl_freemem (hcl, hcl->log.ptr);
		hcl->log.capa = 0;
		hcl->log.len = 0;
	}
}

hcl_mmgr_t* hcl_getmmgr (hcl_t* hcl)
{
	return hcl->mmgr;
}

void* hcl_getxtn (hcl_t* hcl)
{
	return (void*)(hcl + 1);
}


hcl_errnum_t hcl_geterrnum (hcl_t* hcl)
{
	return hcl->errnum;
}

void hcl_seterrnum (hcl_t* hcl, hcl_errnum_t errnum)
{
	hcl->errnum = errnum;
}

int hcl_setoption (hcl_t* hcl, hcl_option_t id, const void* value)
{
	switch (id)
	{
		case HCL_TRAIT:
			hcl->option.trait = *(const int*)value;
			return 0;

		case HCL_LOG_MASK:
			hcl->option.log_mask = *(const unsigned int*)value;
			return 0;

		case HCL_SYMTAB_SIZE:
		{
			hcl_oow_t w;

			w = *(hcl_oow_t*)value;
			if (w <= 0 || w > HCL_SMOOI_MAX) goto einval;

			hcl->option.dfl_symtab_size = *(hcl_oow_t*)value;
			return 0;
		}

		case HCL_SYSDIC_SIZE:
		{
			hcl_oow_t w;

			w = *(hcl_oow_t*)value;
			if (w <= 0 || w > HCL_SMOOI_MAX) goto einval;

			hcl->option.dfl_sysdic_size = *(hcl_oow_t*)value;
			return 0;
		}

		case HCL_PROCSTK_SIZE:
		{
			hcl_oow_t w;

			w = *(hcl_oow_t*)value;
			if (w <= 0 || w > HCL_SMOOI_MAX) goto einval;

			hcl->option.dfl_procstk_size = *(hcl_oow_t*)value;
			return 0;
		}
	}

einval:
	hcl->errnum = HCL_EINVAL;
	return -1;
}

int hcl_getoption (hcl_t* hcl, hcl_option_t id, void* value)
{
	switch  (id)
	{
		case HCL_TRAIT:
			*(int*)value = hcl->option.trait;
			return 0;

		case HCL_LOG_MASK:
			*(unsigned int*)value = hcl->option.log_mask;
			return 0;

		case HCL_SYMTAB_SIZE:
			*(hcl_oow_t*)value = hcl->option.dfl_symtab_size;
			return 0;

		case HCL_SYSDIC_SIZE:
			*(hcl_oow_t*)value = hcl->option.dfl_sysdic_size;
			return 0;

		case HCL_PROCSTK_SIZE:
			*(hcl_oow_t*)value = hcl->option.dfl_procstk_size;
			return 0;
	};

	hcl->errnum = HCL_EINVAL;
	return -1;
}


hcl_cb_t* hcl_regcb (hcl_t* hcl, hcl_cb_t* tmpl)
{
	hcl_cb_t* actual;

	actual = hcl_allocmem (hcl, HCL_SIZEOF(*actual));
	if (!actual) return HCL_NULL;

	*actual = *tmpl;
	actual->next = hcl->cblist;
	actual->prev = HCL_NULL;
	hcl->cblist = actual;

	return actual;
}

void hcl_deregcb (hcl_t* hcl, hcl_cb_t* cb)
{
	if (cb == hcl->cblist)
	{
		hcl->cblist = hcl->cblist->next;
		if (hcl->cblist) hcl->cblist->prev = HCL_NULL;
	}
	else
	{
		if (cb->next) cb->next->prev = cb->prev;
		if (cb->prev) cb->prev->next = cb->next;
	}

	hcl_freemem (hcl, cb);
}

void* hcl_allocmem (hcl_t* hcl, hcl_oow_t size)
{
	void* ptr;

	ptr = HCL_MMGR_ALLOC (hcl->mmgr, size);
	if (!ptr) hcl->errnum = HCL_ESYSMEM;
	return ptr;
}

void* hcl_callocmem (hcl_t* hcl, hcl_oow_t size)
{
	void* ptr;

	ptr = HCL_MMGR_ALLOC (hcl->mmgr, size);
	if (!ptr) hcl->errnum = HCL_ESYSMEM;
	else HCL_MEMSET (ptr, 0, size);
	return ptr;
}

void* hcl_reallocmem (hcl_t* hcl, void* ptr, hcl_oow_t size)
{
	ptr = HCL_MMGR_REALLOC (hcl->mmgr, ptr, size);
	if (!ptr) hcl->errnum = HCL_ESYSMEM;
	return ptr;
}

void hcl_freemem (hcl_t* hcl, void* ptr)
{
	HCL_MMGR_FREE (hcl->mmgr, ptr);
}


void hcl_getsynerr (hcl_t* hcl, hcl_synerr_t* synerr)
{
	HCL_ASSERT (hcl->c != HCL_NULL);
	if (synerr) *synerr = hcl->c->synerr;
}

void hcl_setsynerr (hcl_t* hcl, hcl_synerrnum_t num, const hcl_ioloc_t* loc, const hcl_oocs_t* tgt)
{
	hcl->errnum = HCL_ESYNERR;
	hcl->c->synerr.num = num;

	/* The SCO compiler complains of this ternary operation saying:
	 *    error: operands have incompatible types: op ":" 
	 * it seems to complain of type mismatch between *loc and
	 * hcl->c->tok.loc due to 'const' prefixed to loc. */
	/*hcl->c->synerr.loc = loc? *loc: hcl->c->tok.loc;*/
	if (loc)
		hcl->c->synerr.loc = *loc;
	else
		hcl->c->synerr.loc = hcl->c->tok.loc;
	
	if (tgt) hcl->c->synerr.tgt = *tgt;
	else 
	{
		hcl->c->synerr.tgt.ptr = HCL_NULL;
		hcl->c->synerr.tgt.len = 0;
	}
}