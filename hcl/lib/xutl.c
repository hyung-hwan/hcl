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
#include <hcl-xutl.h>
#include "hcl-prv.h"

#if defined(_WIN32)
#	include <winsock2.h>
#	include <ws2tcpip.h> /* sockaddr_in6 */
#	include <windows.h>
#elif defined(__OS2__)
#	if defined(TCPV40HDRS)
#		define  BSD_SELECT
#	endif
#	include <types.h>
#	include <sys/socket.h>
#	include <netinet/in.h>
#else
#	include <netinet/in.h>
#endif

union sockaddr_t
{
	struct sockaddr sa;
#if (HCL_SIZEOF_STRUCT_SOCKADDR_IN > 0)
	struct sockaddr_in in4;
#endif
#if (HCL_SIZEOF_STRUCT_SOCKADDR_IN6 > 0)
	struct sockaddr_in6 in6;
#endif
};
typedef union sockaddr_t sockaddr_t;

#undef ooch_t
#undef oocs_t
#undef str_to_ipv4
#undef str_to_ipv6
#undef str_to_sockaddr

#define ooch_t hcl_bch_t
#define oocs_t hcl_bcs_t
#define str_to_ipv4 bchars_to_ipv4
#define str_to_ipv6 bchars_to_ipv6
#define str_to_sockaddr hcl_bcharstosckaddr
#include "xutl-sa.h"

#undef ooch_t
#undef oocs_t
#undef str_to_ipv4
#undef str_to_ipv6
#undef str_to_sockaddr

#define ooch_t hcl_uch_t
#define oocs_t hcl_ucs_t
#define str_to_ipv4 uchars_to_ipv4
#define str_to_ipv6 uchars_to_ipv6
#define str_to_sockaddr hcl_ucharstosckaddr
#include "xutl-sa.h"

int hcl_get_sckaddr_info (const hcl_sckaddr_t* sckaddr, hcl_scklen_t* scklen)
{
	sockaddr_t* sa = (sockaddr_t*)sckaddr;
	if (scklen)
	{
		switch (sa->sa.sa_family)
		{
		#if (HCL_SIZEOF_STRUCT_SOCKADDR_IN > 0)
			case AF_INET:
				*scklen = HCL_SIZEOF(sa->in4);
				break;
		#endif

		#if (HCL_SIZEOF_STRUCT_SOCKADDR_IN6 > 0)
			case AF_INET6:
				*scklen = HCL_SIZEOF(sa->in6);
				break;
		#endif

			default:
				*scklen = 0; /* unknown */
				break;
		}
	}
	return sa->sa.sa_family;
}
