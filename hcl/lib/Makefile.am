AUTOMAKE_OPTIONS = nostdinc

CPPFLAGS_ALL_COMMON = \
	-I$(abs_builddir) \
	-I$(abs_srcdir) \
	-I$(includedir)	

LDFLAGS_ALL_COMMON = -L$(abs_builddir) -L$(libdir)

##################################################
# MAIN LIBRARY 
##################################################

CPPFLAGS_LIB_COMMON = $(CPPFLAGS_ALL_COMMON)
LDFLAGS_LIB_COMMON = $(LDFLAGS_ALL_COMMON) -version-info 1:0:0 -no-undefined
LIBADD_LIB_COMMON = $(LIBM)

if ENABLE_LIBLTDL
CPPFLAGS_LIB_COMMON += $(LTDLINCL)
LIBADD_LIB_COMMON += $(LIBLTDL)
else
LIBADD_LIB_COMMON += $(DL_LIBS)
endif

pkgincludedir = $(includedir)
pkglibdir = $(libdir)

pkginclude_HEADERS = \
	hcl-cfg.h \
	hcl-cmn.h \
	hcl-rbt.h \
	hcl-utl.h \
	hcl.h

pkglib_LTLIBRARIES = libhcl.la
libhcl_la_SOURCES = \
	hcl-prv.h \
	logfmtv.h \
	bigint.c \
	comp.c \
	debug.c \
	decode.c \
	dic.c \
	err.c \
	exec.c \
	gc.c \
	hcl.c \
	heap.c \
	logfmt.c \
	obj.c \
	opt-imp.h \
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

bin_PROGRAMS = hcl
hcl_SOURCES = main.c
hcl_CPPFLAGS = $(CPPFLAGS_LIB_COMMON)
hcl_LDFLAGS = $(LDFLAGS_LIB_COMMON)
hcl_LDADD = $(LIBADD_LIB_COMMON) -lhcl #-ldyncall_s
hcl_DEPENDENCIES = libhcl.la


install-data-hook:
	@echo "#ifndef _HCL_CFG_H_" > "$(DESTDIR)$(pkgincludedir)/hcl-cfg.h"
	@echo "#define _HCL_CFG_H_" >> "$(DESTDIR)$(pkgincludedir)/hcl-cfg.h"
	@$(EGREP) "#define[ ]+HCL_" "$(abs_builddir)/hcl-cfg.h" >> "$(DESTDIR)$(pkgincludedir)/hcl-cfg.h" 
	@echo "#endif" >> "$(DESTDIR)$(pkgincludedir)/hcl-cfg.h"
	@rm -f "$(DESTDIR)$(pkgincludedir)/hcl-cfg.h.in"
	@$(SED) 's|/\*#define HCL_HAVE_CFG_H\*/|#define HCL_HAVE_CFG_H|' "$(srcdir)/hcl-cmn.h" > "$(DESTDIR)$(pkgincludedir)/hcl-cmn.h"

uninstall-hook:
	@rm -f "$(DESTDIR)$(pkgincludedir)/hcl-cfg.h"

