AUTOMAKE_OPTIONS = nostdinc

EXTRA_DIST = \
	slim_stream.ads storage.ads  wide_stream.ads \
	slim_stream.adb storage.adb  wide_stream.adb \
	scheme.adb

#bin_PROGRAMS = h2scm


all-local: h2scm$(EXEEXT)

h2scm$(EXEEXT): @abs_builddir@/@ADA_OBJDIR@ @abs_builddir@/../lib/libh2.so @abs_srcdir@/scheme.adb 	
	gprbuild @abs_builddir@/scheme.gpr
	

@abs_builddir@/@ADA_OBJDIR@:
	mkdir -p @abs_builddir@/@ADA_OBJDIR@

clean-local:
	gprclean -P@abs_builddir@/scheme.gpr

