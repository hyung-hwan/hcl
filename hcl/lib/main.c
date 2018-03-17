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
#	if defined(HCL_HAVE_CFG_H) && defined(HCL_ENABLE_LIBLTDL)
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

#	if defined(HCL_ENABLE_LIBLTDL)
#		include <ltdl.h>
#		define USE_LTDL
#		define sys_dl_error() lt_dlerror()
#		define sys_dl_open(x) lt_dlopen(x)
#		define sys_dl_openext(x) lt_dlopenext(x)
#		define sys_dl_close(x) lt_dlclose(x)
#		define sys_dl_getsym(x,n) lt_dlsym(x,n)
#	elif defined(HAVE_DLFCN_H)
#		include <dlfcn.h>
#		define USE_DLFCN
#		define sys_dl_error() dlerror()
#		define sys_dl_open(x) dlopen(x,RTLD_NOW)
#		define sys_dl_openext(x) dlopen(x,RTLD_NOW)
#		define sys_dl_close(x) dlclose(x)
#		define sys_dl_getsym(x,n) dlsym(x,n)
#	else
#		error UNSUPPORTED DYNAMIC LINKER
#	endif

#	if defined(HAVE_TIME_H)
#		include <time.h>
#	endif
#	if defined(HAVE_SYS_TIME_H)
#		include <sys/time.h>
#	endif
#	if defined(HAVE_SIGNAL_H)
#		include <signal.h>
#	endif
#	if defined(HAVE_SYS_MMAN_H)
#		include <sys/mman.h>
#	endif
	
#	include <errno.h>
#	include <unistd.h>
#	include <fcntl.h>


#endif

#if !defined(HCL_DEFAULT_PFMODPREFIX)
#	if defined(_WIN32)
#		define HCL_DEFAULT_PFMODPREFIX "hcl-"
#	elif defined(__OS2__)
#		define HCL_DEFAULT_PFMODPREFIX "hcl"
#	elif defined(__DOS__)
#		define HCL_DEFAULT_PFMODPREFIX "hcl"
#	else
#		define HCL_DEFAULT_PFMODPREFIX "libhcl-"
#	endif
#endif

