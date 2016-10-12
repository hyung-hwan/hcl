/*
 * $Id$
 *
    Copyright (c) 2014-2016 Chung, Hyung-Hwan. All rights reserved.

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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>


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
#elif defined(macintosh)
#	include <Timer.h>
#else
#	include <errno.h>
#	include <unistd.h>
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
};

typedef struct xtn_t xtn_t;
struct xtn_t
{
	const char* read_path; /* main source file */
	const char* print_path; 
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

static HCL_INLINE hcl_ooi_t open_input (hcl_t* hcl, hcl_ioinarg_t* arg)
{
	xtn_t* xtn = hcl_getxtn(hcl);
	bb_t* bb;
	FILE* infp = HCL_NULL, * outfp = HCL_NULL;

	if (arg->includer)
	{
		/* includee */
		hcl_bch_t bcs[1024]; /* TODO: right buffer size */
		hcl_oow_t bcslen = HCL_COUNTOF(bcs);
		hcl_oow_t ucslen = ~(hcl_oow_t)0;

		if (hcl_ucstoutf8 (arg->name, &ucslen, bcs, &bcslen) <= -1)
		{
			hcl_seterrnum (hcl, HCL_EECERR);
			return -1;
		}

/* TODO: make bcs relative to the includer */
#if defined(__MSDOS__) || defined(_WIN32) || defined(__OS2__)
		infp = fopen (bcs, "rb");
#else
		infp = fopen (bcs, "r");
#endif

		if (!infp)
		{
			hcl_seterrnum (hcl, HCL_EIOERR);
			return -1;
		}
	}
	else
	{
		/* main stream */
#if defined(__MSDOS__) || defined(_WIN32) || defined(__OS2__)
		infp = fopen (xtn->read_path, "rb");
		if (xtn->print_path) outfp = fopen (xtn->print_path, "wb");
		else outfp = stdout;
#else
		infp = fopen (xtn->read_path, "r");
		if (xtn->print_path) outfp = fopen (xtn->print_path, "w");
		else outfp = stdout;
#endif
		if (!infp || !outfp)
		{
			if (infp) fclose (infp);
			if (outfp && outfp != stdout) fclose (outfp);
			hcl_seterrnum (hcl, HCL_EIOERR);
			return -1;
		}
	}

	bb = hcl_callocmem (hcl, HCL_SIZEOF(*bb));
	if (!bb)
	{
		if (infp) fclose (infp);
		if (outfp && outfp != stdout) fclose (outfp);
		return -1;
	}

	bb->fp = infp;
	arg->handle = bb;
	return 0;
}
  
