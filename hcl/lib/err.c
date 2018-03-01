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

#include "hcl-prv.h"

static hcl_ooch_t errstr_0[] = {'n','o',' ','e','r','r','o','r','\0'};
static hcl_ooch_t errstr_1[] = {'g','e','n','e','r','i','c',' ','e','r','r','o','r','\0'};
static hcl_ooch_t errstr_2[] = {'n','o','t',' ','i','m','p','l','e','m','e','n','t','e','d','\0'};
static hcl_ooch_t errstr_3[] = {'s','u','b','s','y','s','t','e','m',' ','e','r','r','o','r','\0'};
static hcl_ooch_t errstr_4[] = {'i','n','t','e','r','n','a','l',' ','e','r','r','o','r',' ','t','h','a','t',' ','s','h','o','u','l','d',' ','n','e','v','e','r',' ','h','a','v','e',' ','h','a','p','p','e','n','e','d','\0'};
static hcl_ooch_t errstr_5[] = {'i','n','s','u','f','f','i','c','i','e','n','t',' ','s','y','s','t','e','m',' ','m','e','m','o','r','y','\0'};
static hcl_ooch_t errstr_6[] = {'i','n','s','u','f','f','i','c','i','e','n','t',' ','o','b','j','e','c','t',' ','m','e','m','o','r','y','\0'};
static hcl_ooch_t errstr_7[] = {'i','n','v','a','l','i','d',' ','c','l','a','s','s','/','t','y','p','e','\0'};
static hcl_ooch_t errstr_8[] = {'i','n','v','a','l','i','d',' ','p','a','r','a','m','e','t','e','r',' ','o','r',' ','a','r','g','u','m','e','n','t','\0'};
static hcl_ooch_t errstr_9[] = {'d','a','t','a',' ','n','o','t',' ','f','o','u','n','d','\0'};
static hcl_ooch_t errstr_10[] = {'e','x','i','s','t','i','n','g','/','d','u','p','l','i','c','a','t','e',' ','d','a','t','a','\0'};
static hcl_ooch_t errstr_11[] = {'b','u','s','y','\0'};
static hcl_ooch_t errstr_12[] = {'a','c','c','e','s','s',' ','d','e','n','i','e','d','\0'};
static hcl_ooch_t errstr_13[] = {'o','p','e','r','a','t','i','o','n',' ','n','o','t',' ','p','e','r','m','i','t','t','e','d','\0'};
static hcl_ooch_t errstr_14[] = {'n','o','t',' ','a',' ','d','i','r','e','c','t','o','r','y','\0'};
static hcl_ooch_t errstr_15[] = {'i','n','t','e','r','r','u','p','t','e','d','\0'};
static hcl_ooch_t errstr_16[] = {'p','i','p','e',' ','e','r','r','o','r','\0'};
static hcl_ooch_t errstr_17[] = {'r','e','s','o','u','r','c','e',' ','t','e','m','p','o','r','a','r','i','l','y',' ','u','n','a','v','a','i','l','a','b','l','e','\0'};
static hcl_ooch_t errstr_18[] = {'b','a','d',' ','s','y','s','t','e','m',' ','h','a','n','d','l','e','\0'};
static hcl_ooch_t errstr_19[] = {'t','o','o',' ','m','a','n','y',' ','f','r','a','m','e','s','\0'};
static hcl_ooch_t errstr_20[] = {'m','e','s','s','a','g','e',' ','r','e','c','e','i','v','e','r',' ','e','r','r','o','r','\0'};
static hcl_ooch_t errstr_21[] = {'m','e','s','s','a','g','e',' ','s','e','n','d','i','n','g',' ','e','r','r','o','r','\0'};
static hcl_ooch_t errstr_22[] = {'w','r','o','n','g',' ','n','u','m','b','e','r',' ','o','f',' ','a','r','g','u','m','e','n','t','s','\0'};
static hcl_ooch_t errstr_23[] = {'r','a','n','g','e',' ','e','r','r','o','r','\0'};
static hcl_ooch_t errstr_24[] = {'b','y','t','e','-','c','o','d','e',' ','f','u','l','l','\0'};
static hcl_ooch_t errstr_25[] = {'d','i','c','t','i','o','n','a','r','y',' ','f','u','l','l','\0'};
static hcl_ooch_t errstr_26[] = {'n','o',' ','m','o','r','e',' ','i','n','t','u','t','\0'}; 
static hcl_ooch_t errstr_27[] = {'t','o','o',' ','m','a','n','y',' ','p','a','r','a','m','e','t','e','r','s','\0'};
static hcl_ooch_t errstr_28[] = {'*','*','*',' ','u','n','d','e','f','i','n','e','d',' ','e','r','r','o','r',' ','*','*','*','\0'};
static hcl_ooch_t errstr_29[] = {'d','i','v','i','d','e',' ','b','y',' ','z','e','r','o','\0'};
static hcl_ooch_t errstr_30[] = {'I','/','O',' ','e','r','r','o','r','\0'};
static hcl_ooch_t errstr_31[] = {'e','n','c','o','d','i','n','g',' ','c','o','n','v','e','r','s','i','o','n',' ','e','r','r','o','r','\0'};
static hcl_ooch_t errstr_32[] = {'b','u','f','f','e','r',' ','f','u','l','l','\0'};
static hcl_ooch_t errstr_33[] = {'s','y','n','t','a','x',' ','e','r','r','o','r','\0'};
static hcl_ooch_t errstr_34[] = {'c','a','l','l',' ','e','r','r','o','r','\0'};
static hcl_ooch_t errstr_35[] = {'r','e','c','a','l','l',' ','p','r','o','h','i','b','i','t','e','d','\0'};
static hcl_ooch_t errstr_36[] = {'a','r','g','u','m','e','n','t',' ','n','u','m','b','e','r',' ','e','r','r','o','r','\0'};
static hcl_ooch_t* errstr[] =
{
	errstr_0, errstr_1, errstr_2, errstr_3, errstr_4, errstr_5, errstr_6, errstr_7,
	errstr_8, errstr_9, errstr_10, errstr_11, errstr_12, errstr_13, errstr_14, errstr_15,
	errstr_16, errstr_17, errstr_18, errstr_19, errstr_20, errstr_21, errstr_22, errstr_23,
	errstr_24, errstr_25, errstr_26, errstr_27, errstr_28, errstr_29, errstr_30, errstr_31,
	errstr_32, errstr_33, errstr_34, errstr_35, errstr_36
};


