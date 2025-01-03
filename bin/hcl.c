/*
 * $Id$
 *
    Copyright (c) 2016-2018 Chung, Hyung-Hwan. All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions
    are met:
    1. Redistributions of source code must retain the above copyright
       notice, this list of conditions and the following disclaimer.
    2. Redistributions in binary form must reproduce the above copyright
       notice, this list of conditions and the following disclaimer in the
       documentation and/or other materials provided with the distribution.

    THIS SOFTWARE IS PROVIDED BY THE AUTHOR "AS IS" AND ANY EXPRESS OR
    IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
    OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
    IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
    INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
    NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
    DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
    THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
    THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#if defined(_WIN32)
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <hcl.h>
#include <hcl-chr.h>
#include <hcl-str.h>
#include <hcl-utl.h>
#include <hcl-opt.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <locale.h>

#if defined(HAVE_ISOCLINE_H) && defined(HAVE_ISOCLINE_LIB)
#	include <isocline.h>
#	define USE_ISOCLINE
#endif

#if defined(_WIN32)
#	include <windows.h>
#	include <tchar.h>
#	include <io.h>
#	include <fcntl.h>
#	include <time.h>
#	include <signal.h>

#elif defined(__OS2__)
#	define INCL_DOSMODULEMGR
#	define INCL_DOSPROCESS
#	define INCL_DOSERRORS
#	include <os2.h>
#	include <signal.h>

#elif defined(__DOS__)
#	include <dos.h>
#	include <time.h>
#	include <signal.h>
#elif defined(macintosh)
#	include <Timer.h>
#else

#	include <sys/types.h>
#	include <errno.h>
#	include <unistd.h>
#	include <fcntl.h>

#	if defined(HAVE_TIME_H)
#		include <time.h>
#	endif
#	if defined(HAVE_SYS_TIME_H)
#		include <sys/time.h>
#	endif
#	if defined(HAVE_SIGNAL_H)
#		include <signal.h>
#	endif
#endif

#if defined(__DOS__) || defined(_WIN32) || defined(__OS2__)
#define FOPEN_R_FLAGS "rb"
#else
#define FOPEN_R_FLAGS "r"
#endif

typedef struct xtn_t xtn_t;
struct xtn_t
{
	const char* cci_path; /* main source file */
	/*const char* udi_path; */ /* not implemented as of snow */
	const char* udo_path;

	int vm_running;

	struct
	{
		hcl_bch_t* ptr;
		hcl_bch_t buf[1024]; /* not used if isocline is used */
		hcl_oow_t len;
		hcl_oow_t pos;
		int eof;
		hcl_oow_t ncompexprs; /* number of compiled expressions */
	} feed;
	/*hcl_oop_t sym_errstr;*/
};

/* ========================================================================= */

static hcl_t* g_hcl = HCL_NULL;

/* ========================================================================= */

static int vm_startup (hcl_t* hcl)
{
	xtn_t* xtn = (xtn_t*)hcl_getxtn(hcl);
	xtn->vm_running = 1;
	return 0;
}

static void vm_cleanup (hcl_t* hcl)
{
	xtn_t* xtn = (xtn_t*)hcl_getxtn(hcl);
	xtn->vm_running = 0;

#if defined(USE_ISOCLINE)
	if (xtn->feed.ptr && xtn->feed.ptr != xtn->feed.buf)
	{
		ic_free (xtn->feed.ptr);
		xtn->feed.ptr = HCL_NULL;
	}
#endif
}

/*
static void vm_checkbc (hcl_t* hcl, hcl_oob_t bcode)
{
}
*/

static void on_gc_hcl (hcl_t* hcl)
{
	/*xtn_t* xtn = (xtn_t*)hcl_getxtn(hcl);*/
	/*if (xtn->sym_errstr) xtn->sym_errstr = hcl_moveoop(hcl, xtn->sym_errstr);*/
}

/* ========================================================================= */

