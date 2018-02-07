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
#include "hcl-opt.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <locale.h>


#if defined(_WIN32)
#	include <windows.h>
#	include <tchar.h>
#	if defined(STIX_HAVE_CFG_H)
#		include <ltdl.h>
#		define USE_LTDL
#	endif
#elif defined(__OS2__)
#	define INCL_DOSMODULEMGR
#	define INCL_DOSPROCESS
#	define INCL_DOSERRORS
#	include <os2.h>
#elif defined(__MSDOS__)
#	include <dos.h>
#	include <time.h>
#elif defined(macintosh)
#	include <Timer.h>
#else
#	include <errno.h>
#	include <unistd.h>
#	include <fcntl.h>
#	include <ltdl.h>
#	define USE_LTDL

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


typedef struct bb_t bb_t;
struct bb_t
{
	char buf[1024];
	hcl_oow_t pos;
	hcl_oow_t len;

	FILE* fp;
	hcl_bch_t* fn;
};

typedef struct xtn_t xtn_t;
struct xtn_t
{
	const char* read_path; /* main source file */
	const char* print_path; 

	int logfd;
	int logmask;
	int logfd_istty;
};

/* ========================================================================= */

static void* sys_alloc (hcl_mmgr_t* mmgr, hcl_oow_t size)
{
	return malloc (size);
}

static void* sys_realloc (hcl_mmgr_t* mmgr, void* ptr, hcl_oow_t size)
{
	return realloc (ptr, size);
}

static void sys_free (hcl_mmgr_t* mmgr, void* ptr)
{
	free (ptr);
}

static hcl_mmgr_t sys_mmgr =
{
	sys_alloc,
	sys_realloc,
	sys_free,
	HCL_NULL
};

/* ========================================================================= */


#if defined(_WIN32) || defined(__OS2__) || defined(__DOS__)
#	define IS_PATH_SEP(c) ((c) == '/' || (c) == '\\')
#else
#	define IS_PATH_SEP(c) ((c) == '/')
#endif


static const hcl_bch_t* get_base_name (const hcl_bch_t* path)
{
	const hcl_bch_t* p, * last = HCL_NULL;

	for (p = path; *p != '\0'; p++)
	{
		if (IS_PATH_SEP(*p)) last = p;
	}

	return (last == HCL_NULL)? path: (last + 1);
}

static HCL_INLINE hcl_ooi_t open_input (hcl_t* hcl, hcl_ioinarg_t* arg)
{
	xtn_t* xtn = hcl_getxtn(hcl);
	bb_t* bb = HCL_NULL;

/* TOOD: support predefined include directory as well */
	if (arg->includer)
	{
		/* includee */
		hcl_oow_t ucslen, bcslen, parlen;
		const hcl_bch_t* fn, * fb;

	#if defined(HCL_OOCH_IS_UCH)
		if (hcl_convootobcstr (hcl, arg->name, &ucslen, HCL_NULL, &bcslen) <= -1) goto oops;
	#else
		bcslen = hcl_countbcstr (arg->name);
	#endif

		fn = ((bb_t*)arg->includer->handle)->fn;

		fb = get_base_name (fn);
		parlen = fb - fn;

		bb = hcl_callocmem (hcl, HCL_SIZEOF(*bb) + (HCL_SIZEOF(hcl_bch_t) * (parlen + bcslen + 1)));
		if (!bb) goto oops;

		bb->fn = (hcl_bch_t*)(bb + 1);
		hcl_copybchars (bb->fn, fn, parlen);
	#if defined(HCL_OOCH_IS_UCH)
		hcl_convootobcstr (hcl, arg->name, &ucslen, &bb->fn[parlen], &bcslen);
	#else
		hcl_copybcstr (&bb->fn[parlen], bcslen + 1, arg->name);
	#endif
	}
	else
	{
		/* main stream */
		hcl_oow_t pathlen;

		pathlen = hcl_countbcstr (xtn->read_path);

		bb = hcl_callocmem (hcl, HCL_SIZEOF(*bb) + (HCL_SIZEOF(hcl_bch_t) * (pathlen + 1)));
		if (!bb) goto oops;

		bb->fn = (hcl_bch_t*)(bb + 1);
		hcl_copybcstr (bb->fn, pathlen + 1, xtn->read_path);
	}

#if defined(__DOS__) || defined(_WIN32) || defined(__OS2__)
	bb->fp = fopen (bb->fn, "rb");
#else
	bb->fp = fopen (bb->fn, "r");
#endif
	if (!bb->fp)
	{
		hcl_seterrnum (hcl, HCL_EIOERR);
		goto oops;
	}

	arg->handle = bb;
	return 0;

oops:
	if (bb) 
	{
		if (bb->fp) fclose (bb->fp);
		hcl_freemem (hcl, bb);
	}
	return -1;
}

