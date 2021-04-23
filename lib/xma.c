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

#include <hcl-xma.h>
#include "hcl-prv.h"
#include <assert.h> /* TODO: replace assert() with HCL_ASSERT() or something */


/* 
 * in the following run, movaps tries to write to the address 0x7fffea722f78.
 * since the instruction deals with 16-byte aligned data only, it triggered 
 * the general protection error.
 *
$ gdb ~/xxx/bin/xxx
Program received signal SIGSEGV, Segmentation fault.
0x000000000042156a in set_global (rtx=rtx@entry=0x7fffea718ff8, idx=idx@entry=2,
    var=var@entry=0x0, val=val@entry=0x1, assign=assign@entry=0) at ../../../lib/run.c:358
358				rtx->gbl.fnr = lv;
(gdb) print &rtx->gbl.fnr
$1 = (xxx_int_t *) 0x7fffea722f78
(gdb) disp /i 0x42156a
1: x/i 0x42156a
=> 0x42156a <set_global+874>:	movaps %xmm2,0x9f80(%rbp)
*/

/* set ALIGN to twice the pointer size to prevent unaligned memory access by
 * instructions dealing with data larger than the system word size. e.g. movaps on x86_64 */
#define ALIGN (HCL_SIZEOF_VOID_P * 2) /* this must be a power of 2 */

#define MBLKHDRSIZE (HCL_SIZEOF(hcl_xma_mblk_t))
#define MINALLOCSIZE (HCL_SIZEOF(hcl_xma_fblk_t) - HCL_SIZEOF(hcl_xma_mblk_t))
#define FBLKMINSIZE (MBLKHDRSIZE + MINALLOCSIZE) /* or HCL_SIZEOF(hcl_xma_fblk_t) - need space for the free links when the block is freeed */

/* NOTE: you must ensure that FBLKMINSIZE is equal to ALIGN or multiples of ALIGN */

#define SYS_TO_USR(b) ((hcl_uint8_t*)(b) + MBLKHDRSIZE)
#define USR_TO_SYS(b) ((hcl_uint8_t*)(b) - MBLKHDRSIZE)

/*
 * the xfree array is divided into three region
 * 0 ....................... FIXED ......................... XFIMAX-1 ... XFIMAX
 * |  small fixed-size chains |     large  chains                | huge chain |
 */
#define FIXED HCL_XMA_FIXED
#define XFIMAX(xma) (HCL_COUNTOF(xma->xfree)-1)

#define mblk_size(b) (((hcl_xma_mblk_t*)(b))->size)
#define mblk_prev_size(b) (((hcl_xma_mblk_t*)(b))->prev_size)

#define next_mblk(b) ((hcl_xma_mblk_t*)((hcl_uint8_t*)b + MBLKHDRSIZE + mblk_size(b)))
#define prev_mblk(b) ((hcl_xma_mblk_t*)((hcl_uint8_t*)b - (MBLKHDRSIZE + mblk_prev_size(b))))

struct hcl_xma_mblk_t
{
	hcl_oow_t prev_size;

	/* the block size is shifted by 1 bit and the maximum value is
	 * offset by 1 bit because of the 'free' bit-field.
	 * i could keep 'size' without shifting with bit manipulation 
	 * because the actual size is aligned and the last bit will 
	 * never be 1. i don't think there is a practical use case where
	 * you need to allocate a huge chunk covering the entire 
	 * address space of your machine. */
	hcl_oow_t free: 1;
	hcl_oow_t size: HCL_XMA_SIZE_BITS; /**< block size */
};

struct hcl_xma_fblk_t 
{
	hcl_oow_t prev_size;
	hcl_oow_t free: 1;
	hcl_oow_t size: HCL_XMA_SIZE_BITS;/**< block size */

	/* these two fields are used only if the block is free */
	hcl_xma_fblk_t* free_prev; /**< link to the previous free block */
	hcl_xma_fblk_t* free_next; /**< link to the next free block */
};

