#ifndef _H2_STR_H_
#define _H2_STR_H_

#include <h2/types.h>
#include <h2/macros.h>

#if defined(H2_ENABLE_MCHAR)
#	define h2_xchar_t h2_mchar_t
#	define H2_XFUN(x) H2_FUN_M(x)
#	include "str-priv.h"
#	undef h2_xchar_t
#	undef H2_XFUN
#endif

#if defined(H2_ENABLE_WCHAR)
#	define h2_xchar_t h2_wchar_t
#	define H2_XFUN(x) H2_FUN_W(x)
#	include "str-priv.h"
#	undef h2_xchar_t
#	undef H2_XFUN
#endif

#if defined(H2_ENABLE_WWCHAR)
#	define h2_xchar_t h2_wwchar_t
#	define H2_XFUN(x) H2_FUN_WW(x)
#	include "str-priv.h"
#	undef h2_xchar_t
#	undef H2_XFUN
#endif

#define h2_strcmp  H2_FUN(strcmp)
#define h2_strcpy  H2_FUN(strcpy)
#define h2_strfmc  H2_FUN(strfmc)
#define h2_strlen  H2_FUN(strlen)

#endif