static HCL_INLINE hcl_ooi_t close_input (hcl_t* hcl, hcl_ioinarg_t* arg)
{
	/*xtn_t* xtn = hcl_getxtn(hcl);*/
	bb_t* bb;

	bb = (bb_t*)arg->handle;
	HCL_ASSERT (hcl, bb != HCL_NULL && bb->fp != HCL_NULL);

	fclose (bb->fp);
	hcl_freemem (hcl, bb);

	arg->handle = HCL_NULL;
	return 0;
}


static HCL_INLINE hcl_ooi_t read_input (hcl_t* hcl, hcl_ioinarg_t* arg)
{
	/*xtn_t* xtn = hcl_getxtn(hcl);*/
	bb_t* bb;
	hcl_oow_t bcslen, ucslen, remlen;
	int x;

	bb = (bb_t*)arg->handle;
	HCL_ASSERT (hcl, bb != HCL_NULL && bb->fp != HCL_NULL);
	do
	{
		x = fgetc (bb->fp);
		if (x == EOF)
		{
			if (ferror((FILE*)bb->fp))
			{
				hcl_seterrnum (hcl, HCL_EIOERR);
				return -1;
			}
			break;
		}

		bb->buf[bb->len++] = x;
	}
	while (bb->len < HCL_COUNTOF(bb->buf) && x != '\r' && x != '\n');

#if defined(HCL_OOCH_IS_UCH)
	bcslen = bb->len;
	ucslen = HCL_COUNTOF(arg->buf);
	x = hcl_convbtooochars (hcl, bb->buf, &bcslen, arg->buf, &ucslen);
	if (x <= -1 && ucslen <= 0) return -1;
	/* if ucslen is greater than 0, i see that some characters have been
	 * converted properly */
#else
	bcslen = (bb->len < HCL_COUNTOF(arg->buf))? bb->len: HCL_COUNTOF(arg->buf);
	ucslen = bcslen;
	hcl_copybchars (arg->buf, bb->buf, bcslen);
#endif

	remlen = bb->len - bcslen;
	if (remlen > 0) memmove (bb->buf, &bb->buf[bcslen], remlen);
	bb->len = remlen;
	return ucslen;
}


static hcl_ooi_t read_handler (hcl_t* hcl, hcl_iocmd_t cmd, void* arg)
{
	switch (cmd)
	{
		case HCL_IO_OPEN:
			return open_input (hcl, (hcl_ioinarg_t*)arg);
			
		case HCL_IO_CLOSE:
			return close_input (hcl, (hcl_ioinarg_t*)arg);

		case HCL_IO_READ:
			return read_input (hcl, (hcl_ioinarg_t*)arg);

		default:
			hcl_seterrnum (hcl, HCL_EINTERN);
			return -1;
	}
}

static HCL_INLINE hcl_ooi_t open_output(hcl_t* hcl, hcl_iooutarg_t* arg)
{
	xtn_t* xtn = hcl_getxtn(hcl);
	FILE* fp;

#if defined(__MSDOS__) || defined(_WIN32) || defined(__OS2__)
	if (xtn->print_path) fp = fopen (xtn->print_path, "wb");
	else fp = stdout;
#else
	if (xtn->print_path) fp = fopen (xtn->print_path, "w");
	else fp = stdout;
#endif
	if (!fp)
	{
		hcl_seterrnum (hcl, HCL_EIOERR);
		return -1;
	}

	arg->handle = fp;
	return 0;
}
  
