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


hcl_t* hcl_open (hcl_mmgr_t* mmgr, hcl_oow_t xtnsize, hcl_oow_t heapsize, const hcl_vmprim_t* vmprim, hcl_errnum_t* errnum)
{
	hcl_t* hcl;

	/* if this assertion fails, correct the type definition in hcl.h */
	HCL_ASSERT (hcl, HCL_SIZEOF(hcl_oow_t) == HCL_SIZEOF(hcl_oop_t));

	hcl = (hcl_t*)HCL_MMGR_ALLOC(mmgr, HCL_SIZEOF(*hcl) + xtnsize);
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

static void* alloc_heap (hcl_t* hcl, hcl_oow_t size)
{
	return HCL_MMGR_ALLOC(hcl->mmgr, size);
}

static void free_heap (hcl_t* hcl, void* ptr)
{
	HCL_MMGR_FREE(hcl->mmgr, ptr);
}

int hcl_init (hcl_t* hcl, hcl_mmgr_t* mmgr, hcl_oow_t heapsz, const hcl_vmprim_t* vmprim)
{
	int modtab_inited = 0;

	if (!vmprim->syserrstrb && !vmprim->syserrstru)
	{
		hcl_seterrnum (hcl, HCL_EINVAL);
		return -1;
	}

	HCL_MEMSET (hcl, 0, HCL_SIZEOF(*hcl));
	hcl->mmgr = mmgr;
	hcl->cmgr = hcl_getutf8cmgr();
	hcl->vmprim = *vmprim;
	if (!hcl->vmprim.alloc_heap) hcl->vmprim.alloc_heap = alloc_heap;
	if (!hcl->vmprim.free_heap) hcl->vmprim.free_heap = free_heap;

	hcl->option.log_mask = ~0u;
	hcl->option.log_maxcapa = HCL_DFL_LOG_MAXCAPA;
	hcl->option.dfl_symtab_size = HCL_DFL_SYMTAB_SIZE;
	hcl->option.dfl_sysdic_size = HCL_DFL_SYSDIC_SIZE;
	hcl->option.dfl_procstk_size = HCL_DFL_PROCSTK_SIZE;
#if defined(HCL_BUILD_DEBUG)
	hcl->option.karatsuba_cutoff = HCL_KARATSUBA_CUTOFF; /* this won't be used when NDEBUG is set */
#endif

	hcl->log.capa = HCL_ALIGN_POW2(1, HCL_LOG_CAPA_ALIGN); /* TODO: is this a good initial size? */
	/* alloate the log buffer in advance though it may get reallocated
	 * in put_oocs and put_ooch in logfmt.c. this is to let the logging
	 * routine still function despite some side-effects when
	 * reallocation fails */
	/* +1 required for consistency with put_oocs and put_ooch in logfmt.c */
	hcl->log.ptr = (hcl_ooch_t*)hcl_allocmem(hcl, (hcl->log.capa + 1) * HCL_SIZEOF(*hcl->log.ptr)); 
	if (!hcl->log.ptr) goto oops;

	if (hcl_rbt_init(&hcl->modtab, hcl, HCL_SIZEOF(hcl_ooch_t), 1) <= -1) goto oops;
	modtab_inited = 1;
	hcl_rbt_setstyle(&hcl->modtab, hcl_getrbtstyle(HCL_RBT_STYLE_INLINE_COPIERS));

	fill_bigint_tables (hcl);

	hcl->tagged_brands[HCL_OOP_TAG_SMOOI] = HCL_BRAND_SMOOI;
	hcl->tagged_brands[HCL_OOP_TAG_SMPTR] = HCL_BRAND_SMPTR;
	hcl->tagged_brands[HCL_OOP_TAG_CHAR] = HCL_BRAND_CHARACTER;
	hcl->tagged_brands[HCL_OOP_TAG_ERROR] = HCL_BRAND_ERROR;

	hcl->proc_map_free_first = -1;
	hcl->proc_map_free_last = -1;

	/*hcl->permheap = hcl_makeheap (hcl, what is the best size???);
	if (!hcl->curheap) goto oops; */
	hcl->curheap = hcl_makeheap(hcl, heapsz);
	if (!hcl->curheap) goto oops;
	hcl->newheap = hcl_makeheap(hcl, heapsz);
	if (!hcl->newheap) goto oops;

	return 0;

oops:
	if (hcl->newheap) hcl_killheap (hcl, hcl->newheap);
	if (hcl->curheap) hcl_killheap (hcl, hcl->curheap);
	if (hcl->permheap) hcl_killheap (hcl, hcl->permheap);
	if (modtab_inited) hcl_rbt_fini (&hcl->modtab);
	if (hcl->log.ptr) hcl_freemem (hcl, hcl->log.ptr);
	hcl->log.capa = 0;
	return -1;
}

static hcl_rbt_walk_t unload_module (hcl_rbt_t* rbt, hcl_rbt_pair_t* pair, void* ctx)
{
	hcl_t* hcl = (hcl_t*)ctx;
	hcl_mod_data_t* mdp;

	mdp = (hcl_mod_data_t*)HCL_RBT_VPTR(pair);
	HCL_ASSERT (hcl, mdp != HCL_NULL);

	mdp->pair = HCL_NULL; /* to prevent hcl_closemod() from calling  hcl_rbt_delete() */
	hcl_closemod (hcl, mdp);

	return HCL_RBT_WALK_FORWARD;
}

void hcl_fini (hcl_t* hcl)
{
	hcl_cb_t* cb;

	hcl_rbt_walk (&hcl->modtab, unload_module, hcl);
	hcl_rbt_fini (&hcl->modtab);

	if (hcl->log.len > 0)
	{
		/* flush pending log messages just in case. */
		HCL_ASSERT (hcl, hcl->log.ptr != HCL_NULL);
		vmprim_log_write (hcl, hcl->log.last_mask, hcl->log.ptr, hcl->log.len);
	}

	for (cb = hcl->cblist; cb; cb = cb->next)
	{
		if (cb->fini) cb->fini (hcl);
	}

	if (hcl->log.len > 0)
	{
		/* flush pending log message that could be generated by the fini 
		 * callbacks. however, the actual logging might not be produced at
		 * this point because one of the callbacks could arrange to stop
		 * logging */
		HCL_ASSERT (hcl, hcl->log.ptr != HCL_NULL);
		vmprim_log_write (hcl, hcl->log.last_mask, hcl->log.ptr, hcl->log.len);
	}

	/* deregister all callbacks */
	while (hcl->cblist) hcl_deregcb (hcl, hcl->cblist);

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

	if (hcl->proc_map)
	{
		hcl_freemem (hcl, hcl->proc_map);
		hcl->proc_map_capa = 0;
		hcl->proc_map_free_first = -1;
		hcl->proc_map_free_last = -1;
	}

	if (hcl->code.bc.arr)
	{
		hcl_freengcobj (hcl, (hcl_oop_t)hcl->code.bc.arr);
		hcl->code.bc.arr = HCL_NULL;
		hcl->code.bc.len = 0;
	}

	if (hcl->code.lit.arr)
	{
		hcl_freengcobj (hcl, (hcl_oop_t)hcl->code.lit.arr);
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
	if (hcl->permheap) hcl_killheap (hcl, hcl->permheap);

	if (hcl->log.ptr) 
	{
		hcl_freemem (hcl, hcl->log.ptr);
		hcl->log.capa = 0;
		hcl->log.len = 0;
	}

	if (hcl->inttostr.xbuf.ptr)
	{
		hcl_freemem (hcl, hcl->inttostr.xbuf.ptr);
		hcl->inttostr.xbuf.ptr = HCL_NULL;
		hcl->inttostr.xbuf.capa = 0;
		hcl->inttostr.xbuf.len = 0;
	}

	if (hcl->inttostr.t.ptr)
	{
		hcl_freemem (hcl, hcl->inttostr.t.ptr);
		hcl->inttostr.t.ptr = HCL_NULL;
		hcl->inttostr.t.capa = 0;
	}

	if (hcl->sprintf.xbuf.ptr)
	{
		hcl_freemem (hcl, hcl->sprintf.xbuf.ptr);
		hcl->sprintf.xbuf.ptr = HCL_NULL;
		hcl->sprintf.xbuf.capa = 0;
		hcl->sprintf.xbuf.len = 0;
	}
}

void hcl_reset (hcl_t* hcl)
{
	hcl_oop_t v;
	hcl_oow_t i;

	/* delete all literals shown in the literal frame from the system dictionary 
	 * excluding special kernel symbols. */
	for (i = 0; i < hcl->code.lit.len; i++)
	{
		v = ((hcl_oop_oop_t)hcl->code.lit.arr)->slot[i];
		if (HCL_IS_CONS(hcl, v))
		{
			hcl_oop_t key = HCL_CONS_CAR(v);
			if (!HCL_IS_SYMBOL(hcl,key) || !HCL_OBJ_GET_FLAGS_KERNEL(key))
				hcl_zapatsysdic (hcl, HCL_CONS_CAR(v));
		}
	}

	/* zap the byte code buffer and the literal frame */
	hcl->code.bc.len = 0;
	hcl->code.lit.len = 0;

	/* clean up object memory */
	hcl_gc (hcl);
}

int hcl_setoption (hcl_t* hcl, hcl_option_t id, const void* value)
{
	switch (id)
	{
		case HCL_TRAIT:
			hcl->option.trait = *(const unsigned int*)value;
		#if defined(HCL_BUILD_DEBUG)
			hcl->option.karatsuba_cutoff = ((hcl->option.trait & HCL_DEBUG_BIGINT)? HCL_KARATSUBA_CUTOFF_DEBUG: HCL_KARATSUBA_CUTOFF);
		#endif
			return 0;

		case HCL_LOG_MASK:
			hcl->option.log_mask = *(const int*)value;
			return 0;

		case HCL_LOG_MAXCAPA:
			hcl->option.log_maxcapa = *(hcl_oow_t*)value;
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
	hcl_seterrnum (hcl, HCL_EINVAL);
	return -1;
}

int hcl_getoption (hcl_t* hcl, hcl_option_t id, void* value)
{
	switch  (id)
	{
		case HCL_TRAIT:
			*(unsigned int*)value = hcl->option.trait;
			return 0;

		case HCL_LOG_MASK:
			*(int*)value = hcl->option.log_mask;
			return 0;

		case HCL_LOG_MAXCAPA:
			*(hcl_oow_t*)value = hcl->option.log_maxcapa;
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

	hcl_seterrnum (hcl, HCL_EINVAL);
	return -1;
}


hcl_cb_t* hcl_regcb (hcl_t* hcl, hcl_cb_t* tmpl)
{
	hcl_cb_t* actual;

	actual = (hcl_cb_t*)hcl_allocmem(hcl, HCL_SIZEOF(*actual));
	if (!actual) return HCL_NULL;

	*actual = *tmpl;
	actual->next = hcl->cblist;
	actual->prev = HCL_NULL;
	hcl->cblist = actual;
	
	if (actual->vm_checkpoint) hcl->vm_checkpoint_cb_count++;

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

	if (cb->vm_checkpoint) 
	{
		HCL_ASSERT (hcl, hcl->vm_checkpoint_cb_count > 0);
		hcl->vm_checkpoint_cb_count--;
	}
	hcl_freemem (hcl, cb);
}

void* hcl_allocmem (hcl_t* hcl, hcl_oow_t size)
{
	void* ptr;

	ptr = HCL_MMGR_ALLOC (hcl->mmgr, size);
	if (!ptr) hcl_seterrnum (hcl, HCL_ESYSMEM);
	return ptr;
}

void* hcl_callocmem (hcl_t* hcl, hcl_oow_t size)
{
	void* ptr;

	ptr = HCL_MMGR_ALLOC (hcl->mmgr, size);
	if (!ptr) hcl_seterrnum (hcl, HCL_ESYSMEM);
	else HCL_MEMSET (ptr, 0, size);
	return ptr;
}

void* hcl_reallocmem (hcl_t* hcl, void* ptr, hcl_oow_t size)
{
	ptr = HCL_MMGR_REALLOC (hcl->mmgr, ptr, size);
	if (!ptr) hcl_seterrnum (hcl, HCL_ESYSMEM);
	return ptr;
}

void hcl_freemem (hcl_t* hcl, void* ptr)
{
	HCL_MMGR_FREE (hcl->mmgr, ptr);
}


/* -------------------------------------------------------------------------- */

#define MOD_PREFIX "hcl_mod_"
#define MOD_PREFIX_LEN 8

#if defined(HCL_ENABLE_STATIC_MODULE)

#include "../mod/_arr.h"
#include "../mod/_dic.h"
#include "../mod/_str.h"

static struct
{
	hcl_bch_t* modname;
	int (*modload) (hcl_t* hcl, hcl_mod_t* mod);
}
static_modtab[] = 
{
	{ "arr",      hcl_mod_arr },
	{ "dic",      hcl_mod_dic },
	{ "str",      hcl_mod_str }
};
#endif

hcl_mod_data_t* hcl_openmod (hcl_t* hcl, const hcl_ooch_t* name, hcl_oow_t namelen)
{
	hcl_rbt_pair_t* pair;
	hcl_mod_data_t* mdp;
	hcl_mod_data_t md;
	hcl_mod_load_t load = HCL_NULL;
#if defined(HCL_ENABLE_STATIC_MODULE)
	int n;
#endif

	/* maximum module name length is HCL_MOD_NAME_LEN_MAX. 
	 *   MOD_PREFIX_LEN for MOD_PREFIX
	 *   1 for _ at the end when hcl_mod_xxx_ is attempted.
	 *   1 for the terminating '\0'.
	 */
	hcl_ooch_t buf[MOD_PREFIX_LEN + HCL_MOD_NAME_LEN_MAX + 1 + 1]; 

	/* copy instead of encoding conversion. MOD_PREFIX must not
	 * include a character that requires encoding conversion.
	 * note the terminating null isn't needed in buf here. */
	hcl_copybctooochars (buf, MOD_PREFIX, MOD_PREFIX_LEN); 

	if (namelen > HCL_COUNTOF(buf) - (MOD_PREFIX_LEN + 1 + 1))
	{
		/* module name too long  */
		hcl_seterrnum (hcl, HCL_EINVAL); /* TODO: change the  error number to something more specific */
		return HCL_NULL;
	}

	hcl_copyoochars (&buf[MOD_PREFIX_LEN], name, namelen);
	buf[MOD_PREFIX_LEN + namelen] = '\0';

#if defined(HCL_ENABLE_STATIC_MODULE)
	/* attempt to find a statically linked module */

	/* TODO: binary search ... */
	for (n = 0; n < HCL_COUNTOF(static_modtab); n++)
	{
		if (hcl_compoocharsbcstr(name, namelen, static_modtab[n].modname) == 0) 
		{
			load = static_modtab[n].modload;
			break;
		}
	}

	if (load)
	{
		/* found the module in the staic module table */

		HCL_MEMSET (&md, 0, HCL_SIZEOF(md));
		hcl_copyoochars ((hcl_ooch_t*)md.mod.name, name, namelen);
		/* Note md.handle is HCL_NULL for a static module */

		/* i copy-insert 'md' into the table before calling 'load'.
		 * to pass the same address to load(), query(), etc */
		pair = hcl_rbt_insert(&hcl->modtab, (hcl_ooch_t*)name, namelen, &md, HCL_SIZEOF(md));
		if (pair == HCL_NULL)
		{
			hcl_seterrnum (hcl, HCL_ESYSMEM);
			return HCL_NULL;
		}

		mdp = (hcl_mod_data_t*)HCL_RBT_VPTR(pair);
		if (load(hcl, &mdp->mod) <= -1)
		{
			hcl_rbt_delete (&hcl->modtab, (hcl_ooch_t*)name, namelen);
			return HCL_NULL;
		}

		mdp->pair = pair;

		HCL_DEBUG1 (hcl, "Opened a static module [%js]\n", mdp->mod.name);
		return mdp;
	}
	else
	{
	#if !defined(HCL_ENABLE_DYNAMIC_MODULE)
		HCL_DEBUG2 (hcl, "Cannot find a static module [%.*js]\n", namelen, name);
		hcl_seterrbfmt (hcl, HCL_ENOENT, "unable to find a static module [%.*js]", namelen, name);
		return HCL_NULL;
	#endif
	}
#endif

#if !defined(HCL_ENABLE_DYNAMIC_MODULE)
	HCL_DEBUG2 (hcl, "Cannot open module [%.*js] - module loading disabled\n", namelen, name);
	hcl_seterrbfmt (hcl, HCL_ENOIMPL, "unable to open module [%.*js] - module loading disabled", namelen, name);
	return HCL_NULL;
#endif

	/* attempt to find a dynamic external module */
	HCL_MEMSET (&md, 0, HCL_SIZEOF(md));
	hcl_copyoochars((hcl_ooch_t*)md.mod.name, name, namelen);
	if (hcl->vmprim.dl_open && hcl->vmprim.dl_getsym && hcl->vmprim.dl_close)
	{
		md.handle = hcl->vmprim.dl_open(hcl, &buf[MOD_PREFIX_LEN], HCL_VMPRIM_OPENDL_PFMOD);
	}

	if (md.handle == HCL_NULL) 
	{
		HCL_DEBUG2 (hcl, "Cannot open a module [%.*js]\n", namelen, name);
		hcl_seterrbfmt (hcl, HCL_ENOENT, "unable to open a module [%.*js]", namelen, name);
		return HCL_NULL;
	}

	/* attempt to get hcl_mod_xxx where xxx is the module name*/
	load = (hcl_mod_load_t)hcl->vmprim.dl_getsym(hcl, md.handle, buf);
	if (!load) 
	{
		hcl_seterrbfmt (hcl, hcl_geterrnum(hcl), "unable to get module symbol [%js] in [%.*js]", buf, namelen, name);
		HCL_DEBUG3 (hcl, "Cannot get a module symbol [%js] in [%.*js]\n", buf, namelen, name);
		hcl->vmprim.dl_close (hcl, md.handle);
		return HCL_NULL;
	}

	/* i copy-insert 'md' into the table before calling 'load'.
	 * to pass the same address to load(), query(), etc */
	pair = hcl_rbt_insert(&hcl->modtab, (void*)name, namelen, &md, HCL_SIZEOF(md));
	if (pair == HCL_NULL)
	{
		HCL_DEBUG2 (hcl, "Cannot register a module [%.*js]\n", namelen, name);
		hcl_seterrbfmt (hcl, HCL_ESYSMEM, "unable to register a module [%.*js] for memory shortage", namelen, name);
		hcl->vmprim.dl_close (hcl, md.handle);
		return HCL_NULL;
	}

	mdp = (hcl_mod_data_t*)HCL_RBT_VPTR(pair);
	if (load(hcl, &mdp->mod) <= -1)
	{
		const hcl_ooch_t* oldmsg = hcl_backuperrmsg (hcl);
		hcl_seterrbfmt (hcl, hcl_geterrnum(hcl), "module initializer [%js] returned failure in [%.*js] - %js", buf, namelen, name, oldmsg); 
		HCL_DEBUG3 (hcl, "Module function [%js] returned failure in [%.*js]\n", buf, namelen, name);
		hcl_rbt_delete (&hcl->modtab, name, namelen);
		hcl->vmprim.dl_close (hcl, mdp->handle);
		return HCL_NULL;
	}

	mdp->pair = pair;

	HCL_DEBUG2 (hcl, "Opened a module [%js] - %p\n", mdp->mod.name, mdp->handle);

	/* the module loader must ensure to set a proper query handler */
	HCL_ASSERT (hcl, mdp->mod.query != HCL_NULL);

	return mdp;
}

void hcl_closemod (hcl_t* hcl, hcl_mod_data_t* mdp)
{
	if (mdp->mod.unload) mdp->mod.unload (hcl, &mdp->mod);

	if (mdp->handle) 
	{
		hcl->vmprim.dl_close (hcl, mdp->handle);
		HCL_DEBUG2 (hcl, "Closed a module [%js] - %p\n", mdp->mod.name, mdp->handle);
		mdp->handle = HCL_NULL;
	}
	else
	{
		HCL_DEBUG1 (hcl, "Closed a static module [%js]\n", mdp->mod.name);
	}

	if (mdp->pair)
	{
		/*mdp->pair = HCL_NULL;*/ /* this reset isn't needed as the area will get freed by hcl_rbt_delete()) */
		hcl_rbt_delete (&hcl->modtab, mdp->mod.name, hcl_countoocstr(mdp->mod.name));
	}
}

hcl_pfbase_t* hcl_querymod (hcl_t* hcl, const hcl_ooch_t* pfid, hcl_oow_t pfidlen, hcl_mod_t** mod)
{
	/* primitive function identifier
	 *   _funcname
	 *   modname_funcname
	 */
	hcl_rbt_pair_t* pair;
	hcl_mod_data_t* mdp;
	const hcl_ooch_t* sep;

	hcl_oow_t mod_name_len;
	hcl_pfbase_t* pfbase;

	sep = hcl_rfindoochar (pfid, pfidlen, '.');
	if (!sep)
	{
		/* i'm writing a conservative code here. the compiler should 
		 * guarantee that a period is included in an primitive function identifer.
		 * what if the compiler is broken? imagine a buggy compiler rewritten
		 * in hcl itself? */
		HCL_DEBUG2 (hcl, "Internal error - no period in a primitive function identifier [%.*js] - buggy compiler?\n", pfidlen, pfid);
		hcl_seterrbfmt (hcl, HCL_EINTERN, "no period in a primitive function identifier [%.*js]", pfidlen, pfid);
		return HCL_NULL;
	}

	mod_name_len = sep - pfid;

	/* the first segment through the segment before the last compose a
	 * module id. the last segment is the primitive function name.
	 * for instance, in con.window.open, con.window is a module id and
	 * open is the primitive function name. */
	pair = hcl_rbt_search(&hcl->modtab, pfid, mod_name_len);
	if (pair)
	{
		mdp = (hcl_mod_data_t*)HCL_RBT_VPTR(pair);
		HCL_ASSERT (hcl, mdp != HCL_NULL);
	}
	else
	{
		/* open a module using the part before the last period */
		mdp = hcl_openmod(hcl, pfid, mod_name_len);
		if (!mdp) return HCL_NULL;
	}

	if ((pfbase = mdp->mod.query(hcl, &mdp->mod, sep + 1, pfidlen - mod_name_len - 1)) == HCL_NULL) 
	{
		/* the primitive function is not found. but keep the module open even if it's opened above */
		HCL_DEBUG3 (hcl, "Cannot find a primitive function [%.*js] in a module [%js]\n", pfidlen - mod_name_len - 1, sep + 1, mdp->mod.name);
		hcl_seterrbfmt (hcl, HCL_ENOENT, "unable to find a primitive function [%.*js] in a module [%js]", pfidlen - mod_name_len - 1, sep + 1, mdp->mod.name);
		return HCL_NULL;
	}

	*mod = &mdp->mod;

	HCL_DEBUG4 (hcl, "Found a primitive function [%.*js] in a module [%js] - %p\n",
		pfidlen - mod_name_len - 1, sep + 1, mdp->mod.name, pfbase);
	return pfbase;
}


hcl_pfbase_t* hcl_findpfbase (hcl_t* hcl, hcl_pfinfo_t* pfinfo, hcl_oow_t pfcount, const hcl_ooch_t* name, hcl_oow_t namelen)
{
	int n;

	/* binary search */
#if 0
	/* [NOTE] this algorithm is NOT underflow safe with hcl_oow_t types */
	int left, right, mid;

	for (left = 0, right = pfcount - 1; left <= right; )
	{
		/*mid = (left + right) / 2;*/
		mid = left + ((right - left) / 2);

		n = hcl_compoocharsoocstr (name, namelen, pfinfo[mid].mthname);
		if (n < 0) right = mid - 1; /* this substraction can make right negative. so i can't use hcl_oow_t for the variable */
		else if (n > 0) left = mid + 1;
		else return &pfinfo[mid].base;
	}
#else
	/* [NOTE] this algorithm is underflow safe with hcl_oow_t types */
	hcl_oow_t base, mid, lim;

	for (base = 0, lim = pfcount; lim > 0; lim >>= 1)
	{
		mid = base + (lim >> 1);
		n = hcl_compoocharsoocstr (name, namelen, pfinfo[mid].mthname);
		if (n == 0) return &pfinfo[mid].base;
		if (n > 0) { base = mid + 1; lim--; }
	}
#endif

	hcl_seterrnum (hcl, HCL_ENOENT);
	return HCL_NULL;
}

