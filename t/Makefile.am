AUTOMAKE_OPTIONS = nostdinc

check_SCRIPTS = \
	feed-01.hcl \
	fun-01.hcl \
	insta-01.hcl \
	insta-02.hcl \
	proc-01.hcl \
	ret-01.hcl \
	retvar-01.hcl \
	va-01.hcl \
	var-01.hcl \
	var-02.hcl \
	var-03.hcl

check_ERRORS = \
	call-5001.err \
	class-5001.err \
	do-5001.err \
	feed-5001.err \
	mlist-5001.err \
	var-5001.err \
	var-5002.err \
	var-5004.err

##noinst_SCRIPTS = $(check_SCRIPTS)
EXTRA_DIST = $(check_SCRIPTS) $(check_ERRORS)

check_PROGRAMS =

TESTS = $(check_PROGRAMS) $(check_SCRIPTS) $(check_ERRORS)

TEST_EXTENSIONS = .hcl .err

HCLBIN = $(top_builddir)/bin/hcl

HCL_LOG_COMPILER = $(SHELL) $(abs_srcdir)/run.sh $(HCLBIN) --modlibdirs="@abs_top_builddir@/mod:@abs_top_builddir@/mod/.libs" --heapsize=0
AM_HCL_LOG_FLAGS =

ERR_LOG_COMPILER = $(SHELL) $(abs_srcdir)/err.sh $(HCLBIN) --modlibdirs="@abs_top_builddir@/mod:@abs_top_builddir@/mod/.libs" --heapsize=0
AM_ERR_LOG_FLAGS =
