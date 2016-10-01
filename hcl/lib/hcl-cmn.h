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

#ifndef _HCL_CMN_H_
#define _HCL_CMN_H_

/* WARNING: NEVER CHANGE/DELETE THE FOLLOWING HCL_HAVE_CFG_H DEFINITION. 
 *          IT IS USED FOR DEPLOYMENT BY MAKEFILE.AM */
/*#define HCL_HAVE_CFG_H*/

#if defined(HCL_HAVE_CFG_H)
#	include "hcl-cfg.h"
#elif defined(_WIN32)
#	include "hcl-msw.h"
#elif defined(__OS2__)
#	include "hcl-os2.h"
#elif defined(__MSDOS__)
#	include "hcl-dos.h"
#elif defined(macintosh)
#	include "hcl-mac.h" /* class mac os */
#else
#	error UNSUPPORTED SYSTEM
#endif

#if defined(EMSCRIPTEN)
#	if defined(HCL_SIZEOF___INT128)
#		undef HCL_SIZEOF___INT128 
#		define HCL_SIZEOF___INT128 0
#	endif
#	if defined(HCL_SIZEOF_LONG) && defined(HCL_SIZEOF_INT) && (HCL_SIZEOF_LONG > HCL_SIZEOF_INT)
		/* autoconf doesn't seem to match actual emscripten */
#		undef HCL_SIZEOF_LONG
#		define HCL_SIZEOF_LONG HCL_SIZEOF_INT
#	endif
#endif


/* =========================================================================
 * PRIMITIVE TYPE DEFINTIONS
 * ========================================================================= */

/* hcl_int8_t */
#if defined(HCL_SIZEOF_CHAR) && (HCL_SIZEOF_CHAR == 1)
#	define HCL_HAVE_UINT8_T
#	define HCL_HAVE_INT8_T
	typedef unsigned char      hcl_uint8_t;
	typedef signed char        hcl_int8_t;
#elif defined(HCL_SIZEOF___INT8) && (HCL_SIZEOF___INT8 == 1)
#	define HCL_HAVE_UINT8_T
#	define HCL_HAVE_INT8_T
	typedef unsigned __int8    hcl_uint8_t;
	typedef signed __int8      hcl_int8_t;
#elif defined(HCL_SIZEOF___INT8_T) && (HCL_SIZEOF___INT8_T == 1)
#	define HCL_HAVE_UINT8_T
#	define HCL_HAVE_INT8_T
	typedef unsigned __int8_t  hcl_uint8_t;
	typedef signed __int8_t    hcl_int8_t;
#else
#	define HCL_HAVE_UINT8_T
#	define HCL_HAVE_INT8_T
	typedef unsigned char      hcl_uint8_t;
	typedef signed char        hcl_int8_t;
#endif


/* hcl_int16_t */
#if defined(HCL_SIZEOF_SHORT) && (HCL_SIZEOF_SHORT == 2)
#	define HCL_HAVE_UINT16_T
#	define HCL_HAVE_INT16_T
	typedef unsigned short int  hcl_uint16_t;
	typedef signed short int    hcl_int16_t;
#elif defined(HCL_SIZEOF___INT16) && (HCL_SIZEOF___INT16 == 2)
#	define HCL_HAVE_UINT16_T
#	define HCL_HAVE_INT16_T
	typedef unsigned __int16    hcl_uint16_t;
	typedef signed __int16      hcl_int16_t;
#elif defined(HCL_SIZEOF___INT16_T) && (HCL_SIZEOF___INT16_T == 2)
#	define HCL_HAVE_UINT16_T
#	define HCL_HAVE_INT16_T
	typedef unsigned __int16_t  hcl_uint16_t;
	typedef signed __int16_t    hcl_int16_t;
#else
#	define HCL_HAVE_UINT16_T
#	define HCL_HAVE_INT16_T
	typedef unsigned short int  hcl_uint16_t;
	typedef signed short int    hcl_int16_t;
#endif


/* hcl_int32_t */
#if defined(HCL_SIZEOF_INT) && (HCL_SIZEOF_INT == 4)
#	define HCL_HAVE_UINT32_T
#	define HCL_HAVE_INT32_T
	typedef unsigned int        hcl_uint32_t;
	typedef signed int          hcl_int32_t;
