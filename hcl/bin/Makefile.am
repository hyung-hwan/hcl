AUTOMAKE_OPTIONS = nostdinc

CPPFLAGS_COMMON = \
	-I$(abs_builddir) \
	-I$(abs_builddir)/../lib  \
	-I$(abs_srcdir) \
	-I$(abs_srcdir)/../lib \
	-I$(includedir)	

#### $(LTDLINCL)

CFLAGS_COMMON =
LDFLAGS_COMMON = -L$(abs_builddir)/../lib -L$(libdir)
LIBADD_COMMON =

##################################################
# MAIN LIBRARY 
##################################################

if ENABLE_LIBLTDL
LIBADD_COMMON += $(LTDL_LIBS)
else
LIBADD_COMMON += $(DL_LIBS)
endif

bin_PROGRAMS = hcl
hcl_SOURCES = main.c
hcl_CPPFLAGS = $(CPPFLAGS_COMMON) 
hcl_LDFLAGS = $(LDFLAGS_COMMON)
hcl_LDADD = ../lib/libhcl.la $(LIBADD_COMMON)

if ENABLE_HCLX

bin_PROGRAMS += hcls
hcls_SOURCES = main-s.c
hcls_CPPFLAGS = $(CPPFLAGS_COMMON)
hcls_LDFLAGS = $(LDFLAGS_COMMON)
hcls_LDADD = ../lib/libhclx.la $(LIBADD_COMMON)

bin_PROGRAMS += hclc
hclc_SOURCES = main-c.c
hclc_CPPFLAGS = $(CPPFLAGS_COMMON)
hclc_LDFLAGS = $(LDFLAGS_COMMON)
hclc_LDADD = ../lib/libhclx.la $(LIBADD_COMMON)

bin_PROGRAMS += hclj
hclj_SOURCES = main-j.c
hclj_CPPFLAGS = $(CPPFLAGS_COMMON)
hclj_LDFLAGS = $(LDFLAGS_COMMON)
hclj_LDADD = ../lib/libhclx.la $(LIBADD_COMMON)
endif