static HCL_INLINE hcl_ooi_t close_output (hcl_t* hcl, hcl_iooutarg_t* arg)
{
	/*xtn_t* xtn = hcl_getxtn(hcl);*/
	FILE* fp;

	fp = (FILE*)arg->handle;
	HCL_ASSERT (hcl, fp != HCL_NULL);

	fclose (fp);
	arg->handle = HCL_NULL;
	return 0;
}


static HCL_INLINE hcl_ooi_t write_output (hcl_t* hcl, hcl_iooutarg_t* arg)
{
	/*xtn_t* xtn = hcl_getxtn(hcl);*/
	hcl_bch_t bcsbuf[1024];
	hcl_oow_t bcslen, ucslen, donelen;
	int x;

	donelen = 0;

	do 
	{
#if defined(HCL_OOCH_IS_UCH)
		bcslen = HCL_COUNTOF(bcsbuf);
		ucslen = arg->len - donelen;
		x = hcl_convootobchars (hcl, &arg->ptr[donelen], &ucslen, bcsbuf, &bcslen);
		if (x <= -1 && ucslen <= 0) return -1;
#else
		bcslen = HCL_COUNTOF(bcsbuf);
		ucslen = arg->len - donelen;
		if (ucslen > bcslen) ucslen = bcslen;
		else if (ucslen < bcslen) bcslen = ucslen;
		hcl_copybchars (&arg->buf[donelen], bcsbuf, bcslen);
#endif

		if (fwrite (bcsbuf, HCL_SIZEOF(bcsbuf[0]), bcslen, (FILE*)arg->handle) < bcslen)
		{
			hcl_seterrnum (hcl, HCL_EIOERR);
			return -1;
		}

		donelen += ucslen;
	}
	while (donelen < arg->len);

	return arg->len;
}

static hcl_ooi_t print_handler (hcl_t* hcl, hcl_iocmd_t cmd, void* arg)
{
	switch (cmd)
	{
		case HCL_IO_OPEN:
			return open_output (hcl, (hcl_iooutarg_t*)arg);

		case HCL_IO_CLOSE:
			return close_output (hcl, (hcl_iooutarg_t*)arg);

		case HCL_IO_WRITE:
			return write_output (hcl, (hcl_iooutarg_t*)arg);

		default:
			hcl_seterrnum (hcl, HCL_EINTERN);
			return -1;
	}
}

/* ========================================================================= */

static int write_all (int fd, const char* ptr, hcl_oow_t len)
{
	while (len > 0)
	{
		hcl_ooi_t wr;

		wr = write (1, ptr, len);

		if (wr <= -1)
		{
		#if defined(EAGAIN) && defined(EWOULDBLOCK) && (EAGAIN == EWOULDBLOCK)
			if (errno == EAGAIN) continue;
		#else
			#	if defined(EAGAIN)
			if (errno == EAGAIN) continue;
			#elif defined(EWOULDBLOCK)
			if (errno == EWOULDBLOCK) continue;
			#endif
		#endif
			return -1;
		}

		ptr += wr;
		len -= wr;
	}

	return 0;
}