static int handle_logopt (hcl_t* hcl, const hcl_bch_t* logstr)
{
	const hcl_bch_t* cm, * flt;
	hcl_bitmask_t logmask;
	hcl_oow_t tlen, i;
	hcl_bcs_t fname;

	static struct
	{
		const char* name;
		int op; /* 0: bitwise-OR, 1: bitwise-AND */
		hcl_bitmask_t mask;
	} xtab[] =
	{
		{ "",           0, 0 },

		{ "app",        0, HCL_LOG_APP },
		{ "compiler",   0, HCL_LOG_COMPILER },
		{ "vm",         0, HCL_LOG_VM },
		{ "mnemonic",   0, HCL_LOG_MNEMONIC },
		{ "gc",         0, HCL_LOG_GC },
		{ "ic",         0, HCL_LOG_IC },
		{ "primitive",  0, HCL_LOG_PRIMITIVE },

		/* select a specific level */
		{ "fatal",      0, HCL_LOG_FATAL },
		{ "error",      0, HCL_LOG_ERROR },
		{ "warn",       0, HCL_LOG_WARN },
		{ "info",       0, HCL_LOG_INFO },
		{ "debug",      0, HCL_LOG_DEBUG },

		/* select a specific level or higher */
		{ "fatal+",     0, HCL_LOG_FATAL },
		{ "error+",     0, HCL_LOG_FATAL | HCL_LOG_ERROR },
		{ "warn+",      0, HCL_LOG_FATAL | HCL_LOG_ERROR | HCL_LOG_WARN },
		{ "info+",      0, HCL_LOG_FATAL | HCL_LOG_ERROR | HCL_LOG_WARN | HCL_LOG_INFO },
		{ "debug+",     0, HCL_LOG_FATAL | HCL_LOG_ERROR | HCL_LOG_WARN | HCL_LOG_INFO | HCL_LOG_DEBUG },

		/* select a specific level or lower */
		{ "fatal-",     0, HCL_LOG_FATAL | HCL_LOG_ERROR | HCL_LOG_WARN | HCL_LOG_INFO | HCL_LOG_DEBUG },
		{ "error-",     0, HCL_LOG_ERROR | HCL_LOG_WARN | HCL_LOG_INFO | HCL_LOG_DEBUG },
		{ "warn-",      0, HCL_LOG_WARN | HCL_LOG_INFO | HCL_LOG_DEBUG },
		{ "info-",      0, HCL_LOG_INFO | HCL_LOG_DEBUG },
		{ "debug-",     0, HCL_LOG_DEBUG },

		/* exclude a specific level */
		{ "-fatal",     1, ~(hcl_bitmask_t)HCL_LOG_FATAL },
		{ "-error",     1, ~(hcl_bitmask_t)HCL_LOG_ERROR },
		{ "-warn",      1, ~(hcl_bitmask_t)HCL_LOG_WARN },
		{ "-info",      1, ~(hcl_bitmask_t)HCL_LOG_INFO },
		{ "-debug",     1, ~(hcl_bitmask_t)HCL_LOG_DEBUG },
	};

	cm = hcl_find_bchar_in_bcstr(logstr, ',');
	if (cm)
	{
		fname.len = cm - logstr;
		logmask = 0;

		do
		{
			flt = cm + 1;

			cm = hcl_find_bchar_in_bcstr(flt, ',');
			tlen = (cm)? (cm - flt): hcl_count_bcstr(flt);

			for (i = 0; i < HCL_COUNTOF(xtab); i++)
			{
				if (hcl_comp_bchars_bcstr(flt, tlen, xtab[i].name) == 0)
				{
					if (xtab[i].op) logmask &= xtab[i].mask;
					else logmask |= xtab[i].mask;
					break;
				}
			}

			if (i >= HCL_COUNTOF(xtab))
			{
				fprintf (stderr, "ERROR: unrecognized value  - [%.*s] - [%s]\n", (int)tlen, flt, logstr);
				return -1;
			}
		}
		while (cm);


		if (!(logmask & HCL_LOG_ALL_TYPES)) logmask |= HCL_LOG_ALL_TYPES;  /* no types specified. force to all types */
		if (!(logmask & HCL_LOG_ALL_LEVELS)) logmask |= HCL_LOG_ALL_LEVELS;  /* no levels specified. force to all levels */
	}
	else
	{
		logmask = HCL_LOG_ALL_LEVELS | HCL_LOG_ALL_TYPES;
		fname.len = hcl_count_bcstr(logstr);
	}

	fname.ptr = (hcl_bch_t*)logstr;
	hcl_setoption (hcl, HCL_LOG_TARGET_BCS, &fname);
	hcl_setoption (hcl, HCL_LOG_MASK, &logmask);
	return 0;
}

#if defined(HCL_BUILD_DEBUG)
static int handle_dbgopt (hcl_t* hcl, const hcl_bch_t* str)
{
	/*xtn_t* xtn = (xtn_t*)hcl_getxtn(hcl);*/
	const hcl_bch_t* cm, * flt;
	hcl_oow_t len;
	hcl_bitmask_t trait, dbgopt = 0;

	cm = str - 1;
	do
	{
		flt = cm + 1;

		cm = hcl_find_bchar_in_bcstr(flt, ',');
		len = cm? (cm - flt): hcl_count_bcstr(flt);
		if (len == 0) continue;
		else if (hcl_comp_bchars_bcstr(flt, len, "gc") == 0) dbgopt |= HCL_TRAIT_DEBUG_GC;
		else if (hcl_comp_bchars_bcstr(flt, len, "bigint") == 0) dbgopt |= HCL_TRAIT_DEBUG_BIGINT;
		else
		{
			fprintf (stderr, "ERROR: unknown debug option value - %.*s\n", (int)len, flt);
			return -1;
		}
	}
	while (cm);

	hcl_getoption (hcl, HCL_TRAIT, &trait);
	trait |= dbgopt;
	hcl_setoption (hcl, HCL_TRAIT, &trait);
	return 0;
}
#endif

