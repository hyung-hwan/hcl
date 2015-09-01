#ifndef _H2_TYPES_H_
#define _H2_TYPES_H_

/* WARNING: NEVER CHANGE/DELETE THE FOLLOWING H2_HAVE_CONFIG_H DEFINITION. 
 *          IT IS USED FOR DEPLOYMENT BY MAKEFILE.AM */
/*#define H2_HAVE_CONFIG_H*/

#if defined(H2_HAVE_CONFIG_H)
#	include <h2/config.h>
#elif defined(_WIN32)
#	include <h2/conf-msw.h>
#elif defined(__OS2__)
#	include <h2/conf-os2.h>
#elif defined(__DOS__)
#	include <h2/conf-dos.h>
#elif defined(vms) || defined(__vms)
#	include <h2/conf-vms.h>
#elif defined(macintosh)
#	include <:h2:conf-mac.h> /* class mac os */
#else
#	error Unsupported operating system
#endif

/** \typedef h2_int_t
 * The h2_int_t type defines a signed integer type as large as a pointer.
 */
/** \typedef h2_uint_t
 * The h2_uint_t type defines an unsigned integer type as large as a pointer.
 */
#if (defined(hpux) || defined(__hpux) || defined(__hpux__) || \
     (defined(__APPLE__) && defined(__MACH__))) && \
    (H2_SIZEOF_VOID_P == H2_SIZEOF_LONG)
	typedef signed long h2_int_t;
	typedef unsigned long h2_uint_t;
	#define H2_SIZEOF_INT_T H2_SIZEOF_LONG
	#define H2_SIZEOF_UINT_T H2_SIZEOF_LONG
#elif defined(__SPU__) && (H2_SIZEOF_VOID_P == H2_SIZEOF_LONG)
	typedef signed long h2_int_t;
	typedef unsigned long h2_uint_t;
	#define H2_SIZEOF_INT_T H2_SIZEOF_LONG
	#define H2_SIZEOF_UINT_T H2_SIZEOF_LONG
#elif H2_SIZEOF_VOID_P == H2_SIZEOF_INT
	typedef signed int h2_int_t;
	typedef unsigned int h2_uint_t;
	#define H2_SIZEOF_INT_T H2_SIZEOF_INT
	#define H2_SIZEOF_UINT_T H2_SIZEOF_INT
#elif H2_SIZEOF_VOID_P == H2_SIZEOF_LONG
	typedef signed long h2_int_t;
	typedef unsigned long h2_uint_t;
	#define H2_SIZEOF_INT_T H2_SIZEOF_LONG
	#define H2_SIZEOF_UINT_T H2_SIZEOF_LONG
#elif H2_SIZEOF_VOID_P == H2_SIZEOF_LONG_LONG
	typedef signed long long h2_int_t;
	typedef unsigned long long h2_uint_t;
	#define H2_SIZEOF_INT_T H2_SIZEOF_LONG_LONG
	#define H2_SIZEOF_UINT_T H2_SIZEOF_LONG_LONG
#elif H2_SIZEOF_VOID_P == H2_SIZEOF___INT32
	typedef signed __int32 h2_int_t;
	typedef unsigned __int32 h2_uint_t;
	#define H2_SIZEOF_INT_T H2_SIZEOF___INT32
	#define H2_SIZEOF_UINT_T H2_SIZEOF___INT32
#elif H2_SIZEOF_VOID_P == H2_SIZEOF___INT32_T
	typedef __int32_t h2_int_t;
	typedef __uint32_t h2_uint_t;
	#define H2_SIZEOF_INT_T H2_SIZEOF___INT32_T
	#define H2_SIZEOF_UINT_T H2_SIZEOF___INT32_T
#elif H2_SIZEOF_VOID_P == H2_SIZEOF___INT64
	typedef signed __int64 h2_int_t;
	typedef unsigned __int64 h2_uint_t;
	#define H2_SIZEOF_INT_T H2_SIZEOF___INT64
	#define H2_SIZEOF_UINT_T H2_SIZEOF___INT64
