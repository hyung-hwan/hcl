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


/* ----------------------------------------------------------------------- 
 * DOUBLY LINKED LIST MACROS
 * ----------------------------------------------------------------------- */
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



#define HCL_CONST_SWAP16(x) \
	((qse_uint16_t)((((qse_uint16_t)(x) & (qse_uint16_t)0x00ffU) << 8) | \
	                (((qse_uint16_t)(x) & (qse_uint16_t)0xff00U) >> 8) ))

#define HCL_CONST_SWAP32(x) \
	((qse_uint32_t)((((qse_uint32_t)(x) & (qse_uint32_t)0x000000ffUL) << 24) | \
	                (((qse_uint32_t)(x) & (qse_uint32_t)0x0000ff00UL) <<  8) | \
	                (((qse_uint32_t)(x) & (qse_uint32_t)0x00ff0000UL) >>  8) | \
	                (((qse_uint32_t)(x) & (qse_uint32_t)0xff000000UL) >> 24) ))

#if defined(HCL_ENDIAN_LITTLE)
#       define HCL_CONST_NTOH16(x) HCL_CONST_SWAP16(x)
#       define HCL_CONST_HTON16(x) HCL_CONST_SWAP16(x)
#       define HCL_CONST_NTOH32(x) HCL_CONST_SWAP32(x)
#       define HCL_CONST_HTON32(x) HCL_CONST_SWAP32(x)
#elif defined(HCL_ENDIAN_BIG)
#       define HCL_CONST_NTOH16(x) (x)
#       define HCL_CONST_HTON16(x) (x)
#       define HCL_CONST_NTOH32(x) (x)
#       define HCL_CONST_HTON32(x) (x)
#else
#       error UNKNOWN ENDIAN
#endif


