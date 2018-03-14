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

#include "hcl-s.h"
#include "hcl-prv.h"

#include <stdio.h>
#include <string.h>
#include <errno.h>

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
#	include <pthread.h>
#	include <sys/uio.h>
#	include <poll.h>
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
	int fd;
	hcl_bch_t* fn;
};

typedef struct worker_hcl_xtn_t worker_hcl_xtn_t;
struct worker_hcl_xtn_t
{
	hcl_server_proto_t* proto;
	int vm_running;
};

typedef struct dummy_hcl_xtn_t dummy_hcl_xtn_t;
struct dummy_hcl_xtn_t
{
	hcl_server_t* server;
};


enum hcl_server_proto_token_type_t
{
	HCL_SERVER_PROTO_TOKEN_EOF,
	HCL_SERVER_PROTO_TOKEN_NL,

	HCL_SERVER_PROTO_TOKEN_BEGIN,
	HCL_SERVER_PROTO_TOKEN_END,
	HCL_SERVER_PROTO_TOKEN_SCRIPT,
	HCL_SERVER_PROTO_TOKEN_EXIT,

	/*HCL_SERVER_PROTO_TOKEN_AUTH,*/
	HCL_SERVER_PROTO_TOKEN_KILL_WORKER,
	HCL_SERVER_PROTO_TOKEN_SHOW_WORKERS,

	HCL_SERVER_PROTO_TOKEN_IDENT
};
typedef enum hcl_server_proto_token_type_t hcl_server_proto_token_type_t;

typedef struct hcl_server_proto_token_t hcl_server_proto_token_t;
struct hcl_server_proto_token_t
{
	hcl_server_proto_token_type_t type;
	hcl_ooch_t* ptr;
	hcl_oow_t len;
	hcl_oow_t capa;
	hcl_ioloc_t loc;
};

enum hcl_server_proto_req_state_t
{
	HCL_SERVER_PROTO_REQ_IN_TOP_LEVEL,
	HCL_SERVER_PROTO_REQ_IN_BLOCK_LEVEL
};

#define HCL_SERVER_PROTO_REPLY_BUF_SIZE 1300

enum hcl_server_proto_reply_type_t
{
	HCL_SERVER_PROTO_REPLY_SIMPLE = 0,
	HCL_SERVER_PROTO_REPLY_CHUNKED
};
typedef enum hcl_server_proto_reply_type_t hcl_server_proto_reply_type_t;

struct hcl_server_proto_t
{
	hcl_server_worker_t* worker;

	hcl_t* hcl;
	hcl_iolxc_t* lxc;
	hcl_server_proto_token_t tok;

	struct
	{
		int state;
	} req;

	struct
	{
		hcl_server_proto_reply_type_t type;
		hcl_oow_t nchunks;
		hcl_bch_t buf[HCL_SERVER_PROTO_REPLY_BUF_SIZE];
		hcl_oow_t len;
	} reply;
};

enum hcl_server_worker_state_t
{
	HCL_SERVER_WORKER_STATE_DEAD  = 0,
	HCL_SERVER_WORKER_STATE_ALIVE = 1
};
typedef enum hcl_server_worker_state_t hcl_server_worker_state_t;

struct hcl_server_worker_t
{
	pthread_t thr;
	int sck;
	/* TODO: peer address */

	int claimed;
	time_t time_created;
	hcl_server_worker_state_t state;

	hcl_server_proto_t* proto;

	hcl_server_t* server;
	hcl_server_worker_t* prev_worker;
	hcl_server_worker_t* next_worker;
};

struct hcl_server_t
{
	hcl_mmgr_t*  mmgr;
	hcl_cmgr_t*  cmgr;
	hcl_server_prim_t prim;
	hcl_t* dummy_hcl;

	hcl_errnum_t errnum;
	struct
	{
		hcl_ooch_t buf[2048];
		hcl_oow_t len;
	} errmsg;
	int stopreq;

	struct
	{
		unsigned int trait;
		unsigned int logmask;
		hcl_oow_t worker_stack_size;
		hcl_ntime_t worker_idle_timeout;
		hcl_oow_t actor_heap_size;
		hcl_ntime_t actor_max_runtime;
	} cfg;

	struct
	{
		hcl_server_worker_t* head;
		hcl_server_worker_t* tail;
	} worker_list[2];

	pthread_mutex_t worker_mutex;
	pthread_mutex_t log_mutex;
};

int hcl_server_proto_feed_reply (hcl_server_proto_t* proto, const hcl_ooch_t* ptr, hcl_oow_t len, int escape);

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
	worker_hcl_xtn_t* xtn = (worker_hcl_xtn_t*)hcl_getxtn(hcl);
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

		bb->fd = xtn->proto->worker->sck;
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
		if (bb->fd >= 0 && bb->fd != xtn->proto->worker->sck) close (bb->fd);
		hcl_freemem (hcl, bb);
	}
	return -1;
}

static HCL_INLINE int close_input (hcl_t* hcl, hcl_ioinarg_t* arg)
{
	worker_hcl_xtn_t* xtn = (worker_hcl_xtn_t*)hcl_getxtn(hcl);
	bb_t* bb;

	bb = (bb_t*)arg->handle;
	HCL_ASSERT (hcl, bb != HCL_NULL && bb->fd >= 0);

	if (bb->fd != xtn->proto->worker->sck) close (bb->fd);
	hcl_freemem (hcl, bb);

	arg->handle = HCL_NULL;
	return 0;
}