#elif H2_SIZEOF_VOID_P == H2_SIZEOF___INT64_T
	typedef __int64_t h2_int_t;
	typedef __uint64_t h2_uint_t;
	#define H2_SIZEOF_INT_T H2_SIZEOF___INT64_T
	#define H2_SIZEOF_UINT_T H2_SIZEOF___INT64_T
#elif H2_SIZEOF_VOID_P == H2_SIZEOF___INT128
	typedef signed __int128 h2_int_t;
	typedef unsigned __int128 h2_uint_t;
	#define H2_SIZEOF_INT_T H2_SIZEOF___INT128
	#define H2_SIZEOF_UINT_T H2_SIZEOF___INT128
#elif H2_SIZEOF_VOID_P == H2_SIZEOF___INT128_T
	typedef __int128_t h2_int_t;
	typedef __uint128_t h2_uint_t;
	#define H2_SIZEOF_INT_T H2_SIZEOF___INT128_T
	#define H2_SIZEOF_UINT_T H2_SIZEOF___INT128_T
#else
#	error unsupported pointer size
#endif

/** \typedef h2_long_t
 * The h2_long_t type defines the largest signed integer type that supported. 
 */
/** \typedef h2_ulong_t
 * The h2_ulong_t type defines the largest unsigned integer type supported.
 */
#if H2_SIZEOF_LONG >= H2_SIZEOF_LONG_LONG
	typedef signed long h2_long_t;
	typedef unsigned long h2_ulong_t;
	#define H2_SIZEOF_LONG_T H2_SIZEOF_LONG
	#define H2_SIZEOF_ULONG_T H2_SIZEOF_LONG
#elif H2_SIZEOF_LONG_LONG > 0
	typedef signed long long h2_long_t;
	typedef unsigned long long h2_ulong_t;
	#define H2_SIZEOF_LONG_T H2_SIZEOF_LONG_LONG
	#define H2_SIZEOF_ULONG_T H2_SIZEOF_LONG_LONG
#elif H2_SIZEOF___INT64 > 0
	typedef signed __int64 h2_long_t;
	typedef unsigned __int64 h2_ulong_t;
	#define H2_SIZEOF_LONG_T H2_SIZEOF___INT64
	#define H2_SIZEOF_ULONG_T H2_SIZEOF___INT64
#elif H2_SIZEOF___INT64_T > 0
	typedef __int64_t h2_long_t;
	typedef __uint64_t h2_ulong_t;
	#define H2_SIZEOF_LONG_T H2_SIZEOF___INT64_T
	#define H2_SIZEOF_ULONG_T H2_SIZEOF___INT64_T
#else
	typedef signed long h2_long_t;
	typedef unsigned long h2_ulong_t;
	#define H2_SIZEOF_LONG_T H2_SIZEOF_LONG
	#define H2_SIZEOF_ULONG_T H2_SIZEOF_LONG
#endif

/* these two items are revised whenever the size of a 
 * fixed-size integer is determined */
#define H2_SIZEOF_INTMAX_T  0
#define H2_SIZEOF_UINTMAX_T 0

/** \typedef h2_int8_t
 * The h2_int8_t defines an 8-bit signed integer type.
 */
/** \typedef h2_uint8_t
 * The h2_uint8_t type defines an 8-bit unsigned integer type.
 */
#if H2_SIZEOF_CHAR == 1
#	define H2_HAVE_INT8_T
#	define H2_HAVE_UINT8_T
	typedef signed char h2_int8_t;
	typedef unsigned char h2_uint8_t;
#elif H2_SIZEOF___INT8 == 1
#	define H2_HAVE_INT8_T
#	define H2_HAVE_UINT8_T
	typedef signed __int8 h2_int8_t;
	typedef unsigned __int8 h2_uint8_t;
