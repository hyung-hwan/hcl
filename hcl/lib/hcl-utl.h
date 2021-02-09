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

#ifndef _HCL_UTL_H_
#define _HCL_UTL_H_

#include "hcl-cmn.h"
#include <stdarg.h>

/* =========================================================================
 * DOUBLY LINKED LIST
 * ========================================================================= */
#define HCL_APPEND_TO_LIST(list, node) do { \
	(node)->next = HCL_NULL; \
	(node)->prev = (list)->last; \
	if ((list)->first) (list)->last->next = (node); \
	else (list)->first = (node); \
	(list)->last = (node); \
} while(0)

#define HCL_PREPPEND_TO_LIST(list, node) do { \
	(node)->prev = HCL_NULL; \
	(node)->next = (list)->first; \
	if ((list)->last) (list)->first->prev = (node); \
	else (list)->last = (node); \
	(list)->first = (node); \
} while(0)

#define HCL_DELETE_FROM_LIST(list, node) do { \
	if ((node)->prev) (node)->prev->next = (node)->next; \
	else (list)->first = (node)->next; \
	if ((node)->next) (node)->next->prev = (node)->prev; \
	else (list)->last = (node)->prev; \
} while(0)



#define HCL_APPEND_TO_OOP_LIST(hcl, list, node_type, node, _link) do { \
	(node)->_link.next = (node_type)(hcl)->_nil; \
	(node)->_link.prev = (list)->last; \
	if ((hcl_oop_t)(list)->last != (hcl)->_nil) (list)->last->_link.next = (node); \
	else (list)->first = (node); \
	(list)->last = (node); \
} while(0)

#define HCL_PREPPEND_TO_OOP_LIST(hcl, list, node_type, node, _link) do { \
	(node)->_link.prev = (node_type)(hcl)->_nil; \
	(node)->_link.next = (list)->first; \
	if ((hcl_oop_t)(list)->first != (hcl)->_nil) (list)->first->_link.prev = (node); \
	else (list)->last = (node); \
	(list)->first = (node); \
} while(0)

#define HCL_DELETE_FROM_OOP_LIST(hcl, list, node, _link) do { \
	if ((hcl_oop_t)(node)->_link.prev != (hcl)->_nil) (node)->_link.prev->_link.next = (node)->_link.next; \
	else (list)->first = (node)->_link.next; \
	if ((hcl_oop_t)(node)->_link.next != (hcl)->_nil) (node)->_link.next->_link.prev = (node)->_link.prev; \
	else (list)->last = (node)->_link.prev; \
} while(0)

/*
#define HCL_CLEANUP_FROM_OOP_LIST(hcl, list, node, _link) do { \
	HCL_DELETE_FROM_OOP_LIST (hcl, list, node, _link); \
	(node)->_link.prev = (node_type)(hcl)->_nil; \
	(node)->_link.next = (node_type)(hcl)->_nil; \
} while(0);
*/

/* =========================================================================
 * ENDIAN CHANGE OF A CONSTANT
 * ========================================================================= */
#define HCL_CONST_BSWAP16(x) \
	((hcl_uint16_t)((((hcl_uint16_t)(x) & ((hcl_uint16_t)0xff << 0)) << 8) | \
	                (((hcl_uint16_t)(x) & ((hcl_uint16_t)0xff << 8)) >> 8)))

#define HCL_CONST_BSWAP32(x) \
	((hcl_uint32_t)((((hcl_uint32_t)(x) & ((hcl_uint32_t)0xff <<  0)) << 24) | \
	                (((hcl_uint32_t)(x) & ((hcl_uint32_t)0xff <<  8)) <<  8) | \
	                (((hcl_uint32_t)(x) & ((hcl_uint32_t)0xff << 16)) >>  8) | \
	                (((hcl_uint32_t)(x) & ((hcl_uint32_t)0xff << 24)) >> 24)))

#if defined(HCL_HAVE_UINT64_T)
#define HCL_CONST_BSWAP64(x) \
	((hcl_uint64_t)((((hcl_uint64_t)(x) & ((hcl_uint64_t)0xff <<  0)) << 56) | \
	                (((hcl_uint64_t)(x) & ((hcl_uint64_t)0xff <<  8)) << 40) | \
	                (((hcl_uint64_t)(x) & ((hcl_uint64_t)0xff << 16)) << 24) | \
	                (((hcl_uint64_t)(x) & ((hcl_uint64_t)0xff << 24)) <<  8) | \
	                (((hcl_uint64_t)(x) & ((hcl_uint64_t)0xff << 32)) >>  8) | \
	                (((hcl_uint64_t)(x) & ((hcl_uint64_t)0xff << 40)) >> 24) | \
	                (((hcl_uint64_t)(x) & ((hcl_uint64_t)0xff << 48)) >> 40) | \
	                (((hcl_uint64_t)(x) & ((hcl_uint64_t)0xff << 56)) >> 56)))
#endif

#if defined(HCL_HAVE_UINT128_T)
#define HCL_CONST_BSWAP128(x) \
	((hcl_uint128_t)((((hcl_uint128_t)(x) & ((hcl_uint128_t)0xff << 0)) << 120) | \
	                 (((hcl_uint128_t)(x) & ((hcl_uint128_t)0xff << 8)) << 104) | \
	                 (((hcl_uint128_t)(x) & ((hcl_uint128_t)0xff << 16)) << 88) | \
	                 (((hcl_uint128_t)(x) & ((hcl_uint128_t)0xff << 24)) << 72) | \
	                 (((hcl_uint128_t)(x) & ((hcl_uint128_t)0xff << 32)) << 56) | \
	                 (((hcl_uint128_t)(x) & ((hcl_uint128_t)0xff << 40)) << 40) | \
	                 (((hcl_uint128_t)(x) & ((hcl_uint128_t)0xff << 48)) << 24) | \
	                 (((hcl_uint128_t)(x) & ((hcl_uint128_t)0xff << 56)) << 8) | \
	                 (((hcl_uint128_t)(x) & ((hcl_uint128_t)0xff << 64)) >> 8) | \
	                 (((hcl_uint128_t)(x) & ((hcl_uint128_t)0xff << 72)) >> 24) | \
	                 (((hcl_uint128_t)(x) & ((hcl_uint128_t)0xff << 80)) >> 40) | \
	                 (((hcl_uint128_t)(x) & ((hcl_uint128_t)0xff << 88)) >> 56) | \
	                 (((hcl_uint128_t)(x) & ((hcl_uint128_t)0xff << 96)) >> 72) | \
	                 (((hcl_uint128_t)(x) & ((hcl_uint128_t)0xff << 104)) >> 88) | \
	                 (((hcl_uint128_t)(x) & ((hcl_uint128_t)0xff << 112)) >> 104) | \
	                 (((hcl_uint128_t)(x) & ((hcl_uint128_t)0xff << 120)) >> 120)))
#endif

#if defined(HCL_ENDIAN_LITTLE)