/* ========================================================================= */

#if defined(_WIN32) || defined(__DOS__) || defined(__OS2__) || defined(macintosh)
typedef void(*signal_handler_t)(int);
#elif defined(macintosh)
typedef void(*signal_handler_t)(int); /* TODO: */
#elif defined(SA_SIGINFO)
typedef void(*signal_handler_t)(int, siginfo_t*, void*);
#else
typedef void(*signal_handler_t)(int);
#endif


#if defined(_WIN32) || defined(__DOS__) || defined(__OS2__)
static void handle_sigint (int sig)
{
	if (g_hcl) hcl_abort (g_hcl);
}
#elif defined(macintosh)
/* TODO */
#elif defined(SA_SIGINFO)
static void handle_sigint (int sig, siginfo_t* siginfo, void* ctx)
{
	if (g_hcl) hcl_abort (g_hcl);
}
#else
static void handle_sigint (int sig)
{
	if (g_hcl) hcl_abort (g_hcl);
}
#endif

static void set_signal (int sig, signal_handler_t handler)
{
#if defined(_WIN32) || defined(__DOS__) || defined(__OS2__)
	signal (sig, handler);
#elif defined(macintosh)
	/* TODO: implement this */
#else
	struct sigaction sa;

	memset (&sa, 0, sizeof(sa));
	/*sa.sa_handler = handler;*/
#if defined(SA_SIGINFO)
	sa.sa_flags = SA_SIGINFO;
	sa.sa_sigaction = handler;
#else
	sa.sa_handler = handler;
#endif
	sigemptyset (&sa.sa_mask);

	sigaction (sig, &sa, NULL);
#endif
}

static void set_signal_to_default (int sig)
{
#if defined(_WIN32) || defined(__DOS__) || defined(__OS2__)
	signal (sig, SIG_DFL);
#elif defined(macintosh)
	/* TODO: implement this */
#else
	struct sigaction sa;

	memset (&sa, 0, sizeof(sa));
	sa.sa_handler = SIG_DFL;
	sa.sa_flags = 0;
	sigemptyset (&sa.sa_mask);

	sigaction (sig, &sa, NULL);
#endif
}

/* ========================================================================= */

static void print_info (void)
{
#if defined(HCL_CONFIGURE_CMD) && defined(HCL_CONFIGURE_ARGS)
	printf ("Configured with: %s %s\n", HCL_CONFIGURE_CMD, HCL_CONFIGURE_ARGS);
#elif defined(_WIN32)
	printf("Built for windows\n");
#else
	/* TODO: improve this part */
#endif
}

static void print_synerr (hcl_t* hcl)
{
	hcl_synerr_t synerr;
	xtn_t* xtn;

	xtn = (xtn_t*)hcl_getxtn(hcl);
	hcl_getsynerr (hcl, &synerr);

	hcl_logbfmt (hcl,HCL_LOG_STDERR, "ERROR: ");
	if (synerr.loc.file)
		hcl_logbfmt (hcl, HCL_LOG_STDERR, "%js", synerr.loc.file);
	else
		hcl_logbfmt (hcl, HCL_LOG_STDERR, "%hs", xtn->cci_path);

	hcl_logbfmt (hcl, HCL_LOG_STDERR, "[%zu,%zu] %js",
		synerr.loc.line, synerr.loc.colm,
		(hcl_geterrmsg(hcl) != hcl_geterrstr(hcl)? hcl_geterrmsg(hcl): hcl_geterrstr(hcl))
	);

	if (synerr.tgt.len > 0)
		hcl_logbfmt (hcl, HCL_LOG_STDERR, " - %.*js", synerr.tgt.len, synerr.tgt.val);

	hcl_logbfmt (hcl, HCL_LOG_STDERR, "\n");
}

