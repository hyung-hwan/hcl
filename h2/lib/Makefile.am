AUTOMAKE_OPTIONS = nostdinc


EXTRA_DIST =  \
	h2.ads \
	h2-pool.ads \
	h2-pool.adb  \
	h2-ascii.ads  \
	h2-utf8.ads \
	h2-utf8.adb  \
	h2-os.ads \
	h2-os.adb \
	posix/h2-os-file.adb \
	h2-io.ads \
	h2-io.adb \
	h2-io-file.adb \
	h2-scheme.ads  \
	h2-scheme.adb  \
	h2-scheme-bigint.adb  \
	h2-scheme-token.adb  \
	h2-scheme-execute.adb \
	h2-scheme-execute-apply.adb  \
	h2-scheme-execute-evaluate.adb \
	h2-slim.ads \
	h2-wide.ads \
	h2-wide_wide.ads

noinst_PROGRAMS = sysdef

sysdef_SOURCES = posix/sysdef.c
sysdef_CPPFLAGS = 
sysdef_LDFLAGS =
sysdef_LDADD =

all-local: libh2.so

libh2.so: @abs_builddir@/@ADA_OBJDIR@ @abs_builddir@/posix/h2-sysdef.ads
	gprbuild @abs_builddir@/lib.gpr
	
@abs_builddir@/@ADA_OBJDIR@:
	mkdir -p @abs_builddir@/@ADA_OBJDIR@

# cross-compiling will fail for this.
@abs_builddir@/posix/h2-sysdef.ads:  @abs_builddir@/sysdef
	mkdir -p @abs_builddir@/posix
	@abs_builddir@/sysdef H2.Sysdef > @abs_builddir@/posix/h2-sysdef.ads


clean-local:
	gprclean -P@abs_builddir@/lib.gpr
	rm -f  @abs_builddir@/posix/h2-sysdef.ads