#	if defined(HCL_HAVE_UINT16_T)
#	define HCL_CONST_NTOH16(x) HCL_CONST_BSWAP16(x)
#	define HCL_CONST_HTON16(x) HCL_CONST_BSWAP16(x)
#	define HCL_CONST_HTOBE16(x) HCL_CONST_BSWAP16(x)
#	define HCL_CONST_HTOLE16(x) (x)
#	define HCL_CONST_BE16TOH(x) HCL_CONST_BSWAP16(x)
#	define HCL_CONST_LE16TOH(x) (x)
#	endif

#	if defined(HCL_HAVE_UINT32_T)
#	define HCL_CONST_NTOH32(x) HCL_CONST_BSWAP32(x)
#	define HCL_CONST_HTON32(x) HCL_CONST_BSWAP32(x)
#	define HCL_CONST_HTOBE32(x) HCL_CONST_BSWAP32(x)
#	define HCL_CONST_HTOLE32(x) (x)
#	define HCL_CONST_BE32TOH(x) HCL_CONST_BSWAP32(x)
#	define HCL_CONST_LE32TOH(x) (x)
#	endif

#	if defined(HCL_HAVE_UINT64_T)
#	define HCL_CONST_NTOH64(x) HCL_CONST_BSWAP64(x)
#	define HCL_CONST_HTON64(x) HCL_CONST_BSWAP64(x)
#	define HCL_CONST_HTOBE64(x) HCL_CONST_BSWAP64(x)
#	define HCL_CONST_HTOLE64(x) (x)
#	define HCL_CONST_BE64TOH(x) HCL_CONST_BSWAP64(x)
#	define HCL_CONST_LE64TOH(x) (x)
#	endif

#	if defined(HCL_HAVE_UINT128_T)
#	define HCL_CONST_NTOH128(x) HCL_CONST_BSWAP128(x)
#	define HCL_CONST_HTON128(x) HCL_CONST_BSWAP128(x)
#	define HCL_CONST_HTOBE128(x) HCL_CONST_BSWAP128(x)
#	define HCL_CONST_HTOLE128(x) (x)
#	define HCL_CONST_BE128TOH(x) HCL_CONST_BSWAP128(x)
#	define HCL_CONST_LE128TOH(x) (x)
#endif

#elif defined(HCL_ENDIAN_BIG)

#	if defined(HCL_HAVE_UINT16_T)
#	define HCL_CONST_NTOH16(x) (x)
#	define HCL_CONST_HTON16(x) (x)
#	define HCL_CONST_HTOBE16(x) (x)
#	define HCL_CONST_HTOLE16(x) HCL_CONST_BSWAP16(x)
#	define HCL_CONST_BE16TOH(x) (x)
#	define HCL_CONST_LE16TOH(x) HCL_CONST_BSWAP16(x)
#	endif

#	if defined(HCL_HAVE_UINT32_T)
#	define HCL_CONST_NTOH32(x) (x)
#	define HCL_CONST_HTON32(x) (x)
#	define HCL_CONST_HTOBE32(x) (x)
#	define HCL_CONST_HTOLE32(x) HCL_CONST_BSWAP32(x)
#	define HCL_CONST_BE32TOH(x) (x)
#	define HCL_CONST_LE32TOH(x) HCL_CONST_BSWAP32(x)
#	endif

#	if defined(HCL_HAVE_UINT64_T)
#	define HCL_CONST_NTOH64(x) (x)
#	define HCL_CONST_HTON64(x) (x)
#	define HCL_CONST_HTOBE64(x) (x)
#	define HCL_CONST_HTOLE64(x) HCL_CONST_BSWAP64(x)
#	define HCL_CONST_BE64TOH(x) (x)
#	define HCL_CONST_LE64TOH(x) HCL_CONST_BSWAP64(x)
#	endif

#	if defined(HCL_HAVE_UINT128_T)
#	define HCL_CONST_NTOH128(x) (x)
#	define HCL_CONST_HTON128(x) (x)
#	define HCL_CONST_HTOBE128(x) (x)
#	define HCL_CONST_HTOLE128(x) HCL_CONST_BSWAP128(x)
#	define HCL_CONST_BE128TOH(x) (x)
#	define HCL_CONST_LE128TOH(x) HCL_CONST_BSWAP128(x)
#	endif

#else
#	error UNKNOWN ENDIAN
#endif


/* =========================================================================
 * HASH
 * ========================================================================= */

#if (HCL_SIZEOF_OOW_T == 4)
#	define HCL_HASH_FNV_MAGIC_INIT (0x811c9dc5)
#	define HCL_HASH_FNV_MAGIC_PRIME (0x01000193)
#elif (HCL_SIZEOF_OOW_T == 8)

#	define HCL_HASH_FNV_MAGIC_INIT (0xCBF29CE484222325)
#	define HCL_HASH_FNV_MAGIC_PRIME (0x100000001B3l)

#elif (HCL_SIZEOF_OOW_T == 16)
#	define HCL_HASH_FNV_MAGIC_INIT (0x6C62272E07BB014262B821756295C58D)
#	define HCL_HASH_FNV_MAGIC_PRIME (0x1000000000000000000013B)
#endif

#if defined(HCL_HASH_FNV_MAGIC_INIT)
	/* FNV-1 hash */
#	define HCL_HASH_INIT HCL_HASH_FNV_MAGIC_INIT
#	define HCL_HASH_VALUE(hv,v) (((hv) ^ (v)) * HCL_HASH_FNV_MAGIC_PRIME)

#else
	/* SDBM hash */
#	define HCL_HASH_INIT 0
#	define HCL_HASH_VALUE(hv,v) (((hv) << 6) + ((hv) << 16) - (hv) + (v))
#endif

#define HCL_HASH_VPTL(hv, ptr, len, type) do { \
	hv = HCL_HASH_INIT; \
	HCL_HASH_MORE_VPTL (hv, ptr, len, type); \
} while(0)

#define HCL_HASH_MORE_VPTL(hv, ptr, len, type) do { \
	type* __hcl_hash_more_vptl_p = (type*)(ptr); \
	type* __hcl_hash_more_vptl_q = (type*)__hcl_hash_more_vptl_p + (len); \
	while (__hcl_hash_more_vptl_p < __hcl_hash_more_vptl_q) \
	{ \
		hv = HCL_HASH_VALUE(hv, *__hcl_hash_more_vptl_p); \
		__hcl_hash_more_vptl_p++; \
	} \
} while(0)

#define HCL_HASH_VPTR(hv, ptr, type) do { \
	hv = HCL_HASH_INIT; \
	HCL_HASH_MORE_VPTR (hv, ptr, type); \
} while(0)

#define HCL_HASH_MORE_VPTR(hv, ptr, type) do { \
	type* __hcl_hash_more_vptr_p = (type*)(ptr); \
	while (*__hcl_hash_more_vptr_p) \
	{ \
		hv = HCL_HASH_VALUE(hv, *__hcl_hash_more_vptr_p); \
		__hcl_hash_more_vptr_p++; \
	} \
} while(0)