static HCL_INLINE int read_input (hcl_t* hcl, hcl_ioinarg_t* arg)
{
	worker_hcl_xtn_t* xtn = (worker_hcl_xtn_t*)hcl_getxtn(hcl);
	bb_t* bb;
	hcl_oow_t bcslen, ucslen, remlen;
	int x;

	bb = (bb_t*)arg->handle;
	HCL_ASSERT (hcl, bb != HCL_NULL && bb->fd >= 0);

	if (bb->fd == xtn->proto->worker->sck)
	{
		ssize_t x;

	start_over:
		while (1)
		{
			int n;
			struct pollfd pfd;
			
			if (xtn->proto->worker->server->stopreq)
			{
				hcl_seterrbfmt (hcl, HCL_EGENERIC, "stop requested");
				return -1;
			}

			pfd.fd = bb->fd;
			pfd.events = POLLIN | POLLERR;
			n = poll(&pfd, 1, 10000); /* TOOD: adjust this interval? */
			if (n <= -1)
			{
				if (errno == EINTR) goto start_over;
				hcl_seterrwithsyserr (hcl, errno);
				return -1;
			}
			else if (n >= 1) break;

/* TOOD: idle timeout check - compute idling time and check it against server->cfg.worker_idle_timeout */
		}
		
		x = recv(bb->fd, &bb->buf[bb->len], HCL_COUNTOF(bb->buf) - bb->len, 0);
		if (x <= -1)
		{
			if (errno == EINTR) goto start_over;
			hcl_seterrwithsyserr (hcl, errno);
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
			worker_hcl_xtn_t* xtn = (worker_hcl_xtn_t*)hcl_getxtn(hcl);
			hcl_iooutarg_t* outarg = (hcl_iooutarg_t*)arg;

			if (hcl_server_proto_feed_reply(xtn->proto, outarg->ptr, outarg->len, 0) <= -1) 
			{
				/* TODO: change error code and message. propagage the errormessage from proto */
				hcl_seterrbfmt (hcl, HCL_EIOERR, "failed to write message via proto");

				/* writing failure on the socket is a critical failure. 
				 * execution must get aborted */
				hcl_abort (hcl);
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

static void log_write (hcl_t* hcl, unsigned int mask, const hcl_ooch_t* msg, hcl_oow_t len)
{
	worker_hcl_xtn_t* xtn = (worker_hcl_xtn_t*)hcl_getxtn(hcl);
	hcl_server_t* server;

	server = xtn->proto->worker->server;
	pthread_mutex_lock (&server->log_mutex);
	server->prim.log_write (server, xtn->proto->worker->sck, mask, msg, len);
	pthread_mutex_unlock (&server->log_mutex);
}

static void log_write_for_dummy (hcl_t* hcl, unsigned int mask, const hcl_ooch_t* msg, hcl_oow_t len)
{
	dummy_hcl_xtn_t* xtn = (dummy_hcl_xtn_t*)hcl_getxtn(hcl);
	hcl_server_t* server;

	server = xtn->server;
	pthread_mutex_lock (&server->log_mutex);
	server->prim.log_write (server, -1, mask, msg, len);
	pthread_mutex_unlock (&server->log_mutex);
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
#if defined(HAVE_CLOCK_GETTIME) && defined(CLOCK_MONOTONIC)
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
#if defined(HAVE_NANOSLEEP)
	struct timespec ts;
	ts.tv_sec = dur->sec;
	ts.tv_nsec = dur->nsec;
	nanosleep (&ts, HCL_NULL);
#elif defined(HAVE_USLEEP)
	usleep (HCL_SECNSEC_TO_USEC(dur->sec, dur->nsec));
#else
#	error UNSUPPORT SLEEP
#endif
}

/* ========================================================================= */

static int vm_startup (hcl_t* hcl)
{
	worker_hcl_xtn_t* xtn = (worker_hcl_xtn_t*)hcl_getxtn(hcl);
	xtn->vm_running = 1;
	return 0;
}

static void vm_cleanup (hcl_t* hcl)
{
	worker_hcl_xtn_t* xtn = (worker_hcl_xtn_t*)hcl_getxtn(hcl);
	xtn->vm_running = 0;
}

static void vm_checkbc (hcl_t* hcl, hcl_oob_t bcode)
{
	worker_hcl_xtn_t* xtn = (worker_hcl_xtn_t*)hcl_getxtn(hcl);

	if (xtn->proto->worker->server->stopreq) hcl_abort(hcl);
	
	/* TODO: check how to this vm has been running. too long? abort it */
	/* check agains xtn->proto->worker->server->cfg.actor_max_runtime */
}

/*
static void gc_hcl (hcl_t* hcl)
{
	worker_hcl_xtn_t* xtn = (worker_hcl_xtn_t*)hcl_getxtn(hcl);
}


static void fini_hcl (hcl_t* hcl)
{
	worker_hcl_xtn_t* xtn = (worker_hcl_xtn_t*)hcl_getxtn(hcl);
}
*/
/* ========================================================================= */


union sockaddr_t
{
	struct sockaddr_in in4;
#if (HCL_SIZEOF_STRUCT_SOCKADDR_IN6 > 0)
	struct sockaddr_in6 in6;
#endif
};
typedef union sockaddr_t sockaddr_t;

#undef char_t
#undef oocs_t
#undef str_to_ipv4
#undef str_to_ipv6
#undef str_to_sockaddr

#define ooch_t hcl_bch_t
#define oocs_t hcl_bcs_t
#define str_to_ipv4 bchars_to_ipv4
#define str_to_ipv6 bchars_to_ipv6
#define str_to_sockaddr bchars_to_sockaddr
#include "sa-utl.h"

#undef ooch_t
#undef oocs_t
#undef str_to_ipv4
#undef str_to_ipv6
#undef str_to_sockaddr

#define ooch_t hcl_uch_t
#define oocs_t hcl_ucs_t
#define str_to_ipv4 uchars_to_ipv4
#define str_to_ipv6 uchars_to_ipv6
#define str_to_sockaddr uchars_to_sockaddr
#include "sa-utl.h"
/* ========================================================================= */

#define HCL_SERVER_PROTO_LOG_MASK (HCL_LOG_ERROR | HCL_LOG_APP)

hcl_server_proto_t* hcl_server_proto_open (hcl_oow_t xtnsize, hcl_server_worker_t* worker)
{
	hcl_server_proto_t* proto;
	hcl_vmprim_t vmprim;
	hcl_cb_t hclcb;
	worker_hcl_xtn_t* xtn;
	unsigned int trait;

	HCL_MEMSET (&vmprim, 0, HCL_SIZEOF(vmprim));
	if (worker->server->cfg.trait & HCL_SERVER_TRAIT_USE_LARGE_PAGES)
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

	proto = (hcl_server_proto_t*)HCL_MMGR_ALLOC(worker->server->mmgr, HCL_SIZEOF(*proto));
	if (!proto) return HCL_NULL;

	HCL_MEMSET (proto, 0, HCL_SIZEOF(*proto));
	proto->worker = worker;

	proto->hcl = hcl_open(proto->worker->server->mmgr, HCL_SIZEOF(*xtn), worker->server->cfg.actor_heap_size, &vmprim, HCL_NULL);
	if (!proto->hcl) goto oops;

	xtn = (worker_hcl_xtn_t*)hcl_getxtn(proto->hcl);
	xtn->proto = proto;

	hcl_setoption (proto->hcl, HCL_LOG_MASK, &proto->worker->server->cfg.logmask);
	hcl_setcmgr (proto->hcl, proto->worker->server->cmgr);

	hcl_getoption (proto->hcl, HCL_TRAIT, &trait);
#if defined(HCL_BUILD_DEBUG)
	if (proto->worker->server->cfg.trait & HCL_SERVER_TRAIT_DEBUG_GC) trait |= HCL_DEBUG_GC;
	if (proto->worker->server->cfg.trait & HCL_SERVER_TRAIT_DEBUG_BIGINT) trait |= HCL_DEBUG_BIGINT;
#endif
	hcl_setoption (proto->hcl, HCL_TRAIT, &trait);

	HCL_MEMSET (&hclcb, 0, HCL_SIZEOF(hclcb));
	/*hclcb.fini = fini_hcl;
	hclcb.gc = gc_hcl;*/
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
		HCL_MMGR_FREE (proto->worker->server->mmgr, proto);
	}
	return HCL_NULL;
}

void hcl_server_proto_close (hcl_server_proto_t* proto)
{
	if (proto->tok.ptr) HCL_MMGR_FREE (proto->worker->server->mmgr, proto->tok.ptr);
	hcl_close (proto->hcl);
	HCL_MMGR_FREE (proto->worker->server->mmgr, proto);
}

static int write_reply_chunk (hcl_server_proto_t* proto)
{
	struct msghdr msg;
	struct iovec iov[3];
	hcl_bch_t cl[16]; /* ensure that this is large enough for the chunk length string */
	int index = 0, count = 0;

	if (proto->reply.type == HCL_SERVER_PROTO_REPLY_CHUNKED)
	{
		if (proto->reply.nchunks <= 0)
		{
			/* this is the first chunk */
			iov[count].iov_base = ".OK\n.ENCODING chunked\n.DATA\n";
			iov[count++].iov_len = 28;
		}

		iov[count].iov_base = cl,
		iov[count++].iov_len = snprintf(cl, HCL_SIZEOF(cl), "%s%zu:", (((proto->worker->server->cfg.trait & HCL_SERVER_TRAIT_READABLE_PROTO) && proto->reply.nchunks > 0)? "\n": ""), proto->reply.len); 
	}
	iov[count].iov_base = proto->reply.buf;
	iov[count++].iov_len = proto->reply.len;

	while (1)
	{
		ssize_t nwritten;

		HCL_MEMSET (&msg, 0, HCL_SIZEOF(msg));
		msg.msg_iov = (struct iovec*)&iov[index];
		msg.msg_iovlen = count - index;
		nwritten = sendmsg(proto->worker->sck, &msg, 0);
		/*nwritten = writev(proto->worker->sck, (const struct iovec*)&iov[index], count - index);*/
		if (nwritten <= -1) 
		{
			/* error occurred inside the worker thread shouldn't affect the error information
			 * in the server object. so here, i just log a message */
			hcl_logbfmt (proto->hcl, HCL_SERVER_PROTO_LOG_MASK, "sendmsg failure on %d - %hs\n", proto->worker->sck, strerror(errno));
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

void hcl_server_proto_start_reply (hcl_server_proto_t* proto)
{
	proto->reply.type = HCL_SERVER_PROTO_REPLY_CHUNKED;
	proto->reply.nchunks = 0;
	proto->reply.len = 0;
}

int hcl_server_proto_feed_reply (hcl_server_proto_t* proto, const hcl_ooch_t* ptr, hcl_oow_t len, int escape)
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
			/* i know that these characters don't need encoding conversion */
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

int hcl_server_proto_end_reply (hcl_server_proto_t* proto, const hcl_ooch_t* failmsg)
{
	HCL_ASSERT (proto->hcl, proto->reply.type == HCL_SERVER_PROTO_REPLY_CHUNKED);

	if (failmsg)
	{
		if (proto->reply.nchunks <= 0 && proto->reply.len <= 0)
		{
			static hcl_ooch_t err1[] = { '.','E','R','R','O','R',' ','\"' };
			static hcl_ooch_t err2[] = { '\"','\n' };
			proto->reply.type = HCL_SERVER_PROTO_REPLY_SIMPLE; /* switch to the simple mode forcibly */

		simple_error:
			if (hcl_server_proto_feed_reply(proto, err1, 8, 0) <= -1 ||
			    hcl_server_proto_feed_reply(proto, failmsg, hcl_countoocstr(failmsg), 1) <= -1 ||
			    hcl_server_proto_feed_reply(proto, err2, 2, 0) <= -1) return -1;

			if (write_reply_chunk(proto) <= -1) return -1;
		}
		else 
		{
			/* some chunks have beed emitted. but at the end, an error has occurred.
			 * send -1: as the last chunk. the receiver must rub out the reply
			 * buffer received so far and expect the following .ERROR response */
			static hcl_ooch_t err0[] = { '-','1',':','\n' };
			if (proto->reply.len > 0 && write_reply_chunk(proto) <= -1) return -1;

			proto->reply.type = HCL_SERVER_PROTO_REPLY_SIMPLE; /* switch to the simple mode forcibly */
			proto->reply.nchunks = 0;
			proto->reply.len = 0;

			if (hcl_server_proto_feed_reply(proto, err0, 4, 0) <= -1) return -1;
			goto simple_error;
		}
	}
	else
	{
		if (proto->reply.nchunks <= 0 && proto->reply.len <= 0)
		{
			/* in the chunked mode. but no output has been made so far */
			static hcl_ooch_t ok[] = { '.','O','K',' ','\"','\"','\n' };
			proto->reply.type = HCL_SERVER_PROTO_REPLY_SIMPLE; /* switch to the simple mode forcibly */
			if (hcl_server_proto_feed_reply(proto, ok, 7, 0) <= -1) return -1;
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

static HCL_INLINE int read_char (hcl_server_proto_t* proto)
{
	proto->lxc = hcl_readchar(proto->hcl);
	if (!proto->lxc) return -1;
	return 0;
}

static HCL_INLINE int unread_last_char (hcl_server_proto_t* proto)
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


static HCL_INLINE int add_token_char (hcl_server_proto_t* proto, hcl_ooch_t c)
{
	if (proto->tok.len >= proto->tok.capa)
	{
		hcl_ooch_t* tmp;
		hcl_oow_t capa;

		capa = HCL_ALIGN_POW2(proto->tok.len + 1, 64);
		tmp = (hcl_ooch_t*)HCL_MMGR_REALLOC(proto->worker->server->mmgr, proto->tok.ptr, capa * HCL_SIZEOF(*tmp));
		if (!tmp) 
		{
			hcl_logbfmt (proto->hcl, HCL_SERVER_PROTO_LOG_MASK, "out of memory in allocating a token buffer\n");
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

static void classify_current_ident_token (hcl_server_proto_t* proto)
{
	static struct cmd_t
	{
		hcl_server_proto_token_type_t type;
		hcl_ooch_t name[32];
	} tab[] = 
	{
		{ HCL_SERVER_PROTO_TOKEN_BEGIN,          { '.','B','E','G','I','N','\0' } },
		{ HCL_SERVER_PROTO_TOKEN_END,            { '.','E','N','D','\0' } },
		{ HCL_SERVER_PROTO_TOKEN_SCRIPT,         { '.','S','C','R','I','P','T','\0' } },
		{ HCL_SERVER_PROTO_TOKEN_EXIT,           { '.','E','X','I','T','\0' } },
		
		{ HCL_SERVER_PROTO_TOKEN_KILL_WORKER,    { '.','K','I','L','L','-','W','O','R','K','E','R','\0' } },
		{ HCL_SERVER_PROTO_TOKEN_SHOW_WORKERS,   { '.','S','H','O','W','-','W','O','R','K','E','R','S','\0' } },
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

static int get_token (hcl_server_proto_t* proto)
{
	hcl_ooci_t c;

	GET_CHAR_TO (proto, c);

	/* skip spaces */
	while (is_spacechar(c)) GET_CHAR_TO (proto, c);

	SET_TOKEN_TYPE (proto, HCL_SERVER_PROTO_TOKEN_EOF); /* is it correct? */
	proto->tok.len = 0;
	proto->tok.loc = proto->hcl->c->lxc.l; /* set token location */
	
	switch (c)
	{
		case HCL_OOCI_EOF:
			SET_TOKEN_TYPE (proto, HCL_SERVER_PROTO_TOKEN_EOF);
			break;
			
		case '\n':
			SET_TOKEN_TYPE (proto, HCL_SERVER_PROTO_TOKEN_NL);
			break;

		case '.':
			SET_TOKEN_TYPE (proto, HCL_SERVER_PROTO_TOKEN_IDENT);

			ADD_TOKEN_CHAR(proto, c);
			GET_CHAR_TO(proto, c);
			if (!is_alphachar(c))
			{
				hcl_logbfmt (proto->hcl, HCL_SERVER_PROTO_LOG_MASK, "alphabetic character expected after a period\n");
				return -1;
			}

			do
			{
				ADD_TOKEN_CHAR(proto, c);
				GET_CHAR_TO(proto, c);
			}
			while (is_alphachar(c) || c == '-');

			UNGET_LAST_CHAR (proto);

			classify_current_ident_token (proto);
			break;

		default:
			hcl_logbfmt (proto->hcl, HCL_SERVER_PROTO_LOG_MASK, "unrecognized character - [%jc]\n", c);
			return -1;
	}
	return 0;
}

int hcl_server_proto_handle_request (hcl_server_proto_t* proto)
{
	if (get_token(proto) <= -1) return -1;

	switch (proto->tok.type)
	{
		case HCL_SERVER_PROTO_TOKEN_NL:
			/* ignore new lines */
			break;

		case HCL_SERVER_PROTO_TOKEN_EOF:
			if (proto->req.state != HCL_SERVER_PROTO_REQ_IN_TOP_LEVEL)
			{
				hcl_logbfmt (proto->hcl, HCL_SERVER_PROTO_LOG_MASK, "unexpected EOF without .END\n");
				return -1;
			}

			/* drop connection silently */
			return 0;

		case HCL_SERVER_PROTO_TOKEN_EXIT:
			if (proto->req.state != HCL_SERVER_PROTO_REQ_IN_TOP_LEVEL)
			{
				hcl_logbfmt (proto->hcl, HCL_SERVER_PROTO_LOG_MASK, ".EXIT allowed in the top level only\n");
				return -1;
			}

			if (get_token(proto) <= -1) return -1;
			if (proto->tok.type != HCL_SERVER_PROTO_TOKEN_NL)
			{
				hcl_logbfmt (proto->hcl, HCL_SERVER_PROTO_LOG_MASK, "no new line after .EXIT\n");
				return -1;
			}
			
			hcl_server_proto_start_reply (proto);
			if (hcl_server_proto_end_reply(proto, HCL_NULL) <= -1) 
			{
				hcl_logbfmt (proto->hcl, HCL_SERVER_PROTO_LOG_MASK, "cannot finalize reply for .EXIT\n");
				return -1;
			}
			return 0;

		case HCL_SERVER_PROTO_TOKEN_BEGIN:
			if (proto->req.state != HCL_SERVER_PROTO_REQ_IN_TOP_LEVEL)
			{
				hcl_logbfmt (proto->hcl, HCL_SERVER_PROTO_LOG_MASK, ".BEGIN not allowed to be nested\n");
				return -1;
			}

			if (get_token(proto) <= -1) return -1;
			if (proto->tok.type != HCL_SERVER_PROTO_TOKEN_NL)
			{
				hcl_logbfmt (proto->hcl, HCL_SERVER_PROTO_LOG_MASK, "no new line after .BEGIN\n");
				return -1;
			}

			proto->req.state = HCL_SERVER_PROTO_REQ_IN_BLOCK_LEVEL;
			hcl_reset (proto->hcl);
			break;

		case HCL_SERVER_PROTO_TOKEN_END:
		{
			hcl_oop_t obj;

			if (proto->req.state != HCL_SERVER_PROTO_REQ_IN_BLOCK_LEVEL)
			{
				hcl_logbfmt (proto->hcl, HCL_SERVER_PROTO_LOG_MASK, ".END without opening .BEGIN\n");
				return -1;
			}

			if (get_token(proto) <= -1) return -1;
			if (proto->tok.type != HCL_SERVER_PROTO_TOKEN_NL)
			{
				hcl_logbfmt (proto->hcl, HCL_SERVER_PROTO_LOG_MASK, "no new line after .BEGIN\n");
				return -1;
			}

			hcl_server_proto_start_reply (proto);
			obj = (hcl_getbclen(proto->hcl) > 0)? hcl_execute(proto->hcl): proto->hcl->_nil;
			if (hcl_server_proto_end_reply(proto, (obj? HCL_NULL: hcl_geterrmsg(proto->hcl))) <= -1) 
			{
				hcl_logbfmt (proto->hcl, HCL_SERVER_PROTO_LOG_MASK, "cannot finalize reply for .END\n");
				return -1;
			}

			proto->req.state = HCL_SERVER_PROTO_REQ_IN_TOP_LEVEL;
			break;
		}

		case HCL_SERVER_PROTO_TOKEN_SCRIPT:
		{
			hcl_oop_t obj;

			if (proto->req.state == HCL_SERVER_PROTO_REQ_IN_TOP_LEVEL) hcl_reset(proto->hcl);

			obj = hcl_read(proto->hcl);
			if (!obj)
			{
				hcl_logbfmt (proto->hcl, HCL_SERVER_PROTO_LOG_MASK, "cannot read .SCRIPT contents - %js\n", hcl_geterrmsg(proto->hcl));
				return -1;
			}

			if (get_token(proto) <= -1) return -1;
			if (proto->tok.type != HCL_SERVER_PROTO_TOKEN_NL)
			{
				hcl_logbfmt (proto->hcl, HCL_SERVER_PROTO_LOG_MASK, "no new line after .SCRIPT contest\n");
				return -1;
			}

			if (hcl_compile(proto->hcl, obj) <= -1)
			{
				hcl_logbfmt (proto->hcl, HCL_SERVER_PROTO_LOG_MASK, "cannot compile .SCRIPT contents - %js\n", hcl_geterrmsg(proto->hcl));
				return -1;
			}

			if (proto->req.state == HCL_SERVER_PROTO_REQ_IN_TOP_LEVEL)
			{
				hcl_server_proto_start_reply (proto);
				obj = hcl_execute(proto->hcl);
				if (hcl_server_proto_end_reply(proto, (obj? HCL_NULL: hcl_geterrmsg(proto->hcl))) <= -1) 
				{
					hcl_logbfmt (proto->hcl, HCL_SERVER_PROTO_LOG_MASK, "cannot finalize reply for .SCRIPT\n");
					return -1;
				}
			}
			break;
			
		}

		case HCL_SERVER_PROTO_TOKEN_SHOW_WORKERS:
			hcl_server_proto_start_reply (proto);
			
			pthread_mutex_lock (&proto->worker->server->worker_mutex);
			//print_hcl_server_workers (proto);
			pthread_mutex_unlock (&proto->worker->server->worker_mutex);
			
			if (hcl_server_proto_end_reply(proto, HCL_NULL) <= -1)
			{
				hcl_logbfmt (proto->hcl, HCL_SERVER_PROTO_LOG_MASK, "cannot finalize reply for .SHOW-WORKERS\n");
				return -1;
			}
			
			break;

		case HCL_SERVER_PROTO_TOKEN_KILL_WORKER:
			hcl_server_proto_start_reply (proto);
			
			pthread_mutex_lock (&proto->worker->server->worker_mutex);
			//kill_hcl_server_worker (proto);
			pthread_mutex_unlock (&proto->worker->server->worker_mutex);
			
			if (hcl_server_proto_end_reply(proto, HCL_NULL) <= -1)
			{
				hcl_logbfmt (proto->hcl, HCL_SERVER_PROTO_LOG_MASK, "cannot finalize reply for .KILL-WORKER\n");
				return -1;
			}
			break;

		default:
			hcl_logbfmt (proto->hcl, HCL_SERVER_PROTO_LOG_MASK, "unknown token - %d - %.*js\n", (int)proto->tok.type, proto->tok.len, proto->tok.ptr);
			return -1;
	}

	return 1;
}

/* ========================================================================= */

hcl_server_t* hcl_server_open (hcl_mmgr_t* mmgr, hcl_oow_t xtnsize, hcl_server_prim_t* prim, hcl_errnum_t* errnum)
{
	hcl_server_t* server;
	hcl_t* hcl;
	hcl_vmprim_t vmprim;
	dummy_hcl_xtn_t* xtn;

	server = (hcl_server_t*)HCL_MMGR_ALLOC(mmgr, HCL_SIZEOF(*server) + xtnsize);
	if (!server) 
	{
		if (errnum) *errnum = HCL_ESYSMEM;
		return HCL_NULL;
	}

	HCL_MEMSET (&vmprim, 0, HCL_SIZEOF(vmprim));
	vmprim.log_write = log_write_for_dummy;
	vmprim.syserrstrb = syserrstrb;
	vmprim.dl_open = dl_open;
	vmprim.dl_close = dl_close;
	vmprim.dl_getsym = dl_getsym;
	vmprim.vm_gettime = vm_gettime;
	vmprim.vm_sleep = vm_sleep;

	hcl = hcl_open(mmgr, HCL_SIZEOF(*xtn), 10240, &vmprim, errnum);
	if (!hcl) 
	{
		HCL_MMGR_FREE (mmgr, server);
		return HCL_NULL;
	}

	xtn = (dummy_hcl_xtn_t*)hcl_getxtn(hcl);
	xtn->server = server;

	HCL_MEMSET (server, 0, HCL_SIZEOF(*server) + xtnsize);
	server->mmgr = mmgr;
	server->cmgr = hcl_get_utf8_cmgr();
	server->prim = *prim;
	server->dummy_hcl = hcl;

	server->cfg.logmask = ~0u;
	server->cfg.worker_stack_size = 512000UL; 
	server->cfg.actor_heap_size = 512000UL;

	pthread_mutex_init (&server->worker_mutex, HCL_NULL);
	pthread_mutex_init (&server->log_mutex, HCL_NULL);


	return server;
}

void hcl_server_close (hcl_server_t* server)
{
	pthread_mutex_destroy (&server->log_mutex);
	pthread_mutex_destroy (&server->worker_mutex);

	hcl_close (server->dummy_hcl);
	HCL_MMGR_FREE (server->mmgr, server);
}

static hcl_server_worker_t* alloc_worker (hcl_server_t*  server, int cli_sck)
{
	hcl_server_worker_t* worker;
	
	worker = (hcl_server_worker_t*)HCL_MMGR_ALLOC (server->mmgr, HCL_SIZEOF(*worker));
	if (!worker) return HCL_NULL;

	HCL_MEMSET (worker, 0, HCL_SIZEOF(*worker));
	worker->sck = cli_sck;
	worker->server = server;
	return worker;
}

static void free_worker (hcl_server_worker_t* worker)
{
	if (worker->sck >= 0) close (worker->sck);
	HCL_MMGR_FREE (worker->server->mmgr, worker);
}

static void add_hcl_server_worker_to_server (hcl_server_t* server, hcl_server_worker_state_t wstate, hcl_server_worker_t* worker)
{
	HCL_ASSERT (server->dummy_hcl, worker->server == server);

	if (server->worker_list[wstate].tail)
	{
		server->worker_list[wstate].tail->next_worker = worker;
		worker->prev_worker = server->worker_list[wstate].tail;
		server->worker_list[wstate].tail = worker;
		worker->next_worker = HCL_NULL;
	}
	else
	{
		server->worker_list[wstate].tail = worker;
		server->worker_list[wstate].head = worker;
		worker->prev_worker = HCL_NULL;
		worker->next_worker = HCL_NULL;
	}

	worker->state = wstate;
}

static void zap_worker_in_server (hcl_server_t* server, hcl_server_worker_t* worker)
{
	hcl_server_worker_state_t wstate;
	
	HCL_ASSERT (server->dummy_hcl, worker->server == server);

	wstate = worker->state;
	if (worker->prev_worker) worker->prev_worker->next_worker = worker->next_worker;
	else server->worker_list[wstate].head = worker->next_worker;
	if (worker->next_worker) worker->next_worker->prev_worker = worker->prev_worker;
	else server->worker_list[wstate].tail = worker->prev_worker;

	worker->prev_worker = HCL_NULL;
	worker->next_worker = HCL_NULL;
}

static void* worker_main (void* ctx)
{
	hcl_server_worker_t* worker = (hcl_server_worker_t*)ctx;
	hcl_server_t* server = worker->server;
	sigset_t set;

	sigfillset (&set);
	pthread_sigmask (SIG_BLOCK, &set, HCL_NULL);

	worker->thr = pthread_self();
	worker->proto = hcl_server_proto_open(0, worker);
	if (!worker->proto)
	{
		free_worker (worker);
		return HCL_NULL;
	}

	pthread_mutex_lock (&server->worker_mutex);
	add_hcl_server_worker_to_server (server, HCL_SERVER_WORKER_STATE_ALIVE, worker);
	pthread_mutex_unlock (&server->worker_mutex);

	while (!server->stopreq)
	{
		if (hcl_server_proto_handle_request(worker->proto) <= 0) break;
	}

	hcl_server_proto_close (worker->proto);
	worker->proto = HCL_NULL;

	pthread_mutex_lock (&server->worker_mutex);

	/* close connection before free_client() is called */
	close (worker->sck);
	worker->sck = -1;

	if (!worker->claimed)
	{
		zap_worker_in_server (server, worker);
		add_hcl_server_worker_to_server (server, HCL_SERVER_WORKER_STATE_DEAD, worker);
	}
	pthread_mutex_unlock (&server->worker_mutex);

	return HCL_NULL;
}

static void purge_all_workers (hcl_server_t* server, hcl_server_worker_state_t wstate)
{
	hcl_server_worker_t* worker;

	while (1)
	{
		pthread_mutex_lock (&server->worker_mutex);
		worker = server->worker_list[wstate].head;
		if (worker) 
		{
			zap_worker_in_server (server, worker);
			worker->claimed = 1;

			if (worker->sck >= 0) shutdown (worker->sck, SHUT_RDWR);
		}
		pthread_mutex_unlock (&server->worker_mutex);
		if (!worker) break;

		pthread_join (worker->thr, HCL_NULL);
		free_worker (worker);
	}
}

void hcl_server_logbfmt (hcl_server_t* server, unsigned int mask, const hcl_bch_t* fmt, ...)
{
	va_list ap;
	va_start (ap, fmt);
	hcl_logbfmtv (server->dummy_hcl, mask, fmt, ap);
	va_end (ap);
}

void hcl_server_logufmt (hcl_server_t* server, unsigned int mask, const hcl_uch_t* fmt, ...)
{
	va_list ap;
	va_start (ap, fmt);
	hcl_logufmtv (server->dummy_hcl, mask, fmt, ap);
	va_end (ap);
}


static void set_err_with_syserr (hcl_server_t* server, int syserr, const char* bfmt, ...)
{
	hcl_t* hcl = server->dummy_hcl;
	hcl_errnum_t errnum;
	hcl_oow_t tmplen, tmplen2;
	va_list ap;
	
	static hcl_bch_t b_dash[] = { ' ', '-', ' ', '\0' };
	static hcl_uch_t u_dash[] = { ' ', '-', ' ', '\0' };

	if (hcl->shuterr) return;

	errnum = hcl_syserr_to_errnum(syserr);
	if (hcl->vmprim.syserrstrb)
	{
		hcl->vmprim.syserrstrb (hcl, syserr, hcl->errmsg.tmpbuf.bch, HCL_COUNTOF(hcl->errmsg.tmpbuf.bch));
		
		va_start (ap, bfmt);
		hcl_seterrbfmtv (hcl, errnum, bfmt, ap);
		va_end (ap);

	#if defined(HCL_OOCH_IS_BCH)
		hcl->errmsg.len += hcl_copybcstr (&hcl->errmsg.buf[hcl->errmsg.len], HCL_COUNTOF(hcl->errmsg.buf) - hcl->errmsg.len, b_dash);
		hcl->errmsg.len += hcl_copybcstr (&hcl->errmsg.buf[hcl->errmsg.len], HCL_COUNTOF(hcl->errmsg.buf) - hcl->errmsg.len, hcl->errmsg.tmpbuf.bch);
	#else
		hcl->errmsg.len += hcl_copyucstr (&hcl->errmsg.buf[hcl->errmsg.len], HCL_COUNTOF(hcl->errmsg.buf) - hcl->errmsg.len, u_dash);
		tmplen = hcl_countbcstr(hcl->errmsg.tmpbuf.bch);
		tmplen2 = HCL_COUNTOF(hcl->errmsg.buf) - hcl->errmsg.len;
		hcl_convbtouchars (hcl, hcl->errmsg.tmpbuf.bch, &tmplen, &hcl->errmsg.buf[hcl->errmsg.len], &tmplen2);
		hcl->errmsg.len += tmplen2; /* ignore conversion errors */
	#endif
	}
	else
	{
		HCL_ASSERT (hcl, hcl->vmprim.syserrstru != HCL_NULL);

		hcl->vmprim.syserrstru (hcl, syserr, hcl->errmsg.tmpbuf.uch, HCL_COUNTOF(hcl->errmsg.tmpbuf.uch));

		va_start (ap, bfmt);
		hcl_seterrbfmtv (hcl, errnum, bfmt, ap);
		va_end (ap);

	#if defined(HCL_OOCH_IS_BCH)
		hcl->errmsg.len += hcl_copybcstr (&hcl->errmsg.buf[hcl->errmsg.len], HCL_COUNTOF(hcl->errmsg.buf) - hcl->errmsg.len, b_dash);
		
		tmplen = hcl_countucstr(hcl->errmsg.tmpbuf.uch);
		tmplen2 = HCL_COUNTOF(hcl->errmsg.buf) - hcl->errmsg.len;
		hcl_convutobchars (hcl, hcl->errmsg.tmpbuf.uch, &tmplen, &hcl->errmsg.buf[hcl->errmsg.len], &tmplen2);
		hcl->errmsg.len += tmplen2; /* ignore conversion errors */
	#else
		hcl->errmsg.len += hcl_copyucstr (&hcl->errmsg.buf[hcl->errmsg.len], HCL_COUNTOF(hcl->errmsg.buf) - hcl->errmsg.len, u_dash);
		hcl->errmsg.len += hcl_copyucstr (&hcl->errmsg.buf[hcl->errmsg.len], HCL_COUNTOF(hcl->errmsg.buf) - hcl->errmsg.len, hcl->errmsg.tmpbuf.uch);
	#endif

	}

	server->errnum = errnum;
	hcl_copyoochars (server->errmsg.buf, server->dummy_hcl->errmsg.buf, HCL_COUNTOF(server->errmsg.buf));
	server->errmsg.len = server->dummy_hcl->errmsg.len;
}

int hcl_server_start (hcl_server_t* server, const hcl_bch_t* addrs)
{
	sockaddr_t srv_addr;
	int srv_fd, sck_fam;
	int optval;
	socklen_t srv_len;
	pthread_attr_t thr_attr;

/* TODO: interprete 'addrs' as a command-separated address list 
 *  192.168.1.1:20,[::1]:20,127.0.0.1:345
 */
	sck_fam = bchars_to_sockaddr(server, addrs, hcl_countbcstr(addrs), &srv_addr, &srv_len);
	if (sck_fam <= -1) return -1;

	srv_fd = socket(sck_fam, SOCK_STREAM, 0);
	if (srv_fd == -1)
	{
		set_err_with_syserr (server, errno, "unable to open server socket");
		return -1;
	}

	optval = 1;
	setsockopt (srv_fd, SOL_SOCKET, SO_REUSEADDR, &optval, HCL_SIZEOF(int));

	if (bind(srv_fd, (struct sockaddr*)&srv_addr, srv_len) == -1)
	{
		set_err_with_syserr (server, errno, "unable to bind server socket %d", srv_fd);
		close (srv_fd);
		return -1;
	}

	if (listen(srv_fd, 128) == -1)
	{
		set_err_with_syserr (server, errno, "unable to listen on server socket %d", srv_fd);
		close (srv_fd);
		return -1;
	}

	pthread_attr_init (&thr_attr);
	pthread_attr_setstacksize (&thr_attr, server->cfg.worker_stack_size);

	server->stopreq = 0;
	while (!server->stopreq)
	{
		sockaddr_t cli_addr;
		int cli_fd;
		socklen_t cli_len;
		pthread_t thr;

		hcl_server_worker_t* worker;

		purge_all_workers (server, HCL_SERVER_WORKER_STATE_DEAD);

		cli_len = HCL_SIZEOF(cli_addr);
		cli_fd = accept(srv_fd, (struct sockaddr*)&cli_addr, &cli_len);
		if (cli_fd == -1)
		{
			if (errno != EINTR || !server->stopreq) set_err_with_syserr (server, errno, "unable to accept worker on socket %d", srv_fd);
			break;
		}

		worker = alloc_worker(server, cli_fd);
		if (pthread_create(&thr, &thr_attr, worker_main, worker) != 0)
		{
			free_worker (worker);
		}
	}

	purge_all_workers (server, HCL_SERVER_WORKER_STATE_ALIVE);
	purge_all_workers (server, HCL_SERVER_WORKER_STATE_DEAD);

	pthread_attr_destroy (&thr_attr);
	return 0;
}

void hcl_server_stop (hcl_server_t* server)
{
	server->stopreq = 1;
}

int hcl_server_setoption (hcl_server_t* server, hcl_server_option_t id, const void* value)
{
	switch (id)
	{
		case HCL_SERVER_TRAIT:
			server->cfg.trait = *(const unsigned int*)value;
			return 0;

		case HCL_SERVER_LOG_MASK:
			server->cfg.logmask = *(const unsigned int*)value;
			return 0;

		case HCL_SERVER_WORKER_STACK_SIZE:
			server->cfg.worker_stack_size = *(hcl_oow_t*)value;
			return 0;
			
		case HCL_SERVER_WORKER_IDLE_TIMEOUT:
			server->cfg.worker_idle_timeout = *(hcl_ntime_t*)value;
			return 0;

		case HCL_SERVER_ACTOR_HEAP_SIZE:
			server->cfg.actor_heap_size = *(hcl_oow_t*)value;
			return 0;
			
		case HCL_SERVER_ACTOR_MAX_RUNTIME:
			server->cfg.actor_max_runtime = *(hcl_ntime_t*)value;
			return 0;
	}

	hcl_server_seterrnum (server, HCL_EINVAL);
	return -1;
}

int hcl_server_getoption (hcl_server_t* server, hcl_server_option_t id, void* value)
{
	switch (id)
	{
		case HCL_SERVER_TRAIT:
			*(unsigned int*)value = server->cfg.trait;
			return 0;

		case HCL_SERVER_LOG_MASK:
			*(unsigned int*)value = server->cfg.logmask;
			return 0;

		case HCL_SERVER_WORKER_STACK_SIZE:
			*(hcl_oow_t*)value = server->cfg.worker_stack_size;
			return 0;
			
		case HCL_SERVER_WORKER_IDLE_TIMEOUT:
			*(hcl_ntime_t*)value = server->cfg.worker_idle_timeout;
			return 0;

		case HCL_SERVER_ACTOR_HEAP_SIZE:
			*(hcl_oow_t*)value = server->cfg.actor_heap_size;
			return 0;

		case HCL_SERVER_ACTOR_MAX_RUNTIME:
			*(hcl_ntime_t*)value = server->cfg.actor_max_runtime;
			return 0;
	};

	hcl_server_seterrnum (server, HCL_EINVAL);
	return -1;
}

void* hcl_server_getxtn (hcl_server_t* server)
{
	return (void*)(server + 1);
}

hcl_mmgr_t* hcl_server_getmmgr (hcl_server_t* server)
{
	return server->mmgr;
}

hcl_cmgr_t* hcl_server_getcmgr (hcl_server_t* server)
{
	return server->cmgr;
}

void hcl_server_setcmgr (hcl_server_t* server, hcl_cmgr_t* cmgr)
{
	server->cmgr = cmgr;
}

hcl_errnum_t hcl_server_geterrnum (hcl_server_t* server)
{
	return server->errnum;
}

const hcl_ooch_t* hcl_server_geterrstr (hcl_server_t* server)
{
	return hcl_errnum_to_errstr(server->errnum);
}

const hcl_ooch_t* hcl_server_geterrmsg (hcl_server_t* server)
{
	if (server->errmsg.len <= 0) return hcl_errnum_to_errstr(server->errnum);
	return server->errmsg.buf;
}

void hcl_server_seterrnum (hcl_server_t* server, hcl_errnum_t errnum)
{
	/*if (server->shuterr) return; */
	server->errnum = errnum;
	server->errmsg.len = 0;
}

void hcl_server_seterrbfmt (hcl_server_t* server, hcl_errnum_t errnum, const hcl_bch_t* fmt, ...)
{
	va_list ap;

	va_start (ap, fmt);
	hcl_seterrbfmtv (server->dummy_hcl, errnum, fmt, ap);
	va_end (ap);

	HCL_ASSERT (server->dummy_hcl, HCL_COUNTOF(server->errmsg.buf) == HCL_COUNTOF(server->dummy_hcl->errmsg.buf));
	server->errnum = errnum;
	hcl_copyoochars (server->errmsg.buf, server->dummy_hcl->errmsg.buf, HCL_COUNTOF(server->errmsg.buf));
	server->errmsg.len = server->dummy_hcl->errmsg.len;
}

void hcl_server_seterrufmt (hcl_server_t* server, hcl_errnum_t errnum, const hcl_uch_t* fmt, ...)
{
	va_list ap;

	va_start (ap, fmt);
	hcl_seterrufmtv (server->dummy_hcl, errnum, fmt, ap);
	va_end (ap);

	HCL_ASSERT (server->dummy_hcl, HCL_COUNTOF(server->errmsg.buf) == HCL_COUNTOF(server->dummy_hcl->errmsg.buf));
	server->errnum = errnum;
	hcl_copyoochars (server->errmsg.buf, server->dummy_hcl->errmsg.buf, HCL_COUNTOF(server->errmsg.buf));
	server->errmsg.len = server->dummy_hcl->errmsg.len;
}