/*#define VERIFY*/
#if defined(VERIFY)
static void DBG_VERIFY (hcl_xma_t* xma, const char* desc)
{
	hcl_xma_mblk_t* tmp, * next;
	hcl_oow_t cnt;
	hcl_oow_t fsum, asum; 
#if defined(HCL_XMA_ENABLE_STAT)
	hcl_oow_t isum;
#endif

	for (tmp = (hcl_xma_mblk_t*)xma->start, cnt = 0, fsum = 0, asum = 0; (hcl_uint8_t*)tmp < xma->end; tmp = next, cnt++)
	{
		next = next_mblk(tmp);

		if ((hcl_uint8_t*)tmp == xma->start)
		{
			assert (tmp->prev_size == 0);
		}
		if ((hcl_uint8_t*)next < xma->end)
		{
			assert (next->prev_size == tmp->size);
		}

		if (tmp->free) fsum += tmp->size;
		else asum += tmp->size;
	}

#if defined(HCL_XMA_ENABLE_STAT)
	isum = (xma->stat.nfree + xma->stat.nused) * MBLKHDRSIZE;
	assert (asum == xma->stat.alloc);
	assert (fsum == xma->stat.avail);
	assert (isum == xma->stat.total - (xma->stat.alloc + xma->stat.avail));
	assert (asum + fsum + isum == xma->stat.total);
#endif
}
#else
#define DBG_VERIFY(xma, desc)
#endif

static HCL_INLINE hcl_oow_t szlog2 (hcl_oow_t n) 
{
	/*
	 * 2**x = n;
	 * x = log2(n);
	 * -------------------------------------------
	 * 	unsigned int x = 0;
	 * 	while((n >> x) > 1) ++x;
	 * 	return x;
	 */

#define BITS (HCL_SIZEOF_OOW_T * 8)
	int x = BITS - 1;

#if HCL_SIZEOF_OOW_T >= 128
#	error hcl_oow_t too large. unsupported platform
#endif

#if HCL_SIZEOF_OOW_T >= 64
	if ((n & (~(hcl_oow_t)0 << (BITS-128))) == 0) { x -= 256; n <<= 256; }
#endif
#if HCL_SIZEOF_OOW_T >= 32
	if ((n & (~(hcl_oow_t)0 << (BITS-128))) == 0) { x -= 128; n <<= 128; }
#endif
#if HCL_SIZEOF_OOW_T >= 16
	if ((n & (~(hcl_oow_t)0 << (BITS-64))) == 0) { x -= 64; n <<= 64; }
#endif
#if HCL_SIZEOF_OOW_T >= 8
	if ((n & (~(hcl_oow_t)0 << (BITS-32))) == 0) { x -= 32; n <<= 32; }
#endif
#if HCL_SIZEOF_OOW_T >= 4 
	if ((n & (~(hcl_oow_t)0 << (BITS-16))) == 0) { x -= 16; n <<= 16; }
#endif
#if HCL_SIZEOF_OOW_T >= 2
	if ((n & (~(hcl_oow_t)0 << (BITS-8))) == 0) { x -= 8; n <<= 8; }
#endif
#if HCL_SIZEOF_OOW_T >= 1
	if ((n & (~(hcl_oow_t)0 << (BITS-4))) == 0) { x -= 4; n <<= 4; }
#endif
	if ((n & (~(hcl_oow_t)0 << (BITS-2))) == 0) { x -= 2; n <<= 2; }
	if ((n & (~(hcl_oow_t)0 << (BITS-1))) == 0) { x -= 1; }

	return x;
#undef BITS
}

static HCL_INLINE hcl_oow_t getxfi (hcl_xma_t* xma, hcl_oow_t size) 
{
	hcl_oow_t xfi = ((size) / ALIGN) - 1;
	if (xfi >= FIXED) xfi = szlog2(size) - (xma)->bdec + FIXED;
	if (xfi > XFIMAX(xma)) xfi = XFIMAX(xma);
	return xfi;
}


hcl_xma_t* hcl_xma_open (hcl_mmgr_t* mmgr, hcl_oow_t xtnsize, void* zoneptr, hcl_oow_t zonesize)
{
	hcl_xma_t* xma;

	xma = (hcl_xma_t*)HCL_MMGR_ALLOC(mmgr, HCL_SIZEOF(*xma) + xtnsize);
	if (HCL_UNLIKELY(!xma)) return HCL_NULL;

	if (hcl_xma_init(xma, mmgr, zoneptr, zonesize) <= -1)
	{
		HCL_MMGR_FREE (mmgr, xma);
		return HCL_NULL;
	}

	HCL_MEMSET (xma + 1, 0, xtnsize);
	return xma;
}