#define HCL_HASH_BYTES(hv, ptr, len) HCL_HASH_VPTL(hv, ptr, len, const hcl_uint8_t)
#define HCL_HASH_MORE_BYTES(hv, ptr, len) HCL_HASH_MORE_VPTL(hv, ptr, len, const hcl_uint8_t)

#define HCL_HASH_BCHARS(hv, ptr, len) HCL_HASH_VPTL(hv, ptr, len, const hcl_bch_t)
#define HCL_HASH_MORE_BCHARS(hv, ptr, len) HCL_HASH_MORE_VPTL(hv, ptr, len, const hcl_bch_t)

#define HCL_HASH_UCHARS(hv, ptr, len) HCL_HASH_VPTL(hv, ptr, len, const hcl_uch_t)
#define HCL_HASH_MORE_UCHARS(hv, ptr, len) HCL_HASH_MORE_VPTL(hv, ptr, len, const hcl_uch_t)

#define HCL_HASH_BCSTR(hv, ptr) HCL_HASH_VPTR(hv, ptr, const hcl_bch_t)
#define HCL_HASH_MORE_BCSTR(hv, ptr) HCL_HASH_MORE_VPTR(hv, ptr, const hcl_bch_t)

#define HCL_HASH_UCSTR(hv, ptr) HCL_HASH_VPTR(hv, ptr, const hcl_uch_t)
#define HCL_HASH_MORE_UCSTR(hv, ptr) HCL_HASH_MORE_VPTR(hv, ptr, const hcl_uch_t)


/* =========================================================================
 * PATH-RELATED MACROS
 * ========================================================================= */
#if defined(_WIN32) || defined(__OS2__) || defined(__DOS__)
#	define HCL_IS_PATH_SEP(c) ((c) == '/' || (c) == '\\')
#	define HCL_DFL_PATH_SEP ('\\')
#else
#	define HCL_IS_PATH_SEP(c) ((c) == '/')
#	define HCL_DFL_PATH_SEP ('/')
#endif

/* TODO: handle path with a drive letter or in the UNC notation */
#define HCL_IS_PATH_ABSOLUTE(x) HCL_IS_PATH_SEP(x[0])