#elif defined(HCL_SIZEOF_LONG) && (HCL_SIZEOF_LONG == 4)
#	define HCL_HAVE_UINT32_T
#	define HCL_HAVE_INT32_T
	typedef unsigned long       hcl_uint32_t;
	typedef signed long         hcl_int32_t;
#elif defined(HCL_SIZEOF___INT32) && (HCL_SIZEOF___INT32 == 4)
#	define HCL_HAVE_UINT32_T
#	define HCL_HAVE_INT32_T
	typedef unsigned __int32    hcl_uint32_t;
	typedef signed __int32      hcl_int32_t;
#elif defined(HCL_SIZEOF___INT32_T) && (HCL_SIZEOF___INT32_T == 4)
#	define HCL_HAVE_UINT32_T
#	define HCL_HAVE_INT32_T
	typedef unsigned __int32_t  hcl_uint32_t;
	typedef signed __int32_t    hcl_int32_t;
#elif defined(__MSDOS__)
#	define HCL_HAVE_UINT32_T
#	define HCL_HAVE_INT32_T
	typedef unsigned long int   hcl_uint32_t;
	typedef signed long int     hcl_int32_t;
#else
#	define HCL_HAVE_UINT32_T
#	define HCL_HAVE_INT32_T
	typedef unsigned int        hcl_uint32_t;
	typedef signed int          hcl_int32_t;
#endif

/* hcl_int64_t */
#if defined(HCL_SIZEOF_INT) && (HCL_SIZEOF_INT == 8)
#	define HCL_HAVE_UINT64_T
#	define HCL_HAVE_INT64_T
	typedef unsigned int        hcl_uint64_t;
	typedef signed int          hcl_int64_t;
#elif defined(HCL_SIZEOF_LONG) && (HCL_SIZEOF_LONG == 8)
#	define HCL_HAVE_UINT64_T
#	define HCL_HAVE_INT64_T
	typedef unsigned long       hcl_uint64_t;
	typedef signed long         hcl_int64_t;
#elif defined(HCL_SIZEOF_LONG_LONG) && (HCL_SIZEOF_LONG_LONG == 8)
#	define HCL_HAVE_UINT64_T
#	define HCL_HAVE_INT64_T
	typedef unsigned long long  hcl_uint64_t;
	typedef signed long long    hcl_int64_t;
#elif defined(HCL_SIZEOF___INT64) && (HCL_SIZEOF___INT64 == 8)
#	define HCL_HAVE_UINT64_T
#	define HCL_HAVE_INT64_T
	typedef unsigned __int64    hcl_uint64_t;
	typedef signed __int64      hcl_int64_t;
#elif defined(HCL_SIZEOF___INT64_T) && (HCL_SIZEOF___INT64_T == 8)
#	define HCL_HAVE_UINT64_T
#	define HCL_HAVE_INT64_T
	typedef unsigned __int64_t  hcl_uint64_t;
	typedef signed __int64_t    hcl_int64_t;
#else
	/* no 64-bit integer */
#endif

/* hcl_int128_t */
#if defined(HCL_SIZEOF_INT) && (HCL_SIZEOF_INT == 16)
#	define HCL_HAVE_UINT128_T
#	define HCL_HAVE_INT128_T
	typedef unsigned int        hcl_uint128_t;
	typedef signed int          hcl_int128_t;
#elif defined(HCL_SIZEOF_LONG) && (HCL_SIZEOF_LONG == 16)
#	define HCL_HAVE_UINT128_T
#	define HCL_HAVE_INT128_T
	typedef unsigned long       hcl_uint128_t;
	typedef signed long         hcl_int128_t;
#elif defined(HCL_SIZEOF_LONG_LONG) && (HCL_SIZEOF_LONG_LONG == 16)
#	define HCL_HAVE_UINT128_T
#	define HCL_HAVE_INT128_T
	typedef unsigned long long  hcl_uint128_t;
	typedef signed long long    hcl_int128_t;
#elif defined(HCL_SIZEOF___INT128) && (HCL_SIZEOF___INT128 == 16)
#	define HCL_HAVE_UINT128_T
#	define HCL_HAVE_INT128_T
	typedef unsigned __int128    hcl_uint128_t;
	typedef signed __int128      hcl_int128_t;