#elif H2_SIZEOF___INT8_T == 1
#	define H2_HAVE_INT8_T
#	define H2_HAVE_UINT8_T
	typedef __int8_t h2_int8_t;
	typedef __uint8_t h2_uint8_t;
#endif

#ifdef H2_HAVE_INT8_T
#	define H2_SIZEOF_INT8_T 1
#	define H2_SIZEOF_UINT8_T 1
#	undef  H2_SIZEOF_INTMAX_T
#	undef  H2_SIZEOF_UINTMAX_T
#	define H2_SIZEOF_INTMAX_T 1
#	define H2_SIZEOF_UINTMAX_T 1
#else
#	define H2_SIZEOF_INT8_T 0
#	define H2_SIZEOF_UINT8_T 0
#endif

/** \typedef h2_int16_t
 * The h2_int16_t defines an 16-bit signed integer type.
 */
/** \typedef h2_uint16_t
 * The h2_uint16_t type defines an 16-bit unsigned integer type.
 */
#if H2_SIZEOF_SHORT == 2
#	define H2_HAVE_INT16_T
#	define H2_HAVE_UINT16_T
	typedef signed short h2_int16_t;
	typedef unsigned short h2_uint16_t;
#elif H2_SIZEOF___INT16 == 2
#	define H2_HAVE_INT16_T
#	define H2_HAVE_UINT16_T
	typedef signed __int16 h2_int16_t;
	typedef unsigned __int16 h2_uint16_t;
#elif H2_SIZEOF___INT16_T == 2
#	define H2_HAVE_INT16_T
#	define H2_HAVE_UINT16_T
	typedef __int16_t h2_int16_t;
	typedef __uint16_t h2_uint16_t;
#endif

#ifdef H2_HAVE_INT16_T
#	define H2_SIZEOF_INT16_T 2
#	define H2_SIZEOF_UINT16_T 2
#	undef  H2_SIZEOF_INTMAX_T
#	undef  H2_SIZEOF_UINTMAX_T
#	define H2_SIZEOF_INTMAX_T 2
#	define H2_SIZEOF_UINTMAX_T 2
#else
#	define H2_SIZEOF_INT16_T 0
#	define H2_SIZEOF_UINT16_T 0
#endif

/** \typedef h2_int32_t
 * The h2_int32_t defines an 32-bit signed integer type.
 */
/** \typedef h2_uint32_t
 * The h2_uint32_t type defines an 32-bit unsigned integer type.
 */
#if H2_SIZEOF_INT == 4
#	define H2_HAVE_INT32_T
#	define H2_HAVE_UINT32_T
	typedef signed int h2_int32_t;
	typedef unsigned int h2_uint32_t;
#elif H2_SIZEOF_LONG == 4
#	define H2_HAVE_INT32_T
#	define H2_HAVE_UINT32_T
	typedef signed long h2_int32_t;
	typedef unsigned long h2_uint32_t;
#elif H2_SIZEOF___INT32 == 4
#	define H2_HAVE_INT32_T
#	define H2_HAVE_UINT32_T
	typedef signed __int32 h2_int32_t;
	typedef unsigned __int32 h2_uint32_t;
#elif H2_SIZEOF___INT32_T == 4
#	define H2_HAVE_INT32_T
#	define H2_HAVE_UINT32_T
	typedef __int32_t h2_int32_t;
	typedef __uint32_t h2_uint32_t;
#endif

#ifdef H2_HAVE_INT32_T
#	define H2_SIZEOF_INT32_T 4
#	define H2_SIZEOF_UINT32_T 4
#	undef  H2_SIZEOF_INTMAX_T
#	undef  H2_SIZEOF_UINTMAX_T
#	define H2_SIZEOF_INTMAX_T 4
#	define H2_SIZEOF_UINTMAX_T 4
#else
#	define H2_SIZEOF_INT32_T 0
#	define H2_SIZEOF_UINT32_T 0
#endif

