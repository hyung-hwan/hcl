/*
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

#include <hcl-cmn.h>
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


#if defined(HCL_HAVE_UINT16_T) && (HCL_SIZEOF_UINT16_T == HCL_SIZEOF_OOW_T)
#	define HCL_CONST_NTOHOOW(x) HCL_CONST_NTOH16(x)
#	define HCL_CONST_HTONOOW(x) HCL_CONST_HTON16(x)
#	define HCL_CONST_HTOBEOOW(x) HCL_CONST_HTOBE16(x)
#	define HCL_CONST_HTOLEOOW(x) HCL_CONST_HTOLE16(x)
#	define HCL_CONST_BEOOWTOH(x) HCL_CONST_BE16TOH(x)
#	define HCL_CONST_LEOOWTOH(x) HCL_CONST_LE16TOH(x)
#elif defined(HCL_HAVE_UINT32_T) && (HCL_SIZEOF_UINT32_T == HCL_SIZEOF_OOW_T)
#	define HCL_CONST_NTOHOOW(x) HCL_CONST_NTOH32(x)
#	define HCL_CONST_HTONOOW(x) HCL_CONST_HTON32(x)
#	define HCL_CONST_HTOBEOOW(x) HCL_CONST_HTOBE32(x)
#	define HCL_CONST_HTOLEOOW(x) HCL_CONST_HTOLE32(x)
#	define HCL_CONST_BEOOWTOH(x) HCL_CONST_BE32TOH(x)
#	define HCL_CONST_LEOOWTOH(x) HCL_CONST_LE32TOH(x)
#elif defined(HCL_HAVE_UINT64_T) && (HCL_SIZEOF_UINT64_T == HCL_SIZEOF_OOW_T)
#	define HCL_CONST_NTOHOOW(x) HCL_CONST_NTOH64(x)
#	define HCL_CONST_HTONOOW(x) HCL_CONST_HTON64(x)
#	define HCL_CONST_HTOBEOOW(x) HCL_CONST_HTOBE64(x)
#	define HCL_CONST_HTOLEOOW(x) HCL_CONST_HTOLE64(x)
#	define HCL_CONST_BEOOWTOH(x) HCL_CONST_BE64TOH(x)
#	define HCL_CONST_LEOOWTOH(x) HCL_CONST_LE64TOH(x)
#elif defined(HCL_HAVE_UINT128_T) && (HCL_SIZEOF_UINT128_T == HCL_SIZEOF_OOW_T)
#	define HCL_CONST_NTOHOOW(x) HCL_CONST_NTOH128(x)
#	define HCL_CONST_HTONOOW(x) HCL_CONST_HTON128(x)
#	define HCL_CONST_HTOBEOOW(x) HCL_CONST_HTOBE128(x)
#	define HCL_CONST_HTOLEOOW(x) HCL_CONST_HTOLE128(x)
#	define HCL_CONST_BEOOWTOH(x) HCL_CONST_BE128TOH(x)
#	define HCL_CONST_LEOOWTOH(x) HCL_CONST_LE128TOH(x)
#endif

#if defined(HCL_HAVE_UINT16_T) && (HCL_SIZEOF_UINT16_T == HCL_SIZEOF_OOHW_T)
#	define HCL_CONST_NTOHOOHW(x) HCL_CONST_NTOH16(x)
#	define HCL_CONST_HTONOOHW(x) HCL_CONST_HTON16(x)
#	define HCL_CONST_HTOBEOOHW(x) HCL_CONST_HTOBE16(x)
#	define HCL_CONST_HTOLEOOHW(x) HCL_CONST_HTOLE16(x)
#	define HCL_CONST_BEOOHWTOH(x) HCL_CONST_BE16TOH(x)
#	define HCL_CONST_LEOOHWTOH(x) HCL_CONST_LE16TOH(x)
#elif defined(HCL_HAVE_UINT32_T) && (HCL_SIZEOF_UINT32_T == HCL_SIZEOF_OOHW_T)
#	define HCL_CONST_NTOHOOHW(x) HCL_CONST_NTOH32(x)
#	define HCL_CONST_HTONOOHW(x) HCL_CONST_HTON32(x)
#	define HCL_CONST_HTOBEOOHW(x) HCL_CONST_HTOBE32(x)
#	define HCL_CONST_HTOLEOOHW(x) HCL_CONST_HTOLE32(x)
#	define HCL_CONST_BEOOHWTOH(x) HCL_CONST_BE32TOH(x)
#	define HCL_CONST_LEOOHWTOH(x) HCL_CONST_LE32TOH(x)
#elif defined(HCL_HAVE_UINT64_T) && (HCL_SIZEOF_UINT64_T == HCL_SIZEOF_OOHW_T)
#	define HCL_CONST_NTOHOOHW(x) HCL_CONST_NTOH64(x)
#	define HCL_CONST_HTONOOHW(x) HCL_CONST_HTON64(x)
#	define HCL_CONST_HTOBEOOHW(x) HCL_CONST_HTOBE64(x)
#	define HCL_CONST_HTOLEOOHW(x) HCL_CONST_HTOLE64(x)
#	define HCL_CONST_BEOOHWTOH(x) HCL_CONST_BE64TOH(x)
#	define HCL_CONST_LEOOHWTOH(x) HCL_CONST_LE64TOH(x)
#elif defined(HCL_HAVE_UINT128_T) && (HCL_SIZEOF_UINT128_T == HCL_SIZEOF_OOHW_T)
#	define HCL_CONST_NTOHOOHW(x) HCL_CONST_NTOH128(x)
#	define HCL_CONST_HTONOOHW(x) HCL_CONST_HTON128(x)
#	define HCL_CONST_HTOBEOOHW(x) HCL_CONST_HTOBE128(x)
#	define HCL_CONST_HTOLEOOHW(x) HCL_CONST_HTOLE128(x)
#	define HCL_CONST_BEOOHWTOH(x) HCL_CONST_BE128TOH(x)
#	define HCL_CONST_LEOOHWTOH(x) HCL_CONST_LE128TOH(x)
#endif

#if defined(HCL_USE_OOW_FOR_LIW)
#	define HCL_CONST_NTOHLIW(x)  HCL_CONST_NTOHOOW(x)
#	define HCL_CONST_HTONLIW(x)  HCL_CONST_HTONOOW(x)
#	define HCL_CONST_HTOBELIW(x) HCL_CONST_HTOBEOOW(x)
#	define HCL_CONST_HTOLELIW(x) HCL_CONST_HTOLEOOW(x)
#	define HCL_CONST_BELIWTOH(x) HCL_CONST_BEOOWTOH(x)
#	define HCL_CONST_LELIWTOH(x) HCL_CONST_LEOOWTOH(x)
#else
#	define HCL_CONST_NTOHLIW(x)  HCL_CONST_NTOHOOHW(x)
#	define HCL_CONST_HTONLIW(x)  HCL_CONST_HTONOOHW(x)
#	define HCL_CONST_HTOBELIW(x) HCL_CONST_HTOBEOOHW(x)
#	define HCL_CONST_HTOLELIW(x) HCL_CONST_HTOLEOOHW(x)
#	define HCL_CONST_BELIWTOH(x) HCL_CONST_BEOOHWTOH(x)
#	define HCL_CONST_LELIWTOH(x) HCL_CONST_LEOOHWTOH(x)
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
#	define HCL_DFL_PATH_SEP ('\\')
#	define HCL_ALT_PATH_SEP ('/')
#	define HCL_IS_PATH_SEP(c) ((c) == HCL_DFL_PATH_SEP || (c) == HCL_ALT_PATH_SEP)
#	define HCL_HAVE_ALT_PATH_SEP 1
#else
#	define HCL_DFL_PATH_SEP ('/')
#	define HCL_ALT_PATH_SEP ('/')
#	define HCL_IS_PATH_SEP(c) ((c) == HCL_DFL_PATH_SEP)
#	undef HCL_HAVE_ALT_PATH_SEP
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

static HCL_INLINE hcl_oow_t hcl_hash_liwords(const hcl_liw_t* ptr, hcl_oow_t len)
{
	return hcl_hash_bytes((const hcl_oob_t*)ptr, len * HCL_SIZEOF(hcl_liw_t));
}

#else
#	define hcl_hash_bytes(ptr,len)     hcl_hash_bytes_(ptr, len)
#	define hcl_hash_bchars(ptr,len)    hcl_hash_bytes((const hcl_oob_t*)ptr, len * HCL_SIZEOF(hcl_bch_t))
#	define hcl_hash_uchars(ptr,len)    hcl_hash_bytes((const hcl_oob_t*)ptr, len * HCL_SIZEOF(hcl_uch_t))
#	define hcl_hash_words(ptr,len)     hcl_hash_bytes((const hcl_oob_t*)ptr, len * HCL_SIZEOF(hcl_oow_t))
#	define hcl_hash_halfwords(ptr,len) hcl_hash_bytes((const hcl_oob_t*)ptr, len * HCL_SIZEOF(hcl_oohw_t))
#	define hcl_hash_liwords(ptr,len)   hcl_hash_bytes((const hcl_oob_t*)ptr, len * HCL_SIZEOF(hcl_liw_t))
#endif

#if defined(HCL_OOCH_IS_UCH)
#	define hcl_hash_oochars(ptr,len) hcl_hash_uchars(ptr,len)
#else
#	define hcl_hash_oochars(ptr,len) hcl_hash_bchars(ptr,len)
#endif

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
 * PATH NAME
 * ========================================================================= */