#elif defined(HCL_SIZEOF___INT128_T) && (HCL_SIZEOF___INT128_T == 16)
#	define HCL_HAVE_UINT128_T
#	define HCL_HAVE_INT128_T
	#if defined(__clang__)
	typedef __uint128_t  hcl_uint128_t;
	typedef __int128_t   hcl_int128_t;
	#else
	typedef unsigned __int128_t  hcl_uint128_t;
	typedef signed __int128_t    hcl_int128_t;
	#endif
#else
	/* no 128-bit integer */
#endif

#if defined(HCL_HAVE_UINT8_T) && (HCL_SIZEOF_VOID_P == 1)
#	error UNSUPPORTED POINTER SIZE
#elif defined(HCL_HAVE_UINT16_T) && (HCL_SIZEOF_VOID_P == 2)
	typedef hcl_uint16_t hcl_uintptr_t;
	typedef hcl_int16_t hcl_intptr_t;
	typedef hcl_uint8_t hcl_ushortptr_t;
	typedef hcl_int8_t hcl_shortptr_t;
#elif defined(HCL_HAVE_UINT32_T) && (HCL_SIZEOF_VOID_P == 4)
	typedef hcl_uint32_t hcl_uintptr_t;
	typedef hcl_int32_t hcl_intptr_t;
	typedef hcl_uint16_t hcl_ushortptr_t;
	typedef hcl_int16_t hcl_shortptr_t;
#elif defined(HCL_HAVE_UINT64_T) && (HCL_SIZEOF_VOID_P == 8)
	typedef hcl_uint64_t hcl_uintptr_t;
	typedef hcl_int64_t hcl_intptr_t;
	typedef hcl_uint32_t hcl_ushortptr_t;
	typedef hcl_int32_t hcl_shortptr_t;
#elif defined(HCL_HAVE_UINT128_T) && (HCL_SIZEOF_VOID_P == 16) 
	typedef hcl_uint128_t hcl_uintptr_t;
	typedef hcl_int128_t hcl_intptr_t;
	typedef hcl_uint64_t hcl_ushortptr_t;
	typedef hcl_int64_t hcl_shortptr_t;
#else
#	error UNKNOWN POINTER SIZE
#endif

#define HCL_SIZEOF_INTPTR_T HCL_SIZEOF_VOID_P
#define HCL_SIZEOF_UINTPTR_T HCL_SIZEOF_VOID_P
#define HCL_SIZEOF_SHORTPTR_T (HCL_SIZEOF_VOID_P / 2)
#define HCL_SIZEOF_USHORTPTR_T (HCL_SIZEOF_VOID_P / 2)

#if defined(HCL_HAVE_INT128_T)
#	define HCL_SIZEOF_INTMAX_T 16
#	define HCL_SIZEOF_UINTMAX_T 16
	typedef hcl_int128_t hcl_intmax_t;
	typedef hcl_uint128_t hcl_uintmax_t;
#elif defined(HCL_HAVE_INT64_T)
#	define HCL_SIZEOF_INTMAX_T 8
#	define HCL_SIZEOF_UINTMAX_T 8
	typedef hcl_int64_t hcl_intmax_t;
	typedef hcl_uint64_t hcl_uintmax_t;
#elif defined(HCL_HAVE_INT32_T)
#	define HCL_SIZEOF_INTMAX_T 4
#	define HCL_SIZEOF_UINTMAX_T 4
	typedef hcl_int32_t hcl_intmax_t;
	typedef hcl_uint32_t hcl_uintmax_t;
#elif defined(HCL_HAVE_INT16_T)
#	define HCL_SIZEOF_INTMAX_T 2
#	define HCL_SIZEOF_UINTMAX_T 2
	typedef hcl_int16_t hcl_intmax_t;
	typedef hcl_uint16_t hcl_uintmax_t;
#elif defined(HCL_HAVE_INT8_T)
#	define HCL_SIZEOF_INTMAX_T 1
#	define HCL_SIZEOF_UINTMAX_T 1
	typedef hcl_int8_t hcl_intmax_t;
	typedef hcl_uint8_t hcl_uintmax_t;