void hcl_xma_close (hcl_xma_t* xma)
{
	hcl_xma_fini (xma);
	HCL_MMGR_FREE (xma->_mmgr, xma);
}

int hcl_xma_init (hcl_xma_t* xma, hcl_mmgr_t* mmgr, void* zoneptr, hcl_oow_t zonesize)
{
	hcl_xma_fblk_t* first;
	hcl_oow_t xfi;
	int internal = 0;

	if (!zoneptr)
	{
		/* round 'zonesize' to be the multiples of ALIGN */
		zonesize = HCL_ALIGN_POW2(zonesize, ALIGN);

		/* adjust 'zonesize' to be large enough to hold a single smallest block */
		if (zonesize < FBLKMINSIZE) zonesize = FBLKMINSIZE;

		zoneptr = HCL_MMGR_ALLOC(mmgr, zonesize);
		if (HCL_UNLIKELY(!zoneptr)) return -1;

		internal = 1; /* internally created. must be freed upon hcl_xma_fini() */
	}
	else if (zonesize < FBLKMINSIZE) 
	{
		/* the zone size is too small for an externally allocated zone. */
/* TODO: difference error code from memory allocation failure.. this is not really memory shortage */
		return -1;
	}

	first = (hcl_xma_fblk_t*)zoneptr;

	/* initialize the header part of the free chunk. the entire zone is a single free block */
	first->prev_size = 0;
	first->free = 1;
	first->size = zonesize - MBLKHDRSIZE; /* size excluding the block header */
	first->free_prev = HCL_NULL;
	first->free_next = HCL_NULL;

	HCL_MEMSET (xma, 0, HCL_SIZEOF(*xma));
	xma->_mmgr = mmgr;
	xma->bdec = szlog2(FIXED * ALIGN); /* precalculate the decrement value */

	/* at this point, the 'free' chunk is a only block available */

	/* get the free block index */
	xfi = getxfi(xma, first->size);
	/* locate it into an apporopriate slot */
	xma->xfree[xfi] = first; 
	/* let it be the head, which is natural with only a block */
	xma->start = (hcl_uint8_t*)first;
	xma->end = xma->start + zonesize;
	xma->internal = internal;

	/* initialize some statistical variables */
#if defined(HCL_XMA_ENABLE_STAT)
	xma->stat.total = zonesize;
	xma->stat.alloc = 0;
	xma->stat.avail = zonesize - MBLKHDRSIZE;
	xma->stat.nfree = 1;
	xma->stat.nused = 0;
#endif
	
	return 0;
}

void hcl_xma_fini (hcl_xma_t* xma)
{
	/* the head must point to the free chunk allocated in init().
	 * let's deallocate it */
	if (xma->internal) HCL_MMGR_FREE (xma->_mmgr, xma->start);
	xma->start = HCL_NULL;
	xma->end = HCL_NULL;
}

static HCL_INLINE void attach_to_freelist (hcl_xma_t* xma, hcl_xma_fblk_t* b)
{
	/* 
	 * attach a block to a free list 
	 */

	/* get the free list index for the block size */
	hcl_oow_t xfi = getxfi(xma, b->size); 

	/* let it be the head of the free list doubly-linked */
	b->free_prev = HCL_NULL; 
	b->free_next = xma->xfree[xfi];
	if (xma->xfree[xfi]) xma->xfree[xfi]->free_prev = b;
	xma->xfree[xfi] = b;
}

static HCL_INLINE void detach_from_freelist (hcl_xma_t* xma, hcl_xma_fblk_t* b)
{
	/* detach a block from a free list */
	hcl_xma_fblk_t* p, * n;

	/* alias the previous and the next with short variable names */
	p = b->free_prev;
	n = b->free_next;

	if (p)
	{
		/* the previous item exists. let its 'next' pointer point to 
		 * the block's next item. */
		p->free_next = n;
	}
	else 
	{
		/* the previous item does not exist. the block is the first
 		 * item in the free list. */

		hcl_oow_t xfi = getxfi(xma, b->size);
		assert (b == xma->xfree[xfi]);
		/* let's update the free list head */
		xma->xfree[xfi] = n;
	}

	/* let the 'prev' pointer of the block's next item point to the 
	 * block's previous item */
	if (n) n->free_prev = p;
}