/** \typedef h2_int64_t
 * The h2_int64_t defines an 64-bit signed integer type.
 */
/** \typedef h2_uint64_t
 * The h2_uint64_t type defines an 64-bit unsigned integer type.
 */
#if H2_SIZEOF_INT == 8
#	define H2_HAVE_INT64_T
#	define H2_HAVE_UINT64_T
	typedef signed int h2_int64_t;
	typedef unsigned int h2_uint64_t;
#elif H2_SIZEOF_LONG == 8
#	define H2_HAVE_INT64_T
#	define H2_HAVE_UINT64_T
	typedef signed long h2_int64_t;
	typedef unsigned long h2_uint64_t;
#elif H2_SIZEOF_LONG_LONG == 8
#	define H2_HAVE_INT64_T
#	define H2_HAVE_UINT64_T
	typedef signed long long h2_int64_t;
	typedef unsigned long long h2_uint64_t;
#elif H2_SIZEOF___INT64 == 8
#	define H2_HAVE_INT64_T
#	define H2_HAVE_UINT64_T
	typedef signed __int64 h2_int64_t;
	typedef unsigned __int64 h2_uint64_t;
#elif H2_SIZEOF___INT64_T == 8
#	define H2_HAVE_INT64_T
#	define H2_HAVE_UINT64_T
	typedef __int64_t h2_int64_t;
	typedef __uint64_t h2_uint64_t;
#endif

#ifdef H2_HAVE_INT64_T
#	define H2_SIZEOF_INT64_T 8
#	define H2_SIZEOF_UINT64_T 8
#	undef  H2_SIZEOF_INTMAX_T
#	undef  H2_SIZEOF_UINTMAX_T
#	define H2_SIZEOF_INTMAX_T 8
#	define H2_SIZEOF_UINTMAX_T 8
#else
#	define H2_SIZEOF_INT64_T 0
#	define H2_SIZEOF_UINT64_T 0
#endif

#if H2_SIZEOF_INT == 16
#	define H2_HAVE_INT128_T
#	define H2_HAVE_UINT128_T
	typedef signed int h2_int128_t;
	typedef unsigned int h2_uint128_t;
#elif H2_SIZEOF_LONG == 16
#	define H2_HAVE_INT128_T
#	define H2_HAVE_UINT128_T
	typedef signed long h2_int128_t;
	typedef unsigned long h2_uint128_t;
#elif H2_SIZEOF_LONG_LONG == 16
#	define H2_HAVE_INT128_T
#	define H2_HAVE_UINT128_T
	typedef signed long long h2_int128_t;
	typedef unsigned long long h2_uint128_t;
#elif H2_SIZEOF___INT128 == 16
#	define H2_HAVE_INT128_T
#	define H2_HAVE_UINT128_T
	typedef signed __int128 h2_int128_t;
	typedef unsigned __int128 h2_uint128_t;
#elif (H2_SIZEOF___INT128_T == 16)
#	define H2_HAVE_INT128_T
#	define H2_HAVE_UINT128_T
	typedef __int128_t h2_int128_t;
	typedef __uint128_t h2_uint128_t;
#endif

#ifdef H2_HAVE_INT128_T
#	define H2_SIZEOF_INT128_T 16
#	define H2_SIZEOF_UINT128_T 16
#	undef  H2_SIZEOF_INTMAX_T
#	undef  H2_SIZEOF_UINTMAX_T
#	define H2_SIZEOF_INTMAX_T 16
#	define H2_SIZEOF_UINTMAX_T 16
#else
#	define H2_SIZEOF_INT128_T 0
#	define H2_SIZEOF_UINT128_T 0
#endif

/**
 * The h2_byte_t defines a byte type.
 */
typedef h2_uint8_t h2_byte_t;
#define H2_SIZEOF_BYTE_T H2_SIZEOF_UINT8_T

/**
 * The h2_size_t type defines an unsigned integer type that is as large as
 * to hold a pointer value.
 */