#else
#	error UNKNOWN INTMAX SIZE
#endif

/* =========================================================================
 * BASIC HCL TYPES
 * =========================================================================*/

typedef char                     hcl_bch_t;
typedef int                      hcl_bci_t;

typedef hcl_uint16_t            hcl_uch_t; /* TODO ... wchar_t??? */
typedef hcl_int32_t             hcl_uci_t;

typedef hcl_uint8_t             hcl_oob_t;

/* NOTE: sizeof(hcl_oop_t) must be equal to sizeof(hcl_oow_t) */
typedef hcl_uintptr_t           hcl_oow_t;
typedef hcl_intptr_t            hcl_ooi_t;
#define HCL_SIZEOF_OOW_T HCL_SIZEOF_UINTPTR_T
#define HCL_SIZEOF_OOI_T HCL_SIZEOF_INTPTR_T

typedef hcl_ushortptr_t         hcl_oohw_t; /* half word - half word */
typedef hcl_shortptr_t          hcl_oohi_t; /* signed half word */
#define HCL_SIZEOF_OOHW_T HCL_SIZEOF_USHORTPTR_T
#define HCL_SIZEOF_OOHI_T HCL_SIZEOF_SHORTPTR_T

struct hcl_ucs_t
{
	hcl_uch_t* ptr;
	hcl_oow_t  len;
};
typedef struct hcl_ucs_t hcl_ucs_t;

struct hcl_bcs_t
{
	hcl_bch_t* ptr;
	hcl_oow_t  len;
};
typedef struct hcl_bcs_t hcl_bcs_t;

typedef hcl_uch_t               hcl_ooch_t;
typedef hcl_uci_t               hcl_ooci_t;
typedef hcl_ucs_t               hcl_oocs_t;
#define HCL_OOCH_IS_UCH



/* =========================================================================
 * TIME-RELATED TYPES
 * =========================================================================*/
#define HCL_MSECS_PER_SEC  (1000)
#define HCL_MSECS_PER_MIN  (HCL_MSECS_PER_SEC * HCL_SECS_PER_MIN)
#define HCL_MSECS_PER_HOUR (HCL_MSECS_PER_SEC * HCL_SECS_PER_HOUR)
#define HCL_MSECS_PER_DAY  (HCL_MSECS_PER_SEC * HCL_SECS_PER_DAY)

#define HCL_USECS_PER_MSEC (1000)
#define HCL_NSECS_PER_USEC (1000)
#define HCL_NSECS_PER_MSEC (HCL_NSECS_PER_USEC * HCL_USECS_PER_MSEC)
#define HCL_USECS_PER_SEC  (HCL_USECS_PER_MSEC * HCL_MSECS_PER_SEC)
#define HCL_NSECS_PER_SEC  (HCL_NSECS_PER_USEC * HCL_USECS_PER_MSEC * HCL_MSECS_PER_SEC)

#define HCL_SECNSEC_TO_MSEC(sec,nsec) \
        (((hcl_intptr_t)(sec) * HCL_MSECS_PER_SEC) + ((hcl_intptr_t)(nsec) / HCL_NSECS_PER_MSEC))

#define HCL_SECNSEC_TO_USEC(sec,nsec) \
        (((hcl_intptr_t)(sec) * HCL_USECS_PER_SEC) + ((hcl_intptr_t)(nsec) / HCL_NSECS_PER_USEC))

#define HCL_SECNSEC_TO_NSEC(sec,nsec) \
        (((hcl_intptr_t)(sec) * HCL_NSECS_PER_SEC) + (hcl_intptr_t)(nsec))

#define HCL_SEC_TO_MSEC(sec) ((sec) * HCL_MSECS_PER_SEC)
#define HCL_MSEC_TO_SEC(sec) ((sec) / HCL_MSECS_PER_SEC)

#define HCL_USEC_TO_NSEC(usec) ((usec) * HCL_NSECS_PER_USEC)
#define HCL_NSEC_TO_USEC(nsec) ((nsec) / HCL_NSECS_PER_USEC)

#define HCL_MSEC_TO_NSEC(msec) ((msec) * HCL_NSECS_PER_MSEC)
#define HCL_NSEC_TO_MSEC(nsec) ((nsec) / HCL_NSECS_PER_MSEC)