static hcl_xma_fblk_t* alloc_from_freelist (hcl_xma_t* xma, hcl_oow_t xfi, hcl_oow_t size)
{
	hcl_xma_fblk_t* cand;

	for (cand = xma->xfree[xfi]; cand; cand = cand->free_next)
	{
		if (cand->size >= size)
		{
			hcl_oow_t rem;

			detach_from_freelist (xma, cand);

			rem = cand->size - size;
			if (rem >= FBLKMINSIZE)
			{
				hcl_xma_mblk_t* y, * z;

				/* the remaining part is large enough to hold 
				 * another block. let's split it 
				 */

				/* shrink the size of the 'cand' block */
				cand->size = size;

				/* let 'y' point to the remaining part */
				y = next_mblk(cand);

				/* initialize some fields */
				y->free = 1;
				y->size = rem - MBLKHDRSIZE;
				y->prev_size = cand->size;

				/* add the remaining part to the free list */
				attach_to_freelist (xma, (hcl_xma_fblk_t*)y);

				z = next_mblk(y);
				if ((hcl_uint8_t*)z < xma->end) z->prev_size = y->size;

#if defined(HCL_XMA_ENABLE_STAT)
				xma->stat.avail -= MBLKHDRSIZE;
#endif
			}
#if defined(HCL_XMA_ENABLE_STAT)
			else
			{
				/* decrement the number of free blocks as the current
				 * block is allocated as a whole without being split */
				xma->stat.nfree--;
			}
#endif

			cand->free = 0;
			/*
			cand->free_next = HCL_NULL;
			cand->free_prev = HCL_NULL;
			*/

#if defined(HCL_XMA_ENABLE_STAT)
			xma->stat.nused++;
			xma->stat.alloc += cand->size;
			xma->stat.avail -= cand->size;
#endif
			return cand;
		}
	}

	return HCL_NULL;
}


void* hcl_xma_alloc (hcl_xma_t* xma, hcl_oow_t size)
{
	hcl_xma_fblk_t* cand;
	hcl_oow_t xfi;

	DBG_VERIFY (xma, "alloc start");

	/* round up 'size' to the multiples of ALIGN */
	if (size < MINALLOCSIZE) size = MINALLOCSIZE;
	size = HCL_ALIGN_POW2(size, ALIGN);

	assert (size >= ALIGN);
	xfi = getxfi(xma, size);

	/*if (xfi < XFIMAX(xma) && xma->xfree[xfi])*/
	if (xfi < FIXED && xma->xfree[xfi])
	{
		/* try the best fit */
		cand = xma->xfree[xfi];

		assert (cand->free != 0);
		assert (cand->size == size);

		detach_from_freelist (xma, cand);
		cand->free = 0;

#if defined(HCL_XMA_ENABLE_STAT)
		xma->stat.nfree--;
		xma->stat.nused++;
		xma->stat.alloc += cand->size;
		xma->stat.avail -= cand->size;
#endif
	}
	else if (xfi == XFIMAX(xma))
	{
		/* huge block */
		cand = alloc_from_freelist(xma, XFIMAX(xma), size);
		if (!cand) return HCL_NULL;
	}
	else
	{
		if (xfi >= FIXED)
		{
			/* get the block from its own large chain */
			cand = alloc_from_freelist(xma, xfi, size);
			if (!cand)
			{
				/* borrow a large block from the huge block chain */
				cand = alloc_from_freelist(xma, XFIMAX(xma), size);
			}
		}
		else
		{
			/* borrow a small block from the huge block chain */
			cand = alloc_from_freelist(xma, XFIMAX(xma), size);
			if (!cand) xfi = FIXED - 1;
		}

		if (!cand)
		{
			/* try each large block chain left */
			for (++xfi; xfi < XFIMAX(xma) - 1; xfi++)
			{
				cand = alloc_from_freelist(xma, xfi, size);
				if (cand) break;
			}
			if (!cand) return HCL_NULL;
		}
	}

	DBG_VERIFY (xma, "alloc end");
	return SYS_TO_USR(cand);
}