#if defined(__SIZE_TYPE__) && defined(__SIZEOF_SIZE_T__)
	typedef __SIZE_TYPE__ h2_size_t;
#	define H2_SIZEOF_SIZE_T __SIZEOF_SIZE_T__
#else
	typedef h2_uint_t  h2_size_t;
#	define H2_SIZEOF_SIZE_T H2_SIZEOF_UINT_T
#endif

/**
 * The h2_ssize_t type defines a signed integer type that is as large as
 * to hold a pointer value.
 */
typedef h2_int_t h2_ssize_t;
#define H2_SIZEOF_SSIZE_T H2_SIZEOF_INT_T

/** 
 * The h2_word_t type redefines h2_uint_t. 
 */
typedef h2_uint_t h2_word_t;
#define H2_SIZEOF_WORD_T H2_SIZEOF_UINT_T

/**
 * The h2_uintptr_t redefines h2_uint_t to indicate that you are dealing
 * with a pointer.
 */
typedef h2_uint_t h2_uintptr_t;
#define H2_SIZEOF_UINTPTR_T H2_SIZEOF_UINT_T

/**
 * The h2_untptr_t redefines h2_int_t to indicate that you are dealing
 * with a pointer.
 */
typedef h2_int_t h2_intptr_t;
#define H2_SIZEOF_INTPTR_T H2_SIZEOF_INT_T

/** \typedef h2_intmax_t
 * The h2_llong_t type defines the largest signed integer type supported.
 */
/** \typedef h2_uintmax_t
 * The h2_ullong_t type defines the largest unsigned integer type supported.
 */
#if (H2_SIZEOF_LONG >= H2_SIZEOF_LONG_LONG) && \
    (H2_SIZEOF_LONG >= H2_SIZEOF_INTMAX_T)
	typedef long h2_intmax_t;
	typedef unsigned long h2_uintmax_t;
	#undef H2_SIZEOF_INTMAX_T
	#undef H2_SIZEOF_UINTMAX_T
	#define H2_SIZEOF_INTMAX_T H2_SIZEOF_LONG
	#define H2_SIZEOF_UINTMAX_T H2_SIZEOF_LONG
#elif (H2_SIZEOF_LONG_LONG >= H2_SIZEOF_INTMAX_T) 
	typedef long long h2_intmax_t;
	typedef unsigned long long h2_uintmax_t;
	#undef H2_SIZEOF_INTMAX_T
	#undef H2_SIZEOF_UINTMAX_T
	#define H2_SIZEOF_INTMAX_T H2_SIZEOF_LONG_LONG
	#define H2_SIZEOF_UINTMAX_T H2_SIZEOF_LONG_LONG
#elif (H2_SIZEOF_INTMAX_T == H2_SIZEOF_INT128_T)
	typedef h2_int128_t h2_intmax_t;
	typedef h2_uint128_t h2_uintmax_t;
	/* H2_SIZEOF_INTMAX_T and H2_SIZEOF_UINTMAX_T are
	 * defined when h2_int128_t is defined */
#elif (H2_SIZEOF_INTMAX_T == H2_SIZEOF_INT64_T)
	typedef h2_int64_t h2_intmax_t;
	typedef h2_uint64_t h2_uintmax_t;
	/* H2_SIZEOF_INTMAX_T and H2_SIZEOF_UINTMAX_T are
	 * defined when h2_int64_t is defined */
#elif (H2_SIZEOF_INTMAX_T == H2_SIZEOF_INT32_T)
	typedef h2_int32_t h2_intmax_t;
	typedef h2_uint32_t h2_uintmax_t;
	/* H2_SIZEOF_INTMAX_T and H2_SIZEOF_UINTMAX_T are
	 * defined when h2_int32_t is defined */
#elif (H2_SIZEOF_INTMAX_T == H2_SIZEOF_INT16_T)
	typedef h2_int16_t h2_intmax_t;
	typedef h2_uint16_t h2_uintmax_t;
	/* H2_SIZEOF_INTMAX_T and H2_SIZEOF_UINTMAX_T are
	 * defined when h2_int16_t is defined */