static void print_other_error (hcl_t* hcl)
{
	xtn_t* xtn;
	hcl_loc_t loc;

	xtn = (xtn_t*)hcl_getxtn(hcl);
	hcl_geterrloc(hcl, &loc);

	hcl_logbfmt (hcl,HCL_LOG_STDERR, "ERROR: ");
	if (loc.file)
		hcl_logbfmt (hcl, HCL_LOG_STDERR, "%js", loc.file);
	else
		hcl_logbfmt (hcl, HCL_LOG_STDERR, "%hs", xtn->cci_path);

	hcl_logbfmt (hcl, HCL_LOG_STDERR, "[%zu,%zu] %js", loc.line, loc.colm, hcl_geterrmsg(hcl));

	hcl_logbfmt (hcl, HCL_LOG_STDERR, "\n");
}

static void print_error (hcl_t* hcl, const hcl_bch_t* msghdr)
{
	if (HCL_ERRNUM(hcl) == HCL_ESYNERR) print_synerr (hcl);
	else print_other_error (hcl);
	/*else hcl_logbfmt (hcl, HCL_LOG_STDERR, "ERROR: %hs - [%d] %js\n", msghdr, hcl_geterrnum(hcl), hcl_geterrmsg(hcl));*/
}


#if defined(USE_ISOCLINE)
static void print_incomplete_expression_error (hcl_t* hcl)
{
	/* isocline is supposed to return a full expression.
	 * if something is pending in the feed side, the input isn't complete yet */
	xtn_t* xtn;
	hcl_loc_t loc;

	xtn = hcl_getxtn(hcl);
	hcl_getfeedloc (hcl, &loc);

	hcl_logbfmt (hcl, HCL_LOG_STDERR, "ERROR: ");
	if (loc.file)
		hcl_logbfmt (hcl, HCL_LOG_STDERR, "%js", loc.file);
	else
		hcl_logbfmt (hcl, HCL_LOG_STDERR, "%hs", xtn->cci_path);

	/* if the input is like this
	 *   a := 2; c := {
	 * the second expression is incompelete. however, the whole input is not executed.
	 * the number of compiled expressions so far is in xtn->feed.ncompexprs, however */
	hcl_logbfmt (hcl, HCL_LOG_STDERR, "[%zu,%zu] incomplete expression\n", loc.line, loc.colm);
}
#endif

static void show_prompt (hcl_t* hcl, int level)
{
/* TODO: different prompt per level */
	hcl_resetfeedloc (hcl); /* restore the line number to 1 in the interactive mode */
#if !defined(USE_ISOCLINE)
	hcl_logbfmt (hcl, HCL_LOG_STDOUT, "HCL> ");
	hcl_logbfmt (hcl, HCL_LOG_STDOUT, HCL_NULL); /* flushing */
#endif
}

static hcl_oop_t execute_in_interactive_mode (hcl_t* hcl)
{
	hcl_oop_t retv;

	hcl_decode (hcl, hcl_getcode(hcl), 0, hcl_getbclen(hcl));
	HCL_LOG0 (hcl, HCL_LOG_MNEMONIC, "------------------------------------------\n");
	g_hcl = hcl;
	/*setup_tick ();*/

	retv = hcl_execute(hcl);

	/* flush pending output data in the interactive mode(e.g. printf without a newline) */
	hcl_flushudio (hcl);

	if (!retv)
	{
		print_error (hcl, "execute");
	}
	else
	{
		/* print the result in the interactive mode regardless 'verbose' */
		hcl_logbfmt (hcl, HCL_LOG_STDOUT, "%O\n", retv); /* TODO: show this go to the output handler?? */
		/*
		 * print the value of ERRSTR.
		hcl_oop_cons_t cons = hcl_getatsysdic(hcl, xtn->sym_errstr);
		if (cons)
		{
			HCL_ASSERT (hcl, HCL_IS_CONS(hcl, cons));
			HCL_ASSERT (hcl, HCL_CONS_CAR(cons) == xtn->sym_errstr);
			hcl_print (hcl, HCL_CONS_CDR(cons));
		}
		*/
	}
	/*cancel_tick();*/
	g_hcl = HCL_NULL;

	return retv;
}

