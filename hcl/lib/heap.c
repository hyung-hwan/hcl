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

static void* xma_alloc (hcl_mmgr_t* mmgr, hcl_oow_t size)
{
	return hcl_xma_alloc(mmgr->ctx, size);
}

static void* xma_realloc (hcl_mmgr_t* mmgr, void* ptr, hcl_oow_t size)
{
	return hcl_xma_realloc(mmgr->ctx, ptr, size);
}

static void xma_free (hcl_mmgr_t* mmgr, void* ptr)
{
	return hcl_xma_free (mmgr->ctx, ptr);
}

hcl_heap_t* hcl_makeheap (hcl_t* hcl, hcl_oow_t size)
{
	hcl_heap_t* heap;
	hcl_oow_t alloc_size;

	alloc_size = HCL_SIZEOF(*heap) + size;
	heap = (hcl_heap_t*)hcl->vmprim.alloc_heap(hcl, &alloc_size);
	if (HCL_UNLIKELY(!heap))
	{
		const hcl_ooch_t* oldmsg = hcl_backuperrmsg(hcl);
		hcl_seterrbfmt (hcl, hcl_geterrnum(hcl), "unable to allocate a heap - %js", oldmsg);
		return HCL_NULL;
	}

	/* the vmprim.alloc_heap() function is allowed to create a bigger heap than the requested size.
	 * if the created heap is bigger than requested, the heap will be utilized in full. */
	HCL_ASSERT (hcl, alloc_size >= HCL_SIZEOF(*heap) + size);
	HCL_MEMSET (heap, 0, alloc_size);

	heap->base = (hcl_uint8_t*)(heap + 1);
	heap->size = alloc_size;

	if (size <= 0)
	{
		/* use the existing memory allocator */
		heap->xmmgr = *hcl_getmmgr(hcl);
	}
	else
	{
		/* create a new memory allocator over the allocated heap */
		heap->xma = hcl_xma_open(hcl_getmmgr(hcl), 0, heap->base, heap->size);
		if (HCL_UNLIKELY(!heap->xma))
		{
			hcl->vmprim.free_heap (hcl, heap);
			hcl_seterrbfmt (hcl, HCL_ESYSMEM, "unable to allocate a memory manager over a heap");
			return HCL_NULL;
		}

		heap->xmmgr.alloc = xma_alloc;
		heap->xmmgr.realloc = xma_realloc;
		heap->xmmgr.free = xma_free;
		heap->xmmgr.ctx = heap->xma;
	}

	return heap;
}

void hcl_killheap (hcl_t* hcl, hcl_heap_t* heap)
{
	if (heap->xma) hcl_xma_close (heap->xma);
	hcl->vmprim.free_heap (hcl, heap);
}

void* hcl_callocheapmem (hcl_t* hcl, hcl_heap_t* heap, hcl_oow_t size)
{
	void* ptr;

	ptr = HCL_MMGR_ALLOC(&heap->xmmgr, size);
	if (HCL_UNLIKELY(!ptr)) 
	{
		HCL_DEBUG2 (hcl, "Cannot callocate %zd bytes from heap - ptr %p\n", size, heap);
		hcl_seterrnum (hcl, HCL_EOOMEM);
	}
	else
	{
		HCL_MEMSET (ptr, 0, size);
	}
	return ptr;
}

void* hcl_callocheapmem_noseterr (hcl_t* hcl, hcl_heap_t* heap, hcl_oow_t size)
{
	void* ptr;
	ptr = HCL_MMGR_ALLOC(&heap->xmmgr, size);
	if (HCL_LIKELY(ptr)) HCL_MEMSET (ptr, 0, size);
	return ptr;
}

void hcl_freeheapmem (hcl_t* hcl, hcl_heap_t* heap, void* ptr)
{
	HCL_MMGR_FREE (&heap->xmmgr, ptr);
}