#elif (H2_SIZEOF_INTMAX_T == H2_SIZEOF_INT8_T)
	typedef h2_int8_t h2_intmax_t;
	typedef h2_uint8_t h2_uintmax_t;
	/* H2_SIZEOF_INTMAX_T and H2_SIZEOF_UINTMAX_T are
	 * defined when h2_int8_t is defined */
#else
#	error FATAL. THIS MUST NOT HAPPEN
#endif


/** \typedef h2_flt_t
 * The h2_flt_t type defines the largest floating-pointer number type
 * naturally supported.
 */
#if defined(__FreeBSD__) || defined(__MINGW32__)
	/* TODO: check if the support for long double is complete.
	 *       if so, use long double for h2_flt_t */
	typedef double h2_flt_t;
#	define H2_SIZEOF_FLT_T H2_SIZEOF_DOUBLE
#elif H2_SIZEOF_LONG_DOUBLE > H2_SIZEOF_DOUBLE
	typedef long double h2_flt_t;
#	define H2_SIZEOF_FLT_T H2_SIZEOF_LONG_DOUBLE
#else
	typedef double h2_flt_t;
#	define H2_SIZEOF_FLT_T H2_SIZEOF_DOUBLE
#endif

/** \typedef h2_fltmax_t
 * The h2_fltmax_t type defines the largest floating-pointer number type
 * ever supported.
 */
#if H2_SIZEOF___FLOAT128 >= H2_SIZEOF_FLT_T
	/* the size of long double may be equal to the size of __float128
	 * for alignment on some platforms */
	typedef __float128 h2_fltmax_t;
#	define H2_SIZEOF_FLTMAX_T H2_SIZEOF___FLOAT128
#else
	typedef h2_flt_t h2_fltmax_t;
#	define H2_SIZEOF_FLTMAX_T H2_SIZEOF_FLT_T
#endif

/** \typedef h2_ptrdiff_t
 */
typedef h2_ssize_t h2_ptrdiff_t;
#define H2_SIZEOF_PTRDIFF_T H2_SIZEOF_SSIZE_T

/** \typdef h2_mchar_t
 * The h2_mchar_t type is an alias to the char type.
 */
typedef char        h2_mchar_t;
typedef int         h2_mcint_t;
#define H2_SIZEOF_MCHAR_T H2_SIZEOF_CHAR
#define H2_SIZEOF_MCINT_T H2_SIZEOF_INT_T

/** \typedef h2_wchar_t
 * The h2_wchar_t type defines a 16-bit wide character type.
 */
typedef h2_uint16_t h2_wchar_t;
typedef h2_uint32_t h2_wcint_t;
#define H2_SIZEOF_WCHAR_T H2_SIZEOF_UINT16_T
#define H2_SIZEOF_WCINT_T H2_SIZEOF_UINT32_T

/** typedef h2_wwchar_t
 * The h2_wwchar_t type defines a 32-bit wide character type.
 */
typedef h2_uint32_t h2_wwchar_t;
typedef h2_uint32_t h2_wwcint_t;
#define H2_SIZEOF_WWCHAR_T H2_SIZEOF_UINT32_T
#define H2_SIZEOF_WWCINT_T H2_SIZEOF_UINT32_T

/** typedef h2_mcstr_t
 * The h2_mcstr_t type defines a structure containing a pointer to
 * a h2_mchar_t array and its length
 */
struct h2_mcstr_t
{
	h2_mchar_t* ptr;
	h2_size_t len;
};

/** typedef h2_wcstr_t
 * The h2_wcstr_t type defines a structure containing a pointer to
 * a h2_wchar_t array and its length
 */
struct h2_wcstr_t
{
	h2_wchar_t* ptr;
	h2_size_t len;
};


