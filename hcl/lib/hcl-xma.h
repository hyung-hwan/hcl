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

#ifndef _HCL_XMA_H_
#define _HCL_XMA_H_

/** @file
 * This file defines an extravagant memory allocator. Why? It may be so.
 * The memory allocator allows you to maintain memory blocks from a
 * larger memory chunk allocated with an outer memory allocator.
 * Typically, an outer memory allocator is a standard memory allocator
 * like malloc(). You can isolate memory blocks into a particular chunk.
 *
 * See the example below. Note it omits error handling.
 *
 * @code
 * #include <hcl-xma.h>
 * #include <stdio.h>
 * #include <stdarg.h>
 * void dumper (void* ctx, const char* fmt, ...)
 * {
 * 	va_list ap;
 * 	va_start (ap, fmt);
 * 	vfprintf (fmt, ap);
 * 	va_end (ap);
 * }
 * int main ()
 * {
 *   hcl_xma_t* xma;
 *   void* ptr1, * ptr2;
 *
 *   // create a new memory allocator obtaining a 100K byte zone 
 *   // with the default memory allocator
 *   xma = hcl_xma_open(HCL_NULL, 0, 100000L); 
 *
 *   ptr1 = hcl_xma_alloc(xma, 5000); // allocate a 5K block from the zone
 *   ptr2 = hcl_xma_alloc(xma, 1000); // allocate a 1K block from the zone
 *   ptr1 = hcl_xma_realloc(xma, ptr1, 6000); // resize the 5K block to 6K.
 *
 *   hcl_xma_dump (xma, dumper, HCL_NULL); // dump memory blocks 
 *
 *   // the following two lines are not actually needed as the allocator
 *   // is closed after them.
 *   hcl_xma_free (xma, ptr2); // dispose of the 1K block
 *   hcl_xma_free (xma, ptr1); // dispose of the 6K block
 *
 *   hcl_xma_close (xma); //  destroy the memory allocator
 *   return 0;
 * }
 * @endcode
 */
#include <hcl-cmn.h>

#ifdef HCL_BUILD_DEBUG
#	define HCL_XMA_ENABLE_STAT
#endif

/** @struct hcl_xma_t
 * The hcl_xma_t type defines a simple memory allocator over a memory zone.
 * It can obtain a relatively large zone of memory and manage it.
 */
typedef struct hcl_xma_t hcl_xma_t;

/**
 * The hcl_xma_fblk_t type defines a memory block allocated.
 */
typedef struct hcl_xma_fblk_t hcl_xma_fblk_t;
typedef struct hcl_xma_mblk_t hcl_xma_mblk_t;

#define HCL_XMA_FIXED 32
#define HCL_XMA_SIZE_BITS ((HCL_SIZEOF_OOW_T*8)-1)

struct hcl_xma_t
{
	hcl_mmgr_t* _mmgr;

	hcl_uint8_t* start; /* zone beginning */
	hcl_uint8_t* end; /* zone end */
	int          internal;

	/** pointer array to free memory blocks */
	hcl_xma_fblk_t* xfree[HCL_XMA_FIXED + HCL_XMA_SIZE_BITS + 1]; 

	/** pre-computed value for fast xfree index calculation */
	hcl_oow_t     bdec;

#ifdef HCL_XMA_ENABLE_STAT
	struct
	{
		hcl_oow_t total;
		hcl_oow_t alloc;
		hcl_oow_t avail;
		hcl_oow_t nused;
		hcl_oow_t nfree;
	} stat;
#endif
};

/**
 * The hcl_xma_dumper_t type defines a printf-like output function
 * for hcl_xma_dump().
 */
typedef void (*hcl_xma_dumper_t) (
	void*            ctx,
	const hcl_bch_t* fmt,
	...
);