static char* synerrstr[] = 
{
	"no error",
	"illegal character",
	"comment not closed",
	"string/character not closed",
	"invalid hashed literal",
	"wrong character literal",
	"invalid numeric literal",
	"out of integer range",
	"wrong error literal",
	"wrong smptr literal",

	"sudden end of input",
	"( expected",
	") expected",
	"] expected",
	"} expected",
	"| expected",

	"string expected",
	"byte too small or too large",
	"nesting level too deep",

	"| disallowed",
	". disallowed",
	"#include error",

	"loop body too big",
	"if body too big",
	"lambda block too big",
	"lambda block too deep",
	"argument name list expected",
	"argument name expected",
	"duplicate argument name",
	"variable name expected",
	"wrong number of arguments",
	"too many arguments defined",
	"too many variables defined",
	"variable declaration disallowed",
	"duplicate variable name",
	"disallowed variable name",
	"disallowed argument name",

	"elif without if",
	"else without if",
	"break outside loop",

	"invalid callable",
	"unbalanced key/value pair",
	"unbalanced parenthesis/brace/bracket",
	"empty x-list"
};

/* -------------------------------------------------------------------------- 
 * ERROR NUMBER TO STRING CONVERSION
 * -------------------------------------------------------------------------- */
const hcl_ooch_t* hcl_errnum_to_errstr (hcl_errnum_t errnum)
{
	static hcl_ooch_t e_unknown[] = {'u','n','k','n','o','w','n',' ','e','r','r','o','r','\0'};
	return (errnum >= 0 && errnum < HCL_COUNTOF(errstr))? errstr[errnum]: e_unknown;
}

static const hcl_bch_t* synerr_to_errstr (hcl_synerrnum_t errnum)
{
	static hcl_bch_t e_unknown[] = "unknown error";
	return (errnum >= 0 && errnum < HCL_COUNTOF(synerrstr))? synerrstr[errnum]: e_unknown;
}

