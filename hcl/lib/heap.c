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

hcl_heap_t* hcl_makeheap (hcl_t* hcl, hcl_oow_t size)
{
	hcl_heap_t* heap;

	heap = (hcl_heap_t*)HCL_MMGR_ALLOC(hcl->mmgr, HCL_SIZEOF(*heap) + size);
	if (!heap)
	{
		hcl_seterrnum (hcl, HCL_ESYSMEM);
		return HCL_NULL;
	}

	HCL_MEMSET (heap, 0, HCL_SIZEOF(*heap) + size);

	heap->base = (hcl_uint8_t*)(heap + 1);
	/* adjust the initial allocation pointer to a multiple of the oop size */
	heap->ptr = (hcl_uint8_t*)HCL_ALIGN(((hcl_uintptr_t)heap->base), HCL_SIZEOF(hcl_oop_t));
	heap->limit = heap->base + size;

	HCL_ASSERT (hcl, heap->ptr >= heap->base);
	HCL_ASSERT (hcl, heap->limit >= heap->base ); 
	HCL_ASSERT (hcl, heap->limit - heap->base == size);

	/* if size is too small, heap->ptr may go past heap->limit even at 
	 * this moment depending on the alignment of heap->base. subsequent
	 * calls to subhcl_allocheapmem() are bound to fail. Make sure to
	 * pass a heap size large enough */

	return heap;
}

void hcl_killheap (hcl_t* hcl, hcl_heap_t* heap)
{
	HCL_MMGR_FREE (hcl->mmgr, heap);
}

void* hcl_allocheapmem (hcl_t* hcl, hcl_heap_t* heap, hcl_oow_t size)
{
	hcl_uint8_t* ptr;

	/* check the heap size limit */
	if (heap->ptr >= heap->limit || heap->limit - heap->ptr < size)
	{
		hcl_seterrnum (hcl, HCL_EOOMEM);
		return HCL_NULL;
	}

	/* allocation is as simple as moving the heap pointer */
	ptr = heap->ptr;
	heap->ptr += size;

	return ptr;
}