#if !defined(HCL_DEFAULT_PFMODPOSTFIX)
#	if defined(_WIN32)
#		define HCL_DEFAULT_PFMODPOSTFIX ""
#	elif defined(__OS2__)
#		define HCL_DEFAULT_PFMODPOSTFIX ""
#	elif defined(__DOS__)
#		define HCL_DEFAULT_PFMODPOSTFIX ""
#	else
#		if defined(USE_DLFCN)
#			define HCL_DEFAULT_PFMODPOSTFIX ".so"
#		else
#			define HCL_DEFAULT_PFMODPOSTFIX ""
#		endif
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

	int vm_running;

	int logfd;
	unsigned int logmask;
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
		bcslen = hcl_countbcstr (arg->name);
	#endif

		fn = ((bb_t*)arg->includer->handle)->fn;

		fb = get_base_name (fn);
		parlen = fb - fn;

		bb = (bb_t*)hcl_callocmem (hcl, HCL_SIZEOF(*bb) + (HCL_SIZEOF(hcl_bch_t) * (parlen + bcslen + 1)));
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

		bb = (bb_t*)hcl_callocmem (hcl, HCL_SIZEOF(*bb) + (HCL_SIZEOF(hcl_bch_t) * (pathlen + 1)));
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

	if (!arg->includer)
	{
		xtn->reader_istty = isatty(fileno(bb->fp));
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
	hcl_copybchars (arg->buf, bb->buf, bcslen);
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
		hcl_copybchars (bcsbuf, &arg->ptr[donelen], bcslen);
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

static void* alloc_heap (hcl_t* hcl, hcl_oow_t size)
{
#if defined(HAVE_MMAP) && defined(HAVE_MUNMAP) && defined(MAP_ANONYMOUS)
	/* It's called via hcl_makeheap() when HCL creates a GC heap.
	 * The heap is large in size. I can use a different memory allocation
	 * function instead of an ordinary malloc.
	 * upon failure, it doesn't require to set error information as hcl_makeheap()
	 * set the error number to HCL_EOOMEM. */

#if !defined(MAP_HUGETLB) && (defined(__amd64__) || defined(__x86_64__))
#	define MAP_HUGETLB 0x40000
#endif

	hcl_oow_t* ptr;
	int flags;
	hcl_oow_t actual_size;

	flags = MAP_PRIVATE | MAP_ANONYMOUS;

	#if defined(MAP_HUGETLB)
	flags |= MAP_HUGETLB;
	#endif

	#if defined(MAP_UNINITIALIZED)
	flags |= MAP_UNINITIALIZED;
	#endif

	actual_size = HCL_SIZEOF(hcl_oow_t) + size;
	actual_size = HCL_ALIGN_POW2(actual_size, 2 * 1024 * 1024);
	ptr = (hcl_oow_t*)mmap(NULL, actual_size, PROT_READ | PROT_WRITE, flags, -1, 0);
	if (ptr == MAP_FAILED) 
	{
	#if defined(MAP_HUGETLB)
		flags &= ~MAP_HUGETLB;
		ptr = (hcl_oow_t*)mmap(NULL, actual_size, PROT_READ | PROT_WRITE, flags, -1, 0);
		if (ptr == MAP_FAILED) return HCL_NULL;
	#else
		return HCL_NULL;
	#endif
	}
	*ptr = actual_size;

	return (void*)(ptr + 1);

#else
	return HCL_MMGR_ALLOC(hcl->mmgr, size);
#endif
}

static void free_heap (hcl_t* hcl, void* ptr)
{
#if defined(HAVE_MMAP) && defined(HAVE_MUNMAP)
	hcl_oow_t* actual_ptr;
	actual_ptr = (hcl_oow_t*)ptr - 1;
	munmap (actual_ptr, *actual_ptr);
#else
	return HCL_MMGR_FREE(hcl->mmgr, ptr);
#endif
}

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


static int write_log (hcl_t* hcl, const hcl_bch_t* ptr, hcl_oow_t len)
{
	xtn_t* xtn;

	xtn = hcl_getxtn(hcl);

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
				n = write_all(xtn->logfd, xtn->logbuf.buf, xtn->logbuf.len);
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
				if (write_all(xtn->logfd, ptr, rcapa) <= -1) return -1;
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

static void flush_log (hcl_t* hcl)
{
	xtn_t* xtn;
	xtn = hcl_getxtn(hcl);
	if (xtn->logbuf.len > 0)
	{
		write_all (xtn->logfd, xtn->logbuf.buf, xtn->logbuf.len);
		xtn->logbuf.len = 0;
	}
}

static void log_write (hcl_t* hcl, unsigned int mask, const hcl_ooch_t* msg, hcl_oow_t len)
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
		write_log (hcl, ts, tslen);
	}

	if (xtn->logfd_istty)
	{
		if (mask & HCL_LOG_FATAL) write_log (hcl, "\x1B[1;31m", 7);
		else if (mask & HCL_LOG_ERROR) write_log (hcl, "\x1B[1;32m", 7);
		else if (mask & HCL_LOG_WARN) write_log (hcl, "\x1B[1;33m", 7);
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
			if (write_log (hcl, buf, bcslen) <= -1) break;

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
	write_log (hcl, msg, len);
#endif

	if (xtn->logfd_istty)
	{
		if (mask & (HCL_LOG_FATAL | HCL_LOG_ERROR | HCL_LOG_WARN)) write_log (hcl, "\x1B[0m", 4);
	}

	flush_log (hcl);
}


static void syserrstrb (hcl_t* hcl, int syserr, hcl_bch_t* buf, hcl_oow_t len)
{
#if defined(HAVE_STRERROR_R)
	strerror_r (syserr, buf, len);
#else
	/* this may not be thread safe */
	hcl_copybcstr (buf, len, strerror(syserr));
#endif
}

/* ========================================================================= */

static void* dl_open (hcl_t* hcl, const hcl_ooch_t* name, int flags)
{
#if defined(USE_LTDL) || defined(USE_DLFCN)
	hcl_bch_t stabuf[128], * bufptr;
	hcl_oow_t ucslen, bcslen, bufcapa;
	void* handle;

	#if defined(HCL_OOCH_IS_UCH)
	if (hcl_convootobcstr(hcl, name, &ucslen, HCL_NULL, &bufcapa) <= -1) return HCL_NULL;
	/* +1 for terminating null. but it's not needed because HCL_COUNTOF(HCL_DEFAULT_PFMODPREFIX)
	 * and HCL_COUNTOF(HCL_DEFAULT_PFMODPOSTIFX) include the terminating nulls. Never mind about
	 * the extra 2 characters. */
	#else
	bufcapa = hcl_countbcstr(name);
	#endif
	bufcapa += HCL_COUNTOF(HCL_DEFAULT_PFMODPREFIX) + HCL_COUNTOF(HCL_DEFAULT_PFMODPOSTFIX) + 1; 

	if (bufcapa <= HCL_COUNTOF(stabuf)) bufptr = stabuf;
	else
	{
		bufptr = (hcl_bch_t*)hcl_allocmem(hcl, bufcapa * HCL_SIZEOF(*bufptr));
		if (!bufptr) return HCL_NULL;
	}

	if (flags & HCL_VMPRIM_OPENDL_PFMOD)
	{
		hcl_oow_t len, i, xlen;

		/* opening a primitive function module - mostly libhcl-xxxx */
		len = hcl_copybcstr(bufptr, bufcapa, HCL_DEFAULT_PFMODPREFIX);

		bcslen = bufcapa - len;
	#if defined(HCL_OOCH_IS_UCH)
		hcl_convootobcstr(hcl, name, &ucslen, &bufptr[len], &bcslen);
	#else
		bcslen = hcl_copybcstr(&bufptr[len], bcslen, name);
	#endif

		/* length including the prefix and the name. but excluding the postfix */
		xlen  = len + bcslen; 

		for (i = len; i < xlen; i++) 
		{
			/* convert a period(.) to a dash(-) */
			if (bufptr[i] == '.') bufptr[i] = '-';
		}
 
	retry:
		hcl_copybcstr (&bufptr[xlen], bufcapa - xlen, HCL_DEFAULT_PFMODPOSTFIX);

		/* both prefix and postfix attached. for instance, libhcl-xxx */
		handle = sys_dl_openext(bufptr);
		if (!handle) 
		{
			HCL_DEBUG3 (hcl, "Failed to open(ext) DL %hs[%js] - %hs\n", bufptr, name, sys_dl_error());

			/* try without prefix and postfix */
			bufptr[xlen] = '\0';
			handle = sys_dl_openext(&bufptr[len]);
			if (!handle) 
			{
				hcl_bch_t* dash;
				const hcl_bch_t* dl_errstr;
				dl_errstr = sys_dl_error();
				HCL_DEBUG3 (hcl, "Failed to open(ext) DL %hs[%js] - %hs\n", &bufptr[len], name, dl_errstr);
				hcl_seterrbfmt (hcl, HCL_ESYSERR, "unable to open(ext) DL %js - %hs", name, dl_errstr);

				dash = hcl_rfindbchar(bufptr, hcl_countbcstr(bufptr), '-');
				if (dash) 
				{
					/* remove a segment at the back. 
					 * [NOTE] a dash contained in the original name before
					 *        period-to-dash transformation may cause extraneous/wrong
					 *        loading reattempts. */
					xlen = dash - bufptr;
					goto retry;
				}
			}
			else 
			{
				HCL_DEBUG3 (hcl, "Opened(ext) DL %hs[%js] handle %p\n", &bufptr[len], name, handle);
			}
		}
		else
		{
			HCL_DEBUG3 (hcl, "Opened(ext) DL %hs[%js] handle %p\n", bufptr, name, handle);
		}
	}
	else
	{
		/* opening a raw shared object without a prefix and/or a postfix */
	#if defined(HCL_OOCH_IS_UCH)
		bcslen = bufcapa;
		hcl_convootobcstr(hcl, name, &ucslen, bufptr, &bcslen);
	#else
		bcslen = hcl_copybcstr(bufptr, bufcapa, name);
	#endif

		if (hcl_findbchar(bufptr, bcslen, '.'))
		{
			handle = sys_dl_open(bufptr);
			if (!handle) 
			{
				const hcl_bch_t* dl_errstr;
				dl_errstr = sys_dl_error();
				HCL_DEBUG2 (hcl, "Failed to open DL %hs - %hs\n", bufptr, dl_errstr);
				hcl_seterrbfmt (hcl, HCL_ESYSERR, "unable to open DL %js - %hs", name, dl_errstr);
			}
			else HCL_DEBUG2 (hcl, "Opened DL %hs handle %p\n", bufptr, handle);
		}
		else
		{
			handle = sys_dl_openext(bufptr);
			if (!handle) 
			{
				const hcl_bch_t* dl_errstr;
				dl_errstr = sys_dl_error();
				HCL_DEBUG2 (hcl, "Failed to open(ext) DL %hs - %s\n", bufptr, dl_errstr);
				hcl_seterrbfmt (hcl, HCL_ESYSERR, "unable to open(ext) DL %js - %hs", name, dl_errstr);
			}
			else HCL_DEBUG2 (hcl, "Opened(ext) DL %hs handle %p\n", bufptr, handle);
		}
	}

	if (bufptr != stabuf) hcl_freemem (hcl, bufptr);
	return handle;

#else

/* TODO: support various platforms */
	/* TODO: implemenent this */
	HCL_DEBUG1 (hcl, "Dynamic loading not implemented - cannot open %js\n", name);
	hcl_seterrnum (hcl, HCL_ENOIMPL, "dynamic loading not implemented - cannot open %js", name);
	return HCL_NULL;
#endif
}

static void dl_close (hcl_t* hcl, void* handle)
{
#if defined(USE_LTDL) || defined(USE_DLFCN)
	HCL_DEBUG1 (hcl, "Closed DL handle %p\n", handle);
	sys_dl_close (handle);

#else
	/* TODO: implemenent this */
	HCL_DEBUG1 (hcl, "Dynamic loading not implemented - cannot close handle %p\n", handle);
#endif
}

static void* dl_getsym (hcl_t* hcl, void* handle, const hcl_ooch_t* name)
{
#if defined(USE_LTDL) || defined(USE_DLFCN)
	hcl_bch_t stabuf[64], * bufptr;
	hcl_oow_t bufcapa, ucslen, bcslen, i;
	const hcl_bch_t* symname;
	void* sym;

	#if defined(HCL_OOCH_IS_UCH)
	if (hcl_convootobcstr(hcl, name, &ucslen, HCL_NULL, &bcslen) <= -1) return HCL_NULL;
	#else
	bcslen = hcl_countbcstr (name);
	#endif

	if (bcslen >= HCL_COUNTOF(stabuf) - 2)
	{
		bufcapa = bcslen + 3;
		bufptr = (hcl_bch_t*)hcl_allocmem(hcl, bufcapa * HCL_SIZEOF(*bufptr));
		if (!bufptr) return HCL_NULL;
	}
	else
	{
		bufcapa = HCL_COUNTOF(stabuf);
		bufptr = stabuf;
	}

	bcslen = bufcapa - 1;
	#if defined(HCL_OOCH_IS_UCH)
	hcl_convootobcstr (hcl, name, &ucslen, &bufptr[1], &bcslen);
	#else
	bcslen = hcl_copybcstr(&bufptr[1], bcslen, name);
	#endif

	/* convert a period(.) to an underscore(_) */
	for (i = 1; i <= bcslen; i++) if (bufptr[i] == '.') bufptr[i] = '_';

	symname = &bufptr[1]; /* try the name as it is */

	sym = sys_dl_getsym(handle, symname);
	if (!sym)
	{
		bufptr[0] = '_';
		symname = &bufptr[0]; /* try _name */
		sym = sys_dl_getsym(handle, symname);
		if (!sym)
		{
			bufptr[bcslen + 1] = '_'; 
			bufptr[bcslen + 2] = '\0';

			symname = &bufptr[1]; /* try name_ */
			sym = sys_dl_getsym(handle, symname);

			if (!sym)
			{
				symname = &bufptr[0]; /* try _name_ */
				sym = sys_dl_getsym(handle, symname);
				if (!sym)
				{
					const hcl_bch_t* dl_errstr;
					dl_errstr = sys_dl_error();
					HCL_DEBUG3 (hcl, "Failed to get module symbol %js from handle %p - %hs\n", name, handle, dl_errstr);
					hcl_seterrbfmt (hcl, HCL_ENOENT, "unable to get module symbol %hs - %hs", symname, dl_errstr);
					
				}
			}
		}
	}

	if (sym) HCL_DEBUG3 (hcl, "Loaded module symbol %js from handle %p - %hs\n", name, handle, symname);
	if (bufptr != stabuf) hcl_freemem (hcl, bufptr);
	return sym;

#else
	/* TODO: IMPLEMENT THIS */
	HCL_DEBUG2 (hcl, "Dynamic loading not implemented - Cannot load module symbol %js from handle %p\n", name, handle);
	hcl_seterrbfmt (hcl, HCL_ENOIMPL, "dynamic loading not implemented - Cannot load module symbol %js from handle %p", name, handle);
	return HCL_NULL;
#endif
}

/* ========================================================================= */

static void vm_gettime (hcl_t* hcl, hcl_ntime_t* now)
{
#if defined(_WIN32)
	/* TODO: */
#elif defined(__OS2__)
	ULONG out;

/* TODO: handle overflow?? */
/* TODO: use DosTmrQueryTime() and DosTmrQueryFreq()? */
	DosQuerySysInfo (QSV_MS_COUNT, QSV_MS_COUNT, &out, HCL_SIZEOF(out)); /* milliseconds */
	/* it must return NO_ERROR */
	HCL_INITNTIME (now, HCL_MSEC_TO_SEC(out), HCL_MSEC_TO_NSEC(out));
#elif defined(__DOS__) && (defined(_INTELC32_) || defined(__WATCOMC__))
	clock_t c;

/* TODO: handle overflow?? */
	c = clock ();
	now->sec = c / CLOCKS_PER_SEC;
	#if (CLOCKS_PER_SEC == 100)
		now->nsec = HCL_MSEC_TO_NSEC((c % CLOCKS_PER_SEC) * 10);
	#elif (CLOCKS_PER_SEC == 1000)
		now->nsec = HCL_MSEC_TO_NSEC(c % CLOCKS_PER_SEC);
	#elif (CLOCKS_PER_SEC == 1000000L)
		now->nsec = HCL_USEC_TO_NSEC(c % CLOCKS_PER_SEC);
	#elif (CLOCKS_PER_SEC == 1000000000L)
		now->nsec = (c % CLOCKS_PER_SEC);
	#else
	#	error UNSUPPORTED CLOCKS_PER_SEC
	#endif
#elif defined(macintosh)
	UnsignedWide tick;
	hcl_uint64_t tick64;
	Microseconds (&tick);
	tick64 = *(hcl_uint64_t*)&tick;
	HCL_INITNTIME (now, HCL_USEC_TO_SEC(tick64), HCL_USEC_TO_NSEC(tick64));
#elif defined(HAVE_CLOCK_GETTIME) && defined(CLOCK_MONOTONIC)
	struct timespec ts;
	clock_gettime (CLOCK_MONOTONIC, &ts);
	HCL_INITNTIME(now, ts.tv_sec, ts.tv_nsec);
#elif defined(HAVE_CLOCK_GETTIME) && defined(CLOCK_REALTIME)
	struct timespec ts;
	clock_gettime (CLOCK_REALTIME, &ts);
	HCL_INITNTIME(now, ts.tv_sec, ts.tv_nsec);
#else
	struct timeval tv;
	gettimeofday (&tv, HCL_NULL);
	HCL_INITNTIME(now, tv.tv_sec, HCL_USEC_TO_NSEC(tv.tv_usec));
#endif
}

static void vm_sleep (hcl_t* hcl, const hcl_ntime_t* dur)
{
#if defined(_WIN32)
	xtn_t* xtn = (xtn_t*)hcl_getxtn(hcl);
	if (xtn->waitable_timer)
	{
		LARGE_INTEGER li;
		li.QuadPart = -HCL_SECNSEC_TO_NSEC(dur->sec, dur->nsec);
		if(SetWaitableTimer(timer, &li, 0, HCL_NULL, HCL_NULL, FALSE) == FALSE) goto normal_sleep;
		WaitForSingleObject(timer, INFINITE);
	}
	else
	{
	normal_sleep:
		/* fallback to normal Sleep() */
		Sleep (HCL_SECNSEC_TO_MSEC(dur->sec,dur->nsec));
	}
#elif defined(__OS2__)

	/* TODO: in gui mode, this is not a desirable method??? 
	 *       this must be made event-driven coupled with the main event loop */
	DosSleep (HCL_SECNSEC_TO_MSEC(dur->sec,dur->nsec));

#elif defined(macintosh)

	/* TODO: ... */

#elif defined(__DOS__) && (defined(_INTELC32_) || defined(__WATCOMC__))

	clock_t c;

	c = clock ();
	c += dur->sec * CLOCKS_PER_SEC;

	#if (CLOCKS_PER_SEC == 100)
		c += HCL_NSEC_TO_MSEC(dur->nsec) / 10;
	#elif (CLOCKS_PER_SEC == 1000)
		c += HCL_NSEC_TO_MSEC(dur->nsec);
	#elif (CLOCKS_PER_SEC == 1000000L)
		c += HCL_NSEC_TO_USEC(dur->nsec);
	#elif (CLOCKS_PER_SEC == 1000000000L)
		c += dur->nsec;
	#else
	#	error UNSUPPORTED CLOCKS_PER_SEC
	#endif

/* TODO: handle clock overvlow */
/* TODO: check if there is abortion request or interrupt */
	while (c > clock()) 
	{
		_halt_cpu();
	}

#else
	#if defined(USE_THREAD)
	/* the sleep callback is called only if there is no IO semaphore 
	 * waiting. so i can safely call vm_muxwait() without a muxwait callback
	 * when USE_THREAD is true */
		vm_muxwait (hcl, dur, HCL_NULL);
	#elif defined(HAVE_NANOSLEEP)
		struct timespec ts;
		ts.tv_sec = dur->sec;
		ts.tv_nsec = dur->nsec;
		nanosleep (&ts, HCL_NULL);
	#elif defined(HAVE_USLEEP)
		usleep (HCL_SECNSEC_TO_USEC(dur->sec, dur->nsec));
	#else
	#	error UNSUPPORT SLEEP
	#endif
#endif
}

/* ========================================================================= */

static int vm_startup (hcl_t* hcl)
{
#if defined(_WIN32)
	xtn_t* xtn = (xtn_t*)hcl_getxtn(hcl);
	xtn->waitable_timer = CreateWaitableTimer(HCL_NULL, TRUE, HCL_NULL);

#else

	xtn_t* xtn = (xtn_t*)hcl_getxtn(hcl);
	int pcount = 0, flag;

#if defined(USE_DEVPOLL)
	xtn->ep = open ("/dev/poll", O_RDWR);
	if (xtn->ep == -1) 
	{
		hcl_syserr_to_errnum (errno);
		HCL_DEBUG1 (hcl, "Cannot create devpoll - %hs\n", strerror(errno));
		goto oops;
	}

	flag = fcntl (xtn->ep, F_GETFD);
	if (flag >= 0) fcntl (xtn->ep, F_SETFD, flag | FD_CLOEXEC);

#elif defined(USE_EPOLL)
	#if defined(EPOLL_CLOEXEC)
	xtn->ep = epoll_create1 (EPOLL_CLOEXEC);
	#else
	xtn->ep = epoll_create (1024);
	#endif
	if (xtn->ep == -1) 
	{
		hcl_syserr_to_errnum (errno);
		HCL_DEBUG1 (hcl, "Cannot create epoll - %hs\n", strerror(errno));
		goto oops;
	}

	#if defined(EPOLL_CLOEXEC)
	/* do nothing */
	#else
	flag = fcntl (xtn->ep, F_GETFD);
	if (flag >= 0) fcntl (xtn->ep, F_SETFD, flag | FD_CLOEXEC);
	#endif

#elif defined(USE_POLL)

	MUTEX_INIT (&xtn->ev.reg.pmtx);

#elif defined(USE_SELECT)
	FD_ZERO (&xtn->ev.reg.rfds);
	FD_ZERO (&xtn->ev.reg.wfds);
	xtn->ev.reg.maxfd = -1;
	MUTEX_INIT (&xtn->ev.reg.smtx);
#endif /* USE_DEVPOLL */

#if defined(USE_THREAD)
	if (pipe(xtn->p) == -1)
	{
		hcl_syserr_to_errnum (errno);
		HCL_DEBUG1 (hcl, "Cannot create pipes - %hs\n", strerror(errno));
		goto oops;
	}
	pcount = 2;

	#if defined(O_CLOEXEC)
	flag = fcntl (xtn->p[0], F_GETFD);
	if (flag >= 0) fcntl (xtn->p[0], F_SETFD, flag | FD_CLOEXEC);
	flag = fcntl (xtn->p[1], F_GETFD);
	if (flag >= 0) fcntl (xtn->p[1], F_SETFD, flag | FD_CLOEXEC);
	#endif

	#if defined(O_NONBLOCK)
	flag = fcntl (xtn->p[0], F_GETFL);
	if (flag >= 0) fcntl (xtn->p[0], F_SETFL, flag | O_NONBLOCK);
	flag = fcntl (xtn->p[1], F_GETFL);
	if (flag >= 0) fcntl (xtn->p[1], F_SETFL, flag | O_NONBLOCK);
	#endif

	if (_add_poll_fd(hcl, xtn->p[0], XPOLLIN) <= -1) goto oops;

	pthread_mutex_init (&xtn->ev.mtx, HCL_NULL);
	pthread_cond_init (&xtn->ev.cnd, HCL_NULL);
	pthread_cond_init (&xtn->ev.cnd2, HCL_NULL);

	xtn->iothr_abort = 0;
	xtn->iothr_up = 0;
	/*pthread_create (&xtn->iothr, HCL_NULL, iothr_main, hcl);*/

#endif /* USE_THREAD */

	xtn->vm_running = 1;
	return 0;

oops:

#if defined(USE_THREAD)
	if (pcount > 0)
	{
		close (xtn->p[0]);
		close (xtn->p[1]);
	}
#endif

#if defined(USE_DEVPOLL) || defined(USE_EPOLL)
	if (xtn->ep >= 0)
	{
		close (xtn->ep);
		xtn->ep = -1;
	}
#endif

	return -1;
#endif
}

static void vm_cleanup (hcl_t* hcl)
{
#if defined(_WIN32)
	xtn_t* xtn = (xatn_t*)hcl_getxtn(hcl);
	if (xtn->waitable_timer)
	{
		CloseHandle (xtn->waitable_timer);
		xtn->waitable_timer = HCL_NULL;
	}
#else
	xtn_t* xtn = (xtn_t*)hcl_getxtn(hcl);

	xtn->vm_running = 0;

#if defined(USE_THREAD)
	if (xtn->iothr_up)
	{
		xtn->iothr_abort = 1;
		write (xtn->p[1], "Q", 1);
		pthread_cond_signal (&xtn->ev.cnd);
		pthread_join (xtn->iothr, HCL_NULL);
		xtn->iothr_up = 0;
	}
	pthread_cond_destroy (&xtn->ev.cnd);
	pthread_cond_destroy (&xtn->ev.cnd2);
	pthread_mutex_destroy (&xtn->ev.mtx);

	_del_poll_fd (hcl, xtn->p[0]);
	close (xtn->p[1]);
	close (xtn->p[0]);
#endif /* USE_THREAD */

#if defined(USE_DEVPOLL) 
	if (xtn->ep >= 0)
	{
		close (xtn->ep);
		xtn->ep = -1;
	}
	/*destroy_poll_data_space (hcl);*/
#elif defined(USE_EPOLL)
	if (xtn->ep >= 0)
	{
		close (xtn->ep);
		xtn->ep = -1;
	}
#elif defined(USE_POLL)
	if (xtn->ev.reg.ptr)
	{
		hcl_freemem (hcl, xtn->ev.reg.ptr);
		xtn->ev.reg.ptr = HCL_NULL;
		xtn->ev.reg.len = 0;
		xtn->ev.reg.capa = 0;
	}
	if (xtn->ev.buf)
	{
		hcl_freemem (hcl, xtn->ev.buf);
		xtn->ev.buf = HCL_NULL;
	}
	/*destroy_poll_data_space (hcl);*/
	MUTEX_DESTROY (&xtn->ev.reg.pmtx);
#elif defined(USE_SELECT)
	FD_ZERO (&xtn->ev.reg.rfds);
	FD_ZERO (&xtn->ev.reg.wfds);
	xtn->ev.reg.maxfd = -1;
	MUTEX_DESTROY (&xtn->ev.reg.smtx);
#endif

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
	unsigned int logmask;

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

		logmask = xtn->logmask;
		do
		{
			flt = cm + 1;

			cm = hcl_findbcharinbcstr(flt, ',');
			if (cm) *cm = '\0';

			if (hcl_compbcstr(flt, "app") == 0) logmask |= HCL_LOG_APP;
			else if (hcl_compbcstr(flt, "compiler") == 0) logmask |= HCL_LOG_COMPILER;
			else if (hcl_compbcstr(flt, "vm") == 0) logmask |= HCL_LOG_VM;
			else if (hcl_compbcstr(flt, "mnemonic") == 0) logmask |= HCL_LOG_MNEMONIC;
			else if (hcl_compbcstr(flt, "gc") == 0) logmask |= HCL_LOG_GC;
			else if (hcl_compbcstr(flt, "ic") == 0) logmask |= HCL_LOG_IC;
			else if (hcl_compbcstr(flt, "primitive") == 0) logmask |= HCL_LOG_PRIMITIVE;

			else if (hcl_compbcstr(flt, "fatal") == 0) logmask |= HCL_LOG_FATAL;
			else if (hcl_compbcstr(flt, "error") == 0) logmask |= HCL_LOG_ERROR;
			else if (hcl_compbcstr(flt, "warn") == 0) logmask |= HCL_LOG_WARN;
			else if (hcl_compbcstr(flt, "info") == 0) logmask |= HCL_LOG_INFO;
			else if (hcl_compbcstr(flt, "debug") == 0) logmask |= HCL_LOG_DEBUG;

			else if (hcl_compbcstr(flt, "fatal+") == 0) logmask |= HCL_LOG_FATAL;
			else if (hcl_compbcstr(flt, "error+") == 0) logmask |= HCL_LOG_FATAL | HCL_LOG_ERROR;
			else if (hcl_compbcstr(flt, "warn+") == 0) logmask |= HCL_LOG_FATAL | HCL_LOG_ERROR | HCL_LOG_WARN;
			else if (hcl_compbcstr(flt, "info+") == 0) logmask |= HCL_LOG_FATAL | HCL_LOG_ERROR | HCL_LOG_WARN | HCL_LOG_INFO;
			else if (hcl_compbcstr(flt, "debug+") == 0) logmask |= HCL_LOG_FATAL | HCL_LOG_ERROR | HCL_LOG_WARN | HCL_LOG_INFO | HCL_LOG_DEBUG;

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

	
	xtn->logfd = open (xstr, O_CREAT | O_WRONLY | O_APPEND , 0644);
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

#if 0
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
#endif

/* ========================================================================= */

#if defined(__MSDOS__) && defined(_INTELC32_)
typedef void(*signal_handler_t)(int);
#elif defined(macintosh)
typedef void(*signal_handler_t)(int);
#else
typedef void(*signal_handler_t)(int, siginfo_t*, void*);
#endif


#if defined(__MSDOS__) && defined(_INTELC32_)
	/* TODO: implement this */
#elif defined(macintosh)
	/* TODO: implement this */
#else
static void handle_sigint (int sig, siginfo_t* siginfo, void* ctx)
{
	if (g_hcl) hcl_abort (g_hcl);
}
#endif

static void set_signal (int sig, signal_handler_t handler)
{
#if defined(__MSDOS__) && defined(_INTELC32_)
	/* TODO: implement this */
#elif defined(macintosh)
	/* TODO: implement this */
#else
	struct sigaction sa;

	memset (&sa, 0, sizeof(sa));
	/*sa.sa_handler = handler;*/
	sa.sa_flags = SA_SIGINFO;
	sa.sa_sigaction = handler;
	sigemptyset (&sa.sa_mask);

	sigaction (sig, &sa, NULL);
#endif
}

static void set_signal_to_default (int sig)
{
#if defined(__MSDOS__) && defined(_INTELC32_)
	/* TODO: implement this */
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
	hcl_t* hcl;
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
		"l:m:",
		lopt
	};

	const char* logopt = HCL_NULL;
	hcl_oow_t memsize = MIN_MEMSIZE;
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

			case '\0':
				if (hcl_compbcstr(opt.lngopt, "large-pages") == 0)
				{
					large_pages = 1;
					break;
				}
			#if defined(HCL_BUILD_DEBUG)
				else if (hcl_compbcstr(opt.lngopt, "debug") == 0)
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
		vmprim.alloc_heap = alloc_heap;
		vmprim.free_heap = free_heap;
	}
	vmprim.log_write = log_write;
	vmprim.syserrstrb = syserrstrb;
	vmprim.dl_open = dl_open;
	vmprim.dl_close = dl_close;
	vmprim.dl_getsym = dl_getsym;
	vmprim.vm_gettime = vm_gettime;
	vmprim.vm_sleep = vm_sleep;

	hcl = hcl_open (&sys_mmgr, HCL_SIZEOF(xtn_t), memsize, &vmprim, HCL_NULL);
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
		unsigned int trait = 0;

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

#if defined(HCL_BUILD_DEBUG)
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

	if (hcl_attachio(hcl, read_handler, print_handler) <= -1)
	{
		hcl_logbfmt (hcl, HCL_LOG_STDERR, "ERROR: cannot attach input stream - [%d] %js\n", hcl_geterrnum(hcl), hcl_geterrmsg(hcl));
		hcl_close (hcl);
		return -1;
	}

	{
		hcl_ooch_t errstr[] =  { 'E', 'R', 'R', 'S', 'T', 'R' };
		xtn->sym_errstr = hcl_makesymbol(hcl, errstr, 6);
		if (!xtn->sym_errstr)
		{
			hcl_logbfmt (hcl, HCL_LOG_STDERR, "ERROR: cannot create the ERRSTR symbol - [%d] %js\n", hcl_geterrnum(hcl), hcl_geterrmsg(hcl));
			hcl_close (hcl);
			return -1;
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
				if (xtn->reader_istty) continue;
			}
			else
			{
				hcl_logbfmt (hcl, HCL_LOG_STDERR, "ERROR: cannot read object - [%d] %js\n", hcl_geterrnum(hcl), hcl_geterrmsg(hcl));
			}
			goto oops;
		}

		if (hcl_print(hcl, obj) <= -1)
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
			hcl_logbfmt (hcl, HCL_LOG_STDERR, "EXECUTION OK - EXITED WITH %O\n", retv);
		}

		/*cancel_tick();*/
		g_hcl = HCL_NULL;
		/*hcl_dumpsymtab (hcl);*/
	}
	
	set_signal_to_default (SIGINT);
	hcl_close (hcl);
	return 0;

oops:
	set_signal_to_default (SIGINT);
	hcl_close (hcl);
	return -1;
}
