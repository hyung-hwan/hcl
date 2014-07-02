#include <h2/cmn/str.h>

#if defined(H2_ENABLE_MCHAR)
#	define xchar_t h2_mchar_t
#	define XFUN(x) H2_FUN_M(x)
#	define XCHAR(x) H2_MCHAR(x)
#	include "str-cmp.h"
#	include "str-cpy.h"
#	include "str-fmc.h"
#	include "str-len.h"
#	undef xchar_t
#	undef XFUN
#	undef XCHAR
#endif

#if defined(H2_ENABLE_WCHAR)
#	define xchar_t h2_wchar_t
#	define XFUN(x) H2_FUN_W(x)
#	define XCHAR(x) H2_WCHAR(x)
#	include "str-cmp.h"
#	include "str-cpy.h"
#	include "str-fmc.h"
#	include "str-len.h"
#	undef xchar_t
#	undef XFUN
#	undef XCHAR
#endif

#if defined(H2_ENABLE_WWCHAR)
#	define xchar_t h2_wwchar_t
#	define XFUN(x) H2_FUN_WW(x)
#	define XCHAR(x) H2_WWCHAR(x)
#	include "str-cmp.h"
#	include "str-cpy.h"
#	include "str-fmc.h"
#	include "str-len.h"
#	undef xchar_t
#	undef XFUN
#	undef XCHAR
#endif