static void* _realloc_merge (hcl_xma_t* xma, void* b, hcl_oow_t size)
{
	hcl_xma_mblk_t* blk = (hcl_xma_mblk_t*)USR_TO_SYS(b);

	DBG_VERIFY (xma, "realloc merge start");
	/* rounds up 'size' to be multiples of ALIGN */ 
	if (size < MINALLOCSIZE) size = MINALLOCSIZE;
	size = HCL_ALIGN_POW2(size, ALIGN);

	if (size > blk->size)
	{
		/* grow the current block */
		hcl_oow_t req;
		hcl_xma_mblk_t* n;
		hcl_oow_t rem;

		req = size - blk->size; /* required size additionally */

		n = next_mblk(blk);
		/* check if the next adjacent block is available */
		if ((hcl_uint8_t*)n >= xma->end || !n->free || req > n->size) return HCL_NULL; /* no! */
/* TODO: check more blocks if the next block is free but small in size.
 *       check the previous adjacent blocks also */

		assert (blk->size == n->prev_size);

		/* let's merge the current block with the next block */
		detach_from_freelist (xma, (hcl_xma_fblk_t*)n);

		rem = (MBLKHDRSIZE + n->size) - req;
		if (rem >= FBLKMINSIZE)
		{
			/* 
			 * the remaining part of the next block is large enough 
			 * to hold a block. break the next block.
			 */

			hcl_xma_mblk_t* y, * z;

			blk->size += req;
			y = next_mblk(blk);
			y->free = 1;
			y->size = rem - MBLKHDRSIZE;
			y->prev_size = blk->size;
			attach_to_freelist (xma, (hcl_xma_fblk_t*)y);

			z = next_mblk(y);
			if ((hcl_uint8_t*)z < xma->end) z->prev_size = y->size;

#if defined(HCL_XMA_ENABLE_STAT)
			xma->stat.alloc += req;
			xma->stat.avail -= req; /* req + MBLKHDRSIZE(tmp) - MBLKHDRSIZE(n) */
#endif
		}
		else
		{
			hcl_xma_mblk_t* z;

			/* the remaining part of the next block is too small to form an indepent block.
			 * utilize the whole block by merging to the resizing block */
			blk->size += MBLKHDRSIZE + n->size;

			z = next_mblk(blk);
			if ((hcl_uint8_t*)z < xma->end) z->prev_size = blk->size;

#if defined(HCL_XMA_ENABLE_STAT)
			xma->stat.nfree--;
			xma->stat.alloc += MBLKHDRSIZE + n->size;
			xma->stat.avail -= n->size;
#endif
		}
	}
	else if (size < blk->size)
	{
		/* shrink the block */
		hcl_oow_t rem = blk->size - size;
		if (rem >= FBLKMINSIZE) 
		{
			hcl_xma_mblk_t* n;

			n = next_mblk(blk);

			/* the leftover is large enough to hold a block of minimum size.split the current block */
			if ((hcl_uint8_t*)n < xma->end && n->free)
			{
				hcl_xma_mblk_t* y, * z;

				/* make the leftover block merge with the next block */

				detach_from_freelist (xma, (hcl_xma_fblk_t*)n);

				blk->size = size;

				y = next_mblk(blk); /* update y to the leftover block with the new block size set above */
				y->free = 1;
				y->size = rem + n->size; /* add up the adjust block - (rem + MBLKHDRSIZE(n) + n->size) - MBLKHDRSIZE(y) */
				y->prev_size = blk->size;

				/* add 'y' to the free list */
				attach_to_freelist (xma, (hcl_xma_fblk_t*)y);

				z = next_mblk(y); /* get adjacent block to the merged block */
				if ((hcl_uint8_t*)z < xma->end) z->prev_size = y->size;

#if defined(HCL_XMA_ENABLE_STAT)
				xma->stat.alloc -= rem;
				xma->stat.avail += rem; /* rem - MBLKHDRSIZE(y) + MBLKHDRSIZE(n) */
#endif
			}
			else
			{
				hcl_xma_mblk_t* y;

				/* link the leftover block to the free list */
				blk->size = size;

				y = next_mblk(blk); /* update y to the leftover block with the new block size set above */
				y->free = 1;
				y->size = rem - MBLKHDRSIZE;
				y->prev_size = blk->size;

				attach_to_freelist (xma, (hcl_xma_fblk_t*)y);
				/*n = next_mblk(y);
				if ((hcl_uint8_t*)n < xma->end)*/ n->prev_size = y->size;

#if defined(HCL_XMA_ENABLE_STAT)
				xma->stat.nfree++;
				xma->stat.alloc -= rem;
				xma->stat.avail += y->size;
#endif
			}
		}
	}

	DBG_VERIFY (xma, "realloc merge end");
	return b;
}