static void log_write (hcl_t* hcl, hcl_oow_t mask, const hcl_ooch_t* msg, hcl_oow_t len)
{
#if defined(_WIN32)
#	error NOT IMPLEMENTED 
	
#elif defined(macintosh)
#	error NOT IMPLEMENTED
#else
	hcl_bch_t buf[256];
	hcl_oow_t ucslen, bcslen, msgidx;
	int n;

	xtn_t* xtn = hcl_getxtn(hcl);
	int logfd;

	if (mask & HCL_LOG_STDERR)
	{
		/* the messages that go to STDERR don't get masked out */
		logfd = 2;
	}
	else
	{
		if (!(xtn->logmask & mask & ~HCL_LOG_ALL_LEVELS)) return;  /* check log types */
		if (!(xtn->logmask & mask & ~HCL_LOG_ALL_TYPES)) return;  /* check log levels */

		if (mask & HCL_LOG_STDOUT) logfd = 1;
		else
		{

			logfd = xtn->logfd;
			if (logfd <= -1) return;
		}
	}

/* TODO: beautify the log message.
 *       do classification based on mask. */
	if (!(mask & (HCL_LOG_STDOUT | HCL_LOG_STDERR)))
	{
		time_t now;
		char ts[32];
		size_t tslen;
		struct tm tm, *tmp;

		now = time(NULL);
	#if defined(__DOS__)
		tmp = localtime (&now);
		tslen = strftime (ts, sizeof(ts), "%Y-%m-%d %H:%M:%S ", tmp); /* no timezone info */
		if (tslen == 0) 
		{
			strcpy (ts, "0000-00-00 00:00:00");
			tslen = 19; 
		}
	#else
		tmp = localtime_r (&now, &tm);
		#if defined(HAVE_STRFTIME_SMALL_Z)
		tslen = strftime (ts, sizeof(ts), "%Y-%m-%d %H:%M:%S %z ", tmp);
		#else
		tslen = strftime (ts, sizeof(ts), "%Y-%m-%d %H:%M:%S %Z ", tmp); 
		#endif
		if (tslen == 0) 
		{
			strcpy (ts, "0000-00-00 00:00:00 +0000");
			tslen = 25; 
		}
	#endif
		write_all (logfd, ts, tslen);
	}


	if (xtn->logfd_istty)
	{
		if (mask & HCL_LOG_FATAL) write_all (logfd, "\x1B[1;31m", 7);
		else if (mask & HCL_LOG_ERROR) write_all (logfd, "\x1B[1;32m", 7);
		else if (mask & HCL_LOG_WARN) write_all (logfd, "\x1B[1;33m", 7);
	}

#if defined(HCL_OOCH_IS_UCH)
	msgidx = 0;
	while (len > 0)
	{
		ucslen = len;
		bcslen = HCL_COUNTOF(buf);

		n = hcl_convootobchars (hcl, &msg[msgidx], &ucslen, buf, &bcslen);
		if (n == 0 || n == -2)
		{
			/* n = 0: 
			 *   converted all successfully 
			 * n == -2: 
			 *    buffer not sufficient. not all got converted yet.
			 *    write what have been converted this round. */

			HCL_ASSERT (hcl, ucslen > 0); /* if this fails, the buffer size must be increased */

			/* attempt to write all converted characters */
			if (write_all (logfd, buf, bcslen) <= -1) break;

			if (n == 0) break;
			else
			{
				msgidx += ucslen;
				len -= ucslen;
			}
		}
		else if (n <= -1)
		{
			/* conversion error */
			break;
		}
	}
#else
	write_all (logfd, msg, len);
#endif

	if (xtn->logfd_istty)
	{
		if (mask & (HCL_LOG_FATAL | HCL_LOG_ERROR | HCL_LOG_WARN)) write_all (logfd, "\x1B[0m", 4);
	}

#endif
}


static void syserrstrb (hcl_t* hcl, int syserr, hcl_bch_t* buf, hcl_oow_t len)
{
#if defined(HAVE_STRERROR_R)
	strerror_r (syserr, buf, len);
#else
	/* this is not thread safe */
	hcl_copybcstr (buf, len, strerror(syserr));
#endif
}

static void fini_hcl (hcl_t* hcl)
{
	xtn_t* xtn = hcl_getxtn(hcl);
	if (xtn->logfd >= 0)
	{
		close (xtn->logfd);
		xtn->logfd = -1;
		xtn->logfd_istty = 0;
	}
}