static hcl_oop_t execute_in_batch_mode(hcl_t* hcl, int verbose)
{
	hcl_oop_t retv;

	hcl_decode(hcl, hcl_getcode(hcl), 0, hcl_getbclen(hcl));
	HCL_LOG3(hcl, HCL_LOG_MNEMONIC, "BYTECODES bclen=%zu lflen=%zu ngtmprs=%zu\n", hcl_getbclen(hcl), hcl_getlflen(hcl), hcl_getngtmprs(hcl));
	g_hcl = hcl;
	/*setup_tick ();*/


/* TESTING */
#if 0
{
	hcl_code_t xcode;
	hcl_ptlc_t mem;

	memset (&xcode, 0, HCL_SIZEOF(xcode));
	memset (&mem, 0, HCL_SIZEOF(mem));

	hcl_marshalcodetomem(hcl, &hcl->code, &mem);
	hcl_unmarshalcodefrommem(hcl, &xcode, (const hcl_ptl_t*)&mem);
	hcl_freemem (hcl, mem.ptr);

	hcl_decode(hcl, &xcode, 0, xcode.bc.len);
	hcl_purgecode (hcl, &xcode);
}
#endif
/* END TESTING */

	retv = hcl_execute(hcl);
	hcl_flushudio (hcl);

	if (!retv) print_error (hcl, "execute");
	else if (verbose) hcl_logbfmt (hcl, HCL_LOG_STDERR, "EXECUTION OK - EXITED WITH %O\n", retv);

	/*cancel_tick();*/
	g_hcl = HCL_NULL;
	/*hcl_dumpsymtab (hcl);*/

	return retv;
}

static int on_fed_cnode_in_interactive_mode (hcl_t* hcl, hcl_cnode_t* obj)
{
	xtn_t* xtn = (xtn_t*)hcl_getxtn(hcl);
	int flags = 0;

	/* in the interactive, the compile error must not break the input loop.
	 * this function returns 0 to go on despite a compile-time error.
	 *
	 * if a single line or continued lines contain multiple expressions,
	 * execution is delayed until the last expression is compiled. */

	if (xtn->feed.ncompexprs <= 0)
	{
		/* the first expression in the current user input line.
		 * arrange to clear byte-codes before compiling the expression. */
		flags = HCL_COMPILE_CLEAR_CODE | HCL_COMPILE_CLEAR_FUNBLK;
	}

	if (hcl_compile(hcl, obj, flags) <= -1)
	{
		/*print_error(hcl, "compile"); */
		xtn->feed.pos = xtn->feed.len; /* arrange to discard the rest of the line */
		return -1; /* this causes the feed function to fail and
		              the error hander for to print the error message */
	}

	xtn->feed.ncompexprs++;
	return 0;
}

static int on_fed_cnode_in_batch_mode (hcl_t* hcl, hcl_cnode_t* obj)
{
	/*xtn_t* xtn = (xtn_t*)hcl_getxtn(hcl);*/
	return hcl_compile(hcl, obj, 0);
}

#if defined(USE_ISOCLINE)
static int get_line (hcl_t* hcl, xtn_t* xtn, FILE* fp)
{
	char* inp, * p;
	static int inited = 0;

	if (!inited)
	{
		ic_style_def("kbd","gray underline");     // you can define your own styles
		ic_style_def("ic-prompt","ansi-maroon");  // or re-define system styles
		ic_set_history (HCL_NULL, -1);
		ic_enable_multiline (1);
		ic_enable_multiline_indent (1);
		ic_set_matching_braces ("()[]{}");
		ic_enable_brace_insertion (1);
		ic_set_insertion_braces("()[]{}\"\"''");
		inited = 1;
	}

	if (xtn->feed.eof) return 0;

	xtn->feed.pos = 0;
	xtn->feed.len = 0;
	if (xtn->feed.ptr)
	{
		HCL_ASSERT (hcl, xtn->feed.ptr != xtn->feed.buf);
		ic_free (xtn->feed.ptr);
		xtn->feed.ptr = HCL_NULL;
	}

	inp = ic_readline("HCL");
	if (inp == NULL)
	{
		/* TODO: check if it's an error or Eof */
		xtn->feed.eof = 1;
		if (xtn->feed.len <= 0) return 0;
		return 0;
	}

	xtn->feed.len = hcl_count_bcstr(inp);
	xtn->feed.ptr = inp;
	return 1;
}
#else
static int get_line (hcl_t* hcl, xtn_t* xtn, FILE* fp)
{
	if (xtn->feed.eof) return 0;

	xtn->feed.pos = 0;
	xtn->feed.len = 0;
	xtn->feed.ptr = xtn->feed.buf; /* use the internal buffer */

	while (1)
	{
		int ch = fgetc(fp);
		if (ch == EOF)
		{
			if (ferror(fp))
			{
				hcl_logbfmt (hcl, HCL_LOG_STDERR, "ERROR: failed to read - %hs - %hs\n", xtn->cci_path, strerror(errno));
				return -1;
			}

			xtn->feed.eof = 1;
			if (xtn->feed.len <= 0) return 0;

			break;
		}

		xtn->feed.buf[xtn->feed.len++] = (hcl_bch_t)(unsigned int)ch;
		if (ch == '\n' || xtn->feed.len >= HCL_COUNTOF(xtn->feed.buf)) break;
	}

	return 1;
}
#endif

