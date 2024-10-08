AUTOMAKE_OPTIONS = nostdinc

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
	hcl-chr.h \
	hcl-cmn.h \
	hcl-fmt.h \
	hcl-opt.h \
	hcl-pac1.h \
	hcl-rbt.h \
	hcl-upac.h \
	hcl-utl.h \
	hcl-xma.h

pkglib_LTLIBRARIES = libhcl.la
libhcl_la_SOURCES = \
	hcl-prv.h \
	bigint.c \
	chr.c \
	cnode.c \
	comp.c \
	debug.c \
	decode.c \
	dic.c \
	err.c \
	exec.c \
	fmt-imp.h \
	fmt.c \
	gc.c \
	hcl.c \
	heap.c \
	mb8.c \
	number.c \
	obj.c \
	opt-impl.h \
	opt.c \
	prim.c \
	print.c \
	rbt.c \
	read.c \
	std.c \
	sym.c \
	uch-case.h \
	uch-prop.h \
	utf16.c \
	utf8.c \
	utl.c \
	xchg.c \
	xma.c 
libhcl_la_CPPFLAGS = $(CPPFLAGS_LIB_COMMON)
libhcl_la_LDFLAGS = $(LDFLAGS_LIB_COMMON)
libhcl_la_LIBADD = $(LIBADD_LIB_COMMON)

if ENABLE_STATIC_MODULE
libhcl_la_LIBADD += ../mod/libhcl-core.la
libhcl_la_LIBADD += ../mod/libhcl-dic.la
libhcl_la_LIBADD += ../mod/libhcl-sys.la
endif

if ENABLE_HCLX
pkglib_LTLIBRARIES += libhclx.la
pkginclude_HEADERS += hcl-x.h hcl-tmr.h hcl-json.h
libhclx_la_SOURCES = \
	hcl-json.h \
	hcl-tmr.h \
	hcl-x.h \
	hcl-xtul.h \
	json.c \
	tmr.c \
	x-client.c \
	x-proto.c \
	x-server.c \
	x-sys.c \
	x-utl.c \
	x-utl-sa.h
libhclx_la_CPPFLAGS = $(CPPFLAGS_LIB_COMMON) $(CPPFLAGS_PFMOD)
libhclx_la_LDFLAGS = $(LDFLAGS_LIB_COMMON)
libhclx_la_LIBADD = libhcl.la $(LIBADD_LIB_COMMON)
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