#if defined(__cplusplus)
extern "C" {
#endif

HCL_EXPORT hcl_oow_t hcl_hash_bytes_ (
	const hcl_oob_t* ptr,
	hcl_oow_t        len
);

#if defined(HCL_HAVE_INLINE)
	static HCL_INLINE hcl_oow_t hcl_hash_bytes (const hcl_oob_t* ptr, hcl_oow_t len)
	{
		hcl_oow_t hv;
		HCL_HASH_BYTES (hv, ptr, len);
		/* constrain the hash value to be representable in a small integer
		 * for convenience sake */
		return hv % ((hcl_oow_t)HCL_SMOOI_MAX + 1);
	}

	static HCL_INLINE hcl_oow_t hcl_hash_bchars (const hcl_bch_t* ptr, hcl_oow_t len)
	{
		return hcl_hash_bytes((const hcl_oob_t*)ptr, len * HCL_SIZEOF(hcl_bch_t));
	}

	static HCL_INLINE hcl_oow_t hcl_hash_uchars (const hcl_uch_t* ptr, hcl_oow_t len)
	{
		return hcl_hash_bytes((const hcl_oob_t*)ptr, len * HCL_SIZEOF(hcl_uch_t));
	}

	static HCL_INLINE hcl_oow_t hcl_hash_words (const hcl_oow_t* ptr, hcl_oow_t len)
	{
		return hcl_hash_bytes((const hcl_oob_t*)ptr, len * HCL_SIZEOF(hcl_oow_t));
	}

	static HCL_INLINE hcl_oow_t hcl_hash_halfwords (const hcl_oohw_t* ptr, hcl_oow_t len)
	{
		return hcl_hash_bytes((const hcl_oob_t*)ptr, len * HCL_SIZEOF(hcl_oohw_t));
	}
#else
#	define hcl_hash_bytes(ptr,len)     hcl_hash_bytes_(ptr, len)
#	define hcl_hash_bchars(ptr,len)    hcl_hash_bytes((const hcl_oob_t*)ptr, len * HCL_SIZEOF(hcl_bch_t))
#	define hcl_hash_uchars(ptr,len)    hcl_hash_bytes((const hcl_oob_t*)ptr, len * HCL_SIZEOF(hcl_uch_t))
#	define hcl_hash_words(ptr,len)     hcl_hash_bytes((const hcl_oob_t*)ptr, len * HCL_SIZEOF(hcl_oow_t))
#	define hcl_hash_halfwords(ptr,len) hcl_hash_bytes((const hcl_oob_t*)ptr, len * HCL_SIZEOF(hcl_oohw_t))
#endif

#if defined(HCL_OOCH_IS_UCH)
#	define hcl_hash_oochars(ptr,len) hcl_hash_uchars(ptr,len)
#else
#	define hcl_hash_oochars(ptr,len) hcl_hash_bchars(ptr,len)
#endif

/**
 * The hcl_equal_uchars() function determines equality of two strings
 * of the same length \a len.
 */
HCL_EXPORT int hcl_equal_uchars (
	const hcl_uch_t* str1,
	const hcl_uch_t* str2,
	hcl_oow_t        len
);

HCL_EXPORT int hcl_equal_bchars (
	const hcl_bch_t* str1,
	const hcl_bch_t* str2,
	hcl_oow_t        len
);

/* ------------------------------ */

HCL_EXPORT int hcl_comp_uchars (
	const hcl_uch_t* str1,
	hcl_oow_t        len1,
	const hcl_uch_t* str2,
	hcl_oow_t        len2
);

HCL_EXPORT int hcl_comp_bchars (
	const hcl_bch_t* str1,
	hcl_oow_t        len1,
	const hcl_bch_t* str2,
	hcl_oow_t        len2
);

HCL_EXPORT int hcl_comp_ucstr (
	const hcl_uch_t* str1,
	const hcl_uch_t* str2
);

HCL_EXPORT int hcl_comp_bcstr (
	const hcl_bch_t* str1,
	const hcl_bch_t* str2
);

HCL_EXPORT int hcl_comp_ucstr_bcstr (
	const hcl_uch_t* str1,
	const hcl_bch_t* str2
);

HCL_EXPORT int hcl_comp_uchars_ucstr (
	const hcl_uch_t* str1,
	hcl_oow_t        len,
	const hcl_uch_t* str2
);

HCL_EXPORT int hcl_comp_uchars_bcstr (
	const hcl_uch_t* str1,
	hcl_oow_t        len,
	const hcl_bch_t* str2
);

HCL_EXPORT int hcl_comp_bchars_bcstr (
	const hcl_bch_t* str1,
	hcl_oow_t        len,
	const hcl_bch_t* str2
);

HCL_EXPORT int hcl_comp_bchars_ucstr (
	const hcl_bch_t* str1,
	hcl_oow_t        len,
	const hcl_uch_t* str2
);

/* ------------------------------ */

HCL_EXPORT void hcl_copy_uchars (
	hcl_uch_t*       dst,
	const hcl_uch_t* src,
	hcl_oow_t        len
);

HCL_EXPORT void hcl_copy_bchars (
	hcl_bch_t*       dst,
	const hcl_bch_t* src,
	hcl_oow_t        len
);

HCL_EXPORT void hcl_copy_bchars_to_uchars (
	hcl_uch_t*       dst,
	const hcl_bch_t* src,
	hcl_oow_t        len
);

HCL_EXPORT void hcl_copy_uchars_to_bchars (
	hcl_bch_t*       dst,
	const hcl_uch_t* src,
	hcl_oow_t        len
);

HCL_EXPORT hcl_oow_t hcl_copy_uchars_to_ucstr_unlimited (
	hcl_uch_t*       dst,
	const hcl_uch_t* src,
	hcl_oow_t        len
);

HCL_EXPORT hcl_oow_t hcl_copy_bchars_to_bcstr_unlimited (
	hcl_bch_t*       dst,
	const hcl_bch_t* src,
	hcl_oow_t        len
);

HCL_EXPORT hcl_oow_t hcl_copy_ucstr (
	hcl_uch_t*       dst,
	hcl_oow_t        len,
	const hcl_uch_t* src
);

HCL_EXPORT hcl_oow_t hcl_copy_bcstr (
	hcl_bch_t*       dst,
	hcl_oow_t        len,
	const hcl_bch_t* src
);

HCL_EXPORT hcl_oow_t hcl_copy_uchars_to_ucstr (
	hcl_uch_t*       dst,
	hcl_oow_t        dlen,
	const hcl_uch_t* src,
	hcl_oow_t        slen
);

HCL_EXPORT hcl_oow_t hcl_copy_bchars_to_bcstr (
	hcl_bch_t*       dst,
	hcl_oow_t        dlen,
	const hcl_bch_t* src,
	hcl_oow_t        slen
);

HCL_EXPORT hcl_oow_t hcl_copy_ucstr_unlimited (
	hcl_uch_t*       dst,
	const hcl_uch_t* src
);

HCL_EXPORT hcl_oow_t hcl_copy_bcstr_unlimited (
	hcl_bch_t*       dst,
	const hcl_bch_t* src
);

/* ------------------------------ */

HCL_EXPORT void hcl_fill_uchars (
	hcl_uch_t*       dst,
	const hcl_uch_t  ch,
	hcl_oow_t        len
);

HCL_EXPORT void hcl_fill_bchars (
	hcl_bch_t*       dst,
	const hcl_bch_t  ch,
	hcl_oow_t        len
);

HCL_EXPORT hcl_uch_t* hcl_find_uchar (
	const hcl_uch_t* ptr,
	hcl_oow_t        len,
	hcl_uch_t        c
);

HCL_EXPORT hcl_bch_t* hcl_find_bchar (
	const hcl_bch_t* ptr,
	hcl_oow_t        len,
	hcl_bch_t        c
);

HCL_EXPORT hcl_uch_t* hcl_rfind_uchar (
	const hcl_uch_t* ptr,
	hcl_oow_t        len,
	hcl_uch_t        c
);

HCL_EXPORT hcl_bch_t* hcl_rfind_bchar (
	const hcl_bch_t* ptr,
	hcl_oow_t        len,
	hcl_bch_t        c
);

HCL_EXPORT hcl_uch_t* hcl_find_uchar_in_ucstr (
	const hcl_uch_t* ptr,
	hcl_uch_t        c
);

HCL_EXPORT hcl_bch_t* hcl_find_bchar_in_bcstr (
	const hcl_bch_t* ptr,
	hcl_bch_t        c
);

HCL_EXPORT hcl_oow_t hcl_count_ucstr (
	const hcl_uch_t* str
);

HCL_EXPORT hcl_oow_t hcl_count_bcstr (
	const hcl_bch_t* str
);

#if defined(HCL_OOCH_IS_UCH)
#	define hcl_equal_oochars(str1,str2,len) hcl_equal_uchars(str1,str2,len)
#	define hcl_comp_oochars(str1,len1,str2,len2) hcl_comp_uchars(str1,len1,str2,len2)
#	define hcl_comp_oocstr_bcstr(str1,str2) hcl_comp_ucstr_bcstr(str1,str2)
#	define hcl_comp_oochars_bcstr(str1,len1,str2) hcl_comp_uchars_bcstr(str1,len1,str2)
#	define hcl_comp_oochars_ucstr(str1,len1,str2) hcl_comp_uchars_ucstr(str1,len1,str2)
#	define hcl_comp_oochars_oocstr(str1,len1,str2) hcl_comp_uchars_ucstr(str1,len1,str2)
#	define hcl_comp_oocstr(str1,str2) hcl_comp_ucstr(str1,str2)

#	define hcl_copy_oochars(dst,src,len) hcl_copy_uchars(dst,src,len)
#	define hcl_copy_bchars_to_oochars(dst,src,len) hcl_copy_bchars_to_uchars(dst,src,len)
#	define hcl_copy_oochars_to_bchars(dst,src,len) hcl_copy_uchars_to_bchars(dst,src,len)
#	define hcl_copy_uchars_to_oochars(dst,src,len) hcl_copy_uchars(dst,src,len)
#	define hcl_copy_oochars_to_uchars(dst,src,len) hcl_copy_uchars(dst,src,len)

#	define hcl_copy_oochars_to_oocstr(dst,dlen,src,slen) hcl_copy_uchars_to_ucstr(dst,dlen,src,slen)
#	define hcl_copy_oochars_to_oocstr_unlimited(dst,src,len) hcl_copy_uchars_to_ucstr_unlimited(dst,src,len)
#	define hcl_copy_oocstr(dst,len,src) hcl_copy_ucstr(dst,len,src)
#	define hcl_copy_oocstr_unlimited(dst,src) hcl_copy_ucstr_unlimited(dst,src)

#	define hcl_fill_oochars(dst,ch,len) hcl_fill_uchars(dst,ch,len)
#	define hcl_find_oochar(ptr,len,c) hcl_find_uchar(ptr,len,c)
#	define hcl_rfind_oochar(ptr,len,c) hcl_rfind_uchar(ptr,len,c)
#	define hcl_find_oochar_in_oocstr(ptr,c) hcl_find_uchar_in_ucstr(ptr,c)
#	define hcl_count_oocstr(str) hcl_count_ucstr(str)
#else
#	define hcl_equal_oochars(str1,str2,len) hcl_equal_bchars(str1,str2,len)
#	define hcl_comp_oochars(str1,len1,str2,len2) hcl_comp_bchars(str1,len1,str2,len2)
#	define hcl_comp_oocstr_bcstr(str1,str2) hcl_comp_bcstr(str1,str2)
#	define hcl_comp_oochars_bcstr(str1,len1,str2) hcl_comp_bchars_bcstr(str1,len1,str2)
#	define hcl_comp_oochars_ucstr(str1,len1,str2) hcl_comp_bchars_ucstr(str1,len1,str2)
#	define hcl_comp_oochars_oocstr(str1,len1,str2) hcl_comp_bchars_bcstr(str1,len1,str2)
#	define hcl_comp_oocstr(str1,str2) hcl_comp_bcstr(str1,str2)

#	define hcl_copy_oochars(dst,src,len) hcl_copy_bchars(dst,src,len)
#	define hcl_copy_bchars_to_oochars(dst,src,len) hcl_copy_bchars(dst,src,len)
#	define hcl_copy_oochars_to_bchars(dst,src,len) hcl_copy_bchars(dst,src,len)
#	define hcl_copy_uchars_to_oochars(dst,src,len) hcl_copy_uchars_to_bchars(dst,src,len)
#	define hcl_copy_oochars_to_uchars(dst,src,len) hcl_copy_bchars_to_uchars(dst,src,len)

#	define hcl_copy_oochars_to_oocstr(dst,dlen,src,slen) hcl_copy_bchars_to_bcstr(dst,dlen,src,slen)
#	define hcl_copy_oochars_to_oocstr_unlimited(dst,src,len) hcl_copy_bchars_to_bcstr_unlimited(dst,src,len)
#	define hcl_copy_oocstr(dst,len,src) hcl_copy_bcstr(dst,len,src)
#	define hcl_copy_oocstr_unlimited(dst,src) hcl_copy_bcstr_unlimited(dst,src)

#	define hcl_fill_oochars(dst,ch,len) hcl_fill_bchars(dst,ch,len)
#	define hcl_find_oochar(ptr,len,c) hcl_find_bchar(ptr,len,c)
#	define hcl_rfind_oochar(ptr,len,c) hcl_rfind_bchar(ptr,len,c)
#	define hcl_find_oochar_in_oocstr(ptr,c) hcl_find_bchar_in_bcstr(ptr,c)
#	define hcl_count_oocstr(str) hcl_count_bcstr(str)
#endif

#define HCL_BYTE_TO_BCSTR_RADIXMASK (0xFF)
#define HCL_BYTE_TO_BCSTR_LOWERCASE (1 << 8)

hcl_oow_t hcl_byte_to_bcstr (
	hcl_uint8_t   byte,  
	hcl_bch_t*    buf,
	hcl_oow_t     size,
	int           flagged_radix,
	hcl_bch_t     fill
);


HCL_EXPORT int hcl_conv_bcstr_to_ucstr_with_cmgr (
	const hcl_bch_t* bcs,
	hcl_oow_t*       bcslen,
	hcl_uch_t*       ucs,
	hcl_oow_t*       ucslen,
	hcl_cmgr_t*      cmgr,
	int              all
);
	
HCL_EXPORT int hcl_conv_bchars_to_uchars_with_cmgr (
	const hcl_bch_t* bcs,
	hcl_oow_t*       bcslen,
	hcl_uch_t*       ucs,
	hcl_oow_t*       ucslen,
	hcl_cmgr_t*      cmgr,
	int              all
);

HCL_EXPORT int hcl_conv_ucstr_to_bcstr_with_cmgr (
	const hcl_uch_t* ucs,
	hcl_oow_t*       ucslen,
	hcl_bch_t*       bcs,
	hcl_oow_t*       bcslen,
	hcl_cmgr_t*      cmgr
);	

HCL_EXPORT int hcl_conv_uchars_to_bchars_with_cmgr (
	const hcl_uch_t* ucs,
	hcl_oow_t*       ucslen,
	hcl_bch_t*       bcs,
	hcl_oow_t*       bcslen,
	hcl_cmgr_t*      cmgr
);

#if defined(HCL_OOCH_IS_UCH)
#	define hcl_conv_oocstr_to_bcstr_with_cmgr(oocs,oocslen,bcs,bcslen,cmgr) hcl_conv_ucstr_to_bcstr_with_cmgr(oocs,oocslen,bcs,bcslen,cmgr)
#	define hcl_conv_oochars_to_bchars_with_cmgr(oocs,oocslen,bcs,bcslen,cmgr) hcl_conv_uchars_to_bchars_with_cmgr(oocs,oocslen,bcs,bcslen,cmgr)
#else
#	define hcl_conv_oocstr_to_ucstr_with_cmgr(oocs,oocslen,ucs,ucslen,cmgr) hcl_conv_bcstr_to_ucstr_with_cmgr(oocs,oocslen,ucs,ucslen,cmgr,0)
#	define hcl_conv_oochars_to_uchars_with_cmgr(oocs,oocslen,ucs,ucslen,cmgr) hcl_conv_bchars_to_uchars_with_cmgr(oocs,oocslen,ucs,ucslen,cmgr,0)
#endif

HCL_EXPORT hcl_cmgr_t* hcl_get_utf8_cmgr (
	void
);

/**
 * The hcl_conv_uchars_to_utf8() function converts a unicode character string \a ucs 
 * to a UTF8 string and writes it into the buffer pointed to by \a bcs, but
 * not more than \a bcslen bytes including the terminating null.
 *
 * Upon return, \a bcslen is modified to the actual number of bytes written to
 * \a bcs excluding the terminating null; \a ucslen is modified to the number of
 * wide characters converted.
 *
 * You may pass #HCL_NULL for \a bcs to dry-run conversion or to get the
 * required buffer size for conversion. -2 is never returned in this case.
 *
 * \return
 * - 0 on full conversion,
 * - -1 on no or partial conversion for an illegal character encountered,
 * - -2 on no or partial conversion for a small buffer.
 *
 * \code
 *   const hcl_uch_t ucs[] = { 'H', 'e', 'l', 'l', 'o' };
 *   hcl_bch_t bcs[10];
 *   hcl_oow_t ucslen = 5;
 *   hcl_oow_t bcslen = HCL_COUNTOF(bcs);
 *   n = hcl_conv_uchars_to_utf8 (ucs, &ucslen, bcs, &bcslen);
 *   if (n <= -1)
 *   {
 *      // conversion error
 *   }
 * \endcode
 */
HCL_EXPORT int hcl_conv_uchars_to_utf8 (
	const hcl_uch_t*    ucs,
	hcl_oow_t*          ucslen,
	hcl_bch_t*          bcs,
	hcl_oow_t*          bcslen
);

/**
 * The hcl_conv_utf8_to_uchars() function converts a UTF8 string to a uncide string.
 *
 * It never returns -2 if \a ucs is #HCL_NULL.
 *
 * \code
 *  const hcl_bch_t* bcs = "test string";
 *  hcl_uch_t ucs[100];
 *  hcl_oow_t ucslen = HCL_COUNTOF(buf), n;
 *  hcl_oow_t bcslen = 11;
 *  int n;
 *  n = hcl_conv_utf8_to_uchars (bcs, &bcslen, ucs, &ucslen);
 *  if (n <= -1) { invalid/incomplenete sequence or buffer to small }
 * \endcode
 * 
 * The resulting \a ucslen can still be greater than 0 even if the return
 * value is negative. The value indiates the number of characters converted
 * before the error has occurred.
 *
 * \return 0 on success.
 *         -1 if \a bcs contains an illegal character.
 *         -2 if the wide-character string buffer is too small.
 *         -3 if \a bcs is not a complete sequence.
 */
HCL_EXPORT int hcl_conv_utf8_to_uchars (
	const hcl_bch_t*   bcs,
	hcl_oow_t*         bcslen,
	hcl_uch_t*         ucs,
	hcl_oow_t*         ucslen
);


HCL_EXPORT int hcl_conv_ucstr_to_utf8 (
	const hcl_uch_t*    ucs,
	hcl_oow_t*          ucslen,
	hcl_bch_t*          bcs,
	hcl_oow_t*          bcslen
);

HCL_EXPORT int hcl_conv_utf8_to_ucstr (
	const hcl_bch_t*   bcs,
	hcl_oow_t*         bcslen,
	hcl_uch_t*         ucs,
	hcl_oow_t*         ucslen
);


HCL_EXPORT hcl_oow_t hcl_uc_to_utf8 (
	hcl_uch_t    uc,
	hcl_bch_t*   utf8,
	hcl_oow_t    size
);

HCL_EXPORT hcl_oow_t hcl_utf8_to_uc (
	const hcl_bch_t* utf8,
	hcl_oow_t        size,
	hcl_uch_t*       uc
);

HCL_EXPORT int hcl_ucwidth (
	hcl_uch_t uc
);

/* =========================================================================
 * TIME CALCULATION WITH OVERFLOW/UNDERFLOW DETECTION
 * ========================================================================= */

/** 
 * The hcl_add_ntime() function adds two time structures pointed to by \a x and \a y
 * and stores the result in the structure pointed to by \a z. If it detects overflow/
 * underflow, it stores the largest/least possible value respectively.
 * You may use the HCL_ADD_NTIME() macro if overflow/underflow check isn't needed.
 */
HCL_EXPORT void hcl_add_ntime (
	hcl_ntime_t*       z, 
	const hcl_ntime_t* x,
	const hcl_ntime_t* y
);

/** 
 * The hcl_sub_ntime() function subtracts the time value \a y from the time value \a x
 * and stores the result in the structure pointed to by \a z. If it detects overflow/
 * underflow, it stores the largest/least possible value respectively.
 * You may use the HCL_SUB_NTIME() macro if overflow/underflow check isn't needed.
 */
HCL_EXPORT void hcl_sub_ntime (
	hcl_ntime_t*       z,
	const hcl_ntime_t* x,
	const hcl_ntime_t* y
);

/* =========================================================================
 * BIT SWAP
 * ========================================================================= */
#if defined(HCL_HAVE_INLINE)

#if defined(HCL_HAVE_UINT16_T)
static HCL_INLINE hcl_uint16_t hcl_bswap16 (hcl_uint16_t x)
{
#if defined(HCL_HAVE_BUILTIN_BSWAP16)
	return __builtin_bswap16(x);
#elif defined(__GNUC__) && (defined(__x86_64) || defined(__amd64) || defined(__i386) || defined(i386))
	__asm__ /*volatile*/ ("xchgb %b0, %h0" : "=Q"(x): "0"(x));
	return x;
#elif defined(__GNUC__) && defined(__arm__) && (defined(__ARM_ARCH) && (__ARM_ARCH >= 6))
	__asm__ /*volatile*/ ("rev16 %0, %0" : "+r"(x));
	return x;
#else
	return (x << 8) | (x >> 8);
#endif
}
#endif

#if defined(HCL_HAVE_UINT32_T)
static HCL_INLINE hcl_uint32_t hcl_bswap32 (hcl_uint32_t x)
{
#if defined(HCL_HAVE_BUILTIN_BSWAP32)
	return __builtin_bswap32(x);
#elif defined(__GNUC__) && (defined(__x86_64) || defined(__amd64) || defined(__i386) || defined(i386))
	__asm__ /*volatile*/ ("bswapl %0" : "=r"(x) : "0"(x));
	return x;
#elif defined(__GNUC__) && defined(__aarch64__)
	__asm__ /*volatile*/ ("rev32 %0, %0" : "+r"(x));
	return x;
#elif defined(__GNUC__) && defined(__arm__) && (defined(__ARM_ARCH) && (__ARM_ARCH >= 6))
	__asm__ /*volatile*/ ("rev %0, %0" : "+r"(x));
	return x;
#elif defined(__GNUC__) && defined(__ARM_ARCH)
	hcl_uint32_t tmp;
	__asm__ /*volatile*/ (
		"eor %1, %0, %0, ror #16\n\t"
		"bic %1, %1, #0x00ff0000\n\t"
		"mov %0, %0, ror #8\n\t"
		"eor %0, %0, %1, lsr #8\n\t"
		:"+r"(x), "=&r"(tmp)
	);
	return x;
#else
	return ((x >> 24)) | 
	       ((x >>  8) & ((hcl_uint32_t)0xff << 8)) | 
	       ((x <<  8) & ((hcl_uint32_t)0xff << 16)) | 
	       ((x << 24));
#endif
}
#endif

#if defined(HCL_HAVE_UINT64_T)
static HCL_INLINE hcl_uint64_t hcl_bswap64 (hcl_uint64_t x)
{
#if defined(HCL_HAVE_BUILTIN_BSWAP64)
	return __builtin_bswap64(x);
#elif defined(__GNUC__) && (defined(__x86_64) || defined(__amd64))
	__asm__ /*volatile*/ ("bswapq %0" : "=r"(x) : "0"(x));
	return x;
#elif defined(__GNUC__) && defined(__aarch64__)
	__asm__ /*volatile*/ ("rev %0, %0" : "+r"(x));
	return x;
#else
	return ((x >> 56)) | 
	       ((x >> 40) & ((hcl_uint64_t)0xff << 8)) | 
	       ((x >> 24) & ((hcl_uint64_t)0xff << 16)) | 
	       ((x >>  8) & ((hcl_uint64_t)0xff << 24)) | 
	       ((x <<  8) & ((hcl_uint64_t)0xff << 32)) | 
	       ((x << 24) & ((hcl_uint64_t)0xff << 40)) | 
	       ((x << 40) & ((hcl_uint64_t)0xff << 48)) | 
	       ((x << 56));
#endif
}
#endif

#if defined(HCL_HAVE_UINT128_T)
static HCL_INLINE hcl_uint128_t hcl_bswap128 (hcl_uint128_t x)
{
#if defined(HCL_HAVE_BUILTIN_BSWAP128)
	return __builtin_bswap128(x);
#else
	return ((x >> 120)) | 
	       ((x >> 104) & ((hcl_uint128_t)0xff << 8)) |
	       ((x >>  88) & ((hcl_uint128_t)0xff << 16)) |
	       ((x >>  72) & ((hcl_uint128_t)0xff << 24)) |
	       ((x >>  56) & ((hcl_uint128_t)0xff << 32)) |
	       ((x >>  40) & ((hcl_uint128_t)0xff << 40)) |
	       ((x >>  24) & ((hcl_uint128_t)0xff << 48)) |
	       ((x >>   8) & ((hcl_uint128_t)0xff << 56)) |
	       ((x <<   8) & ((hcl_uint128_t)0xff << 64)) |
	       ((x <<  24) & ((hcl_uint128_t)0xff << 72)) |
	       ((x <<  40) & ((hcl_uint128_t)0xff << 80)) |
	       ((x <<  56) & ((hcl_uint128_t)0xff << 88)) |
	       ((x <<  72) & ((hcl_uint128_t)0xff << 96)) |
	       ((x <<  88) & ((hcl_uint128_t)0xff << 104)) |
	       ((x << 104) & ((hcl_uint128_t)0xff << 112)) |
	       ((x << 120));
#endif
}
#endif

#else

#if defined(HCL_HAVE_UINT16_T)
#	if defined(HCL_HAVE_BUILTIN_BSWAP16)
#	define hcl_bswap16(x) ((hcl_uint16_t)__builtin_bswap16((hcl_uint16_t)(x)))
#	else 
#	define hcl_bswap16(x) ((hcl_uint16_t)(((hcl_uint16_t)(x)) << 8) | (((hcl_uint16_t)(x)) >> 8))
#	endif
#endif

#if defined(HCL_HAVE_UINT32_T)
#	if defined(HCL_HAVE_BUILTIN_BSWAP32)
#	define hcl_bswap32(x) ((hcl_uint32_t)__builtin_bswap32((hcl_uint32_t)(x)))
#	else 
#	define hcl_bswap32(x) ((hcl_uint32_t)(((((hcl_uint32_t)(x)) >> 24)) | \
	                                      ((((hcl_uint32_t)(x)) >>  8) & ((hcl_uint32_t)0xff << 8)) | \
	                                      ((((hcl_uint32_t)(x)) <<  8) & ((hcl_uint32_t)0xff << 16)) | \
	                                      ((((hcl_uint32_t)(x)) << 24))))