/* -------------------------------------------------------------------------- 
 * SYSTEM DEPENDENT FUNCTIONS 
 * -------------------------------------------------------------------------- */

#if defined(HAVE_EXECINFO_H)
#	include <execinfo.h>
#	include <stdlib.h>
#endif

#if defined(_WIN32)
#	include <windows.h>
#elif defined(__OS2__)
#	define INCL_DOSPROCESS
#	define INCL_DOSFILEMGR
#	define INCL_DOSERRORS
#	include <os2.h>
#elif defined(__DOS__)
#	include <dos.h>
#	if defined(_INTELC32_)
#		define DOS_EXIT 0x4C
#	else
#		include <dosfunc.h>
#	endif
#	include <errno.h>
#elif defined(vms) || defined(__vms)
#	define __NEW_STARLET 1
#	include <starlet.h> /* (SYS$...) */
#	include <ssdef.h> /* (SS$...) */
#	include <lib$routines.h> /* (lib$...) */
#elif defined(macintosh)
#	include <MacErrors.h>
#	include <Process.h>
#	include <Dialogs.h>
#	include <TextUtils.h>
#else
#	include <sys/types.h>
#	include <unistd.h>
#	include <signal.h>
#	include <errno.h>
#endif

hcl_errnum_t hcl_syserr_to_errnum (int e)
{
#if defined(__OS2__)
	/* APIRET e */
	switch (e)
	{
		case ERROR_NOT_ENOUGH_MEMORY: return HCL_ESYSMEM;

		case ERROR_INVALID_PARAMETER: 
		case ERROR_INVALID_HANDLE: 
		case ERROR_INVALID_NAME: return HCL_EINVAL;

		case ERROR_ACCESS_DENIED: 
		case ERROR_SHARING_VIOLATION: return HCL_EACCES;

		case ERROR_FILE_NOT_FOUND:
		case ERROR_PATH_NOT_FOUND: return HCL_ENOENT;

		case ERROR_ALREADY_EXISTS: return HCL_EEXIST;

		/*TODO: add more mappings */
		default: return HCL_ESYSERR;
	}
#elif defined(macintosh)
	switch (e)
	{
		case notEnoughMemoryErr: return HCL_ESYSMEM;
		case paramErr: return HCL_EINVAL;

		case qErr: /* queue element not found during deletion */
		case fnfErr: /* file not found */
		case dirNFErr: /* direcotry not found */
		case resNotFound: /* resource not found */
		case resFNotFound: /* resource file not found */
		case nbpNotFound: /* name not found on remove */
			return HCL_ENOENT;

		/*TODO: add more mappings */
		default: return HCL_ESYSERR;
	}

#else
	switch (e)
	{
		case ENOMEM: return HCL_ESYSMEM;
		case EINVAL: return HCL_EINVAL;

	#if defined(EBUSY)
		case EBUSY: return HCL_EBUSY;
	#endif
		case EACCES: return HCL_EACCES;
	#if defined(EPERM)
		case EPERM: return HCL_EPERM;
	#endif
	#if defined(ENOTDIR)
		case ENOTDIR: return HCL_ENOTDIR;
	#endif
		case ENOENT: return HCL_ENOENT;
	#if defined(EEXIST)
		case EEXIST: return HCL_EEXIST;
	#endif
	#if defined(EINTR)
		case EINTR:  return HCL_EINTR;
	#endif

	#if defined(EPIPE)
		case EPIPE:  return HCL_EPIPE;
	#endif

	#if defined(EAGAIN) && defined(EWOULDBLOCK) && (EAGAIN != EWOULDBLOCK)
		case EAGAIN: 
		case EWOULDBLOCK: return HCL_EAGAIN;
	#elif defined(EAGAIN)
		case EAGAIN: return HCL_EAGAIN;
	#elif defined(EWOULDBLOCK)
		case EWOULDBLOCK: return HCL_EAGAIN;
	#endif

	#if defined(EBADF)
		case EBADF: return HCL_EBADHND;
	#endif



	#if defined(EIO)
		case EIO: return HCL_EIOERR;
	#endif

		default: return HCL_ESYSERR;
	}
#endif
}