const hcl_bch_t* hcl_get_base_name_from_bcstr_path (
	const hcl_bch_t* path
);

const hcl_uch_t* hcl_get_base_name_from_ucstr_path (
	const hcl_uch_t* path
);

#if defined(HCL_OOCH_IS_UCH)
#define hcl_get_base_name_from_path(x) hcl_get_base_name_from_ucstr_path(x)
#else
#define hcl_get_base_name_from_path(x) hcl_get_base_name_from_bcstr_path(x)
#endif

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

#if defined(HCL_HAVE_UINT16_T) && (HCL_SIZEOF_UINT16_T == HCL_SIZEOF_OOW_T)
#	define hcl_ntohoow(x) hcl_ntoh16(x)
#	define hcl_htonoow(x) hcl_hton16(x)
#	define hcl_htobeoow(x) hcl_htobe116(x)
#	define hcl_beoowtoh(x) hcl_be16toh(x)
#	define hcl_htoleoow(x) hcl_htole16(x)
#	define hcl_leoowtoh(x) hcl_le16toh(x)
#elif defined(HCL_HAVE_UINT32_T) && (HCL_SIZEOF_UINT32_T == HCL_SIZEOF_OOW_T)
#	define hcl_ntohoow(x) hcl_ntoh32(x)
#	define hcl_htonoow(x) hcl_hton32(x)
#	define hcl_htobeoow(x) hcl_htobe32(x)
#	define hcl_beoowtoh(x) hcl_be32toh(x)
#	define hcl_htoleoow(x) hcl_htole32(x)
#	define hcl_leoowtoh(x) hcl_le32toh(x)
#elif defined(HCL_HAVE_UINT64_T) && (HCL_SIZEOF_UINT64_T == HCL_SIZEOF_OOW_T)
#	define hcl_ntohoow(x) hcl_ntoh64(x)
#	define hcl_htonoow(x) hcl_hton64(x)
#	define hcl_htobeoow(x) hcl_htobe64(x)
#	define hcl_beoowtoh(x) hcl_be64toh(x)
#	define hcl_htoleoow(x) hcl_htole64(x)
#	define hcl_leoowtoh(x) hcl_le64toh(x)
#elif defined(HCL_HAVE_UINT128_T) && (HCL_SIZEOF_UINT128_T == HCL_SIZEOF_OOW_T)
#	define hcl_ntohoow(x) hcl_ntoh128(x)
#	define hcl_htonoow(x) hcl_hton128(x)
#	define hcl_htobeoow(x) hcl_htobe128(x)
#	define hcl_beoowtoh(x) hcl_be128toh(x)
#	define hcl_htoleoow(x) hcl_htole128(x)
#	define hcl_leoowtoh(x) hcl_le128toh(x)
#endif