#define HCL_SEC_TO_NSEC(sec) ((sec) * HCL_NSECS_PER_SEC)
#define HCL_NSEC_TO_SEC(nsec) ((nsec) / HCL_NSECS_PER_SEC)

#define HCL_SEC_TO_USEC(sec) ((sec) * HCL_USECS_PER_SEC)
#define HCL_USEC_TO_SEC(usec) ((usec) / HCL_USECS_PER_SEC)

typedef struct hcl_ntime_t hcl_ntime_t;
struct hcl_ntime_t
{
	hcl_intptr_t  sec;
	hcl_int32_t   nsec; /* nanoseconds */
};

#define HCL_INITNTIME(c,s,ns) (((c)->sec = (s)), ((c)->nsec = (ns)))
#define HCL_CLEARNTIME(c) HCL_INITNTIME(c, 0, 0)

#define HCL_ADDNTIME(c,a,b) \
	do { \
		(c)->sec = (a)->sec + (b)->sec; \
		(c)->nsec = (a)->nsec + (b)->nsec; \
		while ((c)->nsec >= HCL_NSECS_PER_SEC) { (c)->sec++; (c)->nsec -= HCL_NSECS_PER_SEC; } \
	} while(0)

#define HCL_ADDNTIMESNS(c,a,s,ns) \
	do { \
		(c)->sec = (a)->sec + (s); \
		(c)->nsec = (a)->nsec + (ns); \
		while ((c)->nsec >= HCL_NSECS_PER_SEC) { (c)->sec++; (c)->nsec -= HCL_NSECS_PER_SEC; } \
	} while(0)

#define HCL_SUBNTIME(c,a,b) \
	do { \
		(c)->sec = (a)->sec - (b)->sec; \
		(c)->nsec = (a)->nsec - (b)->nsec; \
		while ((c)->nsec < 0) { (c)->sec--; (c)->nsec += HCL_NSECS_PER_SEC; } \
	} while(0)

#define HCL_SUBNTIMESNS(c,a,s,ns) \
	do { \
		(c)->sec = (a)->sec - s; \
		(c)->nsec = (a)->nsec - ns; \
		while ((c)->nsec < 0) { (c)->sec--; (c)->nsec += HCL_NSECS_PER_SEC; } \
	} while(0)


#define HCL_CMPNTIME(a,b) (((a)->sec == (b)->sec)? ((a)->nsec - (b)->nsec): ((a)->sec - (b)->sec))

/* =========================================================================
 * PRIMITIVE MACROS
 * ========================================================================= */
#define HCL_UCI_EOF ((hcl_ooci_t)-1)
#define HCL_UCI_NL  ((hcl_ooci_t)'\n')

#define HCL_SIZEOF(x) (sizeof(x))
#define HCL_COUNTOF(x) (sizeof(x) / sizeof(x[0]))

/**
 * The HCL_OFFSETOF() macro returns the offset of a field from the beginning
 * of a structure.
 */
#define HCL_OFFSETOF(type,member) ((hcl_uintptr_t)&((type*)0)->member)

/**
 * The HCL_ALIGNOF() macro returns the alignment size of a structure.
 * Note that this macro may not work reliably depending on the type given.
 */
#define HCL_ALIGNOF(type) HCL_OFFSETOF(struct { hcl_uint8_t d1; type d2; }, d2)
        /*(sizeof(struct { hcl_uint8_t d1; type d2; }) - sizeof(type))*/

#if defined(__cplusplus)
#	if (__cplusplus >= 201103L) /* C++11 */
#		define HCL_NULL nullptr
#	else
#		define HCL_NULL (0)
#	endif
#else
#	define HCL_NULL ((void*)0)
#endif

/* make a bit mask that can mask off low n bits */
#define HCL_LBMASK(type,n) (~(~((type)0) << (n))) 
#define HCL_LBMASK_SAFE(type,n) (((n) < HCL_SIZEOF(type) * 8)? HCL_LBMASK(type,n): ~(type)0)

/* make a bit mask that can mask off hig n bits */
#define HCL_HBMASK(type,n) (~(~((type)0) >> (n)))
#define HCL_HBMASK_SAFE(type,n) (((n) < HCL_SIZEOF(type) * 8)? HCL_HBMASK(type,n): ~(type)0)