/* -------------------------------------------------------------------------- 
 * ERROR NUMBER/MESSAGE HANDLING
 * -------------------------------------------------------------------------- */

const hcl_ooch_t* hcl_geterrstr (hcl_t* hcl)
{
	return hcl_errnum_to_errstr (hcl->errnum);
}

const hcl_ooch_t* hcl_geterrmsg (hcl_t* hcl)
{
	if (hcl->errmsg.len <= 0) return hcl_errnum_to_errstr(hcl->errnum);
	return hcl->errmsg.buf;
}

const hcl_ooch_t* hcl_backuperrmsg (hcl_t* hcl)
{
	hcl_copyoocstr (hcl->errmsg.tmpbuf.ooch, HCL_COUNTOF(hcl->errmsg.tmpbuf.ooch), hcl_geterrmsg(hcl));
	return hcl->errmsg.tmpbuf.ooch;
}

void hcl_seterrnum (hcl_t* hcl, hcl_errnum_t errnum)
{
	if (hcl->shuterr) return;
	hcl->errnum = errnum; 
	hcl->errmsg.len = 0; 
}

void hcl_seterrwithsyserr (hcl_t* hcl, int syserr)
{
	if (hcl->shuterr) return;

	if (hcl->vmprim.syserrstrb)
	{
		hcl->vmprim.syserrstrb (hcl, syserr, hcl->errmsg.tmpbuf.bch, HCL_COUNTOF(hcl->errmsg.tmpbuf.bch));
		hcl_seterrbfmt (hcl, hcl_syserr_to_errnum(syserr), "%hs", hcl->errmsg.tmpbuf.bch);
	}
	else
	{
		HCL_ASSERT (hcl, hcl->vmprim.syserrstru != HCL_NULL);
		hcl->vmprim.syserrstru (hcl, syserr, hcl->errmsg.tmpbuf.uch, HCL_COUNTOF(hcl->errmsg.tmpbuf.uch));
		hcl_seterrbfmt (hcl, hcl_syserr_to_errnum(syserr), "%ls", hcl->errmsg.tmpbuf.uch);
	}
}

void hcl_getsynerr (hcl_t* hcl, hcl_synerr_t* synerr)
{
	HCL_ASSERT (hcl, hcl->c != HCL_NULL);
	if (synerr) *synerr = hcl->c->synerr;
}

void hcl_setsynerrbfmt (hcl_t* hcl, hcl_synerrnum_t num, const hcl_ioloc_t* loc, const hcl_oocs_t* tgt, const hcl_bch_t* msgfmt, ...)
{
	static hcl_bch_t syntax_error[] = "syntax error - ";

	if (hcl->shuterr) return;

	if (msgfmt) 
	{
		va_list ap;
		int i, selen;

		va_start (ap, msgfmt);
		hcl_seterrbfmtv (hcl, HCL_ESYNERR, msgfmt, ap);
		va_end (ap);

		selen = HCL_COUNTOF(syntax_error) - 1;
		HCL_MEMMOVE (&hcl->errmsg.buf[selen], &hcl->errmsg.buf[0], HCL_SIZEOF(hcl->errmsg.buf[0]) * (HCL_COUNTOF(hcl->errmsg.buf) - selen));
		for (i = 0; i < selen; i++) hcl->errmsg.buf[i] = syntax_error[i];
		hcl->errmsg.buf[HCL_COUNTOF(hcl->errmsg.buf) - 1] = '\0';
	}
	else 
	{
		hcl_seterrbfmt (hcl, HCL_ESYNERR, "%hs%hs", syntax_error, synerr_to_errstr(num));
	}
	hcl->c->synerr.num = num;

	/* The SCO compiler complains of this ternary operation saying:
	 *    error: operands have incompatible types: op ":" 
	 * it seems to complain of type mismatch between *loc and
	 * hcl->c->tok.loc due to 'const' prefixed to loc. */
	/*hcl->c->synerr.loc = loc? *loc: hcl->c->tok.loc;*/
	if (loc)
	{
		hcl->c->synerr.loc = *loc;
	}
	else
	{
		hcl->c->synerr.loc = hcl->c->tok.loc;
	}

	if (tgt) 
	{
		hcl->c->synerr.tgt = *tgt;
	}
	else 
	{
		hcl->c->synerr.tgt.ptr = HCL_NULL;
		hcl->c->synerr.tgt.len = 0;
	}
}