#if defined(__cplusplus)
extern "C" {
#endif

/**
 * The hcl_xma_open() function creates a memory allocator. It obtains a memory
 * zone of the @a zonesize bytes with the memory manager @a mmgr. It also makes
 * available the extension area of the @a xtnsize bytes that you can get the
 * pointer to with hcl_xma_getxtn().
 *
 * @return pointer to a memory allocator on success, #HCL_NULL on failure
 */
HCL_EXPORT hcl_xma_t* hcl_xma_open (
	hcl_mmgr_t* mmgr,    /**< memory manager */
	hcl_oow_t   xtnsize, /**< extension size in bytes */
	void*       zoneptr,
	hcl_oow_t   zonesize /**< zone size in bytes */
);

/**
 * The hcl_xma_close() function destroys a memory allocator. It also frees
 * the memory zone obtained, which invalidates the memory blocks within 
 * the zone. Call this function to destroy a memory allocator created with
 * hcl_xma_open().
 */
HCL_EXPORT void hcl_xma_close (
	hcl_xma_t* xma /**< memory allocator */
);

#if defined(HCL_HAVE_INLINE)
static HCL_INLINE hcl_mmgr_t* hcl_xma_getmmgr (hcl_xma_t* xma) { return xma->_mmgr; }
#else
#	define hcl_xma_getmmgr(xma) (((hcl_xma_t*)(xma))->_mmgr)
#endif

#if defined(HCL_HAVE_INLINE)
static HCL_INLINE void* hcl_xma_getxtn (hcl_xma_t* xma) { return (void*)(xma + 1); }
#else
#define hcl_xma_getxtn(xma) ((void*)((hcl_xma_t*)(xma) + 1))
#endif

/**
 * The hcl_xma_init() initializes a memory allocator. If you have the hcl_xma_t
 * structure statically declared or already allocated, you may pass the pointer
 * to this function instead of calling hcl_xma_open(). It obtains a memory zone
 * of @a zonesize bytes with the memory manager @a mmgr. Unlike hcl_xma_open(),
 * it does not accept the extension size, thus not creating an extention area.
 * @return 0 on success, -1 on failure
 */
HCL_EXPORT int hcl_xma_init (
	hcl_xma_t*  xma,     /**< memory allocator */
	hcl_mmgr_t* mmgr,    /**< memory manager */
	void*       zoneptr,  /**< pointer to memory zone. if #HCL_NULL, a zone is auto-created */
	hcl_oow_t   zonesize /**< zone size in bytes */
);

/**
 * The hcl_xma_fini() function finalizes a memory allocator. Call this 
 * function to finalize a memory allocator initialized with hcl_xma_init().
 */
HCL_EXPORT void hcl_xma_fini (
	hcl_xma_t* xma /**< memory allocator */
);

/**
 * The hcl_xma_alloc() function allocates @a size bytes.
 * @return pointer to a memory block on success, #HCL_NULL on failure
 */
HCL_EXPORT void* hcl_xma_alloc (
	hcl_xma_t* xma, /**< memory allocator */
	hcl_oow_t  size /**< size in bytes */
);

HCL_EXPORT void* hcl_xma_calloc (
	hcl_xma_t* xma,
	hcl_oow_t  size
);

/**
 * The hcl_xma_alloc() function resizes the memory block @a b to @a size bytes.
 * @return pointer to a resized memory block on success, #HCL_NULL on failure
 */
HCL_EXPORT void* hcl_xma_realloc (
	hcl_xma_t* xma,  /**< memory allocator */
	void*      b,    /**< memory block */
	hcl_oow_t  size  /**< new size in bytes */
);

/**
 * The hcl_xma_alloc() function frees the memory block @a b.
 */
HCL_EXPORT void hcl_xma_free (
	hcl_xma_t* xma, /**< memory allocator */
	void*      b    /**< memory block */
);

/**
 * The hcl_xma_dump() function dumps the contents of the memory zone
 * with the output function @a dumper provided. The debug build shows
 * more statistical counters.
 */
HCL_EXPORT void hcl_xma_dump (
	hcl_xma_t*       xma,    /**< memory allocator */
	hcl_xma_dumper_t dumper, /**< output function */
	void*            ctx     /**< first parameter to output function */
);

#if defined(__cplusplus)
}
#endif

#endif