/* get 'length' bits starting from the bit at the 'offset' */
#define HCL_GETBITS(type,value,offset,length) \
	((((type)(value)) >> (offset)) & HCL_LBMASK(type,length))

#define HCL_CLEARBITS(type,value,offset,length) \
	(((type)(value)) & ~(HCL_LBMASK(type,length) << (offset)))

#define HCL_SETBITS(type,value,offset,length,bits) \
	(value = (HCL_CLEARBITS(type,value,offset,length) | (((bits) & HCL_LBMASK(type,length)) << (offset))))

#define HCL_FLIPBITS(type,value,offset,length) \
	(((type)(value)) ^ (HCL_LBMASK(type,length) << (offset)))

#define HCL_ORBITS(type,value,offset,length,bits) \
	(value = (((type)(value)) | (((bits) & HCL_LBMASK(type,length)) << (offset))))


/** 
 * The HCL_BITS_MAX() macros calculates the maximum value that the 'nbits'
 * bits of an unsigned integer of the given 'type' can hold.
 * \code
 * printf ("%u", HCL_BITS_MAX(unsigned int, 5));
 * \endcode
 */
/*#define HCL_BITS_MAX(type,nbits) ((((type)1) << (nbits)) - 1)*/
#define HCL_BITS_MAX(type,nbits) ((~(type)0) >> (HCL_SIZEOF(type) * 8 - (nbits)))

/* =========================================================================
 * MMGR
 * ========================================================================= */
typedef struct hcl_mmgr_t hcl_mmgr_t;

/** 
 * allocate a memory chunk of the size \a n.
 * \return pointer to a memory chunk on success, #HCL_NULL on failure.
 */
typedef void* (*hcl_mmgr_alloc_t)   (hcl_mmgr_t* mmgr, hcl_oow_t n);
/** 
 * resize a memory chunk pointed to by \a ptr to the size \a n.
 * \return pointer to a memory chunk on success, #HCL_NULL on failure.
 */
typedef void* (*hcl_mmgr_realloc_t) (hcl_mmgr_t* mmgr, void* ptr, hcl_oow_t n);
/**
 * free a memory chunk pointed to by \a ptr.
 */
typedef void  (*hcl_mmgr_free_t)    (hcl_mmgr_t* mmgr, void* ptr);

/**
 * The hcl_mmgr_t type defines the memory management interface.
 * As the type is merely a structure, it is just used as a single container
 * for memory management functions with a pointer to user-defined data. 
 * The user-defined data pointer \a ctx is passed to each memory management 
 * function whenever it is called. You can allocate, reallocate, and free 
 * a memory chunk.
 *
 * For example, a hcl_xxx_open() function accepts a pointer of the hcl_mmgr_t
 * type and the xxx object uses it to manage dynamic data within the object. 
 */
struct hcl_mmgr_t
{
	hcl_mmgr_alloc_t   alloc;   /**< allocation function */
	hcl_mmgr_realloc_t realloc; /**< resizing function */
	hcl_mmgr_free_t    free;    /**< disposal function */
	void*               ctx;     /**< user-defined data pointer */
};

/**
 * The HCL_MMGR_ALLOC() macro allocates a memory block of the \a size bytes
 * using the \a mmgr memory manager.
 */
#define HCL_MMGR_ALLOC(mmgr,size) ((mmgr)->alloc(mmgr,size))

/**
 * The HCL_MMGR_REALLOC() macro resizes a memory block pointed to by \a ptr 
 * to the \a size bytes using the \a mmgr memory manager.
 */
#define HCL_MMGR_REALLOC(mmgr,ptr,size) ((mmgr)->realloc(mmgr,ptr,size))

/** 
 * The HCL_MMGR_FREE() macro deallocates the memory block pointed to by \a ptr.
 */
#define HCL_MMGR_FREE(mmgr,ptr) ((mmgr)->free(mmgr,ptr))


/* =========================================================================
 * CMGR
 * =========================================================================*/

typedef struct hcl_cmgr_t hcl_cmgr_t;

typedef hcl_oow_t (*hcl_cmgr_bctouc_t) (
	const hcl_bch_t*   mb, 
	hcl_oow_t         size,
	hcl_uch_t*         wc
);

