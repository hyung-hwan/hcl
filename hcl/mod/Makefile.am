AUTOMAKE_OPTIONS = nostdinc

##AM_CFLAGS = $(PTHREAD_CFLAGS)

CPPFLAGS_COMMON = \
	-I$(abs_builddir) \
	-I$(abs_builddir)/../lib \
	-I$(abs_srcdir) \
	-I$(abs_srcdir)/../lib \
	-I$(includedir)

if ENABLE_STATIC_MODULE

##################################################
# STATIC MODULES BUILT INTO MAIN LIBRARY
##################################################
LDFLAGS_COMMON =-L$(libdir) -version-info 1:0:0 -no-undefined
LIBADD_COMMON =

noinst_LTLIBRARIES =

noinst_LTLIBRARIES += libhcl-arr.la
noinst_LTLIBRARIES += libhcl-dic.la
noinst_LTLIBRARIES += libhcl-str.la
noinst_LTLIBRARIES += libhcl-sys.la

else

##################################################
# DYNAMIC MODULES 
##################################################
LDFLAGS_COMMON = -L$(abs_builddir)/../lib -L$(libdir) -version-info 1:0:0 -no-undefined
LIBADD_COMMON = -lhcl

pkgmodexecdir = $(libdir)
pkgmodexec_LTLIBRARIES =

pkgmodexec_LTLIBRARIES += libhcl-arr.la 
pkgmodexec_LTLIBRARIES += libhcl-dic.la 
pkgmodexec_LTLIBRARIES += libhcl-str.la 
pkgmodexec_LTLIBRARIES += libhcl-sys.la 

endif

libhcl_arr_la_SOURCES = arr.c _arr.h
libhcl_arr_la_CPPFLAGS = $(CPPFLAGS_COMMON)
libhcl_arr_la_LDFLAGS = $(LDFLAGS_COMMON)
libhcl_arr_la_LIBADD = $(LIBADD_COMMON) 

libhcl_dic_la_SOURCES = dic.c _dic.h
libhcl_dic_la_CPPFLAGS = $(CPPFLAGS_COMMON)
libhcl_dic_la_LDFLAGS = $(LDFLAGS_COMMON)
libhcl_dic_la_LIBADD = $(LIBADD_COMMON) 

libhcl_str_la_SOURCES = str.c _str.h
libhcl_str_la_CPPFLAGS = $(CPPFLAGS_COMMON)
libhcl_str_la_LDFLAGS = $(LDFLAGS_COMMON)
libhcl_str_la_LIBADD = $(LIBADD_COMMON) 

libhcl_sys_la_SOURCES = sys.c _sys.h
libhcl_sys_la_CPPFLAGS = $(CPPFLAGS_COMMON)
libhcl_sys_la_LDFLAGS = $(LDFLAGS_COMMON)
libhcl_sys_la_LIBADD = $(LIBADD_COMMON) 
