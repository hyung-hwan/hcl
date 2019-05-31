AUTOMAKE_OPTIONS = nostdinc

CPPFLAGS_ALL_COMMON = \
	-I$(abs_builddir) \
	-I$(abs_builddir)/../lib  \
	-I$(abs_srcdir) \
	-I$(abs_srcdir)/../lib \
	-I$(includedir)	

LDFLAGS_ALL_COMMON = -L$(abs_builddir) -L$(abs_builddir)/../lib -L$(libdir)

##################################################
# MAIN LIBRARY 
##################################################

CPPFLAGS_LIB_COMMON = $(CPPFLAGS_ALL_COMMON) $(LTDLINCL)
LDFLAGS_LIB_COMMON = $(LDFLAGS_ALL_COMMON) -version-info 1:0:0 -no-undefined
LIBADD_LIB_COMMON = $(LIBM) 

if ENABLE_LIBLTDL
LIBADD_LIB_COMMON += $(LTDL_LIBS)
else
LIBADD_LIB_COMMON += $(DL_LIBS)
endif

bin_PROGRAMS = hcl
hcl_SOURCES = main.c ../lib/cb-impl.h ../lib/cb-impl.c
hcl_CPPFLAGS = $(CPPFLAGS_ALL_COMMON) $(CPPFLAGS_PFMOD)
hcl_LDFLAGS = $(LDFLAGS_ALL_COMMON)
hcl_LDADD = $(LIBADD_LIB_COMMON) -lhcl
hcl_DEPENDENCIES = ../lib/libhcl.la

if ENABLE_HCLEX

## cb-impl.c is included in libhclx.
## so ../lib/cb-impl.h and ../lib/cb-impl.c are not listed as sources

bin_PROGRAMS += hcls
hcls_SOURCES = main-s.c
hcls_CPPFLAGS = $(CPPFLAGS_ALL_COMMON)
hcls_LDFLAGS = $(LDFLAGS_ALL_COMMON)
hcls_LDADD = $(LIBADD_LIB_COMMON) $(PTHREAD_LIBS) -lhclx
hcls_DEPENDENCIES = ../lib/libhclx.la

bin_PROGRAMS += hclc
hclc_SOURCES = main-c.c
hclc_CPPFLAGS = $(CPPFLAGS_ALL_COMMON)
hclc_LDFLAGS = $(LDFLAGS_ALL_COMMON)
hclc_LDADD = $(LIBADD_LIB_COMMON) $(PTHREAD_LIBS) -lhclx
hclc_DEPENDENCIES = ../lib/libhclx.la

bin_PROGRAMS += hclj
hclj_SOURCES = main-j.c
hclj_CPPFLAGS = $(CPPFLAGS_ALL_COMMON)
hclj_LDFLAGS = $(LDFLAGS_ALL_COMMON)
hclj_LDADD = $(LIBADD_LIB_COMMON) $(PTHREAD_LIBS) -lhclx
hclj_DEPENDENCIES = ../lib/libhclx.la
endif