typedef hcl_oow_t (*hcl_cmgr_uctobc_t) (
	hcl_uch_t    wc,
	hcl_bch_t*   mb,
	hcl_oow_t   size
);

/**
 * The hcl_cmgr_t type defines the character-level interface to 
 * multibyte/wide-character conversion. This interface doesn't 
 * provide any facility to store conversion state in a context
 * independent manner. This leads to the limitation that it can
 * handle a stateless multibyte encoding only.
 */
struct hcl_cmgr_t
{
	hcl_cmgr_bctouc_t bctouc;
	hcl_cmgr_uctobc_t uctobc;
};

/* =========================================================================
 * MACROS THAT CHANGES THE BEHAVIORS OF THE C COMPILER/LINKER
 * =========================================================================*/

#if defined(__BORLANDC__) && (__BORLANDC__ < 0x500)
#	define HCL_IMPORT
#	define HCL_EXPORT
#	define HCL_PRIVATE
#elif defined(_WIN32) || (defined(__WATCOMC__) && !defined(__WINDOWS_386__))
#	define HCL_IMPORT __declspec(dllimport)
#	define HCL_EXPORT __declspec(dllexport)
#	define HCL_PRIVATE 
#elif defined(__GNUC__) && (__GNUC__>=4)
#	define HCL_IMPORT __attribute__((visibility("default")))
#	define HCL_EXPORT __attribute__((visibility("default")))
#	define HCL_PRIVATE __attribute__((visibility("hidden")))
/*#	define HCL_PRIVATE __attribute__((visibility("internal")))*/
#else
#	define HCL_IMPORT
#	define HCL_EXPORT
#	define HCL_PRIVATE
#endif

#if defined(__STDC_VERSION__) && (__STDC_VERSION__>=199901L)
	/* C99 has inline */
#	define HCL_INLINE inline
#	define HCL_HAVE_INLINE
#elif defined(__GNUC__) && defined(__GNUC_GNU_INLINE__)
	/* gcc disables inline when -std=c89 or -ansi is used. 
	 * so use __inline__ supported by gcc regardless of the options */
#	define HCL_INLINE /*extern*/ __inline__
#	define HCL_HAVE_INLINE
#else
#	define HCL_INLINE 
#	undef HCL_HAVE_INLINE
#endif




/**
 * The HCL_TYPE_IS_SIGNED() macro determines if a type is signed.
 * \code
 * printf ("%d\n", (int)HCL_TYPE_IS_SIGNED(int));
 * printf ("%d\n", (int)HCL_TYPE_IS_SIGNED(unsigned int));
 * \endcode
 */
#define HCL_TYPE_IS_SIGNED(type) (((type)0) > ((type)-1))

/**
 * The HCL_TYPE_IS_SIGNED() macro determines if a type is unsigned.
 * \code
 * printf ("%d\n", HCL_TYPE_IS_UNSIGNED(int));
 * printf ("%d\n", HCL_TYPE_IS_UNSIGNED(unsigned int));
 * \endcode
 */
#define HCL_TYPE_IS_UNSIGNED(type) (((type)0) < ((type)-1))

#define HCL_TYPE_SIGNED_MAX(type) \
	((type)~((type)1 << ((type)HCL_SIZEOF(type) * 8 - 1)))
#define HCL_TYPE_UNSIGNED_MAX(type) ((type)(~(type)0))

#define HCL_TYPE_SIGNED_MIN(type) \
	((type)((type)1 << ((type)HCL_SIZEOF(type) * 8 - 1)))
#define HCL_TYPE_UNSIGNED_MIN(type) ((type)0)

#define HCL_TYPE_MAX(type) \
	((HCL_TYPE_IS_SIGNED(type)? HCL_TYPE_SIGNED_MAX(type): HCL_TYPE_UNSIGNED_MAX(type)))
#define HCL_TYPE_MIN(type) \
	((HCL_TYPE_IS_SIGNED(type)? HCL_TYPE_SIGNED_MIN(type): HCL_TYPE_UNSIGNED_MIN(type)))


/* =========================================================================
 * COMPILER FEATURE TEST MACROS
 * =========================================================================*/