static int feed_loop (hcl_t* hcl, xtn_t* xtn, int verbose)
{
	FILE* fp = HCL_NULL;
	int is_tty;

#if defined(_WIN32) && defined(__STDC_WANT_SECURE_LIB__)
	errno_t err = fopen_s(&fp, xtn->cci_path, FOPEN_R_FLAGS);
	if (err != 0)
	{
		hcl_logbfmt(hcl, HCL_LOG_STDERR, "ERROR: failed to open - %hs - %hs\n", xtn->cci_path, strerror(err));
		goto oops;
	}
#else
	fp = fopen(xtn->cci_path, FOPEN_R_FLAGS);
	if (!fp)
	{
		hcl_logbfmt (hcl, HCL_LOG_STDERR, "ERROR: failed to open - %hs - %hs\n", xtn->cci_path, strerror(errno));
		goto oops;
	}
#endif

#if defined(_WIN32)
	is_tty = _isatty(_fileno(fp));
#else
	is_tty = isatty(fileno(fp));
#endif

	/* override the default cnode handler. the default one simply
	 * compiles the expression node without execution */
	/*if (hcl_beginfeed(hcl, is_tty? on_fed_cnode_in_interactive_mode: HCL_NULL) <= -1)*/
	if (hcl_beginfeed(hcl, is_tty? on_fed_cnode_in_interactive_mode: on_fed_cnode_in_batch_mode) <= -1)
	{
		hcl_logbfmt (hcl, HCL_LOG_STDERR, "ERROR: cannot begin feed - [%d] %js\n", hcl_geterrnum(hcl), hcl_geterrmsg(hcl));
		goto oops;
	}

	if (is_tty)
	{
		/* interactive mode */
		show_prompt (hcl, 0);

		while (1)
		{
			int n;
			hcl_oow_t pos;
			hcl_oow_t len;

		#if defined(USE_ISOCLINE)
			int lf_injected = 0;
		#endif

			/* read a line regardless of the actual expression */
			n = get_line(hcl, xtn, fp);
			if (n <= -1) goto oops;
			if (n == 0) break;

			/* feed the line */
			pos = xtn->feed.pos;
			/* update xtn->feed.pos before calling hcl_feedbchars() so that the callback sees the updated value */
			xtn->feed.pos = xtn->feed.len;
			len = xtn->feed.len - pos;
			n = hcl_feedbchars(hcl, &xtn->feed.ptr[pos], len);
		#if defined(USE_ISOCLINE)
		chars_fed:
		#endif
			if (n <= -1)
			{
				print_error (hcl, "feed"); /* syntax error or something - mostly compile error */

		#if defined(USE_ISOCLINE)
			reset_on_feed_error:
		#endif
				hcl_resetfeed (hcl);
				hcl_clearcode (hcl); /* clear the compiled code but not executed yet in advance */
				xtn->feed.ncompexprs = 0; /* next time, on_fed_cnode_in_interactive_mode() clears code and fnblks */
				/*if (len > 0)*/ show_prompt (hcl, 0); /* show prompt after error */
			}
			else
			{
				if (!hcl_feedpending(hcl))
				{
					if (xtn->feed.ncompexprs > 0)
					{
						if (hcl_getbclen(hcl) > 0) execute_in_interactive_mode (hcl);
						xtn->feed.ncompexprs = 0;
					}
					else
					{
						HCL_ASSERT (hcl, hcl_getbclen(hcl) == 0);
						/* usually this part is reached if the input string is
						 * one or more whilespaces and/or comments only */
					}
					show_prompt (hcl, 0); /* show prompt after execution */
				}
		#if defined(USE_ISOCLINE)
				else if (!lf_injected)
				{
					/* in this mode, one input string must be composed of one or more
					 * complete expression. however, it doesn't isocline doesn't include
					 * the ending line-feed in the returned input string. inject one to the feed */
					static const char lf = '\n';
					lf_injected = 1;
					n = hcl_feedbchars(hcl, &lf, 1);
					goto chars_fed;
				}
				else
				{
					print_incomplete_expression_error (hcl);
					goto reset_on_feed_error;
				}
		#endif
			}
		}

	#if !defined(USE_ISOCLINE)
		/* eof is given, usually with ctrl-D, no new line is output after the prompt.
		 * this results in the OS prompt on the same line as this program's prompt.
		 * however ISOCLINE prints a newline upon ctrl-D. print \n when ISOCLINE is
		 * not used */
		hcl_logbfmt (hcl, HCL_LOG_STDOUT, "\n");
	#endif
	}
	else
	{
		/* non-interactive mode */
		while (1)
		{
			hcl_bch_t buf[1024];
			hcl_oow_t xlen;

			xlen = fread(buf, HCL_SIZEOF(buf[0]), HCL_COUNTOF(buf), fp);
			if (xlen > 0 && hcl_feedbchars(hcl, buf, xlen) <= -1) goto endfeed_error;
			if (xlen < HCL_COUNTOF(buf))
			{
				if (ferror(fp))
				{
					hcl_logbfmt (hcl, HCL_LOG_STDERR, "ERROR: failed to read - %hs - %hs\n", xtn->cci_path, strerror(errno));
					goto oops;
				}
				break;
			}
		}
	}

	if (hcl_endfeed(hcl) <= -1)
	{
	endfeed_error:
		print_error (hcl, "endfeed");
		goto oops; /* TODO: proceed or just exit? */
	}
	fclose (fp);

	if (!is_tty && hcl_getbclen(hcl) > 0) execute_in_batch_mode (hcl, verbose);
	return 0;

oops:
	if (fp) fclose (fp);
	return -1;
}