#	endif
#endif

#if defined(HCL_HAVE_UINT64_T)
#	if defined(HCL_HAVE_BUILTIN_BSWAP64)
#	define hcl_bswap64(x) ((hcl_uint64_t)__builtin_bswap64((hcl_uint64_t)(x)))
#	else 
#	define hcl_bswap64(x) ((hcl_uint64_t)(((((hcl_uint64_t)(x)) >> 56)) | \
	                                      ((((hcl_uint64_t)(x)) >> 40) & ((hcl_uint64_t)0xff << 8)) | \
	                                      ((((hcl_uint64_t)(x)) >> 24) & ((hcl_uint64_t)0xff << 16)) | \
	                                      ((((hcl_uint64_t)(x)) >>  8) & ((hcl_uint64_t)0xff << 24)) | \
	                                      ((((hcl_uint64_t)(x)) <<  8) & ((hcl_uint64_t)0xff << 32)) | \
	                                      ((((hcl_uint64_t)(x)) << 24) & ((hcl_uint64_t)0xff << 40)) | \
	                                      ((((hcl_uint64_t)(x)) << 40) & ((hcl_uint64_t)0xff << 48)) | \
	                                      ((((hcl_uint64_t)(x)) << 56))))
#	endif
#endif

#if defined(HCL_HAVE_UINT128_T)
#	if defined(HCL_HAVE_BUILTIN_BSWAP128)
#	define hcl_bswap128(x) ((hcl_uint128_t)__builtin_bswap128((hcl_uint128_t)(x)))
#	else 
#	define hcl_bswap128(x) ((hcl_uint128_t)(((((hcl_uint128_t)(x)) >> 120)) |  \
	                                        ((((hcl_uint128_t)(x)) >> 104) & ((hcl_uint128_t)0xff << 8)) | \
	                                        ((((hcl_uint128_t)(x)) >>  88) & ((hcl_uint128_t)0xff << 16)) | \
	                                        ((((hcl_uint128_t)(x)) >>  72) & ((hcl_uint128_t)0xff << 24)) | \
	                                        ((((hcl_uint128_t)(x)) >>  56) & ((hcl_uint128_t)0xff << 32)) | \
	                                        ((((hcl_uint128_t)(x)) >>  40) & ((hcl_uint128_t)0xff << 40)) | \
	                                        ((((hcl_uint128_t)(x)) >>  24) & ((hcl_uint128_t)0xff << 48)) | \
	                                        ((((hcl_uint128_t)(x)) >>   8) & ((hcl_uint128_t)0xff << 56)) | \
	                                        ((((hcl_uint128_t)(x)) <<   8) & ((hcl_uint128_t)0xff << 64)) | \
	                                        ((((hcl_uint128_t)(x)) <<  24) & ((hcl_uint128_t)0xff << 72)) | \
	                                        ((((hcl_uint128_t)(x)) <<  40) & ((hcl_uint128_t)0xff << 80)) | \
	                                        ((((hcl_uint128_t)(x)) <<  56) & ((hcl_uint128_t)0xff << 88)) | \
	                                        ((((hcl_uint128_t)(x)) <<  72) & ((hcl_uint128_t)0xff << 96)) | \
	                                        ((((hcl_uint128_t)(x)) <<  88) & ((hcl_uint128_t)0xff << 104)) | \
	                                        ((((hcl_uint128_t)(x)) << 104) & ((hcl_uint128_t)0xff << 112)) | \
	                                        ((((hcl_uint128_t)(x)) << 120))))