static int handle_logopt (hcl_t* hcl, const hcl_bch_t* str)
{
	xtn_t* xtn = hcl_getxtn (hcl);
	hcl_bch_t* xstr = (hcl_bch_t*)str;
	hcl_bch_t* cm, * flt;

	cm = hcl_findbcharinbcstr (xstr, ',');
	if (cm) 
	{
		/* i duplicate this string for open() below as open() doesn't 
		 * accept a length-bounded string */
		xstr = hcl_dupbchars (hcl, str, hcl_countbcstr(str));
		if (!xstr) 
		{
			fprintf (stderr, "ERROR: out of memory in duplicating %s\n", str);
			return -1;
		}

		cm = hcl_findbcharinbcstr(xstr, ',');
		*cm = '\0';

		do
		{
			flt = cm + 1;

			cm = hcl_findbcharinbcstr(flt, ',');
			if (cm) *cm = '\0';

			if (hcl_compbcstr(flt, "app") == 0) xtn->logmask |= HCL_LOG_APP;
			else if (hcl_compbcstr(flt, "compiler") == 0) xtn->logmask |= HCL_LOG_COMPILER;
			else if (hcl_compbcstr(flt, "vm") == 0) xtn->logmask |= HCL_LOG_VM;
			else if (hcl_compbcstr(flt, "mnemonic") == 0) xtn->logmask |= HCL_LOG_MNEMONIC;
			else if (hcl_compbcstr(flt, "gc") == 0) xtn->logmask |= HCL_LOG_GC;
			else if (hcl_compbcstr(flt, "ic") == 0) xtn->logmask |= HCL_LOG_IC;
			else if (hcl_compbcstr(flt, "primitive") == 0) xtn->logmask |= HCL_LOG_PRIMITIVE;

			else if (hcl_compbcstr(flt, "fatal") == 0) xtn->logmask |= HCL_LOG_FATAL;
			else if (hcl_compbcstr(flt, "error") == 0) xtn->logmask |= HCL_LOG_ERROR;
			else if (hcl_compbcstr(flt, "warn") == 0) xtn->logmask |= HCL_LOG_WARN;
			else if (hcl_compbcstr(flt, "info") == 0) xtn->logmask |= HCL_LOG_INFO;
			else if (hcl_compbcstr(flt, "debug") == 0) xtn->logmask |= HCL_LOG_DEBUG;

			else if (hcl_compbcstr(flt, "fatal+") == 0) xtn->logmask |= HCL_LOG_FATAL;
			else if (hcl_compbcstr(flt, "error+") == 0) xtn->logmask |= HCL_LOG_FATAL | HCL_LOG_ERROR;
			else if (hcl_compbcstr(flt, "warn+") == 0) xtn->logmask |= HCL_LOG_FATAL | HCL_LOG_ERROR | HCL_LOG_WARN;
			else if (hcl_compbcstr(flt, "info+") == 0) xtn->logmask |= HCL_LOG_FATAL | HCL_LOG_ERROR | HCL_LOG_WARN | HCL_LOG_INFO;
			else if (hcl_compbcstr(flt, "debug+") == 0) xtn->logmask |= HCL_LOG_FATAL | HCL_LOG_ERROR | HCL_LOG_WARN | HCL_LOG_INFO | HCL_LOG_DEBUG;

			else
			{
				fprintf (stderr, "ERROR: unknown log option value - %s\n", flt);
				if (str != xstr) hcl_freemem (hcl, xstr);
				return -1;
			}
		}
		while (cm);


		if (!(xtn->logmask & HCL_LOG_ALL_TYPES)) xtn->logmask |= HCL_LOG_ALL_TYPES;  /* no types specified. force to all types */
		if (!(xtn->logmask & HCL_LOG_ALL_LEVELS)) xtn->logmask |= HCL_LOG_ALL_LEVELS;  /* no levels specified. force to all levels */
	}
	else
	{
		xtn->logmask = HCL_LOG_ALL_LEVELS | HCL_LOG_ALL_TYPES;
	}

	xtn->logfd = open (xstr, O_CREAT | O_WRONLY | O_APPEND , 0644);
	if (xtn->logfd == -1)
	{
		fprintf (stderr, "ERROR: cannot open a log file %s\n", xstr);
		if (str != xstr) hcl_freemem (hcl, xstr);
		return -1;
	}

#if defined(HAVE_ISATTY)
	xtn->logfd_istty = isatty(xtn->logfd);
#endif

	if (str != xstr) hcl_freemem (hcl, xstr);
	return 0;
}

