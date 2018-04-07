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
#	define HCL_CONST_NTOH16(x) HCL_CONST_SWAP16(x)
#	define HCL_CONST_HTON16(x) HCL_CONST_SWAP16(x)
#	define HCL_CONST_NTOH32(x) HCL_CONST_SWAP32(x)
#	define HCL_CONST_HTON32(x) HCL_CONST_SWAP32(x)
#elif defined(HCL_ENDIAN_BIG)
#	define HCL_CONST_NTOH16(x) (x)
#	define HCL_CONST_HTON16(x) (x)
#	define HCL_CONST_NTOH32(x) (x)
#	define HCL_CONST_HTON32(x) (x)
#else
#	error UNKNOWN ENDIAN
#endif




#if (HCL_SIZEOF_SOCKLEN_T == 1)
	#if defined(HCL_SOCKLEN_T_IS_SIGNED)
		typedef hcl_int8_t hcl_scklen_t;
	#else
		typedef hcl_uint8_t hcl_scklen_t;
	#endif
#elif (HCL_SIZEOF_SOCKLEN_T == 2)
	#if defined(HCL_SOCKLEN_T_IS_SIGNED)
		typedef hcl_int16_t hcl_scklen_t;
	#else
		typedef hcl_uint16_t hcl_scklen_t;
	#endif
#elif (HCL_SIZEOF_SOCKLEN_T == 4)
	#if defined(HCL_SOCKLEN_T_IS_SIGNED)
		typedef hcl_int32_t hcl_scklen_t;
	#else
		typedef hcl_uint32_t hcl_scklen_t;
	#endif
#elif (HCL_SIZEOF_SOCKLEN_T == 8)
	#if defined(HCL_SOCKLEN_T_IS_SIGNED)
		typedef hcl_int64_t hcl_scklen_t;
	#else
		typedef hcl_uint64_t hcl_scklen_t;
	#endif
#else
	#undef HCL_SIZEOF_SOCKLEN_T
	#define HCL_SIZEOF_SOCKLEN_T HCL_SIZEOF_INT
	#define HCL_SOCKLEN_T_IS_SIGNED
	typedef int hcl_scklen_t;
#endif


struct hcl_sckaddr_t
{
#define HCL_SCKADDR_DATA_SIZE 0

#if (HCL_SIZEOF_STRUCT_SOCKADDR_IN > HCL_SCKADDR_DATA_SIZE)
	#undef HCL_SCKADDR_DATA_SIZE
	#define HCL_SCKADDR_DATA_SIZE HCL_SIZEOF_STRUCT_SOCKADDR_IN
#endif
#if (HCL_SIZEOF_STRUCT_SOCKADDR_IN6 > HCL_SCKADDR_DATA_SIZE)
	#undef HCL_SCKADDR_DATA_SIZE
	#define HCL_SCKADDR_DATA_SIZE HCL_SIZEOF_STRUCT_SOCKADDR_IN6
#endif
#if (HCL_SIZEOF_STRUCT_SOCKADDR_UN > HCL_SCKADDR_DATA_SIZE)
	#undef HCL_SCKADDR_DATA_SIZE
	#define HCL_SCKADDR_DATA_SIZE HCL_SIZEOF_STRUCT_SOCKADDR_UN
#endif
#if (HCL_SIZEOF_STRUCT_SOCKADDR_LL > HCL_SCKADDR_DATA_SIZE)
	#undef HCL_SCKADDR_DATA_SIZE
	#define HCL_SCKADDR_DATA_SIZE HCL_SIZEOF_STRUCT_SOCKADDR_LL
#endif

#if (HCL_SCKADDR_DATA_SIZE == 0)
	#undef HCL_SCKADDR_DATA_SIZE
	#define HCL_SCKADDR_DATA_SIZE 64
#endif
	hcl_uint8_t storage[HCL_SCKADDR_DATA_SIZE];
};
typedef struct hcl_sckaddr_t hcl_sckaddr_t;

