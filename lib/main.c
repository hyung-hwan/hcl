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
#include "cb-impl.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <locale.h>

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

#elif defined(__DOS__)
#	include <dos.h>
#	include <time.h>
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
#if defined(_WIN32)
	HANDLE waitable_timer;
#endif
	const char* read_path; /* main source file */
	const char* print_path; 

	int vm_running;

	int logfd;
	hcl_bitmask_t logmask;
	int logfd_istty;
	
	struct 
	{
		hcl_bch_t buf[4096];
		hcl_oow_t len;
	} logbuf;

	int reader_istty;
	
	hcl_oop_t sym_errstr;
};

/* ========================================================================= */

static void* sys_alloc (hcl_mmgr_t* mmgr, hcl_oow_t size)
{
	return malloc(size);
}

static void* sys_realloc (hcl_mmgr_t* mmgr, void* ptr, hcl_oow_t size)
{
	return realloc(ptr, size);
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

static const hcl_bch_t* get_base_name (const hcl_bch_t* path)
{
	const hcl_bch_t* p, * last = HCL_NULL;

	for (p = path; *p != '\0'; p++)
	{
		if (HCL_IS_PATH_SEP(*p)) last = p;
	}

	return (last == HCL_NULL)? path: (last + 1);
}

static HCL_INLINE int open_input (hcl_t* hcl, hcl_ioinarg_t* arg)
{
	xtn_t* xtn = (xtn_t*)hcl_getxtn(hcl);
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
		bcslen = hcl_count_bcstr (arg->name);
	#endif

		fn = ((bb_t*)arg->includer->handle)->fn;

		fb = get_base_name (fn);
		parlen = fb - fn;

		bb = (bb_t*)hcl_callocmem (hcl, HCL_SIZEOF(*bb) + (HCL_SIZEOF(hcl_bch_t) * (parlen + bcslen + 1)));
		if (!bb) goto oops;

		bb->fn = (hcl_bch_t*)(bb + 1);
		hcl_copy_bchars (bb->fn, fn, parlen);
	#if defined(HCL_OOCH_IS_UCH)
		hcl_convootobcstr (hcl, arg->name, &ucslen, &bb->fn[parlen], &bcslen);
	#else
		hcl_copy_bcstr (&bb->fn[parlen], bcslen + 1, arg->name);
	#endif
	}
	else
	{
		/* main stream */
		hcl_oow_t pathlen;

		pathlen = hcl_count_bcstr (xtn->read_path);

		bb = (bb_t*)hcl_callocmem (hcl, HCL_SIZEOF(*bb) + (HCL_SIZEOF(hcl_bch_t) * (pathlen + 1)));
		if (!bb) goto oops;

		bb->fn = (hcl_bch_t*)(bb + 1);
		hcl_copy_bcstr (bb->fn, pathlen + 1, xtn->read_path);
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

	if (!arg->includer)
	{
	#if defined(HAVE_ISATTY)
		xtn->reader_istty = isatty(fileno(bb->fp));
	#endif
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

static HCL_INLINE int close_input (hcl_t* hcl, hcl_ioinarg_t* arg)
{
	/*xtn_t* xtn = (xtn_t*)hcl_getxtn(hcl);*/
	bb_t* bb;

	bb = (bb_t*)arg->handle;
	HCL_ASSERT (hcl, bb != HCL_NULL && bb->fp != HCL_NULL);

	fclose (bb->fp);
	hcl_freemem (hcl, bb);

	arg->handle = HCL_NULL;
	return 0;
}


static HCL_INLINE int read_input (hcl_t* hcl, hcl_ioinarg_t* arg)
{
	/*xtn_t* xtn = (xtn_t*)hcl_getxtn(hcl);*/
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
	hcl_copy_bchars (arg->buf, bb->buf, bcslen);
#endif

	remlen = bb->len - bcslen;
	if (remlen > 0) memmove (bb->buf, &bb->buf[bcslen], remlen);
	bb->len = remlen;

	arg->xlen = ucslen;
	return 0;
}

static int read_handler (hcl_t* hcl, hcl_iocmd_t cmd, void* arg)
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

static HCL_INLINE int open_output(hcl_t* hcl, hcl_iooutarg_t* arg)
{
	xtn_t* xtn = (xtn_t*)hcl_getxtn(hcl);
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
  
static HCL_INLINE int close_output (hcl_t* hcl, hcl_iooutarg_t* arg)
{
	/*xtn_t* xtn = (xtn_t*)hcl_getxtn(hcl);*/
	FILE* fp;

	fp = (FILE*)arg->handle;
	HCL_ASSERT (hcl, fp != HCL_NULL);

	if (fp != stdout) fclose (fp);
	arg->handle = HCL_NULL;
	return 0;
}


static HCL_INLINE int write_output (hcl_t* hcl, hcl_iooutarg_t* arg)
{
	/*xtn_t* xtn = (xtn_t*)hcl_getxtn(hcl);*/
	hcl_bch_t bcsbuf[1024];
	hcl_oow_t bcslen, ucslen, donelen;
	int x;

	donelen = 0;

	do 
	{
	#if defined(HCL_OOCH_IS_UCH)
		bcslen = HCL_COUNTOF(bcsbuf);
		ucslen = arg->len - donelen;
		x = hcl_convootobchars(hcl, &arg->ptr[donelen], &ucslen, bcsbuf, &bcslen);
		if (x <= -1 && ucslen <= 0) return -1;
	#else
		bcslen = HCL_COUNTOF(bcsbuf);
		ucslen = arg->len - donelen;
		if (ucslen > bcslen) ucslen = bcslen;
		else if (ucslen < bcslen) bcslen = ucslen;
		hcl_copy_bchars (bcsbuf, &arg->ptr[donelen], bcslen);
	#endif

		if (fwrite(bcsbuf, HCL_SIZEOF(bcsbuf[0]), bcslen, (FILE*)arg->handle) < bcslen)
		{
			hcl_seterrnum (hcl, HCL_EIOERR);
			return -1;
		}

		donelen += ucslen;
	}
	while (donelen < arg->len);

	arg->xlen = arg->len;
	return 0;
}

static int print_handler (hcl_t* hcl, hcl_iocmd_t cmd, void* arg)
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

static int write_all (int fd, const hcl_bch_t* ptr, hcl_oow_t len)
{
	while (len > 0)
	{
		hcl_ooi_t wr;

		wr = write(fd, ptr, len);

		if (wr <= -1)
		{
		#if defined(EAGAIN) && defined(EWOULDBLOCK) && (EAGAIN == EWOULDBLOCK)
			if (errno == EAGAIN) continue;
		#else
			#if defined(EAGAIN)
			if (errno == EAGAIN) continue;
			#elif defined(EWOULDBLOCK)
			if (errno == EWOULDBLOCK) continue;
			#endif
		#endif

		#if defined(EINTR)
			/* TODO: would this interfere with non-blocking nature of this VM? */
			if (errno == EINTR) continue;
		#endif
			return -1;
		}

		ptr += wr;
		len -= wr;
	}

	return 0;
}


static int write_log (hcl_t* hcl, int fd, const hcl_bch_t* ptr, hcl_oow_t len)
{
	xtn_t* xtn;

	xtn = (xtn_t*)hcl_getxtn(hcl);

	while (len > 0)
	{
		if (xtn->logbuf.len > 0)
		{
			hcl_oow_t rcapa, cplen;

			rcapa = HCL_COUNTOF(xtn->logbuf.buf) - xtn->logbuf.len;
			cplen = (len >= rcapa)? rcapa: len;

			memcpy (&xtn->logbuf.buf[xtn->logbuf.len], ptr, cplen);
			xtn->logbuf.len += cplen;
			ptr += cplen;
			len -= cplen;

			if (xtn->logbuf.len >= HCL_COUNTOF(xtn->logbuf.buf))
			{
				int n;
				n = write_all(fd, xtn->logbuf.buf, xtn->logbuf.len);
				xtn->logbuf.len = 0;
				if (n <= -1) return -1;
			}
		}
		else
		{
			hcl_oow_t rcapa;

			rcapa = HCL_COUNTOF(xtn->logbuf.buf);
			if (len >= rcapa)
			{
				if (write_all(fd, ptr, rcapa) <= -1) return -1;
				ptr += rcapa;
				len -= rcapa;
			}
			else
			{
				memcpy (xtn->logbuf.buf, ptr, len);
				xtn->logbuf.len += len;
				ptr += len;
				len -= len;
				
			}
		}
	}

	return 0;
}

static void flush_log (hcl_t* hcl, int fd)
{
	xtn_t* xtn;
	xtn = (xtn_t*)hcl_getxtn(hcl);
	if (xtn->logbuf.len > 0)
	{
		write_all (fd, xtn->logbuf.buf, xtn->logbuf.len);
		xtn->logbuf.len = 0;
	}
}

static void log_write (hcl_t* hcl, hcl_bitmask_t mask, const hcl_ooch_t* msg, hcl_oow_t len)
{
	hcl_bch_t buf[256];
	hcl_oow_t ucslen, bcslen, msgidx;
	int n;

	xtn_t* xtn = (xtn_t*)hcl_getxtn(hcl);
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
	#if defined(_WIN32)
		tmp = localtime(&now);
		tslen = strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S %z ", tmp);
		if (tslen == 0) 
		{
			tslen = strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S ", tmp);
			if (tslen == 0)
			{
				strcpy (ts, "0000-00-00 00:00:00 +0000 ");
				tslen = 26; 
			}
		}
	#elif defined(__DOS__)
		tmp = localtime(&now);
		tslen = strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S ", tmp); /* no timezone info */
		if (tslen == 0) 
		{
			strcpy (ts, "0000-00-00 00:00:00 ");
			tslen = 20; 
		}
	#else
		#if defined(__OS2__)
		tmp = _localtime(&now, &tm);
		#elif defined(HAVE_LOCALTIME_R)
		tmp = localtime_r(&now, &tm);
		#else
		tmp = localtime(&now);
		#endif
		#if defined(HAVE_STRFTIME_SMALL_Z)
		tslen = strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S %z ", tmp);
		#else
		tslen = strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S %Z ", tmp); 
		#endif
		if (tslen == 0) 
		{
			strcpy (ts, "0000-00-00 00:00:00 +0000 ");
			tslen = 26; 
		}
	#endif
		write_log (hcl, logfd, ts, tslen);
	}

	if (logfd == xtn->logfd && xtn->logfd_istty)
	{
		if (mask & HCL_LOG_FATAL) write_log (hcl, logfd, "\x1B[1;31m", 7);
		else if (mask & HCL_LOG_ERROR) write_log (hcl, logfd, "\x1B[1;32m", 7);
		else if (mask & HCL_LOG_WARN) write_log (hcl, logfd, "\x1B[1;33m", 7);
	}

#if defined(HCL_OOCH_IS_UCH)
	msgidx = 0;
	while (len > 0)
	{
		ucslen = len;
		bcslen = HCL_COUNTOF(buf);

		n = hcl_convootobchars(hcl, &msg[msgidx], &ucslen, buf, &bcslen);
		if (n == 0 || n == -2)
		{
			/* n = 0: 
			 *   converted all successfully 
			 * n == -2: 
			 *    buffer not sufficient. not all got converted yet.
			 *    write what have been converted this round. */

			HCL_ASSERT (hcl, ucslen > 0); /* if this fails, the buffer size must be increased */

			/* attempt to write all converted characters */
			if (write_log(hcl, logfd, buf, bcslen) <= -1) break;

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
	write_log (hcl, logfd, msg, len);
#endif

	if (logfd == xtn->logfd && xtn->logfd_istty)
	{
		if (mask & (HCL_LOG_FATAL | HCL_LOG_ERROR | HCL_LOG_WARN)) write_log (hcl, logfd, "\x1B[0m", 4);
	}

	flush_log (hcl, logfd);
}

/* ========================================================================= */

static int vm_startup (hcl_t* hcl)
{
	xtn_t* xtn = (xtn_t*)hcl_getxtn(hcl);

#if defined(_WIN32)
	xtn->waitable_timer = CreateWaitableTimer(HCL_NULL, TRUE, HCL_NULL);
#endif

	xtn->vm_running = 1;
	return 0;
}

static void vm_cleanup (hcl_t* hcl)
{
	xtn_t* xtn = (xtn_t*)hcl_getxtn(hcl);

	xtn->vm_running = 0;
	
#if defined(_WIN32)
	if (xtn->waitable_timer)
	{
		CloseHandle (xtn->waitable_timer);
		xtn->waitable_timer = HCL_NULL;
	}
#endif
}

/*
static void vm_checkbc (hcl_t* hcl, hcl_oob_t bcode)
{
}
*/

static void gc_hcl (hcl_t* hcl)
{
	xtn_t* xtn = (xtn_t*)hcl_getxtn(hcl);
	if (xtn->sym_errstr) xtn->sym_errstr = hcl_moveoop(hcl, xtn->sym_errstr);
}

static void fini_hcl (hcl_t* hcl)
{
	xtn_t* xtn = (xtn_t*)hcl_getxtn(hcl);
	if (xtn->logfd >= 0)
	{
		close (xtn->logfd);
		xtn->logfd = -1;
		xtn->logfd_istty = 0;
	}
}

/* ========================================================================= */

static int handle_logopt (hcl_t* hcl, const hcl_bch_t* str)
{
	xtn_t* xtn = (xtn_t*)hcl_getxtn (hcl);
	hcl_bch_t* xstr = (hcl_bch_t*)str;
	hcl_bch_t* cm, * flt;
	hcl_bitmask_t logmask;

	cm = hcl_find_bchar_in_bcstr (xstr, ',');
	if (cm) 
	{
		/* i duplicate this string for open() below as open() doesn't 
		 * accept a length-bounded string */
		xstr = hcl_dupbchars (hcl, str, hcl_count_bcstr(str));
		if (!xstr) 
		{
			fprintf (stderr, "ERROR: out of memory in duplicating %s\n", str);
			return -1;
		}

		cm = hcl_find_bchar_in_bcstr(xstr, ',');
		*cm = '\0';

		logmask = xtn->logmask;
		do
		{
			flt = cm + 1;

			cm = hcl_find_bchar_in_bcstr(flt, ',');
			if (cm) *cm = '\0';

			if (hcl_comp_bcstr(flt, "app") == 0) logmask |= HCL_LOG_APP;
			else if (hcl_comp_bcstr(flt, "compiler") == 0) logmask |= HCL_LOG_COMPILER;
			else if (hcl_comp_bcstr(flt, "vm") == 0) logmask |= HCL_LOG_VM;
			else if (hcl_comp_bcstr(flt, "mnemonic") == 0) logmask |= HCL_LOG_MNEMONIC;
			else if (hcl_comp_bcstr(flt, "gc") == 0) logmask |= HCL_LOG_GC;
			else if (hcl_comp_bcstr(flt, "ic") == 0) logmask |= HCL_LOG_IC;
			else if (hcl_comp_bcstr(flt, "primitive") == 0) logmask |= HCL_LOG_PRIMITIVE;

			else if (hcl_comp_bcstr(flt, "fatal") == 0) logmask |= HCL_LOG_FATAL;
			else if (hcl_comp_bcstr(flt, "error") == 0) logmask |= HCL_LOG_ERROR;
			else if (hcl_comp_bcstr(flt, "warn") == 0) logmask |= HCL_LOG_WARN;
			else if (hcl_comp_bcstr(flt, "info") == 0) logmask |= HCL_LOG_INFO;
			else if (hcl_comp_bcstr(flt, "debug") == 0) logmask |= HCL_LOG_DEBUG;

			else if (hcl_comp_bcstr(flt, "fatal+") == 0) logmask |= HCL_LOG_FATAL;
			else if (hcl_comp_bcstr(flt, "error+") == 0) logmask |= HCL_LOG_FATAL | HCL_LOG_ERROR;
			else if (hcl_comp_bcstr(flt, "warn+") == 0) logmask |= HCL_LOG_FATAL | HCL_LOG_ERROR | HCL_LOG_WARN;
			else if (hcl_comp_bcstr(flt, "info+") == 0) logmask |= HCL_LOG_FATAL | HCL_LOG_ERROR | HCL_LOG_WARN | HCL_LOG_INFO;
			else if (hcl_comp_bcstr(flt, "debug+") == 0) logmask |= HCL_LOG_FATAL | HCL_LOG_ERROR | HCL_LOG_WARN | HCL_LOG_INFO | HCL_LOG_DEBUG;

			else
			{
				fprintf (stderr, "ERROR: unknown log option value - %s\n", flt);
				if (str != xstr) hcl_freemem (hcl, xstr);
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
	}

#if defined(_WIN32)
	xtn->logfd = _open(xstr, _O_CREAT | _O_WRONLY | _O_APPEND | _O_BINARY , 0644);
#else
	xtn->logfd = open(xstr, O_CREAT | O_WRONLY | O_APPEND , 0644);
#endif
	if (xtn->logfd == -1)
	{
		fprintf (stderr, "ERROR: cannot open a log file %s\n", xstr);
		if (str != xstr) hcl_freemem (hcl, xstr);
		return -1;
	}

	xtn->logmask = logmask;
#if defined(HAVE_ISATTY)
	xtn->logfd_istty = isatty(xtn->logfd);
#endif

	if (str != xstr) hcl_freemem (hcl, xstr);
	return 0;
}

#if defined(HCL_BUILD_DEBUG)
static int handle_dbgopt (hcl_t* hcl, const hcl_bch_t* str)
{
	xtn_t* xtn = (xtn_t*)hcl_getxtn(hcl);
	const hcl_bch_t* cm, * flt;
	hcl_oow_t len;
	hcl_bitmask_t trait, dbgopt = 0;

	cm = str - 1;
	do
	{
		flt = cm + 1;

		cm = hcl_find_bchar_in_bcstr(flt, ',');
		len = cm? (cm - flt): hcl_count_bcstr(flt);
		if (hcl_comp_bchars_bcstr (flt, len, "gc") == 0)  dbgopt |= HCL_DEBUG_GC;
		else if (hcl_comp_bchars_bcstr (flt, len, "bigint") == 0)  dbgopt |= HCL_DEBUG_BIGINT;
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


/* ========================================================================= */

#if defined(_WIN32) || defined(__MSDOS__) || defined(__OS2__) || defined(macintosh)
typedef void(*signal_handler_t)(int);
#elif defined(macintosh)
typedef void(*signal_handler_t)(int); /* TODO: */
#elif defined(SA_SIGINFO)
typedef void(*signal_handler_t)(int, siginfo_t*, void*);
#else
typedef void(*signal_handler_t)(int);
#endif


#if defined(_WIN32) || defined(__MSDOS__) || defined(__OS2__) 
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
#if defined(_WIN32) || defined(__MSDOS__) || defined(__OS2__) 
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
#if defined(_WIN32) || defined(__MSDOS__) || defined(__OS2__) 
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

static void print_synerr (hcl_t* hcl)
{
	hcl_synerr_t synerr;
	xtn_t* xtn;

	xtn = (xtn_t*)hcl_getxtn (hcl);
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
	hcl_t* hcl = HCL_NULL;
	xtn_t* xtn;
	hcl_vmprim_t vmprim;
	hcl_cb_t hclcb;

	hcl_bci_t c;
	static hcl_bopt_lng_t lopt[] =
	{
		{ ":log",         'l' },
		{ ":memsize",     'm' },
		{ "large-pages",  '\0' },
#if defined(HCL_BUILD_DEBUG)
		{ ":debug",       '\0' }, /* NOTE: there is no short option for --debug */
#endif
		{ HCL_NULL,       '\0' }
	};
	static hcl_bopt_t opt =
	{
		"l:m:v",
		lopt
	};

	const char* logopt = HCL_NULL;
	hcl_oow_t memsize = MIN_MEMSIZE;
	int verbose = 0;
	int large_pages = 0;

#if defined(HCL_BUILD_DEBUG)
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

			case 'v':
				verbose = 1;
				break;

			case '\0':
				if (hcl_comp_bcstr(opt.lngopt, "large-pages") == 0)
				{
					large_pages = 1;
					break;
				}
			#if defined(HCL_BUILD_DEBUG)
				else if (hcl_comp_bcstr(opt.lngopt, "debug") == 0)
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
	if (large_pages)
	{
		vmprim.alloc_heap = hcl_vmprim_alloc_heap;
		vmprim.free_heap = hcl_vmprim_free_heap;
	}
	vmprim.log_write = log_write;
	vmprim.syserrstrb = hcl_vmprim_syserrstrb;
	vmprim.assertfail = hcl_vmprim_assertfail;
	vmprim.dl_startup = hcl_vmprim_dl_startup;
	vmprim.dl_cleanup = hcl_vmprim_dl_cleanup;
	vmprim.dl_open = hcl_vmprim_dl_open;
	vmprim.dl_close = hcl_vmprim_dl_close;
	vmprim.dl_getsym = hcl_vmprim_dl_getsym;
	vmprim.gettime = hcl_vmprim_gettime;
	vmprim.sleep = hcl_vmprim_sleep;

	hcl = hcl_open(&sys_mmgr, HCL_SIZEOF(xtn_t), memsize, &vmprim, HCL_NULL);
	if (!hcl)
	{
		printf ("ERROR: cannot open hcl\n");
		goto oops;
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
		hcl_bitmask_t trait = 0;

		/*trait |= HCL_NOGC;*/
		trait |= HCL_AWAIT_PROCS;
		hcl_setoption (hcl, HCL_TRAIT, &trait);

		/* disable GC logs */
		/*trait = ~HCL_LOG_GC;
		hcl_setoption (hcl, HCL_LOG_MASK, &trait);*/
	}

	xtn = (xtn_t*)hcl_getxtn (hcl);
	xtn->logfd = -1;
	xtn->logfd_istty = 0;

	memset (&hclcb, 0, HCL_SIZEOF(hclcb));
	hclcb.fini = fini_hcl;
	hclcb.gc = gc_hcl;
	hclcb.vm_startup =  vm_startup;
	hclcb.vm_cleanup = vm_cleanup;
	/*hclcb.vm_checkbc = vm_checkbc;*/
	hcl_regcb (hcl, &hclcb);

	if (logopt)
	{
		if (handle_logopt (hcl, logopt) <= -1) goto oops;
	}
	else
	{
		/* default logging mask when no logging option is set */
		xtn->logmask = HCL_LOG_ALL_TYPES | HCL_LOG_ERROR | HCL_LOG_FATAL;
	}

#if defined(HCL_BUILD_DEBUG)
	if (dbgopt)
	{
		if (handle_dbgopt (hcl, dbgopt) <= -1) goto oops;
	}
#endif

	if (hcl_ignite(hcl) <= -1)
	{
		hcl_logbfmt (hcl, HCL_LOG_STDERR, "cannot ignite hcl - [%d] %js\n", hcl_geterrnum(hcl), hcl_geterrmsg(hcl));
		goto oops;
	}

	if (hcl_addbuiltinprims(hcl) <= -1)
	{
		hcl_logbfmt (hcl, HCL_LOG_STDERR, "cannot add builtin primitives - [%d] %js\n", hcl_geterrnum(hcl), hcl_geterrmsg(hcl));
		goto oops;
	}

	xtn->read_path = argv[opt.ind++];
	if (opt.ind < argc) xtn->print_path = argv[opt.ind++];

	if (hcl_attachio(hcl, read_handler, print_handler) <= -1)
	{
		hcl_logbfmt (hcl, HCL_LOG_STDERR, "ERROR: cannot attach input stream - [%d] %js\n", hcl_geterrnum(hcl), hcl_geterrmsg(hcl));
		goto oops;
	}

	{
		hcl_ooch_t errstr[] =  { 'E', 'R', 'R', 'S', 'T', 'R' };
		xtn->sym_errstr = hcl_makesymbol(hcl, errstr, 6);
		if (!xtn->sym_errstr)
		{
			hcl_logbfmt (hcl, HCL_LOG_STDERR, "ERROR: cannot create the ERRSTR symbol - [%d] %js\n", hcl_geterrnum(hcl), hcl_geterrmsg(hcl));
			goto oops;
		}
		HCL_OBJ_SET_FLAGS_KERNEL (xtn->sym_errstr, 1);
	}

	/* -- from this point onward, any failure leads to jumping to the oops label 
	 * -- instead of returning -1 immediately. --*/
	set_signal (SIGINT, handle_sigint);

	while (1)
	{
		hcl_oop_t obj;
/*
static int count = 0;
if (count %5 == 0) hcl_reset (hcl);
count++;
*/
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
				if (xtn->reader_istty && hcl_getsynerrnum(hcl) != HCL_SYNERR_EOF) 
				{
					/* TODO: drain remaining data in the reader including the actual inputstream and buffered data in hcl */	
					continue;
				}
			}
			else
			{
				hcl_logbfmt (hcl, HCL_LOG_STDERR, "ERROR: cannot read object - [%d] %js\n", hcl_geterrnum(hcl), hcl_geterrmsg(hcl));
			}
			goto oops;
		}

		if (verbose && hcl_print(hcl, obj) <= -1)
		{
			hcl_logbfmt (hcl, HCL_LOG_STDERR, "ERROR: cannot print object - [%d] %js\n", hcl_geterrnum(hcl), hcl_geterrmsg(hcl));
		}
		else
		{
			hcl_oow_t code_offset;

			code_offset = hcl_getbclen(hcl);

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

				if (!xtn->reader_istty) goto oops;
			}
			else if (xtn->reader_istty)
			{
				hcl_oop_t retv;

				hcl_decode (hcl, code_offset, hcl_getbclen(hcl));
				HCL_LOG0 (hcl, HCL_LOG_MNEMONIC, "------------------------------------------\n");
				g_hcl = hcl;
				//setup_tick ();

				retv = hcl_executefromip(hcl, code_offset);
				if (!retv)
				{
					hcl_logbfmt (hcl, HCL_LOG_STDERR, "ERROR: cannot execute - [%d] %js\n", hcl_geterrnum(hcl), hcl_geterrmsg(hcl));
				}
				else
				{
					hcl_logbfmt (hcl, HCL_LOG_STDERR, "OK: EXITED WITH %O\n", retv);
				
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
				//cancel_tick();
				g_hcl = HCL_NULL;
			}
		}
	}

	if (!xtn->reader_istty)
	{
		hcl_oop_t retv;

		hcl_decode (hcl, 0, hcl_getbclen(hcl));
		HCL_LOG2 (hcl, HCL_LOG_MNEMONIC, "BYTECODES bclen = > %zu lflen => %zu\n", hcl_getbclen(hcl), hcl_getlflen(hcl));
		g_hcl = hcl;
		/*setup_tick ();*/

		retv = hcl_execute(hcl);
		if (!retv)
		{
			hcl_logbfmt (hcl, HCL_LOG_STDERR, "ERROR: cannot execute - [%d] %js\n", hcl_geterrnum(hcl), hcl_geterrmsg(hcl));
		}
		else
		{
			if (verbose) hcl_logbfmt (hcl, HCL_LOG_STDERR, "EXECUTION OK - EXITED WITH %O\n", retv);
		}

		/*cancel_tick();*/
		g_hcl = HCL_NULL;
		/*hcl_dumpsymtab (hcl);*/
	}
	
	set_signal_to_default (SIGINT);
	hcl_close (hcl);

	return 0;

oops:
	set_signal_to_default (SIGINT); /* harmless to call multiple times without set_signal() */
	if (hcl) hcl_close (hcl);
	return -1;
}
