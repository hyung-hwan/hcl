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


pkgincludedir = $(includedir)
pkglibdir = $(libdir)

pkginclude_HEADERS = \
	hcl-cfg.h \
	hcl-cmn.h \
	hcl-rbt.h \
	hcl-tmr.h \
	hcl-utl.h \
	hcl.h

pkglib_LTLIBRARIES = libhcl.la
libhcl_la_SOURCES = \
	hcl-prv.h \
	logfmtv.h \
	sa-utl.h \
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
	tmr.c \
	utf8.c \
	utl.c 
libhcl_la_CPPFLAGS = $(CPPFLAGS_LIB_COMMON)
libhcl_la_LDFLAGS = $(LDFLAGS_LIB_COMMON)
libhcl_la_LIBADD = $(LIBADD_LIB_COMMON)
libhcl_la_DEPENDENCIES =

if ENABLE_HCLS
pkglib_LTLIBRARIES += libhcls.la
libhcls_la_SOURCES = hcl-s.c hcl-s.h
libhcls_la_CPPFLAGS = $(CPPFLAGS_LIB_COMMON)
libhcls_la_LDFLAGS = $(LDFLAGS_LIB_COMMON)
libhcls_la_LIBADD = $(LIBADD_LIB_COMMON) $(PTHREAD_LIBS)
libhcls_la_DEPENDENCIES =
endif

if ENABLE_STATIC_MODULE
libhcl_la_LIBADD += -lhcl-arr
libhcl_la_DEPENDENCIES += $(abs_builddir)/../mod/libhcl-arr.la
libhcl_la_LIBADD += -lhcl-dic
libhcl_la_DEPENDENCIES += $(abs_builddir)/../mod/libhcl-dic.la
libhcl_la_LIBADD += -lhcl-str
libhcl_la_DEPENDENCIES += $(abs_builddir)/../mod/libhcl-str.la
endif

bin_PROGRAMS = hcl
hcl_SOURCES = main.c 
hcl_CPPFLAGS = $(CPPFLAGS_LIB_COMMON)
hcl_LDFLAGS = $(LDFLAGS_LIB_COMMON)
hcl_LDADD = $(LIBADD_LIB_COMMON) -lhcl #-ldyncall_s
if ENABLE_STATIC_MODULE
hcl_DEPENDENCIES = libhcl.la
endif

if ENABLE_HCLS
bin_PROGRAMS += hcls
hcls_SOURCES = main2.c
hcls_CPPFLAGS = $(CPPFLAGS_LIB_COMMON)
hcls_LDFLAGS = $(LDFLAGS_LIB_COMMON)
hcls_LDADD = $(LIBADD_LIB_COMMON) $(PTHREAD_LIBS) -lhcl -lhcls #-ldyncall_s
if ENABLE_STATIC_MODULE
hcls_DEPENDENCIES = libhcl.la libhcls.la
endif
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

