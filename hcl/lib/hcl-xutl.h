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

#ifndef _HCL_XUTL_H_
#define _HCL_XUTL_H_

#include "hcl-cmn.h"

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
