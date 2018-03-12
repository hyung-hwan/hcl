

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

#	include <sys/types.h>
#	include <sys/socket.h>
#	include <netinet/in.h>
#	include <arpa/inet.h>
#	include <pthread.h>
#	include <sys/uio.h>
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

typedef union sck_addr_t sck_addr_t;
union sck_addr_t
{
	struct sockaddr_in in4;
	struct sockaddr_in6 in6;
};

typedef struct bb_t bb_t;
struct bb_t
{
	char buf[1024];
	hcl_oow_t pos;
	hcl_oow_t len;
	int fd;
	hcl_bch_t* fn;
};

typedef struct proto_t proto_t;
typedef struct client_t client_t;
typedef struct server_t server_t;

typedef struct xtn_t xtn_t;
struct xtn_t
{
	proto_t* proto;
	int vm_running;

	int logfd;
	int logmask;
	int logfd_istty;
};

enum proto_token_type_t
{
	PROTO_TOKEN_EOF,
	PROTO_TOKEN_NL,

	PROTO_TOKEN_BEGIN,
	PROTO_TOKEN_END,
	PROTO_TOKEN_SCRIPT,
	PROTO_TOKEN_EXIT,

	PROTO_TOKEN_OK,
	PROTO_TOKEN_ERROR,
	PROTO_TOKEN_LENGTH,
	PROTO_TOKEN_ENCODING,

	PROTO_TOKEN_IDENT
};

typedef enum proto_token_type_t proto_token_type_t;

typedef struct proto_token_t proto_token_t;
struct proto_token_t
{
	proto_token_type_t type;
	hcl_ooch_t* ptr;
	hcl_oow_t len;
	hcl_oow_t capa;
	hcl_ioloc_t loc;
};

enum proto_req_state_t
{
	PROTO_REQ_IN_TOP_LEVEL,
	PROTO_REQ_IN_BLOCK_LEVEL
};

#define PROTO_REPLY_BUF_SIZE 1300

enum proto_reply_type_t
{
	PROTO_REPLY_SIMPLE = 0,
	PROTO_REPLY_CHUNKED
};
typedef enum proto_reply_type_t proto_reply_type_t;

struct proto_t
{
	client_t* client;

	hcl_t* hcl;
	hcl_iolxc_t* lxc;
	proto_token_t tok;

	struct
	{
		int state;
	} req;

	struct
	{
		proto_reply_type_t type;
		hcl_oow_t nchunks;
		hcl_bch_t buf[PROTO_REPLY_BUF_SIZE];
		hcl_oow_t len;

		int inject_nl; /* inject nl before the chunk length */
	} reply;
};

enum client_state_t
{
	CLIENT_STATE_DEAD  = 0,
	CLIENT_STATE_ALIVE = 1
};
typedef enum client_state_t client_state_t;

struct client_t
{
	pthread_t thr;
	int sck;
	/* TODO: peer address */

	time_t time_created;
	client_state_t state;

	proto_t* proto;

	server_t* server;
	client_t* prev_client;
	client_t* next_client;
};

struct server_t
{
	int stopreq;

	struct
	{
		hcl_oow_t memsize; /* hcl heap memory size */
		unsigned int dbgopt;
		int large_pages;
		const char* logopt;
	} cfg;

	struct
	{
		client_t* head;
		client_t* tail;
	} client_list[2];

	pthread_mutex_t client_mutex;
	pthread_mutex_t log_mutex;
};


int proto_feed_reply (proto_t* proto, const hcl_ooch_t* ptr, hcl_oow_t len, int escape);

/* ========================================================================= */

#define MB_1 (256UL*1024*1024)

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

		bb->fd = open(bb->fn, O_RDONLY, 0);
	}
	else
	{
		/* main stream */
		hcl_oow_t pathlen = 0;
		bb = (bb_t*)hcl_callocmem (hcl, HCL_SIZEOF(*bb) + (HCL_SIZEOF(hcl_bch_t) * (pathlen + 1)));
		if (!bb) goto oops;

		/*bb->fn = (hcl_bch_t*)(bb + 1);
		hcl_copybcstr (bb->fn, pathlen + 1, "");*/

		bb->fd = xtn->proto->client->sck;
	}
	if (bb->fd <= -1)
	{
		hcl_seterrnum (hcl, HCL_EIOERR);
		goto oops;
	}

	arg->handle = bb;
	return 0;

oops:
	if (bb) 
	{
		if (bb->fd >= 0 && bb->fd != xtn->proto->client->sck) close (bb->fd);
		hcl_freemem (hcl, bb);
	}
	return -1;
}