#	endif
#endif

#endif /* HCL_HAVE_INLINE */


#if defined(HCL_ENDIAN_LITTLE)

#	if defined(HCL_HAVE_UINT16_T)
#	define hcl_hton16(x) hcl_bswap16(x)
#	define hcl_ntoh16(x) hcl_bswap16(x)
#	define hcl_htobe16(x) hcl_bswap16(x)
#	define hcl_be16toh(x) hcl_bswap16(x)
#	define hcl_htole16(x) ((hcl_uint16_t)(x))
#	define hcl_le16toh(x) ((hcl_uint16_t)(x))
#	endif

#	if defined(HCL_HAVE_UINT32_T)
#	define hcl_hton32(x) hcl_bswap32(x)
#	define hcl_ntoh32(x) hcl_bswap32(x)
#	define hcl_htobe32(x) hcl_bswap32(x)
#	define hcl_be32toh(x) hcl_bswap32(x)
#	define hcl_htole32(x) ((hcl_uint32_t)(x))
#	define hcl_le32toh(x) ((hcl_uint32_t)(x))
#	endif

#	if defined(HCL_HAVE_UINT64_T)
#	define hcl_hton64(x) hcl_bswap64(x)
#	define hcl_ntoh64(x) hcl_bswap64(x)
#	define hcl_htobe64(x) hcl_bswap64(x)
#	define hcl_be64toh(x) hcl_bswap64(x)
#	define hcl_htole64(x) ((hcl_uint64_t)(x))
#	define hcl_le64toh(x) ((hcl_uint64_t)(x))
#	endif

