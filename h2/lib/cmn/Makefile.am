AUTOMAKE_OPTIONS = nostdinc

AM_CPPFLAGS = \
	-I$(top_builddir)/include \
	-I$(top_srcdir)/include

lib_LTLIBRARIES = libh2cmn.la

#noinst_HEADERS = \
#	cp949.h \
#	cp950.h \
#	fmt.h \
#	fmt-intmax.h \
#	fmt-out.h \
#	fs.h \
#	mem.h \
#	str-dyn.h \
#	str-fcpy.h \
#	str-fmt.h \
#	str-join.h \
#	str-subst.h \
#	syscall.h \
#	syserr.h \
#	tre.h \
#	tre-ast.h \
#	tre-compile.h \
#	tre-match-utils.h \
#	tre-parse.h \
#	tre-stack.h
#
#libh2cmn_la_SOURCES = \
#	alg-base64.c \
#	alg-rand.c \
#	alg-search.c  \
#	alg-sort.c \
#	assert.c \
#	chr.c \
#	dir.c \
#	dll.c \
#	env.c \
#	gdl.c \
#	htb.c \
#	fio.c \
#	fma.c \
#	fmt-intmax.c \
#	fmt-out.c \
#	fs.c \
#	fs-err.c \
#	fs-move.c \
#	glob.c \
#	hton.c \
#	ipad.c \
#	lda.c \
#	main.c \
#	mb8.c \
#	mbwc.c \
#	mbwc-str.c \
#	mem.c \
#	mux.c \
#	nwad.c \
#	nwad-skad.c \
#	nwif.c \
#	nwif-cfg.c \
#	nwio.c \
#	oht.c \
#	opt.c \
#	path-basename.c \
#	path-canon.c \
#	pio.c \
#	pma.c \
#	rbt.c \
#	rex.c \
#	sio.c \
#	sll.c \
#	slmb.c \
#	str-beg.c \
#	str-cat.c \
#	str-chr.c \
#	str-cnv.c \
#	str-cmp.c \
#	str-cpy.c \
#	str-del.c \
#	str-dup.c \
#	str-dyn.c \
#	str-end.c \
#	str-excl.c \
#	str-fcpy.c \
#	str-fmt.c \
#	str-fnmat.c \
#	str-incl.c \
#	str-join.c \
#	str-len.c \
#	str-pac.c \
#	str-pbrk.c \
#	str-put.c \
#	str-rev.c \
#	str-rot.c \
#	str-set.c \
#	str-spl.c \
#	str-spn.c \
#	str-str.c \
#	str-subst.c \
#	str-tok.c \
#	str-trm.c \
#	str-word.c \
#	task.c \
#	time.c \
#	tio.c \
#	tre.c \
#	tre-ast.c \
#	tre-compile.c \
#	tre-match-backtrack.c \
#	tre-match-parallel.c \
#	tre-parse.c \
#	tre-stack.c \
#	uri.c \
#	utf8.c \
#	xma.c

noinst_HEADERS =  \
	str-cmp.h \
	str-cpy.h \
	str-fmc.h \
	str-len.h

libh2cmn_la_SOURCES = \
	str.c \
	utf16.c \
	utf8.c


#if ENABLE_BUNDLED_UNICODE
#libh2cmn_la_SOURCES += uni.c 
#endif
#
#if ENABLE_XCMGRS
#libh2cmn_la_SOURCES += cp949.c cp950.c
#endif

libh2cmn_la_LDFLAGS = -version-info 1:0:0 -no-undefined
libh2cmn_la_LIBADD = $(SOCKET_LIBS) $(QUADMATH_LIBS)

#if ENABLE_CXX
#
#lib_LTLIBRARIES += libh2cmnxx.la
#libh2cmnxx_la_SOURCES = \
#	Mmgr.cpp StdMmgr.cpp
#libh2cmnxx_la_LDFLAGS = -version-info 1:0:0 -no-undefined
#libh2cmnxx_la_LIBADD = 
#
#endif