/** typedef h2_wwcstr_t
 * The h2_wwcstr_t type defines a structure containing a pointer to
 * a h2_wwchar_t array and its length
 */
struct h2_wwcstr_t
{
	h2_wwchar_t* ptr;
	h2_size_t len;
};

typedef struct h2_mcstr_t h2_mcstr_t;
typedef struct h2_wcstr_t h2_wcstr_t;
typedef struct h2_wwcstr_t h2_wwcstr_t;

/** typedef h2_char_t
 * The h2_char_t type defines the primary character type.
 */
#if defined(H2_CHAR_IS_MCHAR)
	typedef h2_mchar_t h2_char_t;
	typedef h2_mcint_t h2_cint_t;
	typedef h2_mcstr_t h2_cstr_t;
#elif defined(H2_CHAR_IS_WCHAR)
	typedef h2_wchar_t h2_char_t;
	typedef h2_wcint_t h2_cint_t;
	typedef h2_wcstr_t h2_cstr_t;
#elif defined(H2_CHAR_IS_WWCHAR)
	typedef h2_wwchar_t h2_char_t;
	typedef h2_wwcint_t h2_cint_t;
	typedef h2_wwcstr_t h2_cstr_t;
#else
#	error Unknown default character type
#endif  

/** typedef h2_wxchar_t
 * The h2_wxchar_t type defines the widest character type supported.
 */
typedef h2_wwchar_t h2_wxchar_t;
typedef h2_wwcint_t h2_wxcint_t;
#define H2_SIZEOF_WXCHAR_T H2_SIZEOF_WWCHAR_T
#define H2_SIZEOF_WXCINT_T H2_SIZEOF_WWCINT_T


#if defined(_WIN32) && defined(H2_ENABLE_WCHAR)
#	define H2_OSCHAR_IS_WCHAR 1
#elif defined(H2_ENABLE_MCHAR)
#	define H2_OSCHAR_IS_MCHAR 1
#else
#	error Required OS character type is not enabled.
#endif

/** typedef h2_oschar_t
 * The h2_oschar_t type defines the character type the operating system
 * calls use. 
 */
#if defined(H2_OSCHAR_IS_MCHAR)
	typedef h2_mchar_t h2_oschar_t;
	typedef h2_mcint_t h2_oscint_t;
	typedef h2_mcstr_t h2_oscstr_t;
#	define H2_SIZEOF_OSCHAR_T H2_SIZEOF_MCHAR_T
#	define H2_SIZEOF_OSCINT_T H2_SIZEOF_MCINT_T
#elif defined(H2_OSCHAR_IS_WCHAR)
	typedef h2_wchar_t h2_oschar_t;
	typedef h2_wcint_t h2_oscint_t;
	typedef h2_wcstr_t h2_oscstr_t;
#	define H2_SIZEOF_OSCHAR_T H2_SIZEOF_WCHAR_T
#	define H2_SIZEOF_OSCINT_T H2_SIZEOF_WCINT_T
#elif defined(H2_OSCHAR_IS_WWCHAR)
	typedef h2_wwchar_t h2_oschar_t;
	typedef h2_wwcint_t h2_oscint_t;
	typedef h2_wwcstr_t h2_oscstr_t;
#	define H2_SIZEOF_OSCHAR_T H2_SIZEOF_WWCHAR_T
#	define H2_SIZEOF_OSCINT_T H2_SIZEOF_WWCINT_T
#else
#	error Unknown OS character type
#endif  