#if !defined(NDEBUG)
static int handle_dbgopt (hcl_t* hcl, const hcl_bch_t* str)
{
	xtn_t* xtn = hcl_getxtn (hcl);
	const hcl_bch_t* cm, * flt;
	hcl_oow_t len;
	unsigned int trait, dbgopt = 0;

	cm = str - 1;
	do
	{
		flt = cm + 1;

		cm = hcl_findbcharinbcstr(flt, ',');
		len = cm? (cm - flt): hcl_countbcstr(flt);
		if (hcl_compbcharsbcstr (flt, len, "gc") == 0)  dbgopt |= HCL_DEBUG_GC;
		else if (hcl_compbcharsbcstr (flt, len, "bigint") == 0)  dbgopt |= HCL_DEBUG_BIGINT;
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

static hcl_t* g_hcl = HCL_NULL;

/* ========================================================================= */


#if defined(__MSDOS__) && defined(_INTELC32_)
static void (*prev_timer_intr_handler) (void);

#pragma interrupt(timer_intr_handler)
static void timer_intr_handler (void)
{
	/*
	_XSTACK *stk;
	int r;
	stk = (_XSTACK *)_get_stk_frame();
	r = (unsigned short)stk_ptr->eax;   
	*/

	/* The timer interrupt (normally) occurs 18.2 times per second. */
	if (g_hcl) hcl_switchprocess (g_hcl);
	_chain_intr(prev_timer_intr_handler);
}

#elif defined(macintosh)

static TMTask g_tmtask;
static ProcessSerialNumber g_psn;

#define TMTASK_DELAY 50 /* milliseconds if positive, microseconds(after negation) if negative */

static pascal void timer_intr_handler (TMTask* task)
{
	if (g_hcl) hcl_switchprocess (g_hcl);
	WakeUpProcess (&g_psn);
	PrimeTime ((QElem*)&g_tmtask, TMTASK_DELAY);
}

#else
static void arrange_process_switching (int sig)
{
	if (g_hcl) hcl_switchprocess (g_hcl);
}
#endif

static void setup_tick (void)
{
#if defined(__MSDOS__) && defined(_INTELC32_)

	prev_timer_intr_handler = _dos_getvect (0x1C);
	_dos_setvect (0x1C, timer_intr_handler);

#elif defined(macintosh)

	GetCurrentProcess (&g_psn);
	memset (&g_tmtask, 0, HCL_SIZEOF(g_tmtask));
	g_tmtask.tmAddr = NewTimerProc (timer_intr_handler);
	InsXTime ((QElem*)&g_tmtask);

	PrimeTime ((QElem*)&g_tmtask, TMTASK_DELAY);

#elif defined(HAVE_SETITIMER) && defined(SIGVTALRM) && defined(ITIMER_VIRTUAL)
	struct itimerval itv;
	struct sigaction act;

	sigemptyset (&act.sa_mask);
	act.sa_handler = arrange_process_switching;
	act.sa_flags = 0;
	sigaction (SIGVTALRM, &act, HCL_NULL);

	itv.it_interval.tv_sec = 0;
	itv.it_interval.tv_usec = 100; /* 100 microseconds */
	itv.it_value.tv_sec = 0;
	itv.it_value.tv_usec = 100;
	setitimer (ITIMER_VIRTUAL, &itv, HCL_NULL);
#else

#	error UNSUPPORTED
#endif
}

static void cancel_tick (void)
{
#if defined(__MSDOS__) && defined(_INTELC32_)

	_dos_setvect (0x1C, prev_timer_intr_handler);

#elif defined(macintosh)
	RmvTime ((QElem*)&g_tmtask);
	/*DisposeTimerProc (g_tmtask.tmAddr);*/

#elif defined(HAVE_SETITIMER) && defined(SIGVTALRM) && defined(ITIMER_VIRTUAL)
	struct itimerval itv;
	struct sigaction act;

	itv.it_interval.tv_sec = 0;
	itv.it_interval.tv_usec = 0;
	itv.it_value.tv_sec = 0; /* make setitimer() one-shot only */
	itv.it_value.tv_usec = 0;
	setitimer (ITIMER_VIRTUAL, &itv, HCL_NULL);

	sigemptyset (&act.sa_mask); 
	act.sa_handler = SIG_IGN; /* ignore the signal potentially fired by the one-shot arrange above */
	act.sa_flags = 0;
	sigaction (SIGVTALRM, &act, HCL_NULL);

#else
#	error UNSUPPORTED
#endif
}

/* ========================================================================= */


/* ========================================================================= */

static void print_synerr (hcl_t* hcl)
{
	hcl_synerr_t synerr;
	xtn_t* xtn;

	xtn = hcl_getxtn (hcl);
	hcl_getsynerr (hcl, &synerr);

	hcl_logbfmt (hcl,HCL_LOG_STDERR, "ERROR: ");
	if (synerr.loc.file)
	{
		hcl_logbfmt (hcl, HCL_LOG_STDERR, "%js", synerr.loc.file);
	}
	else
	{
		hcl_logbfmt (hcl, HCL_LOG_STDERR, "%s", xtn->read_path);
	}

	hcl_logbfmt (hcl, HCL_LOG_STDERR, "[%zu,%zu] %js", 
		synerr.loc.line, synerr.loc.colm,
		(hcl_geterrmsg(hcl) != hcl_geterrstr(hcl)? hcl_geterrmsg(hcl): hcl_geterrstr(hcl))
	);

	if (synerr.tgt.len > 0)
	{
		hcl_logbfmt (hcl, HCL_LOG_STDERR, " - %.*js", synerr.tgt.len, synerr.tgt.ptr);
	}

	hcl_logbfmt (hcl, HCL_LOG_STDERR, "\n");
}

#define MIN_MEMSIZE 512000ul
 
int main (int argc, char* argv[])
{
	hcl_t* hcl;
	xtn_t* xtn;
	hcl_vmprim_t vmprim;
	hcl_cb_t hclcb;

	hcl_bci_t c;
	static hcl_bopt_lng_t lopt[] =
	{
		{ ":log",         'l' },
		{ ":memsize",     'm' },
#if !defined(NDEBUG)
		{ ":debug",       '\0' }, /* NOTE: there is no short option for --debug */
#endif
		{ HCL_NULL,       '\0' }
	};
	static hcl_bopt_t opt =
	{
		"l:m:",
		lopt
	};

	const char* logopt = HCL_NULL;
	hcl_oow_t memsize = MIN_MEMSIZE;

#if !defined(NDEBUG)
	const char* dbgopt = HCL_NULL;
#endif

	setlocale (LC_ALL, "");

#if !defined(macintosh)
	if (argc < 2)
	{
	print_usage:
		fprintf (stderr, "Usage: %s filename ...\n", argv[0]);
		return -1;
	}

	while ((c = hcl_getbopt (argc, argv, &opt)) != HCL_BCI_EOF)
	{
		switch (c)
		{
			case 'l':
				logopt = opt.arg;
				break;

			case 'm':
				memsize = strtoul(opt.arg, HCL_NULL, 0);
				if (memsize <= MIN_MEMSIZE) memsize = MIN_MEMSIZE;
				break;

			case '\0':
				
			#if !defined(NDEBUG)
				if (hcl_compbcstr(opt.lngopt, "debug") == 0)
				{
					dbgopt = opt.arg;
					break;
				}
			#endif

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

	if (opt.ind >= argc) goto print_usage;
#endif


	memset (&vmprim, 0, HCL_SIZEOF(vmprim));
	vmprim.log_write = log_write;
	vmprim.syserrstrb = syserrstrb;

	hcl = hcl_open (&sys_mmgr, HCL_SIZEOF(xtn_t), 2048000lu, &vmprim, HCL_NULL);
	if (!hcl)
	{
		printf ("cannot open hcl\n");
		return -1;
	}


	{
		hcl_oow_t tab_size;

		tab_size = 5000;
		hcl_setoption (hcl, HCL_SYMTAB_SIZE, &tab_size);
		tab_size = 5000;
		hcl_setoption (hcl, HCL_SYSDIC_SIZE, &tab_size);
		tab_size = 600;
		hcl_setoption (hcl, HCL_PROCSTK_SIZE, &tab_size);
	}

	{
		int trait = 0;

		/*trait |= HCL_NOGC;*/
		trait |= HCL_AWAIT_PROCS;
		hcl_setoption (hcl, HCL_TRAIT, &trait);

		/* disable GC logs */
		trait = ~HCL_LOG_GC;
		hcl_setoption (hcl, HCL_LOG_MASK, &trait);
	}

	xtn = hcl_getxtn (hcl);
	xtn->logfd = -1;
	xtn->logfd_istty = 0;

	memset (&hclcb, 0, HCL_SIZEOF(hclcb));
	hclcb.fini = fini_hcl;
	hcl_regcb (hcl, &hclcb);


	if (logopt)
	{
		if (handle_logopt (hcl, logopt) <= -1) 
		{
			hcl_close (hcl);
			return -1;
		}
	}
	else
	{
		/* default logging mask when no logging option is set */
		xtn->logmask = HCL_LOG_ALL_TYPES | HCL_LOG_ERROR | HCL_LOG_FATAL;
	}

#if !defined(NDEBUG)
	if (dbgopt)
	{
		if (handle_dbgopt (hcl, dbgopt) <= -1)
		{
			hcl_close (hcl);
			return -1;
		}
	}
#endif

	if (hcl_ignite(hcl) <= -1)
	{
		hcl_logbfmt (hcl, HCL_LOG_STDERR, "cannot ignite hcl - [%d] %js\n", hcl_geterrnum(hcl), hcl_geterrmsg(hcl));
		hcl_close (hcl);
		return -1;
	}

	if (hcl_addbuiltinprims(hcl) <= -1)
	{
		hcl_logbfmt (hcl, HCL_LOG_STDERR, "cannot add builtin primitives - [%d] %js\n", hcl_geterrnum(hcl), hcl_geterrmsg(hcl));
		hcl_close (hcl);
		return -1;
	}

	xtn->read_path = argv[opt.ind++];
	if (opt.ind < argc) xtn->print_path = argv[opt.ind++];

	if (hcl_attachio (hcl, read_handler, print_handler) <= -1)
	{
		hcl_logbfmt (hcl, HCL_LOG_STDERR, "ERROR: cannot attache input stream - [%d] %js\n", hcl_geterrnum(hcl), hcl_geterrmsg(hcl));
		hcl_close (hcl);
		return -1;
	}

	while (1)
	{
		hcl_oop_t obj;

		obj = hcl_read(hcl);
		if (!obj)
		{
			if (hcl->errnum == HCL_EFINIS)
			{
				/* end of input */
				break;
			}
			else if (hcl->errnum == HCL_ESYNERR)
			{
				print_synerr (hcl);
			}
			else
			{
				hcl_logbfmt (hcl, HCL_LOG_STDERR, "ERROR: cannot read object - [%d] %js\n", hcl_geterrnum(hcl), hcl_geterrmsg(hcl));
			}

			break;
		}


		if (hcl_print(hcl, obj) <= -1)
		{
			hcl_logbfmt (hcl, HCL_LOG_STDERR, "ERROR: cannot print object - [%d] %js\n", hcl_geterrnum(hcl), hcl_geterrmsg(hcl));
		}
		else
		{
			hcl_proutbfmt (hcl, 0, "\n");
			if (hcl_compile(hcl, obj) <= -1)
			{
				if (hcl->errnum == HCL_ESYNERR)
				{
					print_synerr (hcl);
				}
				else
				{
					hcl_logbfmt (hcl, HCL_LOG_STDERR, "ERROR: cannot compile object - [%d] %js\n", hcl_geterrnum(hcl), hcl_geterrmsg(hcl));
				}

				/* carry on? */
			}
		}
	}

hcl_decode (hcl, 0, hcl->code.bc.len);
HCL_LOG0 (hcl, HCL_LOG_MNEMONIC, "------------------------------------------\n");
g_hcl = hcl;
//setup_tick ();
if (hcl_execute(hcl) <= -1)
{
	hcl_logbfmt (hcl, HCL_LOG_STDERR, "ERROR: cannot execute - [%d] %js\n", hcl_geterrnum(hcl), hcl_geterrmsg(hcl));

}
//cancel_tick();
g_hcl = HCL_NULL;



{
HCL_LOG0 (hcl, HCL_LOG_MNEMONIC, "------------------------------------------\n");
HCL_LOG2 (hcl, HCL_LOG_MNEMONIC, "BYTECODES hcl->code.bc.len = > %lu hcl->code.lit.len => %lu\n", 
	(unsigned long int)hcl->code.bc.len, (unsigned long int)hcl->code.lit.len);
hcl_decode (hcl, 0, hcl->code.bc.len);

/*hcl_dumpsymtab (hcl);*/
}


	hcl_close (hcl);

#if defined(_WIN32) && defined(_DEBUG)
getchar();
#endif
	return 0;
}