#	if defined(HCL_HAVE_UINT128_T)

#	define hcl_hton128(x) hcl_bswap128(x)
#	define hcl_ntoh128(x) hcl_bswap128(x)
#	define hcl_htobe128(x) hcl_bswap128(x)
#	define hcl_be128toh(x) hcl_bswap128(x)
#	define hcl_htole128(x) ((hcl_uint128_t)(x))
#	define hcl_le128toh(x) ((hcl_uint128_t)(x))
#	endif

#elif defined(HCL_ENDIAN_BIG)

#	if defined(HCL_HAVE_UINT16_T)
#	define hcl_hton16(x) ((hcl_uint16_t)(x))
#	define hcl_ntoh16(x) ((hcl_uint16_t)(x))
#	define hcl_htobe16(x) ((hcl_uint16_t)(x))
#	define hcl_be16toh(x) ((hcl_uint16_t)(x))
#	define hcl_htole16(x) hcl_bswap16(x)
#	define hcl_le16toh(x) hcl_bswap16(x)
#	endif

#	if defined(HCL_HAVE_UINT32_T)
#	define hcl_hton32(x) ((hcl_uint32_t)(x))
#	define hcl_ntoh32(x) ((hcl_uint32_t)(x))
#	define hcl_htobe32(x) ((hcl_uint32_t)(x))
#	define hcl_be32toh(x) ((hcl_uint32_t)(x))
#	define hcl_htole32(x) hcl_bswap32(x)
#	define hcl_le32toh(x) hcl_bswap32(x)
#	endif