void* hcl_xma_calloc (hcl_xma_t* xma, hcl_oow_t size)
{
	void* ptr = hcl_xma_alloc(xma, size);
	if (ptr) HCL_MEMSET (ptr, 0, size);
	return ptr;
}

void* hcl_xma_realloc (hcl_xma_t* xma, void* b, hcl_oow_t size)
{
	void* n;

	if (b == HCL_NULL) 
	{
		/* 'realloc' with NULL is the same as 'alloc' */
		n = hcl_xma_alloc(xma, size);
	}
	else
	{
		/* try reallocation by merging the adjacent continuous blocks */
		n = _realloc_merge (xma, b, size);
		if (!n)
		{
			/* reallocation by merging failed. fall back to the slow
			 * allocation-copy-free scheme */
			n = hcl_xma_alloc(xma, size);
			if (n)
			{
				HCL_MEMCPY (n, b, size);
				hcl_xma_free (xma, b);
			}
		}
	}

	return n;
}

void hcl_xma_free (hcl_xma_t* xma, void* b)
{
	hcl_xma_mblk_t* blk = (hcl_xma_mblk_t*)USR_TO_SYS(b);
	hcl_xma_mblk_t* x, * y;
	hcl_oow_t org_blk_size;

	DBG_VERIFY (xma, "free start");

	org_blk_size = blk->size;

#if defined(HCL_XMA_ENABLE_STAT)
	/* update statistical variables */
	xma->stat.nused--;
	xma->stat.alloc -= org_blk_size;
#endif

	x = prev_mblk(blk);
	y = next_mblk(blk);
	if (((hcl_uint8_t*)x >= xma->start && x->free) && ((hcl_uint8_t*)y < xma->end && y->free))
	{
		/*
		 * Merge the block with surrounding blocks
		 *
		 *                    blk 
		 *                     |
		 *                     v
		 * +------------+------------+------------+------------+
		 * |     X      |            |     Y      |     Z      |
		 * +------------+------------+------------+------------+
		 *         
		 *  
		 * +--------------------------------------+------------+
		 * |     X                                |     Z      |
		 * +--------------------------------------+------------+
		 *
		 */
		
		hcl_xma_mblk_t* z = next_mblk(y);
		hcl_oow_t ns = MBLKHDRSIZE + org_blk_size + MBLKHDRSIZE;
		hcl_oow_t bs = ns + y->size;

		detach_from_freelist (xma, (hcl_xma_fblk_t*)x);
		detach_from_freelist (xma, (hcl_xma_fblk_t*)y);

		x->size += bs;
		attach_to_freelist (xma, (hcl_xma_fblk_t*)x);

		z = next_mblk(x);
		if ((hcl_uint8_t*)z < xma->end) z->prev_size = x->size;

#if defined(HCL_XMA_ENABLE_STAT)
		xma->stat.nfree--;
		xma->stat.avail += ns;
#endif
	}
	else if ((hcl_uint8_t*)y < xma->end && y->free)
	{
		/*
		 * Merge the block with the next block
		 *
		 *   blk
		 *    |
		 *    v
		 * +------------+------------+------------+
		 * |            |     Y      |     Z      |
		 * +------------+------------+------------+
		 * 
		 * 
		 *
		 *   blk
		 *    |
		 *    v
		 * +-------------------------+------------+
		 * |                         |     Z      |
		 * +-------------------------+------------+
		 * 
		 * 
		 */
		hcl_xma_mblk_t* z = next_mblk(y);

		/* detach y from the free list */
		detach_from_freelist (xma, (hcl_xma_fblk_t*)y);

		/* update the block availability */
		blk->free = 1;
		/* update the block size. MBLKHDRSIZE for the header space in x */
		blk->size += MBLKHDRSIZE + y->size;

		/* update the backward link of Y */
		if ((hcl_uint8_t*)z < xma->end) z->prev_size = blk->size;

		/* attach blk to the free list */
		attach_to_freelist (xma, (hcl_xma_fblk_t*)blk);

#if defined(HCL_XMA_ENABLE_STAT)
		xma->stat.avail += org_blk_size + MBLKHDRSIZE;
#endif
	}
	else if ((hcl_uint8_t*)x >= xma->start && x->free)
	{
		/*
		 * Merge the block with the previous block 
		 *
		 *                 blk
		 *                 |
		 *                 v
		 * +------------+------------+------------+
		 * |     X      |            |     Y      |
		 * +------------+------------+------------+
		 *
		 * +-------------------------+------------+
		 * |     X                   |     Y      |
		 * +-------------------------+------------+
		 */
		detach_from_freelist (xma, (hcl_xma_fblk_t*)x);

		x->size += MBLKHDRSIZE + org_blk_size;

		assert (y == next_mblk(x));
		if ((hcl_uint8_t*)y < xma->end) y->prev_size = x->size;

		attach_to_freelist (xma, (hcl_xma_fblk_t*)x);

#if defined(HCL_XMA_ENABLE_STAT)
		xma->stat.avail += MBLKHDRSIZE + org_blk_size;
#endif
	}
	else
	{
		blk->free = 1;
		attach_to_freelist (xma, (hcl_xma_fblk_t*)blk);

#if defined(HCL_XMA_ENABLE_STAT)
		xma->stat.nfree++;
		xma->stat.avail += org_blk_size;
#endif
	}

	DBG_VERIFY (xma, "free end");
}