#if defined(__cplusplus)
extern "C" {
#endif

HCL_EXPORT hcl_oow_t hcl_hash_bytes (
	const hcl_oob_t* ptr,
	hcl_oow_t        len
);

#if defined(HCL_HAVE_INLINE)
	static HCL_INLINE hcl_oow_t hcl_hashbchars (const hcl_bch_t* ptr, hcl_oow_t len)
	{
		return hcl_hash_bytes((const hcl_oob_t*)ptr, len * HCL_SIZEOF(hcl_bch_t));
	}

	static HCL_INLINE hcl_oow_t hcl_hashuchars (const hcl_uch_t* ptr, hcl_oow_t len)
	{
		return hcl_hash_bytes((const hcl_oob_t*)ptr, len * HCL_SIZEOF(hcl_uch_t));
	}

	static HCL_INLINE hcl_oow_t hcl_hashwords (const hcl_oow_t* ptr, hcl_oow_t len)
	{
		return hcl_hash_bytes((const hcl_oob_t*)ptr, len * HCL_SIZEOF(hcl_oow_t));
	}

	static HCL_INLINE hcl_oow_t hcl_hashhalfwords (const hcl_oohw_t* ptr, hcl_oow_t len)
	{
		return hcl_hash_bytes((const hcl_oob_t*)ptr, len * HCL_SIZEOF(hcl_oohw_t));
	}
#else
#	define hcl_hashbchars(ptr,len)    hcl_hash_bytes((const hcl_oob_t*)ptr, len * HCL_SIZEOF(hcl_bch_t))
#	define hcl_hashuchars(ptr,len)    hcl_hash_bytes((const hcl_oob_t*)ptr, len * HCL_SIZEOF(hcl_uch_t))
#	define hcl_hashwords(ptr,len)     hcl_hash_bytes((const hcl_oob_t*)ptr, len * HCL_SIZEOF(hcl_oow_t))
#	define hcl_hashhalfwords(ptr,len) hcl_hash_bytes((const hcl_oob_t*)ptr, len * HCL_SIZEOF(hcl_oohw_t))
#endif

#if defined(HCL_OOCH_IS_UCH)
#	define hcl_hashoochars(ptr,len) hcl_hashuchars(ptr,len)
#else
#	define hcl_hashoochars(ptr,len) hcl_hashbchars(ptr,len)
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
#	define hcl_copy_oocstr(dst,len,src) hcl_copy_ucstr(dst,len,src)
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
#	define hcl_copy_oocstr(dst,len,src) hcl_copy_bcstr(dst,len,src)
#	define hcl_find_oochar(ptr,len,c) hcl_find_bchar(ptr,len,c)
#	define hcl_rfind_oochar(ptr,len,c) hcl_rfind_bchar(ptr,len,c)
#	define hcl_find_oochar_in_oocstr(ptr,c) hcl_find_bchar_in_bcstr(ptr,c)
#	define hcl_count_oocstr(str) hcl_count_bcstr(str)
#endif





HCL_EXPORT int hcl_conv_bcs_to_ucs_with_cmgr (
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

HCL_EXPORT int hcl_conv_ucs_to_bcs_with_cmgr (
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
#	define hcl_conv_oocs_to_bcs_with_cmgr(oocs,oocslen,bcs,bcslen,cmgr) hcl_conv_ucs_to_bcs_with_cmgr(oocs,oocslen,bcs,bcslen,cmgr)
#	define hcl_conv_oochars_to_bchars_with_cmgr(oocs,oocslen,bcs,bcslen,cmgr) hcl_conv_uchars_to_bchars_with_cmgr(oocs,oocslen,bcs,bcslen,cmgr)
#else
#	define hcl_conv_oocs_to_ucs_with_cmgr(oocs,oocslen,ucs,ucslen,cmgr) hcl_conv_bcs_to_ucs_with_cmgr(oocs,oocslen,ucs,ucslen,cmgr,0)
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


HCL_EXPORT int hcl_ucharstosckaddr (
	hcl_t*           hcl,
	const hcl_uch_t* str,
	hcl_oow_t        len,
	hcl_sckaddr_t*   sckaddr,
	hcl_scklen_t*    scklen
);

HCL_EXPORT int hcl_bcharstosckaddr (
	hcl_t*           hcl,
	const hcl_bch_t* str,
	hcl_oow_t        len,
	hcl_sckaddr_t*   sckaddr,
	hcl_scklen_t*    scklen
);

#if defined(HCL_HAVE_INLINE)
	static HCL_INLINE int hcl_uchars_to_sckaddr (const hcl_uch_t* str, hcl_oow_t len, hcl_sckaddr_t* sckaddr, hcl_scklen_t* scklen)
	{
		return hcl_ucharstosckaddr(HCL_NULL, str, len, sckaddr, scklen);
	}
	static HCL_INLINE int hcl_bchars_to_sckaddr (const hcl_bch_t* str, hcl_oow_t len, hcl_sckaddr_t* sckaddr, hcl_scklen_t* scklen)
	{
		return hcl_bcharstosckaddr(HCL_NULL, str, len, sckaddr, scklen);
	}
#else
	#define hcl_uchars_to_sckaddr(str,len,sckaddr,scklen) hcl_ucharstosckaddr(HCL_NULL,str,len,sckaddr,scklen)
	#define hcl_bchars_to_sckaddr(str,len,sckaddr,scklen) hcl_bcharstosckaddr(HCL_NULL,str,len,sckaddr,scklen)
#endif

#if defined(HCL_OOCH_IS_UCH)
#	define hcl_oocharstosckaddr hcl_ucharstosckaddr
#	define hcl_oochars_to_sckaddr hcl_uchars_to_sckaddr
#else
#	define hcl_oocharstosckaddr hcl_bcharstosckaddr
#	define hcl_oochars_to_sckaddr hcl_bchars_to_sckaddr
#endif

/**
 * The hcl_get_sckaddr_info() function returns the socket family.
 * if \a scklen is not #HCL_NULL, it also sets the actual address length
 * in the memory pointed to by it.
 */
HCL_EXPORT int hcl_get_sckaddr_info (
	const hcl_sckaddr_t* sckaddr,
	hcl_scklen_t*        scklen
);

#if defined(__cplusplus)
}
#endif


#endif