static HCL_INLINE hcl_ooi_t close_input (hcl_t* hcl, hcl_ioinarg_t* arg)
{
	xtn_t* xtn = hcl_getxtn(hcl);
	bb_t* bb;

	bb = (bb_t*)arg->handle;
	HCL_ASSERT (bb != HCL_NULL && bb->fp != HCL_NULL);

	if (bb->fp) fclose (bb->fp);
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
	HCL_ASSERT (bb != HCL_NULL && bb->fp != HCL_NULL);
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

	bcslen = bb->len;
	ucslen = HCL_COUNTOF(arg->buf);
	x = hcl_utf8toucs (bb->buf, &bcslen, arg->buf, &ucslen);
	if (x <= -1 && ucslen <= 0)
	{
		hcl_seterrnum (hcl, HCL_EECERR);
		return -1;
	}

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
			hcl->errnum = HCL_EINTERN;
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
	HCL_ASSERT (fp != HCL_NULL);

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
		bcslen = HCL_COUNTOF(bcsbuf);
		ucslen = arg->len - donelen;
		x = hcl_ucstoutf8 (&arg->ptr[donelen], &ucslen, bcsbuf, &bcslen);
		if (x <= -1 && ucslen <= 0)
		{
			hcl_seterrnum (hcl, HCL_EECERR);
			return -1;
		}

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
			hcl->errnum = HCL_EINTERN;
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
			if (errno == EAGAIN || errno == EWOULDBLOCK)
			{
				continue;
			}
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
	
#else
	hcl_bch_t buf[256];
	hcl_oow_t ucslen, bcslen, msgidx;
	int n;
	char ts[64];
	size_t tslen;
	struct tm tm, *tmp;
	time_t now;


if (mask & HCL_LOG_GC) return; /* don't show gc logs */

/* TODO: beautify the log message.
 *       do classification based on mask. */

	now = time(NULL);
#if defined(__MSDOS__)
	tmp = localtime (&now);
#else
	tmp = localtime_r (&now, &tm);
#endif
	tslen = strftime (ts, sizeof(ts), "%Y-%m-%d %H:%M:%S %z ", tmp);
	if (tslen == 0) 
	{
		strcpy (ts, "0000-00-00 00:00:00 +0000");
		tslen = 25; 
	}
	if (write_all (1, ts, tslen) <= -1) 
	{
		char ttt[10];
		snprintf (ttt, sizeof(ttt), "ERR: %d\n", errno);
		write (1, ttt, strlen(ttt));
	}

	msgidx = 0;
	while (len > 0)
	{
		ucslen = len;
		bcslen = HCL_COUNTOF(buf);

		n = hcl_ucstoutf8 (&msg[msgidx], &ucslen, buf, &bcslen);
		if (n == 0 || n == -2)
		{
			/* n = 0: 
			 *   converted all successfully 
			 * n == -2: 
			 *    buffer not sufficient. not all got converted yet.
			 *    write what have been converted this round. */

			HCL_ASSERT (ucslen > 0); /* if this fails, the buffer size must be increased */

			/* attempt to write all converted characters */
			if (write_all (1, buf, bcslen) <= -1) break;

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
#endif
}

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


static char* syntax_error_msg[] = 
{
	"no error",
	"illegal character",
	"comment not closed",
	"string not closed",
	"invalid hashed literal",
	"wrong character literal",
	"invalid numeric literal",
	"out of integer range",

	"sudden end of input",
	"( expected",
	") expected",
	"] expected",
	"| expected",

	"string expected",
	"byte too small or too large",
	"nesting level too deep",

	"| disallowed",
	". disallowed",
	"#include error",

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

	"break outside loop"
};

static void print_synerr (hcl_t* hcl)
{
	hcl_synerr_t synerr;
	hcl_bch_t bcs[1024]; /* TODO: right buffer size */
	hcl_oow_t bcslen, ucslen;
	xtn_t* xtn;

	xtn = hcl_getxtn (hcl);
	hcl_getsynerr (hcl, &synerr);

	printf ("ERROR: ");
	if (synerr.loc.file)
	{
		bcslen = HCL_COUNTOF(bcs);
		ucslen = ~(hcl_oow_t)0;
		if (hcl_ucstoutf8 (synerr.loc.file, &ucslen, bcs, &bcslen) >= 0)
		{
			printf ("%.*s ", (int)bcslen, bcs);
		}
	}
	else
	{
		printf ("%s ", xtn->read_path);
	}

	printf ("syntax error at line %lu column %lu - %s", 
		(unsigned long int)synerr.loc.line, (unsigned long int)synerr.loc.colm,
		syntax_error_msg[synerr.num]);
	if (synerr.tgt.len > 0)
	{
		bcslen = HCL_COUNTOF(bcs);
		ucslen = synerr.tgt.len;

		if (hcl_ucstoutf8 (synerr.tgt.ptr, &ucslen, bcs, &bcslen) >= 0)
		{
			printf (" [%.*s]", (int)bcslen, bcs);
		}

	}
	printf ("\n");
}

hcl_ooch_t str_hcl[] = { 'S', 't', 'i', 'x' };
hcl_ooch_t str_my_object[] = { 'M', 'y', 'O', 'b','j','e','c','t' };
hcl_ooch_t str_main[] = { 'm', 'a', 'i', 'n' };

int main (int argc, char* argv[])
{
	hcl_t* hcl;
	xtn_t* xtn;
	hcl_vmprim_t vmprim;

#if !defined(macintosh)
	if (argc < 2)
	{
		fprintf (stderr, "Usage: %s filename ...\n", argv[0]);
		return -1;
	}
#endif

	memset (&vmprim, 0, HCL_SIZEOF(vmprim));
	vmprim.log_write = log_write;

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
	}

	if (hcl_ignite(hcl) <= -1)
	{
		printf ("cannot ignite hcl - %d\n", hcl_geterrnum(hcl));
		hcl_close (hcl);
		return -1;
	}

	if (hcl_addbuiltinprims(hcl) <= -1)
	{
		printf ("cannot add builtin primitives - %d\n", hcl_geterrnum(hcl));
		hcl_close (hcl);
		return -1;
	}

	xtn = hcl_getxtn (hcl);

#if defined(macintosh)
	i = 20;
	xtn->read_path = "test.st";
#endif

	xtn->read_path = argv[1];
	if (argc >= 2) xtn->print_path = argv[2];

	if (hcl_attachio (hcl, read_handler, print_handler) <= -1)
	{
		printf ("ERROR: cannot attache input stream - %d\n", hcl_geterrnum(hcl));
		hcl_close (hcl);
		return -1;
	}

	while (1)
	{
		hcl_oop_t obj;

		obj = hcl_read (hcl);
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
				printf ("ERROR: cannot read object - %d\n", hcl_geterrnum(hcl));
			}

			break;
		}


		if (hcl_print (hcl, obj) <= -1)
		{
			printf ("ERROR: cannot print object - %d\n", hcl_geterrnum(hcl));
		}
		else
		{
			hcl_print (hcl, HCL_CHAR_TO_OOP('\n'));
			if (hcl_compile (hcl, obj) <= -1)
			{
				if (hcl->errnum == HCL_ESYNERR)
				{
					print_synerr (hcl);
				}
				else
				{
					printf ("ERROR: cannot compile object - %d\n", hcl_geterrnum(hcl));
				}

				/* carry on? */
			}
		}
	}

hcl_decode (hcl, 0, hcl->code.bc.len);
HCL_LOG0 (hcl, HCL_LOG_MNEMONIC, "------------------------------------------\n");
g_hcl = hcl;
setup_tick ();
if (hcl_execute (hcl) <= -1)
{
	printf ("ERROR: cannot execute - %d\n", hcl_geterrnum(hcl));
}
cancel_tick();
g_hcl = HCL_NULL;


{
HCL_LOG0 (hcl, HCL_LOG_MNEMONIC, "------------------------------------------\n");
HCL_LOG2 (hcl, HCL_LOG_MNEMONIC, "BYTECODES hcl->code.bc.len = > %lu hcl->code.lit.len => %lu\n", 
	(unsigned long int)hcl->code.bc.len, (unsigned long int)hcl->code.lit.len);
hcl_decode (hcl, 0, hcl->code.bc.len);

hcl_dumpsymtab (hcl);
}
	hcl_close (hcl);

#if defined(_WIN32) && defined(_DEBUG)
getchar();
#endif
	return 0;
}