#if defined(__cplusplus)
extern "C" {
#endif

HCL_EXPORT hcl_oow_t hcl_hashbytes (
	const hcl_oob_t* ptr,
	hcl_oow_t        len
);

#if defined(HCL_HAVE_INLINE)
	static HCL_INLINE hcl_oow_t hcl_hashbchars (const hcl_bch_t* ptr, hcl_oow_t len)
	{
		return hcl_hashbytes((const hcl_oob_t*)ptr, len * HCL_SIZEOF(hcl_bch_t));
	}

	static HCL_INLINE hcl_oow_t hcl_hashuchars (const hcl_uch_t* ptr, hcl_oow_t len)
	{
		return hcl_hashbytes((const hcl_oob_t*)ptr, len * HCL_SIZEOF(hcl_uch_t));
	}

	static HCL_INLINE hcl_oow_t hcl_hashwords (const hcl_oow_t* ptr, hcl_oow_t len)
	{
		return hcl_hashbytes((const hcl_oob_t*)ptr, len * HCL_SIZEOF(hcl_oow_t));
	}

	static HCL_INLINE hcl_oow_t hcl_hashhalfwords (const hcl_oohw_t* ptr, hcl_oow_t len)
	{
		return hcl_hashbytes((const hcl_oob_t*)ptr, len * HCL_SIZEOF(hcl_oohw_t));
	}
#else
#	define hcl_hashbchars(ptr,len)    hcl_hashbytes((const hcl_oob_t*)ptr, len * HCL_SIZEOF(hcl_bch_t))
#	define hcl_hashuchars(ptr,len)    hcl_hashbytes((const hcl_oob_t*)ptr, len * HCL_SIZEOF(hcl_uch_t))
#	define hcl_hashwords(ptr,len)     hcl_hashbytes((const hcl_oob_t*)ptr, len * HCL_SIZEOF(hcl_oow_t))
#	define hcl_hashhalfwords(ptr,len) hcl_hashbytes((const hcl_oob_t*)ptr, len * HCL_SIZEOF(hcl_oohw_t))
#endif

#if defined(HCL_OOCH_IS_UCH)
#	define hcl_hashoochars(ptr,len) hcl_hashuchars(ptr,len)
#else
#	define hcl_hashoochars(ptr,len) hcl_hashbchars(ptr,len)
#endif

/**
 * The hcl_equaluchars() function determines equality of two strings
 * of the same length \a len.
 */
HCL_EXPORT int hcl_equaluchars (
	const hcl_uch_t* str1,
	const hcl_uch_t* str2,
	hcl_oow_t        len
);

HCL_EXPORT int hcl_equalbchars (
	const hcl_bch_t* str1,
	const hcl_bch_t* str2,
	hcl_oow_t        len
);

HCL_EXPORT int hcl_compuchars (
	const hcl_uch_t* str1,
	hcl_oow_t        len1,
	const hcl_uch_t* str2,
	hcl_oow_t        len2
);

HCL_EXPORT int hcl_compbchars (
	const hcl_bch_t* str1,
	hcl_oow_t        len1,
	const hcl_bch_t* str2,
	hcl_oow_t        len2
);

HCL_EXPORT int hcl_compucstr (
	const hcl_uch_t* str1,
	const hcl_uch_t* str2
);

HCL_EXPORT int hcl_compbcstr (
	const hcl_bch_t* str1,
	const hcl_bch_t* str2
);

HCL_EXPORT int hcl_compucbcstr (
	const hcl_uch_t* str1,
	const hcl_bch_t* str2
);

HCL_EXPORT int hcl_compucharsucstr (
	const hcl_uch_t* str1,
	hcl_oow_t        len,
	const hcl_uch_t* str2
);

HCL_EXPORT int hcl_compucharsbcstr (
	const hcl_uch_t* str1,
	hcl_oow_t        len,
	const hcl_bch_t* str2
);

HCL_EXPORT int hcl_compbcharsbcstr (
	const hcl_bch_t* str1,
	hcl_oow_t        len,
	const hcl_bch_t* str2
);

HCL_EXPORT int hcl_compbcharsucstr (
	const hcl_bch_t* str1,
	hcl_oow_t        len,
	const hcl_uch_t* str2
);

HCL_EXPORT void hcl_copyuchars (
	hcl_uch_t*       dst,
	const hcl_uch_t* src,
	hcl_oow_t        len
);

HCL_EXPORT void hcl_copybchars (
	hcl_bch_t*       dst,
	const hcl_bch_t* src,
	hcl_oow_t        len
);

HCL_EXPORT void hcl_copybtouchars (
	hcl_uch_t*       dst,
	const hcl_bch_t* src,
	hcl_oow_t        len
);

HCL_EXPORT hcl_oow_t hcl_copyucstr (
	hcl_uch_t*       dst,
	hcl_oow_t        len,
	const hcl_uch_t* src
);

HCL_EXPORT hcl_oow_t hcl_copybcstr (
	hcl_bch_t*       dst,
	hcl_oow_t        len,
	const hcl_bch_t* src
);

HCL_EXPORT hcl_uch_t* hcl_finduchar (
	const hcl_uch_t* ptr,
	hcl_oow_t        len,
	hcl_uch_t        c
);

HCL_EXPORT hcl_bch_t* hcl_findbchar (
	const hcl_bch_t* ptr,
	hcl_oow_t        len,
	hcl_bch_t        c
);

HCL_EXPORT hcl_uch_t* hcl_rfinduchar (
	const hcl_uch_t* ptr,
	hcl_oow_t        len,
	hcl_uch_t        c
);

HCL_EXPORT hcl_bch_t* hcl_rfindbchar (
	const hcl_bch_t* ptr,
	hcl_oow_t        len,
	hcl_bch_t        c
);

HCL_EXPORT hcl_uch_t* hcl_finducharinucstr (
	const hcl_uch_t* ptr,
	hcl_uch_t        c
);

HCL_EXPORT hcl_bch_t* hcl_findbcharinbcstr (
	const hcl_bch_t* ptr,
	hcl_bch_t        c
);

HCL_EXPORT hcl_oow_t hcl_countucstr (
	const hcl_uch_t* str
);

HCL_EXPORT hcl_oow_t hcl_countbcstr (
	const hcl_bch_t* str
);

#if defined(HCL_OOCH_IS_UCH)
#	define hcl_equaloochars(str1,str2,len) hcl_equaluchars(str1,str2,len)
#	define hcl_compoochars(str1,len1,str2,len2) hcl_compuchars(str1,len1,str2,len2)
#	define hcl_compoocbcstr(str1,str2) hcl_compucbcstr(str1,str2)
#	define hcl_compoocharsbcstr(str1,len1,str2) hcl_compucharsbcstr(str1,len1,str2)
#	define hcl_compoocharsucstr(str1,len1,str2) hcl_compucharsucstr(str1,len1,str2)
#	define hcl_compoocharsoocstr(str1,len1,str2) hcl_compucharsucstr(str1,len1,str2)
#	define hcl_compoocstr(str1,str2) hcl_compucstr(str1,str2)
#	define hcl_copyoochars(dst,src,len) hcl_copyuchars(dst,src,len)
#	define hcl_copybctooochars(dst,src,len) hcl_copybtouchars(dst,src,len)
#	define hcl_copyoocstr(dst,len,src) hcl_copyucstr(dst,len,src)
#	define hcl_findoochar(ptr,len,c) hcl_finduchar(ptr,len,c)
#	define hcl_rfindoochar(ptr,len,c) hcl_rfinduchar(ptr,len,c)
#	define hcl_findoocharinoocstr(ptr,c) hcl_finducharinucstr(ptr,c)
#	define hcl_countoocstr(str) hcl_countucstr(str)
#else
#	define hcl_equaloochars(str1,str2,len) hcl_equalbchars(str1,str2,len)
#	define hcl_compoochars(str1,len1,str2,len2) hcl_compbchars(str1,len1,str2,len2)
#	define hcl_compoocbcstr(str1,str2) hcl_compbcstr(str1,str2)
#	define hcl_compoocharsbcstr(str1,len1,str2) hcl_compbcharsbcstr(str1,len1,str2)
#	define hcl_compoocharsucstr(str1,len1,str2) hcl_compbcharsucstr(str1,len1,str2)
#	define hcl_compoocharsoocstr(str1,len1,str2) hcl_compbcharsbcstr(str1,len1,str2)
#	define hcl_compoocstr(str1,str2) hcl_compbcstr(str1,str2)
#	define hcl_copyoochars(dst,src,len) hcl_copybchars(dst,src,len)
#	define hcl_copybctooochars(dst,src,len) hcl_copybchars(dst,src,len)
#	define hcl_copyoocstr(dst,len,src) hcl_copybcstr(dst,len,src)
#	define hcl_findoochar(ptr,len,c) hcl_findbchar(ptr,len,c)
#	define hcl_rfindoochar(ptr,len,c) hcl_rfindbchar(ptr,len,c)
#	define hcl_findoocharinoocstr(ptr,c) hcl_findbcharinbcstr(ptr,c)
#	define hcl_countoocstr(str) hcl_countbcstr(str)
#endif



HCL_EXPORT int hcl_copyoocstrtosbuf (
	hcl_t*            hcl,
	const hcl_ooch_t* str,
	int                id
);

HCL_EXPORT int hcl_concatoocstrtosbuf (
	hcl_t*            hcl,
	const hcl_ooch_t* str,
	int                id
);

HCL_EXPORT hcl_cmgr_t* hcl_getutf8cmgr (
	void
);

/**
 * The hcl_convutoutf8chars() function converts a unicode character string \a ucs 
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
 *   n = hcl_convutoutf8chars (ucs, &ucslen, bcs, &bcslen);
 *   if (n <= -1)
 *   {
 *      // conversion error
 *   }
 * \endcode
 */
HCL_EXPORT int hcl_convutoutf8chars (
	const hcl_uch_t*    ucs,
	hcl_oow_t*          ucslen,
	hcl_bch_t*          bcs,
	hcl_oow_t*          bcslen
);

/**
 * The hcl_convutf8touchars() function converts a UTF8 string to a uncide string.
 *
 * It never returns -2 if \a ucs is #HCL_NULL.
 *
 * \code
 *  const hcl_bch_t* bcs = "test string";
 *  hcl_uch_t ucs[100];
 *  hcl_oow_t ucslen = HCL_COUNTOF(buf), n;
 *  hcl_oow_t bcslen = 11;
 *  int n;
 *  n = hcl_convutf8touchars (bcs, &bcslen, ucs, &ucslen);
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
HCL_EXPORT int hcl_convutf8touchars (
	const hcl_bch_t*   bcs,
	hcl_oow_t*         bcslen,
	hcl_uch_t*         ucs,
	hcl_oow_t*         ucslen
);


HCL_EXPORT int hcl_convutoutf8cstr (
	const hcl_uch_t*    ucs,
	hcl_oow_t*          ucslen,
	hcl_bch_t*          bcs,
	hcl_oow_t*          bcslen
);

HCL_EXPORT int hcl_convutf8toucstr (
	const hcl_bch_t*   bcs,
	hcl_oow_t*         bcslen,
	hcl_uch_t*         ucs,
	hcl_oow_t*         ucslen
);


HCL_EXPORT hcl_oow_t hcl_uctoutf8 (
	hcl_uch_t    uc,
	hcl_bch_t*   utf8,
	hcl_oow_t    size
);

HCL_EXPORT hcl_oow_t hcl_utf8touc (
	const hcl_bch_t* utf8,
	hcl_oow_t        size,
	hcl_uch_t*       uc
);

HCL_EXPORT int hcl_ucwidth (
	hcl_uch_t uc
);


/* ------------------------------------------------------------------------- */

#if defined(HCL_HAVE_UINT16_T)
HCL_EXPORT hcl_uint16_t hcl_ntoh16 (
	hcl_uint16_t x
);

HCL_EXPORT hcl_uint16_t hcl_hton16 (
	hcl_uint16_t x
);
#endif

#if defined(HCL_HAVE_UINT32_T)
HCL_EXPORT hcl_uint32_t hcl_ntoh32 (
	hcl_uint32_t x
);

HCL_EXPORT hcl_uint32_t hcl_hton32 (
	hcl_uint32_t x
);
#endif

#if defined(HCL_HAVE_UINT64_T)
HCL_EXPORT hcl_uint64_t hcl_ntoh64 (
	hcl_uint64_t x
);

HCL_EXPORT hcl_uint64_t hcl_hton64 (
	hcl_uint64_t x
);
#endif

#if defined(HCL_HAVE_UINT128_T)
HCL_EXPORT hcl_uint128_t hcl_ntoh128 (
	hcl_uint128_t x
);

HCL_EXPORT hcl_uint128_t hcl_hton128 (
	hcl_uint128_t x
);
#endif


#if defined(__cplusplus)
}
#endif


#endif