#if defined(HCL_HAVE_UINT16_T) && (HCL_SIZEOF_UINT16_T == HCL_SIZEOF_OOHW_T)
#	define hcl_ntohoohw(x) hcl_ntoh16(x)
#	define hcl_htonoohw(x) hcl_hton16(x)
#	define hcl_htobeoohw(x) hcl_htobe116(x)
#	define hcl_beoohwtoh(x) hcl_be16toh(x)
#	define hcl_htoleoohw(x) hcl_htole16(x)
#	define hcl_leoohwtoh(x) hcl_le16toh(x)
#elif defined(HCL_HAVE_UINT32_T) && (HCL_SIZEOF_UINT32_T == HCL_SIZEOF_OOHW_T)
#	define hcl_ntohoohw(x) hcl_ntoh32(x)
#	define hcl_htonoohw(x) hcl_hton32(x)
#	define hcl_htobeoohw(x) hcl_htobe32(x)
#	define hcl_beoohwtoh(x) hcl_be32toh(x)
#	define hcl_htoleoohw(x) hcl_htole32(x)
#	define hcl_leoohwtoh(x) hcl_le32toh(x)
#elif defined(HCL_HAVE_UINT64_T) && (HCL_SIZEOF_UINT64_T == HCL_SIZEOF_OOHW_T)
#	define hcl_ntohoohw(x) hcl_ntoh64(x)
#	define hcl_htonoohw(x) hcl_hton64(x)
#	define hcl_htobeoohw(x) hcl_htobe64(x)
#	define hcl_beoohwtoh(x) hcl_be64toh(x)
#	define hcl_htoleoohw(x) hcl_htole64(x)
#	define hcl_leoohwtoh(x) hcl_le64toh(x)
#elif defined(HCL_HAVE_UINT128_T) && (HCL_SIZEOF_UINT128_T == HCL_SIZEOF_OOHW_T)
#	define hcl_ntohoohw(x) hcl_ntoh128(x)
#	define hcl_htonoohw(x) hcl_hton128(x)
#	define hcl_htobeoohw(x) hcl_htobe128(x)
#	define hcl_beoohwtoh(x) hcl_be128toh(x)
#	define hcl_htoleoohw(x) hcl_htole128(x)
#	define hcl_leoohwtoh(x) hcl_le128toh(x)
#endif

#if defined(HCL_USE_OOW_FOR_LIW)
#	define hcl_ntohliw(x)  hcl_ntohoow(x)
#	define hcl_htonliw(x)  hcl_htonoow(x)
#	define hcl_htobeliw(x) hcl_htobeoow(x)
#	define hcl_beliwtoh(x) hcl_beoowtoh(x)
#	define hcl_htoleliw(x) hcl_htoleoow(x)
#	define hcl_leliwtoh(x) hcl_leoowtoh(x)
#else
#	define hcl_ntohliw(x)  hcl_ntohoohw(x)
#	define hcl_htonliw(x)  hcl_htonoohw(x)
#	define hcl_htobeliw(x) hcl_htobeoohw(x)
#	define hcl_beliwtoh(x) hcl_beoohwtoh(x)
#	define hcl_htoleliw(x) hcl_htoleoohw(x)
#	define hcl_leliwtoh(x) hcl_leoohwtoh(x)
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
