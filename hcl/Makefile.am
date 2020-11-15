AUTOMAKE_OPTION = foreign
ACLOCAL_AMFLAGS = -I m4

EXTRA_DIST = t t/test-bi.hcl

SUBDIRS =

if ENABLE_STATIC_MODULE
SUBDIRS += mod lib bin
else
SUBDIRS += lib mod bin
endif

DIST_SUBDIRS = $(SUBDIRS) ##t

distclean-local:
	@rm -rf $(top_srcdir)/autom4te.cache