void hcl_setsynerrufmt (hcl_t* hcl, hcl_synerrnum_t num, const hcl_ioloc_t* loc, const hcl_oocs_t* tgt, const hcl_uch_t* msgfmt, ...)
{
	static hcl_bch_t syntax_error[] = "syntax error - ";

	if (hcl->shuterr) return;

	if (msgfmt) 
	{
		va_list ap;
		int i, selen;

		va_start (ap, msgfmt);
		hcl_seterrufmtv (hcl, HCL_ESYNERR, msgfmt, ap);
		va_end (ap);

		selen = HCL_COUNTOF(syntax_error) - 1;
		HCL_MEMMOVE (&hcl->errmsg.buf[selen], &hcl->errmsg.buf[0], HCL_SIZEOF(hcl->errmsg.buf[0]) * (HCL_COUNTOF(hcl->errmsg.buf) - selen));
		for (i = 0; i < selen; i++) hcl->errmsg.buf[i] = syntax_error[i];
		hcl->errmsg.buf[HCL_COUNTOF(hcl->errmsg.buf) - 1] = '\0';
	}
	else 
	{
		hcl_seterrbfmt (hcl, HCL_ESYNERR, "%hs%hs", syntax_error, synerr_to_errstr(num));
	}
	hcl->c->synerr.num = num;

	/* The SCO compiler complains of this ternary operation saying:
	 *    error: operands have incompatible types: op ":" 
	 * it seems to complain of type mismatch between *loc and
	 * hcl->c->tok.loc due to 'const' prefixed to loc. */
	/*hcl->c->synerr.loc = loc? *loc: hcl->c->tok.loc;*/
	if (loc)
	{
		hcl->c->synerr.loc = *loc;
	}
	else
	{
		hcl->c->synerr.loc = hcl->c->tok.loc;
	}

	if (tgt)
	{
		hcl->c->synerr.tgt = *tgt;
	}
	else 
	{
		hcl->c->synerr.tgt.ptr = HCL_NULL;
		hcl->c->synerr.tgt.len = 0;
	}
}


/* -------------------------------------------------------------------------- 
 * ASSERTION FAILURE HANDLERsemaphore heap full
 * -------------------------------------------------------------------------- */

void hcl_assertfailed (hcl_t* hcl, const hcl_bch_t* expr, const hcl_bch_t* file, hcl_oow_t line)
{
#if defined(HAVE_BACKTRACE)
	void* btarray[128];
	hcl_oow_t btsize;
	char** btsyms;
#endif

	hcl_logbfmt (hcl, HCL_LOG_UNTYPED | HCL_LOG_FATAL, "ASSERTION FAILURE: %s at %s:%zu\n", expr, file, line);

#if defined(HAVE_BACKTRACE)
	btsize = backtrace (btarray, HCL_COUNTOF(btarray));
	btsyms = backtrace_symbols (btarray, btsize);
	if (btsyms)
	{
		hcl_oow_t i;
		hcl_logbfmt (hcl, HCL_LOG_UNTYPED | HCL_LOG_DEBUG, "[BACKTRACE]\n");

		for (i = 0; i < btsize; i++)
		{
			hcl_logbfmt(hcl, HCL_LOG_UNTYPED | HCL_LOG_DEBUG, "  %s\n", btsyms[i]);
		}
		free (btsyms);
	}
#endif


#if defined(_WIN32)
	ExitProcess (249);
#elif defined(__OS2__)
	DosExit (EXIT_PROCESS, 249);
#elif defined(__DOS__)
	{
		union REGS regs;
		regs.h.ah = DOS_EXIT;
		regs.h.al = 249;
		intdos (&regs, &regs);
	}
#elif defined(vms) || defined(__vms)
	lib$stop (SS$_ABORT); /* use SS$_OPCCUS instead? */
	/* this won't be reached since lib$stop() terminates the process */
	sys$exit (SS$_ABORT); /* this condition code can be shown with
	                       * 'show symbol $status' from the command-line. */
#elif defined(macintosh)

	ExitToShell ();

#else

	kill (getpid(), SIGABRT);
	_exit (1);
#endif
}