#if !defined(__has_builtin) && defined(_INTELC32_)
	/* intel c code builder 1.0 ended up with an error without this override */
	#define __has_builtin(x) 0
#endif

/*
#if !defined(__is_identifier)
	#define __is_identifier(x) 0
#endif

#if !defined(__has_attribute)
	#define __has_attribute(x) 0
#endif
*/


#if defined(__has_builtin) 
	#if __has_builtin(__builtin_ctz)
		#define HCL_HAVE_BUILTIN_CTZ
	#endif

	#if __has_builtin(__builtin_uadd_overflow)
		#define HCL_HAVE_BUILTIN_UADD_OVERFLOW 
	#endif
	#if __has_builtin(__builtin_uaddl_overflow)
		#define HCL_HAVE_BUILTIN_UADDL_OVERFLOW 
	#endif
	#if __has_builtin(__builtin_uaddll_overflow)
		#define HCL_HAVE_BUILTIN_UADDLL_OVERFLOW 
	#endif
	#if __has_builtin(__builtin_umul_overflow)
		#define HCL_HAVE_BUILTIN_UMUL_OVERFLOW 
	#endif
	#if __has_builtin(__builtin_umull_overflow)
		#define HCL_HAVE_BUILTIN_UMULL_OVERFLOW 
	#endif
	#if __has_builtin(__builtin_umulll_overflow)
		#define HCL_HAVE_BUILTIN_UMULLL_OVERFLOW 
	#endif

	#if __has_builtin(__builtin_sadd_overflow)
		#define HCL_HAVE_BUILTIN_SADD_OVERFLOW 
	#endif
	#if __has_builtin(__builtin_saddl_overflow)
		#define HCL_HAVE_BUILTIN_SADDL_OVERFLOW 
	#endif
	#if __has_builtin(__builtin_saddll_overflow)
		#define HCL_HAVE_BUILTIN_SADDLL_OVERFLOW 
	#endif
	#if __has_builtin(__builtin_smul_overflow)
		#define HCL_HAVE_BUILTIN_SMUL_OVERFLOW 
	#endif
	#if __has_builtin(__builtin_smull_overflow)
		#define HCL_HAVE_BUILTIN_SMULL_OVERFLOW 
	#endif
	#if __has_builtin(__builtin_smulll_overflow)
		#define HCL_HAVE_BUILTIN_SMULLL_OVERFLOW 
	#endif

	#if __has_builtin(__builtin_expect)
		#define HCL_HAVE_BUILTIN_EXPECT
	#endif
#elif defined(__GNUC__) && defined(__GNUC_MINOR__)

	#if (__GNUC__ >= 4) || (__GNUC__ == 3 && __GNUC_MINOR__ >= 4)
		#define HCL_HAVE_BUILTIN_CTZ
		#define HCL_HAVE_BUILTIN_EXPECT
	#endif

	#if (__GNUC__ >= 5)
		#define HCL_HAVE_BUILTIN_UADD_OVERFLOW
		#define HCL_HAVE_BUILTIN_UADDL_OVERFLOW
		#define HCL_HAVE_BUILTIN_UADDLL_OVERFLOW
		#define HCL_HAVE_BUILTIN_UMUL_OVERFLOW
		#define HCL_HAVE_BUILTIN_UMULL_OVERFLOW
		#define HCL_HAVE_BUILTIN_UMULLL_OVERFLOW

		#define HCL_HAVE_BUILTIN_SADD_OVERFLOW
		#define HCL_HAVE_BUILTIN_SADDL_OVERFLOW
		#define HCL_HAVE_BUILTIN_SADDLL_OVERFLOW
		#define HCL_HAVE_BUILTIN_SMUL_OVERFLOW
		#define HCL_HAVE_BUILTIN_SMULL_OVERFLOW
		#define HCL_HAVE_BUILTIN_SMULLL_OVERFLOW
	#endif

#endif


#if defined(HCL_HAVE_BUILTIN_EXPECT)
#	define HCL_LIKELY(x) (__builtin_expect(!!x,1))
#	define HCL_UNLIKELY(x) (__builtin_expect(!!x,0))
#else
#	define HCL_LIKELY(x) (x)
#	define HCL_UNLIKELY(x) (x)
#endif

#endif