static HCL_INLINE int close_input (hcl_t* hcl, hcl_ioinarg_t* arg)
{
	xtn_t* xtn = (xtn_t*)hcl_getxtn(hcl);
	bb_t* bb;

	bb = (bb_t*)arg->handle;
	HCL_ASSERT (hcl, bb != HCL_NULL && bb->fd >= 0);

	if (bb->fd != xtn->proto->client->sck) close (bb->fd);
	hcl_freemem (hcl, bb);

	arg->handle = HCL_NULL;
	return 0;
}


static HCL_INLINE int read_input (hcl_t* hcl, hcl_ioinarg_t* arg)
{
	xtn_t* xtn = (xtn_t*)hcl_getxtn(hcl);
	bb_t* bb;
	hcl_oow_t bcslen, ucslen, remlen;
	int x;

	bb = (bb_t*)arg->handle;
	HCL_ASSERT (hcl, bb != HCL_NULL && bb->fd >= 0);

	if (bb->fd == xtn->proto->client->sck)
	{
		ssize_t x;

/* TOOD: timeout, etc */
		x = recv (bb->fd, &bb->buf[bb->len], HCL_COUNTOF(bb->buf) - bb->len, 0);
		if (x <= -1)
		{
			hcl_seterrnum (hcl, HCL_EIOERR);
			return -1;
		}

		bb->len += x;
	}
	else
	{
		ssize_t x;
		x = read(bb->fd, &bb->buf[bb->len], HCL_COUNTOF(bb->buf) - bb->len);
		if (x <= -1)
		{
			hcl_seterrnum (hcl, HCL_EIOERR);
			return -1;
		}

		bb->len += x;
	}

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

static int print_handler (hcl_t* hcl, hcl_iocmd_t cmd, void* arg)
{
	switch (cmd)
	{
		case HCL_IO_OPEN:
			return 0;

		case HCL_IO_CLOSE:
			return 0;

		case HCL_IO_WRITE:
		{
			xtn_t* xtn = (xtn_t*)hcl_getxtn(hcl);
			hcl_iooutarg_t* outarg = (hcl_iooutarg_t*)arg;

			if (proto_feed_reply(xtn->proto, outarg->ptr, outarg->len, 0) <= -1) 
			{
				/* TODO: change error code and message. propagage the errormessage from proto */
				hcl_seterrbfmt (hcl, HCL_ESYSERR, "failed to write message via proto");
				return -1;
			}
			outarg->xlen = outarg->len;
			return 0;
		}

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

static int write_all (int fd, const char* ptr, hcl_oow_t len)
{
	while (len > 0)
	{
		hcl_ooi_t wr;

		wr = write (fd, ptr, len);

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

static HCL_INLINE void __log_write (hcl_t* hcl, int mask, const hcl_ooch_t* msg, hcl_oow_t len)
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

		tmp = localtime_r (&now, &tm);
		#if defined(HAVE_STRFTIME_SMALL_Z)
		tslen = strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S %z ", tmp);
		#else
		tslen = strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S %Z ", tmp); 
		#endif
		if (tslen == 0) 
		{
			strcpy (ts, "0000-00-00 00:00:00 +0000");
			tslen = 25; 
		}

/* TODO: less write system calls by having a buffer */
		write_all (logfd, ts, tslen);

		tslen = snprintf (ts, sizeof(ts), "[%d] ", xtn->proto->client->sck);
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
}

static void log_write (hcl_t* hcl, int mask, const hcl_ooch_t* msg, hcl_oow_t len)
{
	xtn_t* xtn = (xtn_t*)hcl_getxtn(hcl);
	pthread_mutex_lock (&xtn->proto->client->server->log_mutex);
	__log_write (hcl, mask, msg, len);
	pthread_mutex_unlock (&xtn->proto->client->server->log_mutex);
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
	xtn_t* xtn = (xtn_t*)hcl_getxtn(hcl);
	xtn->vm_running = 1;
	return 0;
}

static void vm_cleanup (hcl_t* hcl)
{
	xtn_t* xtn = (xtn_t*)hcl_getxtn(hcl);
	xtn->vm_running = 0;
}

static void vm_checkbc (hcl_t* hcl, hcl_oob_t bcode)
{
	xtn_t* xtn = (xtn_t*)hcl_getxtn(hcl);

	/* TODO: check how to this vm has been running. too long? abort it */

	/* TODO: check if the client connection is ok? if not, abort it */

}

/*
static void gc_hcl (hcl_t* hcl)
{
}
*/

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

#if defined(HCL_BUILD_DEBUG)
static int parse_dbgopt (const char* str, unsigned int* dbgoptp)
{
	const hcl_bch_t* cm, * flt;
	hcl_oow_t len;
	unsigned int dbgopt = 0;

	cm = str - 1;
	do
	{
		flt = cm + 1;

		cm = strchr(flt, ',');
		len = cm? (cm - flt): strlen(flt);
		if (strncasecmp(flt, "gc", len) == 0)  dbgopt |= HCL_DEBUG_GC;
		else if (strncasecmp (flt, "bigint", len) == 0)  dbgopt |= HCL_DEBUG_BIGINT;
		else
		{
			fprintf (stderr, "ERROR: unknown debug option value - %.*s\n", (int)len, flt);
			return -1;
		}
	}
	while (cm);

	*dbgoptp = dbgopt;
	return 0;
}
#endif
/* ========================================================================= */

static server_t* g_server = HCL_NULL;

/* ========================================================================= */

typedef void (*signal_handler_t) (int, siginfo_t*, void*);

static void handle_sigint (int sig, siginfo_t* siginfo, void* ctx)
{
	if (g_server) g_server->stopreq = 1;
}

static void set_signal (int sig, signal_handler_t handler)
{
	struct sigaction sa;

	memset (&sa, 0, sizeof(sa));
	/*sa.sa_handler = handler;*/
	sa.sa_flags = SA_SIGINFO;
	sa.sa_sigaction = handler;
	sigemptyset (&sa.sa_mask);

	sigaction (sig, &sa, NULL);
}

static void set_signal_to_default (int sig)
{
	struct sigaction sa; 

	memset (&sa, 0, sizeof(sa));
	sa.sa_handler = SIG_DFL;
	sa.sa_flags = 0;
	sigemptyset (&sa.sa_mask);

	sigaction (sig, &sa, NULL);
}

/* ========================================================================= */

proto_t* proto_open (hcl_oow_t xtnsize, client_t* client)
{
	proto_t* proto;
	hcl_vmprim_t vmprim;
	hcl_cb_t hclcb;
	xtn_t* xtn;
	unsigned int trait;

	memset (&vmprim, 0, HCL_SIZEOF(vmprim));
	if (client->server->cfg.large_pages)
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

	proto = (proto_t*)malloc(sizeof(*proto));
	if (!proto) return NULL;

	memset (proto, 0, sizeof(*proto));
	proto->client = client;

	proto->hcl = hcl_open(&sys_mmgr, HCL_SIZEOF(xtn_t), client->server->cfg.memsize, &vmprim, HCL_NULL);
	if (!proto->hcl)  goto oops;

	xtn = (xtn_t*)hcl_getxtn(proto->hcl);
	xtn->logfd = -1;
	xtn->logfd_istty = 0;
	xtn->proto = proto;

	hcl_getoption (proto->hcl, HCL_TRAIT, &trait);
	trait |= proto->client->server->cfg.dbgopt;
	hcl_setoption (proto->hcl, HCL_TRAIT, &trait);

	if (proto->client->server->cfg.logopt &&
	    handle_logopt(proto->hcl, proto->client->server->cfg.logopt) <= -1) goto oops;

	memset (&hclcb, 0, HCL_SIZEOF(hclcb));
	hclcb.fini = fini_hcl;
	/*hclcb.gc = gc_hcl;*/
	hclcb.vm_startup =  vm_startup;
	hclcb.vm_cleanup = vm_cleanup;
	hclcb.vm_checkbc = vm_checkbc;
	hcl_regcb (proto->hcl, &hclcb);

	if (hcl_ignite(proto->hcl) <= -1) goto oops;
	if (hcl_addbuiltinprims(proto->hcl) <= -1) goto oops;

	if (hcl_attachio(proto->hcl, read_handler, print_handler) <= -1) goto oops;
	return proto;

oops:
	if (proto)
	{
		if (proto->hcl) hcl_close (proto->hcl);
		free (proto);
	}
	return NULL;
}

void proto_close (proto_t* proto)
{
	if (proto->tok.ptr) free (proto->tok.ptr);
	hcl_close (proto->hcl);
	free (proto);
}

static int write_reply_chunk (proto_t* proto)
{
	struct msghdr msg;
	struct iovec iov[3];
	hcl_bch_t cl[16]; /* ensure that this is large enough for the chunk length string */
	int index = 0, count = 0;

	if (proto->reply.type == PROTO_REPLY_CHUNKED)
	{
		if (proto->reply.nchunks <= 0)
		{
			/* this is the first chunk */
			iov[count].iov_base = ".OK\n.ENCODING chunked\n";
			iov[count++].iov_len = 22;
		}

		iov[count].iov_base = cl,
		iov[count++].iov_len = snprintf(cl, sizeof(cl), "%s%zu:", ((proto->reply.inject_nl && proto->reply.nchunks > 0)? "\n": ""), proto->reply.len); 
	}
	iov[count].iov_base = proto->reply.buf;
	iov[count++].iov_len = proto->reply.len;

	while (1)
	{
		ssize_t nwritten;

		memset (&msg, 0, sizeof(msg));
		msg.msg_iov = (struct iovec*)&iov[index];
		msg.msg_iovlen = count - index;
		nwritten = sendmsg(proto->client->sck, &msg, 0);
		/*nwritten = writev(proto->client->sck, (const struct iovec*)&iov[index], count - index);*/
		if (nwritten <= -1) 
		{
			fprintf (stderr, "sendmsg failure - %s\n", strerror(errno));
			return -1;
		}

		while (index < count && (size_t)nwritten >= iov[index].iov_len)
			nwritten -= iov[index++].iov_len;

		if (index == count) break;

		iov[index].iov_base = (void*)((hcl_uint8_t*)iov[index].iov_base + nwritten);
		iov[index].iov_len -= nwritten;
	}

	if (proto->reply.len <= 0)
	{
		/* this should be the last chunk */
		proto->reply.nchunks = 0;
	}
	else
	{
		proto->reply.nchunks++;
		proto->reply.len = 0;
	}

	return 0;
}

void proto_start_reply (proto_t* proto)
{
	proto->reply.type = PROTO_REPLY_CHUNKED;
	proto->reply.nchunks = 0;
	proto->reply.len = 0;
}

int proto_feed_reply (proto_t* proto, const hcl_ooch_t* ptr, hcl_oow_t len, int escape)
{
#if defined(HCL_OOCH_IS_BCH)
	/* nothing */
#else
	hcl_oow_t bcslen, ucslen, donelen;
	int x;
#endif

#if defined(HCL_OOCH_IS_BCH)
	while (len > 0)
	{
		if (escape && (*ptr == '\\' || *ptr == '\"'))
		{
			if (proto->reply.len >= HCL_COUNTOF(proto->reply.buf) && write_reply_chunk(proto) <=-1) return -1;
			proto->reply.buf[proto->reply.len++] = '\\';
		}

		if (proto->reply.len >= HCL_COUNTOF(proto->reply.buf) && write_reply_chunk(proto) <=-1) return -1;
		proto->reply.buf[proto->reply.len++] = *ptr++;
		len--;
	}

	return 0;
#else
	donelen = 0;
	while (donelen < len)
	{
		if (escape && (*ptr == '\\' || *ptr == '\"'))
		{
			/* i know that these characters don't need conversion */
			if (proto->reply.len >= HCL_COUNTOF(proto->reply.buf) && write_reply_chunk(proto) <=-1) return -1;
			proto->reply.buf[proto->reply.len++] = '\\';
		}

		bcslen = HCL_COUNTOF(proto->reply.buf) - proto->reply.len;
		if (bcslen < HCL_BCSIZE_MAX)
		{
			if (write_reply_chunk(proto) <=-1) return -1;
			bcslen = HCL_COUNTOF(proto->reply.buf) - proto->reply.len;
		}
		ucslen = len - donelen;

		x = hcl_convootobchars(proto->hcl, &ptr[donelen], &ucslen, &proto->reply.buf[proto->reply.len], &bcslen);
		if (x <= -1 && ucslen <= 0) return -1;

		donelen += ucslen;
		proto->reply.len += bcslen;
	}
#endif
	return 0;
}

int proto_end_reply (proto_t* proto, const hcl_ooch_t* failmsg)
{
	HCL_ASSERT (proto->hcl, proto->reply.type == PROTO_REPLY_CHUNKED);

	if (failmsg)
	{
		if (proto->reply.nchunks <= 0 && proto->reply.len <= 0)
		{
			static hcl_ooch_t err1[] = { '.','E','R','R','O','R',' ','\"' };
			static hcl_ooch_t err2[] = { '\"','\n' };
			proto->reply.type = PROTO_REPLY_SIMPLE; /* switch to the simple mode forcibly */

		simple_error:
			if (proto_feed_reply(proto, err1, 8, 0) <= -1 ||
			    proto_feed_reply(proto, failmsg, hcl_countoocstr(failmsg), 1) <= -1 ||
			    proto_feed_reply(proto, err2, 2, 0) <= -1) return -1;

			if (write_reply_chunk(proto) <= -1) return -1;
		}
		else 
		{
			/* some chunks have beed emitted. but at the end, an error has occurred.
			 * send -1: as the last chunk. the receiver must rub out the reply
			 * buffer received so far and expect the following .ERROR response */
			static hcl_ooch_t err0[] = { '-','1',':','\n' };
			if (proto->reply.len > 0 && write_reply_chunk(proto) <= -1) return -1;

			proto->reply.type = PROTO_REPLY_SIMPLE; /* switch to the simple mode forcibly */
			proto->reply.nchunks = 0;
			proto->reply.len = 0;

			if (proto_feed_reply(proto, err0, 4, 0) <= -1) return -1;
			goto simple_error;
		}
	}
	else
	{
		if (proto->reply.nchunks <= 0 && proto->reply.len <= 0)
		{
			/* in the chunked mode. but no output has been made so far */
			static hcl_ooch_t ok[] = { '.','O','K',' ','\"','\"','\n' };
			proto->reply.type = PROTO_REPLY_SIMPLE; /* switch to the simple mode forcibly */
			if (proto_feed_reply(proto, ok, 7, 0) <= -1) return -1;
			if (write_reply_chunk(proto) <= -1) return -1;
		}
		else
		{
			if (proto->reply.len > 0 && write_reply_chunk(proto) <= -1) return -1;
			if (write_reply_chunk(proto) <= -1) return -1; /* write 0: */
		}
	}

	return 0;
}

static HCL_INLINE int is_spacechar (hcl_ooci_t c)
{
	/* TODO: handle other space unicode characters */
	switch (c)
	{
		case ' ':
		case '\f': /* formfeed */
		case '\r': /* carriage return */
		case '\t': /* horizon tab */
		case '\v': /* vertical tab */
			return 1;

		default:
			return 0;
	}
}

static HCL_INLINE int read_char (proto_t* proto)
{
	proto->lxc = hcl_readchar(proto->hcl);
	if (!proto->lxc) return -1;
	return 0;
}

static HCL_INLINE int unread_last_char (proto_t* proto)
{
	return hcl_unreadchar(proto->hcl, proto->lxc);
}

#define GET_CHAR_TO(proto,c) \
	do { \
		if (read_char(proto) <= -1) return -1; \
		c = (proto)->lxc->c; \
	} while(0)

#define UNGET_LAST_CHAR(proto) \
	do { \
		if (unread_last_char(proto) <= -1) return -1; \
	} while (0)

#define CLEAR_TOKEN_NAME(proto) ((proto)->tok.name.len = 0)
#define SET_TOKEN_TYPE(proto,tv) ((proto)->tok.type = (tv))
#define ADD_TOKEN_CHAR(proto,c) \
	do { if (add_token_char(proto, c) <= -1) return -1; } while (0)


static HCL_INLINE int add_token_char (proto_t* proto, hcl_ooch_t c)
{
	if (proto->tok.len >= proto->tok.capa)
	{
		hcl_ooch_t* tmp;
		hcl_oow_t capa;

		capa = HCL_ALIGN_POW2(proto->tok.len + 1, 64);
		tmp = (hcl_ooch_t*)realloc(proto->tok.ptr, capa * sizeof(*tmp));
		if (!tmp) 
		{
			fprintf (stderr, "cannot alloc memory when adding a character to the token buffer\n");
			return -1;
		}

		proto->tok.ptr = tmp;
		proto->tok.capa = capa;
	}

	proto->tok.ptr[proto->tok.len++] = c;
	return 0;
}

static HCL_INLINE int is_alphachar (hcl_ooci_t c)
{
/* TODO: support full unicode */
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

static void classify_current_ident_token (proto_t* proto)
{
	static struct cmd_t
	{
		proto_token_type_t type;
		hcl_ooch_t name[32];
	} tab[] = 
	{
		{ PROTO_TOKEN_BEGIN,   { '.','B','E','G','I','N','\0' } },
		{ PROTO_TOKEN_END,     { '.','E','N','D','\0' } },
		{ PROTO_TOKEN_SCRIPT,  { '.','S','C','R','I','P','T','\0' } },
		{ PROTO_TOKEN_EXIT,    { '.','E','X','I','T','\0' } },
		/* TODO: add more */
	};
	hcl_oow_t i;

	for (i = 0; i < HCL_COUNTOF(tab); i++)
	{
		if (hcl_compoocharsoocstr(proto->tok.ptr, proto->tok.len, tab[i].name) == 0) 
		{
			SET_TOKEN_TYPE (proto, tab[i].type);
			break;
		}
	}
}

static int get_token (proto_t* proto)
{
	hcl_ooci_t c;

	GET_CHAR_TO (proto, c);

	/* skip spaces */
	while (is_spacechar(c)) GET_CHAR_TO (proto, c);

	SET_TOKEN_TYPE (proto, PROTO_TOKEN_EOF); /* is it correct? */
	proto->tok.len = 0;
	/*proto->tok.loc = hcl->c->lxc.l;*/ /* set token location */
	
	switch (c)
	{
		case HCL_OOCI_EOF:
			SET_TOKEN_TYPE (proto, PROTO_TOKEN_EOF);
			break;
			
		case '\n':
			SET_TOKEN_TYPE (proto, PROTO_TOKEN_NL);
			break;

		case '.':
			SET_TOKEN_TYPE (proto, PROTO_TOKEN_IDENT);

			ADD_TOKEN_CHAR(proto, c);
			GET_CHAR_TO(proto, c);
			if (!is_alphachar(c))
			{
				fprintf (stderr, "alphabetic character is expected...\n");
				return -1;
			}

			do
			{
				ADD_TOKEN_CHAR(proto, c);
				GET_CHAR_TO(proto, c);
			}
			while (is_alphachar(c));

			UNGET_LAST_CHAR (proto);

			classify_current_ident_token (proto);
			break;

		default:
			fprintf (stderr, "unrecognized character found - code [%x]\n", c);
			return -1;
	}
	return 0;
}

int proto_handle_request (proto_t* proto)
{
	if (get_token(proto) <= -1) return -1;

	switch (proto->tok.type)
	{
		case PROTO_TOKEN_EOF:
			if (proto->req.state != PROTO_REQ_IN_TOP_LEVEL)
			{
				fprintf (stderr, "Unexpected EOF without .END\n");
				return -1;
			}

			/* TODO: send some response */
			return 0;

		case PROTO_TOKEN_EXIT:
	/* TODO: erorr - not valid inside BEGIN ... END */
			if (proto->req.state != PROTO_REQ_IN_TOP_LEVEL)
			{
				fprintf (stderr, ".EXIT allowed in the top level only\n");
				return -1;
			}

			/* TODO: send response */
			return 0;

		case PROTO_TOKEN_BEGIN:
			if (proto->req.state != PROTO_REQ_IN_TOP_LEVEL)
			{
				fprintf (stderr, ".BEGIN allowed in the top level only\n");
				return -1;
			}

			if (get_token(proto) <= -1) return -1;
			if (proto->tok.type != PROTO_TOKEN_NL)
			{
				fprintf (stderr, "no new line after BEGIN\n");
				return -1;
			}

			proto->req.state = PROTO_REQ_IN_BLOCK_LEVEL;
			hcl_reset (proto->hcl);
			break;

		case PROTO_TOKEN_END:
		{
			hcl_oop_t obj;

			if (proto->req.state != PROTO_REQ_IN_BLOCK_LEVEL)
			{
				fprintf (stderr, ".END allowed in the block level only\n");
				return -1;
			}

			if (get_token(proto) <= -1) return -1;
			if (proto->tok.type != PROTO_TOKEN_NL)
			{
				fprintf (stderr, "no new line after END\n");
				return -1;
			}

			
			proto_start_reply (proto);
			obj = (hcl_getbclen(proto->hcl) > 0)? hcl_execute(proto->hcl): proto->hcl->_nil;
			if (proto_end_reply(proto, (obj? NULL: hcl_geterrmsg(proto->hcl))) <= -1) 
			{
				fprintf (stderr, "cannot end chunked reply\n");
				return -1;
			}

			proto->req.state = PROTO_REQ_IN_TOP_LEVEL;
			break;
		}

		case PROTO_TOKEN_SCRIPT:
		{
			hcl_oop_t obj;

			if (proto->req.state == PROTO_REQ_IN_TOP_LEVEL) hcl_reset(proto->hcl);

			obj = hcl_read(proto->hcl);
			if (!obj)
			{
				fprintf (stderr, "cannot read script contents\n");
				return -1;
			}

			if (get_token(proto) <= -1) return -1;
			if (proto->tok.type != PROTO_TOKEN_NL)
			{
				fprintf (stderr, "no new line after script contents\n");
				return -1;
			}

			if (hcl_compile(proto->hcl, obj) <= -1)
			{
				fprintf (stderr, "cannot compile script contents\n");
				return -1;
			}

			if (proto->req.state == PROTO_REQ_IN_TOP_LEVEL)
			{
				proto_start_reply (proto);
				obj = hcl_execute(proto->hcl);
				if (proto_end_reply(proto, (obj? NULL: hcl_geterrmsg(proto->hcl))) <= -1) 
				{
					fprintf (stderr, "cannot end chunked reply\n");
					return -1;
				}
			}
			break;
		}

		default:
			fprintf (stderr, "unknwn token - %d\n", proto->tok.type);
			return -1;
	}

	return 1;
}

/* ========================================================================= */

static void zap_all_clients (server_t* server)
{
}

static client_t* alloc_client (server_t*  server, int cli_sck)
{
	client_t* client;
	
	client = (client_t*)malloc(sizeof(*client));
	if (!client) return NULL;

	memset (client, 0, sizeof(*client));
	client->sck = cli_sck;
	client->server = server;
	return client;
}

static void free_client (client_t* client)
{
	if (client->sck >= 0) close (client->sck);
/* TODO: do anything to server? */
	free (client);
}

static void* start_client (void* ctx);

server_t* server_open (size_t xtnsize, hcl_oow_t memsize, const char* logopt, unsigned int dbgopt, int large_pages)
{
	server_t* server;

	server = (server_t*)malloc(sizeof(*server) + xtnsize);
	if (!server) return NULL;

	memset (server, 0, sizeof(*server) + xtnsize);
	server->cfg.memsize = memsize;
	server->cfg.logopt = logopt; 
	server->cfg.dbgopt = dbgopt;
	server->cfg.large_pages = large_pages;
	pthread_mutex_init (&server->client_mutex, NULL);
	pthread_mutex_init (&server->log_mutex, NULL);
	return server;
}

void server_close (server_t* server)
{
	pthread_mutex_destroy (&server->log_mutex);
	pthread_mutex_destroy (&server->client_mutex);
	free (server);
}


int add_client_to_server (server_t* server, client_state_t cstate, client_t* client)
{
	if (client->server != server) 
	{
		fprintf (stderr, "cannot add client %p to server - client not beloing to server\n", client); /* TODO: use a client id when printing instead of the pointer */
		return -1;
	}


	if (server->client_list[cstate].tail)
	{
		server->client_list[cstate].tail->next_client = client;
		client->prev_client = server->client_list[cstate].tail;
		server->client_list[cstate].tail = client;
		client->next_client = NULL;
	}
	else
	{
		server->client_list[cstate].tail = client;
		server->client_list[cstate].head = client;
		client->prev_client = NULL;
		client->next_client = NULL;
	}

	client->state = cstate;
	return 0;
}

int zap_client_in_server (server_t* server, client_t* client)
{
	if (client->server != server) 
	{
		fprintf (stderr, "cannot zap client %p from server - client not beloing to server\n", client); /* TODO: use a client id when printing instead of the pointer */
		return -1;
	}

	if (client->prev_client) client->prev_client->next_client = client->next_client;
	else server->client_list[client->state].head = client->next_client;
	if (client->next_client) client->next_client->prev_client = client->prev_client;
	else server->client_list[client->state].tail = client->prev_client;

	return 0;
}

static void* start_client (void* ctx)
{
	client_t* client = (client_t*)ctx;
	server_t* server = client->server;

	client->thr = pthread_self();
	client->proto = proto_open(0, client); /* TODO: get this from argumen */
	if (!client->proto)
	{
		free_client (client);
		return NULL;
	}

	pthread_mutex_lock (&server->client_mutex);
	add_client_to_server (server, CLIENT_STATE_ALIVE, client);
	pthread_mutex_unlock (&server->client_mutex);

	while (!server->stopreq)
	{
		if (proto_handle_request(client->proto) <= 0) break;
	}

	proto_close (client->proto);
	client->proto = NULL;

	pthread_mutex_lock (&server->client_mutex);
	zap_client_in_server (server, client);
	add_client_to_server (server, CLIENT_STATE_DEAD, client);
	pthread_mutex_unlock (&server->client_mutex);

	//free_client (client);
	return NULL;
}

void purge_all_clients (server_t* server, client_state_t cstate)
{
	client_t* client, * next;

	client = server->client_list[cstate].head;
	while (client)
	{
		next = client->next_client;

		pthread_join (client->thr, NULL);
		zap_client_in_server (server, client);
		free_client (client);

		client = next;
	}
}

int server_start (server_t* server, const char* addrs)
{
	sck_addr_t srv_addr;
	int srv_fd, sck_fam;
	int optval;
	socklen_t srv_len;

/* TODO: interprete 'addrs' as a command-separated address list 
 *  192.168.1.1:20,[::1]:20,127.0.0.1:345
 */
	memset (&srv_addr, 0, sizeof(srv_addr));
	if (inet_pton (AF_INET6, addrs, &srv_addr.in6.sin6_addr) != 1)
	{
		if (inet_pton (AF_INET, addrs, &srv_addr.in4.sin_addr) != 1)
		{
			fprintf (stderr, "cannot open convert server address %s - %s\n", addrs, strerror(errno));
			return -1;
		}
		else
		{
			srv_addr.in4.sin_family = AF_INET;
			srv_addr.in4.sin_port = htons(8888); /* TODO: change it */
			srv_len = sizeof(srv_addr.in4);
			sck_fam = AF_INET;
		}
	}
	else
	{
		srv_addr.in6.sin6_family = AF_INET6;
		srv_addr.in6.sin6_port = htons(8888); /* TODO: change it */
		srv_len = sizeof(srv_addr.in6);
		sck_fam = AF_INET6;
	}

	srv_fd = socket(sck_fam, SOCK_STREAM, 0);
	if (srv_fd == -1)
	{
		fprintf (stderr, "cannot open server socket - %s\n", strerror(errno));
		return -1;
	}

	optval = 1;
	setsockopt (srv_fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(int));

	if (bind(srv_fd, (struct sockaddr*)&srv_addr, srv_len) == -1)
	{
		fprintf (stderr, "cannot bind server socket %d - %s\n", srv_fd, strerror(errno));
		close (srv_fd);
		return -1;
	}

	if (listen (srv_fd, 128) == -1)
	{
		fprintf (stderr, "cannot listen on server socket %d - %s\n", srv_fd, strerror(errno));
		close (srv_fd);
		return -1;
	}

	server->stopreq = 0;
	while (!server->stopreq)
	{
		sck_addr_t cli_addr;
		int cli_fd;
		socklen_t cli_len;
		pthread_t thr;
		client_t* client;

		purge_all_clients (server, CLIENT_STATE_DEAD);

		cli_len = sizeof(cli_addr);
		cli_fd = accept(srv_fd, (struct sockaddr*)&cli_addr, &cli_len);
		if (cli_fd == -1)
		{
			if (errno != EINTR || !server->stopreq)
			{
				fprintf (stderr, "cannot accept client on socket %d - %s\n", srv_fd, strerror(errno));
			}
			break;
		}

		client = alloc_client(server, cli_fd);
		if (pthread_create(&thr, NULL, start_client, client) != 0)
		{
			free_client (client);
		}
	}

	purge_all_clients (server, CLIENT_STATE_DEAD); // is thread protection needed?
	purge_all_clients (server, CLIENT_STATE_ALIVE); // is thread protection needed?
	return 0;
}

void server_stop (server_t* server)
{
	server->stopreq = 1;
}
/* ========================================================================= */

static void print_synerr (hcl_t* hcl)
{
	hcl_synerr_t synerr;
	xtn_t* xtn;

	xtn = (xtn_t*)hcl_getxtn(hcl);
	hcl_getsynerr (hcl, &synerr);

	hcl_logbfmt (hcl,HCL_LOG_STDERR, "ERROR: ");
	if (synerr.loc.file)
	{
		hcl_logbfmt (hcl, HCL_LOG_STDERR, "%js", synerr.loc.file);
	}
	else
	{
		/*hcl_logbfmt (hcl, HCL_LOG_STDERR, "%s", xtn->read_path);*/
		hcl_logbfmt (hcl, HCL_LOG_STDERR, "client");
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
 
int main_server (int argc, char* argv[])
{
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

	server_t* server;

	const char* logopt = HCL_NULL;
	hcl_oow_t memsize = MIN_MEMSIZE;
	int large_pages = 0;
	unsigned int dbgopt = 0;

	setlocale (LC_ALL, "");

	if (argc < 2)
	{
	print_usage:
		fprintf (stderr, "Usage: %s bind-address:port\n", argv[0]);
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
					if (parse_dbgopt (opt.arg, &dbgopt) <= -1) return -1;
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

	server = server_open(0, memsize, logopt, dbgopt, large_pages);
	if (!server)
	{
		fprintf (stderr, "cannot open server\n");
		return -1;
	}

	g_server = server;
	set_signal (SIGINT, handle_sigint);
	server_start (server, argv[opt.ind]);
	set_signal_to_default (SIGINT);
	g_server = NULL;

	server_close (server);
	return 0;
}