enum h2_char_type_t
{
	H2_MCHAR_T  = 0,
	H2_WCHAR_T  = 1,
	H2_WWCHAR_T = 2,

#if defined(H2_CHAR_IS_MCHAR)
	H2_CHAR_T = H2_MCHAR_T,
#elif defined(H2_CHAR_IS_WCHAR)
	H2_CHAR_T = H2_WCHAR_T,
#elif defined(H2_CHAR_IS_WWCHAR)
	H2_CHAR_T = H2_WWCHAR_T,
#else
#	error Unknown default character type
#endif  

#if defined(H2_OSCHAR_IS_MCHAR)
	H2_OSCHAR_T = H2_MCHAR_T,
#elif defined(H2_OSCHAR_IS_WCHAR)
	H2_OSCHAR_T = H2_WCHAR_T,
#elif defined(H2_OSCHAR_IS_WWCHAR)
	H2_OSCHAR_T = H2_WWCHAR_T,
#else
#	error Unknown OS character type
#endif  
};

typedef enum h2_char_type_t h2_char_type_t;


typedef struct h2_cmgr_t h2_cmgr_t;

typedef h2_size_t (*h2_cmgr_mbtowc_t) (
	h2_cmgr_t*        cmgr,
	const h2_mchar_t* mb,
	h2_size_t         size,
	h2_wxchar_t*      wc
);

typedef h2_size_t (*h2_cmgr_wctomb_t) (
	h2_cmgr_t*   cmgr,
	h2_wxchar_t  wc,
	h2_mchar_t*  mb,
	h2_size_t    size
);

/*
typedef h2_size_t (*h2_cmgr_mtowx_t) (
	h2_cmgr_t*        cmgr,
	const h2_mchar_t* mb,
	h2_size_t         size,
	h2_wxchar_t*      wc
);

typedef h2_size_t (*h2_cmgr_wxtom_t) (
	h2_cmgr_t*   cmgr,
	h2_wxchar_t  wc,
	h2_mchar_t*  mb,
	h2_size_t    size
);
*/




/**
 * The h2_cmgr_t type defines the character-level interface to
 * multibyte/wide-character conversion. This interface doesn't
 * provide any facility to store conversion state in a context
 * independent manner. This leads to the limitation that it can
 * handle a stateless multibyte encoding only.
 */
struct h2_cmgr_t
{
	h2_cmgr_mbtowc_t mbtowc;
	h2_cmgr_wctomb_t wctomb;

/*
	h2_cmgr_mtowx_t mtowx;
	h2_cmgr_wxtom_t wxtom;
*/

	
	void* ctx;
};




typedef struct h2_mmgr_t h2_mmgr_t;

/**
 * allocate a memory chunk of the size @a n.
 * @return pointer to a memory chunk on success, QSE_NULL on failure.
 */
typedef void* (*h2_mmgr_alloc_t)   (h2_mmgr_t* mmgr, h2_size_t n);
/**
 * resize a memory chunk pointed to by @a ptr to the size @a n.
 * @return pointer to a memory chunk on success, QSE_NULL on failure.
 */
typedef void* (*h2_mmgr_realloc_t) (h2_mmgr_t* mmgr, void* ptr, h2_size_t n);
/**
 * free a memory chunk pointed to by @a ptr.
 */
typedef void  (*h2_mmgr_free_t)    (h2_mmgr_t* mmgr, void* ptr);

/**
 * The h2_mmgr_t type defines the memory management interface.
 * As the type is merely a structure, it is just used as a single container
 * for memory management functions with a pointer to user-defined data.
 * The user-defined data pointer @a ctx is passed to each memory management
 * function whenever it is called. You can allocate, reallocate, and free
 * a memory chunk.
 *
 * For example, a h2_xxx_open() function accepts a pointer of the h2_mmgr_t
 * type and the xxx object uses it to manage dynamic data within the object.
 */
struct h2_mmgr_t
{
	h2_mmgr_alloc_t   alloc;   /**< allocation function */
	h2_mmgr_realloc_t realloc; /**< resizing function */
	h2_mmgr_free_t    free;    /**< disposal function */
	void*             ctx;     /**< user-defined data pointer */
};

/* Special definition to use Unicode APIs on Windows */
#if defined(_WIN32)
#	if !defined(UNICODE)
#		define UNICODE
#	endif
#	if !defined(_UNICODE)
#		define _UNICODE
#	endif
#endif


#endif