#	if defined(HCL_HAVE_UINT64_T)
#	define hcl_hton64(x) ((hcl_uint64_t)(x))
#	define hcl_ntoh64(x) ((hcl_uint64_t)(x))
#	define hcl_htobe64(x) ((hcl_uint64_t)(x))
#	define hcl_be64toh(x) ((hcl_uint64_t)(x))
#	define hcl_htole64(x) hcl_bswap64(x)
#	define hcl_le64toh(x) hcl_bswap64(x)
#	endif

#	if defined(HCL_HAVE_UINT128_T)
#	define hcl_hton128(x) ((hcl_uint128_t)(x))
#	define hcl_ntoh128(x) ((hcl_uint128_t)(x))
#	define hcl_htobe128(x) ((hcl_uint128_t)(x))
#	define hcl_be128toh(x) ((hcl_uint128_t)(x))
#	define hcl_htole128(x) hcl_bswap128(x)
#	define hcl_le128toh(x) hcl_bswap128(x)
#	endif

#else
#	error UNKNOWN ENDIAN
#endif

/* =========================================================================
 * BIT POSITION
 * ========================================================================= */
static HCL_INLINE int hcl_get_pos_of_msb_set_pow2 (hcl_oow_t x)
{
	/* the caller must ensure that x is power of 2. if x happens to be zero,
	 * the return value is undefined as each method used may give different result. */
#if defined(HCL_HAVE_BUILTIN_CTZLL) && (HCL_SIZEOF_OOW_T == HCL_SIZEOF_LONG_LONG)
	return __builtin_ctzll(x); /* count the number of trailing zeros */
#elif defined(HCL_HAVE_BUILTIN_CTZL) && (HCL_SIZEOF_OOW_T == HCL_SIZEOF_LONG)
	return __builtin_ctzl(x); /* count the number of trailing zeros */
#elif defined(HCL_HAVE_BUILTIN_CTZ) && (HCL_SIZEOF_OOW_T == HCL_SIZEOF_INT)
	return __builtin_ctz(x); /* count the number of trailing zeros */
#elif defined(__GNUC__) && (defined(__x86_64) || defined(__amd64) || defined(__i386) || defined(i386))
	hcl_oow_t pos;
	/* use the Bit Scan Forward instruction */
#if 1
	__asm__ volatile (
		"bsf %1,%0\n\t"
		: "=r"(pos) /* output */
		: "r"(x) /* input */
	);
#else
	__asm__ volatile (
		"bsf %[X],%[EXP]\n\t"
		: [EXP]"=r"(pos) /* output */
		: [X]"r"(x) /* input */
	);
#endif
	return (int)pos;
#elif defined(__GNUC__) && defined(__aarch64__) || (defined(__arm__) && (defined(__ARM_ARCH) && (__ARM_ARCH >= 5)))
	hcl_oow_t n;
	/* CLZ is available in ARMv5T and above. there is no instruction to
	 * count trailing zeros or something similar. using RBIT with CLZ
	 * would be good in ARMv6T2 and above to avoid further calculation
	 * afte CLZ */
	__asm__ volatile (
		"clz %0,%1\n\t"
		: "=r"(n) /* output */
		: "r"(x) /* input */
	);
	return (int)(HCL_OOW_BITS - n - 1); 
	/* TODO: PPC - use cntlz, cntlzw, cntlzd, SPARC - use lzcnt, MIPS clz */
#else
	int pos = 0;
	while (x >>= 1) pos++;
	return pos;
#endif
}

static HCL_INLINE int hcl_get_pos_of_msb_set (hcl_oow_t x)
{
	/* x doesn't have to be power of 2. if x is zero, the result is undefined */
#if defined(HCL_HAVE_BUILTIN_CLZLL) && (HCL_SIZEOF_OOW_T == HCL_SIZEOF_LONG_LONG)
	return HCL_OOW_BITS - __builtin_clzll(x) - 1; /* count the number of leading zeros */
#elif defined(HCL_HAVE_BUILTIN_CLZL) && (HCL_SIZEOF_OOW_T == HCL_SIZEOF_LONG)
	return HCL_OOW_BITS - __builtin_clzl(x) - 1; /* count the number of leading zeros */
#elif defined(HCL_HAVE_BUILTIN_CLZ) && (HCL_SIZEOF_OOW_T == HCL_SIZEOF_INT)
	return HCL_OOW_BITS - __builtin_clz(x) - 1; /* count the number of leading zeros */
#elif defined(__GNUC__) && (defined(__x86_64) || defined(__amd64) || defined(__i386) || defined(i386))
	/* bit scan reverse. not all x86 CPUs have LZCNT. */
	hcl_oow_t pos;
	__asm__ volatile (
		"bsr %1,%0\n\t"
		: "=r"(pos) /* output */
		: "r"(x) /* input */
	);
	return (int)pos;
#elif defined(__GNUC__) && defined(__aarch64__) || (defined(__arm__) && (defined(__ARM_ARCH) && (__ARM_ARCH >= 5)))
	hcl_oow_t n;
	__asm__ volatile (
		"clz %0,%1\n\t"
		: "=r"(n) /* output */
		: "r"(x) /* input */
	);
	return (int)(HCL_OOW_BITS - n - 1); 
	/* TODO: PPC - use cntlz, cntlzw, cntlzd, SPARC - use lzcnt, MIPS clz */
#else
	int pos = 0;
	while (x >>= 1) pos++;
	return pos;
#endif
}
#if defined(__cplusplus)
}
#endif


#endif
