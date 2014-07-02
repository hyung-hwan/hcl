
#ifndef _H2_MACROS_H_
#define _H2_MACROS_H_

#include <h2/types.h>

#define H2_MCHAR(x) ((h2_mchar_t)(x))
#define H2_WCHAR(x) ((h2_wchar_t)(x))
#define H2_WWCHAR(x) ((h2_wwchar_t)(x))

#define H2_FUN_M(x)  h2_ ## x ## _m
#define H2_FUN_W(x)  h2_ ## x ## _w
#define H2_FUN_WW(x) h2_ ## x ## _ww

#if defined(H2_CHAR_IS_MCHAR)
#	define H2_CHAR(x) H2_MCHAR(x)
#	define H2_FUN(x) H2_FUN_M(x)
#elif defined(H2_CHAR_IS_WCHAR)
#	define H2_CHAR(x) H2_WCHAR(x)
#	define H2_FUN(x) H2_FUN_W(x)
#elif defined(H2_CHAR_IS_WWCHAR)
#	define H2_CHAR(x) H2_WWCHAR(x)
#	define H2_FUN(x) H2_FUN_WW(x)
#else
#	error Unknown default character type
#endif  


/* Basic character codes... */
#define H2_CHAR_NUL 0
#define H2_CHAR_SPC 32


/**
 * The H2_SIZEOF() macro gets data size in bytes. It is equivalent to the
 * sizeof operator. The following code snippet should print sizeof(int)*128.
 * @code
 * int x[128];
 * printf ("%d\n", (int)H2_SIZEOF(x));
 * @endcode
 */
#define H2_SIZEOF(n)  (sizeof(n))

/**
 * The H2_COUNTOF() macro returns the number elements in an array.
 * The following code snippet should print 128.
 * @code
 * int x[128];
 * printf ("%d\n", (int)H2_COUNTOF(x));
 * @endcode
 */
#define H2_COUNTOF(n) (sizeof(n)/sizeof(n[0]))

/** 
 * The #H2_NULL macro defines a special pointer value to indicate an error or
 * that it does not point to anything.
 */
#ifdef __cplusplus
#	if H2_SIZEOF_VOID_P == H2_SIZEOF_INT
#		define H2_NULL (0)
#	elif H2_SIZEOF_VOID_P == H2_SIZEOF_LONG
#		define H2_NULL (0l)
#	elif H2_SIZEOF_VOID_P == H2_SIZEOF_LONG_LONG
#		define H2_NULL (0ll)
#	else
#		define H2_NULL (0)
#	endif
#else
#	define H2_NULL ((void*)0)
#endif


/* TODO: define these properly */
#define H2_ASSERT(expr) ((void)0)
#define H2_ASSERTX(expr,desc) ((void)0)



#if defined(__STDC_VERSION__) && (__STDC_VERSION__>=199901L)
#	define H2_INLINE inline
#	define H2_HAVE_INLINE
#elif defined(__GNUC__) && defined(__GNUC_GNU_INLINE__)
#	define H2_INLINE /*extern*/ inline
#	define H2_HAVE_INLINE
#else
#	define H2_INLINE
#	undef H2_HAVE_INLINE
#endif

#endif