/* #define DEFAULT_HEAPSIZE (512000ul) */
#define DEFAULT_HEAPSIZE (0ul) /* don't use the pre-allocated heap */

int main (int argc, char* argv[])
{
	hcl_t* hcl = HCL_NULL;
	xtn_t* xtn;
	hcl_cb_t hclcb;

	hcl_bci_t c;
	static hcl_bopt_lng_t lopt[] =
	{
#if defined(HCL_BUILD_DEBUG)
		{ ":debug",       '\0' },
#endif
		{ ":heapsize",    '\0' },
		{ ":log",         'l'  },
		{ "info",         '\0' },
		{ ":modlibdirs",  '\0' },

		{ HCL_NULL,       '\0' }
	};
	static hcl_bopt_t opt =
	{
		"l:v",
		lopt
	};

	const char* logopt = HCL_NULL;
	hcl_oow_t heapsize = DEFAULT_HEAPSIZE;
	int verbose = 0;
	int show_info = 0;
	const char* modlibdirs = HCL_NULL;

#if defined(HCL_BUILD_DEBUG)
	const char* dbgopt = HCL_NULL;
#endif

	setlocale (LC_ALL, "");

#if !defined(macintosh)
	if (argc < 2)
	{
	print_usage:
		fprintf (stderr, "Usage: %s [options] script-filename [output-filename]\n", argv[0]);
		fprintf (stderr, "Options are:\n");
		fprintf (stderr, " -v  show verbose messages\n");
		return -1;
	}

	while ((c = hcl_getbopt(argc, argv, &opt)) != HCL_BCI_EOF)
	{
		switch (c)
		{
			case 'l':
				logopt = opt.arg;
				break;

			case 'v':
				verbose = 1;
				break;

			case '\0':
				if (hcl_comp_bcstr(opt.lngopt, "heapsize") == 0)
				{
					heapsize = strtoul(opt.arg, HCL_NULL, 0);
					break;
				}
			#if defined(HCL_BUILD_DEBUG)
				else if (hcl_comp_bcstr(opt.lngopt, "debug") == 0)
				{
					dbgopt = opt.arg;
					break;
				}
			#endif
				else if (hcl_comp_bcstr(opt.lngopt, "info") == 0)
				{
					show_info = 1;
					break;
				}
				else if (hcl_comp_bcstr(opt.lngopt, "modlibdirs") == 0)
				{
					modlibdirs = opt.arg;
					break;
				}

				goto print_usage;

			case ':':
				if (opt.lngopt)
					fprintf (stderr, "bad argument for '%s'\n", opt.lngopt);
				else
					fprintf (stderr, "bad argument for '%c'\n", opt.opt);

				return -1;

			default:
				goto print_usage;
		}
	}

	if ((opt.ind + 1) != argc && (opt.ind + 2) != argc && !show_info) goto print_usage;
#endif

	hcl = hcl_openstd(HCL_SIZEOF(xtn_t), HCL_NULL);
	if (HCL_UNLIKELY(!hcl))
	{
		printf ("ERROR: cannot open hcl\n");
		goto oops;
	}

	xtn = (xtn_t*)hcl_getxtn(hcl);

	{
		hcl_oow_t tab_size;
		tab_size = 5000;
		hcl_setoption (hcl, HCL_SYMTAB_SIZE, &tab_size);
		tab_size = 5000;
		hcl_setoption (hcl, HCL_SYSDIC_SIZE, &tab_size);
		tab_size = 600; /* TODO: choose a better stack size or make this user specifiable */
		hcl_setoption (hcl, HCL_PROCSTK_SIZE, &tab_size);
	}

	{
		hcl_bitmask_t trait = 0;

		/*trait |= HCL_TRAIT_NOGC;*/
		trait |= HCL_TRAIT_AWAIT_PROCS;
		trait |= HCL_TRAIT_LANG_ENABLE_EOL;
		hcl_setoption (hcl, HCL_TRAIT, &trait);
	}

	if (modlibdirs)
	{
	#if defined(HCL_OOCH_IS_UCH)
		hcl_ooch_t* tmp;
		tmp = hcl_dupbtoucstr(hcl, modlibdirs, HCL_NULL);
		if (HCL_UNLIKELY(!tmp))
		{
			hcl_logbfmt (hcl, HCL_LOG_STDERR,"ERROR: cannot duplicate modlibdirs - [%d] %js\n", hcl_geterrnum(hcl), hcl_geterrmsg(hcl));
			goto oops;
		}

		if (hcl_setoption(hcl, HCL_MOD_LIBDIRS, tmp) <= -1)
		{
			hcl_logbfmt (hcl, HCL_LOG_STDERR,"ERROR: cannot set modlibdirs - [%d] %js\n", hcl_geterrnum(hcl), hcl_geterrmsg(hcl));
			hcl_freemem (hcl, tmp);
			goto oops;
		}
		hcl_freemem (hcl, tmp);
	#else
		if (hcl_setoption(hcl, HCL_MOD_LIBDIRS, modlibdirs) <= -1)
		{
			hcl_logbfmt (hcl, HCL_LOG_STDERR,"ERROR: cannot set modlibdirs - [%d] %js\n", hcl_geterrnum(hcl), hcl_geterrmsg(hcl));
			goto oops;
		}
	#endif
	}

	memset (&hclcb, 0, HCL_SIZEOF(hclcb));
	hclcb.on_gc = on_gc_hcl;
	hclcb.vm_startup = vm_startup;
	hclcb.vm_cleanup = vm_cleanup;
	/*hclcb.vm_checkbc = vm_checkbc;*/
	hcl_regcb (hcl, &hclcb);

	if (logopt)
	{
		if (handle_logopt(hcl, logopt) <= -1) goto oops;
	}

#if defined(HCL_BUILD_DEBUG)
	if (dbgopt)
	{
		if (handle_dbgopt(hcl, dbgopt) <= -1) goto oops;
	}
#endif

	if (show_info)
	{
		print_info ();
		return 0;
	}

	if (hcl_ignite(hcl, heapsize) <= -1)
	{
		hcl_logbfmt (hcl, HCL_LOG_STDERR, "cannot ignite hcl - [%d] %js\n", hcl_geterrnum(hcl), hcl_geterrmsg(hcl));
		goto oops;
	}

	if (hcl_addbuiltinprims(hcl) <= -1)
	{
		hcl_logbfmt (hcl, HCL_LOG_STDERR, "cannot add builtin primitives - [%d] %js\n", hcl_geterrnum(hcl), hcl_geterrmsg(hcl));
		goto oops;
	}

	xtn->cci_path = argv[opt.ind++]; /* input source code file */
	if (opt.ind < argc) xtn->udo_path = argv[opt.ind++];

	if (hcl_attachcciostdwithbcstr(hcl, xtn->cci_path) <= -1)
	{
		hcl_logbfmt (hcl, HCL_LOG_STDERR, "ERROR: cannot attach source input stream - [%d] %js\n", hcl_geterrnum(hcl), hcl_geterrmsg(hcl));
		goto oops;
	}

	if (hcl_attachudiostdwithbcstr(hcl, "", xtn->udo_path) <= -1) /* TODO: add udi path */
	{
		hcl_logbfmt (hcl, HCL_LOG_STDERR, "ERROR: cannot attach user data streams - [%d] %js\n", hcl_geterrnum(hcl), hcl_geterrmsg(hcl));
		goto oops;
	}

	/* -- from this point onward, any failure leads to jumping to the oops label
	 * -- instead of returning -1 immediately. --*/
	set_signal (SIGINT, handle_sigint);

#if 0
// TODO: change the option name
// in the INTERACTIVE mode, the compiler generates MAKE_FUNCTION for lambda functions.
// in the non-INTERACTIVE mode, the compiler generates MAKE_BLOCK for lambda functions.
{
	hcl_bitmask_t trait;
	hcl_getoption (hcl, HCL_TRAIT, &trait);
	trait |= HCL_TRAIT_INTERACTIVE;
	hcl_setoption (hcl, HCL_TRAIT, &trait);
}
#endif

	if (feed_loop(hcl, xtn, verbose) <= -1) goto oops;

	set_signal_to_default (SIGINT);
	hcl_close (hcl);

	return 0;

oops:
	set_signal_to_default (SIGINT); /* harmless to call multiple times without set_signal() */
	if (hcl) hcl_close (hcl);
	return -1;
}
