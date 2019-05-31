AUTOMAKE_OPTIONS = nostdinc

AM_CFLAGS = $(PTHREAD_CFLAGS)

CPPFLAGS_ALL_COMMON = \
	-I$(abs_builddir) \
	-I$(abs_srcdir) \
	-I$(includedir)	

if ENABLE_STATIC_MODULE
LDFLAGS_ALL_COMMON = -L$(abs_builddir)/../mod -L$(abs_builddir) -L$(libdir)
else
LDFLAGS_ALL_COMMON = -L$(abs_builddir) -L$(libdir)
endif


##################################################
# MAIN LIBRARY 
##################################################

CPPFLAGS_LIB_COMMON = $(CPPFLAGS_ALL_COMMON)
LDFLAGS_LIB_COMMON = $(LDFLAGS_ALL_COMMON) -version-info 1:0:0 -no-undefined
LIBADD_LIB_COMMON = $(LIBM)

if ENABLE_LIBLTDL
LIBADD_LIB_COMMON += $(LTDL_LIBS)
else
LIBADD_LIB_COMMON += $(DL_LIBS)
endif

if ENABLE_LIBUNWIND
LIBADD_LIB_COMMON += $(UNWIND_LIBS)
endif

if WIN32
# you must adjust the value of DEFAULT_MODPOSTFIX according 
# to the first number in -version-info above
CPPFLAGS_PFMOD = -DHCL_DEFAULT_PFMODPREFIX=\"libhcl-\" -DHCL_DEFAULT_PFMODPOSTFIX=\"-1.dll\"
else
CPPFLAGS_PFMOD = -DHCL_DEFAULT_PFMODPREFIX=\"$(libdir)/libhcl-\" 
if MACOSX
CPPFLAGS_PFMOD += -DHCL_DEFAULT_PFMODPOSTFIX=\".dylib\"
else
CPPFLAGS_PFMOD += -DHCL_DEFAULT_PFMODPOSTFIX=\".so\"
endif
endif

pkgincludedir = $(includedir)
pkglibdir = $(libdir)

pkginclude_HEADERS = \
	hcl.h \
	hcl-cmn.h \
	hcl-opt.h \
	hcl-rbt.h \
	hcl-utl.h

pkglib_LTLIBRARIES = libhcl.la
libhcl_la_SOURCES = \
	hcl.h \
	hcl-cmn.h \
	hcl-opt.h \
	hcl-rbt.h \
	hcl-utl.h \
	hcl-prv.h \
	bigint.c \
	comp.c \
	debug.c \
	decode.c \
	dic.c \
	err.c \
	exec.c \
	fmt.c \
	gc.c \
	hcl.c \
	heap.c \
	number.c \
	obj.c \
	opt-impl.h \
	opt.c \
	prim.c \
	print.c \
	rbt.c \
	read.c \
	sym.c \
	utf8.c \
	utl.c 
libhcl_la_CPPFLAGS = $(CPPFLAGS_LIB_COMMON)
libhcl_la_LDFLAGS = $(LDFLAGS_LIB_COMMON)
libhcl_la_LIBADD = $(LIBADD_LIB_COMMON)
libhcl_la_DEPENDENCIES =

if ENABLE_STATIC_MODULE
libhcl_la_LIBADD += -lhcl-arr
libhcl_la_DEPENDENCIES += $(abs_builddir)/../mod/libhcl-arr.la
libhcl_la_LIBADD += -lhcl-dic
libhcl_la_DEPENDENCIES += $(abs_builddir)/../mod/libhcl-dic.la
libhcl_la_LIBADD += -lhcl-str
libhcl_la_DEPENDENCIES += $(abs_builddir)/../mod/libhcl-str.la
libhcl_la_LIBADD += -lhcl-sys
libhcl_la_DEPENDENCIES += $(abs_builddir)/../mod/libhcl-sys.la
endif

if ENABLE_HCLEX
pkglib_LTLIBRARIES += libhclx.la
pkginclude_HEADERS += hcl-c.h hcl-s.h hcl-tmr.h hcl-xutl.h hcl-json.h
libhclx_la_SOURCES = \
	tmr.c hcl-tmr.h \
	xutl.c xutl-sa.h hcl-xutl.h \
	json.c hcl-json.h \
	cb-impl.h cb-impl.c \
	hcl-s.c hcl-s.h \
	hcl-c.c hcl-c.h
libhclx_la_CPPFLAGS = $(CPPFLAGS_LIB_COMMON) $(CPPFLAGS_PFMOD)
libhclx_la_LDFLAGS = $(LDFLAGS_LIB_COMMON)
libhclx_la_LIBADD = $(LIBADD_LIB_COMMON) $(PTHREAD_LIBS) -lhcl
libhclx_la_DEPENDENCIES = libhcl.la
endif


install-data-hook:
	@echo "#ifndef _HCL_CFG_H_" > "$(DESTDIR)$(pkgincludedir)/hcl-cfg.h"
	@echo "#define _HCL_CFG_H_" >> "$(DESTDIR)$(pkgincludedir)/hcl-cfg.h"
	@$(EGREP) "#define[ ]+HCL_" "$(abs_builddir)/hcl-cfg.h" >> "$(DESTDIR)$(pkgincludedir)/hcl-cfg.h" 
	@echo "#endif" >> "$(DESTDIR)$(pkgincludedir)/hcl-cfg.h"
	@rm -f "$(DESTDIR)$(pkgincludedir)/hcl-cfg.h.in"
	@$(SED) 's|/\*#define HCL_HAVE_CFG_H\*/|#define HCL_HAVE_CFG_H|' "$(srcdir)/hcl-cmn.h" > "$(DESTDIR)$(pkgincludedir)/hcl-cmn.h"

uninstall-hook:
	@rm -f "$(DESTDIR)$(pkgincludedir)/hcl-cfg.h"