void hcl_xma_dump (hcl_xma_t* xma, hcl_xma_dumper_t dumper, void* ctx)
{
	hcl_xma_mblk_t* tmp;
	hcl_oow_t fsum, asum; 
#if defined(HCL_XMA_ENABLE_STAT)
	hcl_oow_t isum;
#endif

	dumper (ctx, "<XMA DUMP>\n");

#if defined(HCL_XMA_ENABLE_STAT)
	dumper (ctx, "== statistics ==\n");
	dumper (ctx, "total = %zu\n", xma->stat.total);
	dumper (ctx, "alloc = %zu\n", xma->stat.alloc);
	dumper (ctx, "avail = %zu\n", xma->stat.avail);
#endif

	dumper (ctx, "== blocks ==\n");
	dumper (ctx, " size               avail address\n");
	for (tmp = (hcl_xma_mblk_t*)xma->start, fsum = 0, asum = 0; (hcl_uint8_t*)tmp < xma->end; tmp = next_mblk(tmp))
	{
		dumper (ctx, " %-18zu %-5u %p\n", tmp->size, (unsigned int)tmp->free, tmp);
		if (tmp->free) fsum += tmp->size;
		else asum += tmp->size;
	}

#if defined(HCL_XMA_ENABLE_STAT)
	isum = (xma->stat.nfree + xma->stat.nused) * MBLKHDRSIZE;
#endif

	dumper (ctx, "---------------------------------------\n");
	dumper (ctx, "Allocated blocks: %18zu bytes\n", asum);
	dumper (ctx, "Available blocks: %18zu bytes\n", fsum);


#if defined(HCL_XMA_ENABLE_STAT)
	dumper (ctx, "Internal use    : %18zu bytes\n", isum);
	dumper (ctx, "Total           : %18zu bytes\n", (asum + fsum + isum));
#endif

#if defined(HCL_XMA_ENABLE_STAT)
	assert (asum == xma->stat.alloc);
	assert (fsum == xma->stat.avail);
	assert (isum == xma->stat.total - (xma->stat.alloc + xma->stat.avail));
	assert (asum + fsum + isum == xma->stat.total);
#endif
}

