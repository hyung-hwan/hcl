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
/* OS/2 for other platforms than x86?
 * If so, the endian should be defined selectively
 */
#define HCL_ENDIAN_LITTLE

#if defined(__WATCOMC__)
#	define HCL_SIZEOF_CHAR        1
#	define HCL_SIZEOF_SHORT       2
#	define HCL_SIZEOF_INT         4
#	define HCL_SIZEOF_LONG        4
#	if (__WATCOMC__ < 1200)
#		define HCL_SIZEOF_LONG_LONG   0
#	else
#		define HCL_SIZEOF_LONG_LONG   8
#	endif
#	define HCL_SIZEOF_VOID_P      4
#	define HCL_SIZEOF_FLOAT       4
#	define HCL_SIZEOF_DOUBLE      8
#	define HCL_SIZEOF_LONG_DOUBLE 8
#	define HCL_SIZEOF_WCHAR_T     2

#	define HCL_SIZEOF___INT8      1
#	define HCL_SIZEOF___INT16     2
#	define HCL_SIZEOF___INT32     4
#	define HCL_SIZEOF___INT64     8
#	define HCL_SIZEOF___INT128    0

#	define HCL_SIZEOF_OFF64_T     0
#	define HCL_SIZEOF_OFF_T       8

/* I don't know the exact mbstate size.
 * but this should be large enough */
#	define HCL_SIZEOF_MBSTATE_T   HCL_SIZEOF_LONG
/* TODO: check the exact value */
#	define HCL_MBLEN_MAX          8

	/* these two have only to be large enough */
#	define HCL_SIZEOF_STRUCT_SOCKADDR_IN 32
#	define HCL_SIZEOF_STRUCT_SOCKADDR_IN6 64
#	define HCL_SIZEOF_SOCKLEN_T 4

#elif defined(__BORLANDC__)
#	define HCL_SIZEOF_CHAR        1
#	define HCL_SIZEOF_SHORT       2
#	define HCL_SIZEOF_INT         4
#	define HCL_SIZEOF_LONG        4
#	define HCL_SIZEOF_LONG_LONG   0
#	define HCL_SIZEOF_VOID_P      4
#	define HCL_SIZEOF_FLOAT       4
#	define HCL_SIZEOF_DOUBLE      8
#	define HCL_SIZEOF_LONG_DOUBLE 10
#	define HCL_SIZEOF_WCHAR_T     2

#	define HCL_SIZEOF___INT8      0
#	define HCL_SIZEOF___INT16     0
#	define HCL_SIZEOF___INT32     0
#	define HCL_SIZEOF___INT64     0
#	define HCL_SIZEOF___INT128    0

#	define HCL_SIZEOF_OFF64_T     0
#	define HCL_SIZEOF_OFF_T       4

#	define HCL_SIZEOF_MBSTATE_T   HCL_SIZEOF_LONG
#	define HCL_MBLEN_MAX          8

#	define HCL_SIZEOF_STRUCT_SOCKADDR_IN 32
#	define HCL_SIZEOF_STRUCT_SOCKADDR_IN6 64
#	define HCL_SIZEOF_SOCKLEN_T 4

#elif defined(__IBMC__) || defined(__IBMCPP__)

	/* visualage c or c++ */
#   define HCL_SIZEOF_CHAR        1
#   define HCL_SIZEOF_SHORT       2
#   define HCL_SIZEOF_INT         4
#   define HCL_SIZEOF_LONG        4
#   define HCL_SIZEOF_LONG_LONG   0
#   define HCL_SIZEOF_VOID_P      4
#   define HCL_SIZEOF_FLOAT       4
#   define HCL_SIZEOF_DOUBLE      8
#   define HCL_SIZEOF_LONG_DOUBLE 16
#   define HCL_SIZEOF_WCHAR_T     2

#   define HCL_SIZEOF___INT8      0
#   define HCL_SIZEOF___INT16     0
#   define HCL_SIZEOF___INT32     0
#   define HCL_SIZEOF___INT64     0
#   define HCL_SIZEOF___INT128    0

#   define HCL_SIZEOF_OFF64_T     0
#   define HCL_SIZEOF_OFF_T       4

#   define HCL_SIZEOF_MBSTATE_T   HCL_SIZEOF_LONG
#   define HCL_MBLEN_MAX          8

#   define HCL_SIZEOF_STRUCT_SOCKADDR_IN 32
#   define HCL_SIZEOF_STRUCT_SOCKADDR_IN6 64
#   define HCL_SIZEOF_SOCKLEN_T 4

#else
#	error Define the size of various data types.
#endif
