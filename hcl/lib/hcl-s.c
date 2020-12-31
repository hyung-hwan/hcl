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
#include "hcl-tmr.h"
#include "hcl-xutl.h"
#include "cb-impl.h"

#include <stdio.h>
#include <string.h>
#include <errno.h>

#define HCL_SERVER_TOKEN_NAME_ALIGN 64
#define HCL_SERVER_WID_MAP_ALIGN 512
#define HCL_SERVER_PROTO_REPLY_BUF_SIZE 1300

#if defined(_WIN32)
#	include <windows.h>
#	include <tchar.h>
#	if defined(HCL_HAVE_CFG_H) && defined(HCL_ENABLE_LIBLTDL)
#		include <ltdl.h>
#		define USE_LTDL
#	else
#		define USE_WIN_DLL
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
#	elif defined(HAVE_DLFCN_H)
#		include <dlfcn.h>
#		define USE_DLFCN
#	elif defined(__APPLE__) || defined(__MACOSX__)
#		define USE_MACH_O_DYLD
#		include <mach-o/dyld.h>
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
#	if defined(HAVE_SYS_UIO_H)
#		include <sys/uio.h>
#	endif
#	if defined(HAVE_SYS_EPOLL_H)
#		include <sys/epoll.h>
#	endif

#	include <unistd.h>
#	include <fcntl.h>
#	include <sys/types.h>
#	include <sys/socket.h>
#	include <netinet/in.h>
#	include <pthread.h>
#	include <poll.h>
#endif

#if !defined(HCL_DEFAULT_PFMODDIR)
#	define HCL_DEFAULT_PFMODDIR ""
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
#		elif defined(USE_MACH_O_DYLD)
#			define HCL_DEFAULT_PFMODPOSTFIX ".dylib"
#		else
#			define HCL_DEFAULT_PFMODPOSTFIX ""
#		endif
#	endif
#endif

struct bb_t
{
	char buf[1024];
	hcl_oow_t pos;
	hcl_oow_t len;
	int fd;
	hcl_bch_t* fn;
};
typedef struct bb_t bb_t;

struct worker_hcl_xtn_t
{
	hcl_server_proto_t* proto;
	int vm_running;
};
typedef struct worker_hcl_xtn_t worker_hcl_xtn_t;

struct server_hcl_xtn_t
{
	hcl_server_t* server;
};
typedef struct server_hcl_xtn_t server_hcl_xtn_t;

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

	HCL_SERVER_PROTO_TOKEN_IDENT,
	HCL_SERVER_PROTO_TOKEN_NUMBER
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
	hcl_tmr_index_t exec_runtime_event_index;

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
	HCL_SERVER_WORKER_STATE_ALIVE = 1,
	HCL_SERVER_WORKER_STATE_ZOMBIE = 2 /* the worker is not chained in the server's client list */
};
typedef enum hcl_server_worker_state_t hcl_server_worker_state_t;

enum hcl_server_worker_opstate_t
{
	HCL_SERVER_WORKER_OPSTATE_IDLE = 0,
	HCL_SERVER_WORKER_OPSTATE_ERROR = 1,
	HCL_SERVER_WORKER_OPSTATE_WAIT = 2,
	HCL_SERVER_WORKER_OPSTATE_READ = 3,
	HCL_SERVER_WORKER_OPSTATE_COMPILE = 4,
	HCL_SERVER_WORKER_OPSTATE_EXECUTE = 5
};
typedef enum hcl_server_worker_opstate_t hcl_server_worker_opstate_t;

struct hcl_server_worker_t
{
	pthread_t thr;
	hcl_oow_t wid;

	int sck;
	hcl_sckaddr_t peeraddr;

	int claimed;
	
	hcl_ntime_t alloc_time;
	hcl_server_worker_state_t state;
	hcl_server_worker_opstate_t opstate;
	hcl_server_proto_t* proto;

	hcl_server_t* server;
	hcl_server_worker_t* prev_worker;
	hcl_server_worker_t* next_worker;
};

struct hcl_server_wid_map_data_t
{
	int used;
	union
	{
		hcl_server_worker_t* worker;
		hcl_oow_t            next;
	} u;
};
typedef struct hcl_server_wid_map_data_t hcl_server_wid_map_data_t;

typedef struct hcl_server_listener_t hcl_server_listener_t;
struct hcl_server_listener_t
{
	int sck;
	hcl_sckaddr_t sckaddr;
	hcl_server_listener_t* next_listener;
};

struct hcl_server_t
{
	hcl_oow_t    _instsize;
	hcl_mmgr_t*  _mmgr;
	hcl_cmgr_t*  _cmgr;
	hcl_server_prim_t prim;

	/* [NOTE]
	 *  this dummy_hcl is used when the main thread requires logging mostly.
	 *  as there is no explicit locking when calling HCL_LOG() functions,
	 *  the code must ensure that the logging functions are called in the
	 *  context of the main server thraed only.  error message setting is
	 *  also performed in the main thread context for the same reason.
	 * 
	 *  however, you may have noticed mixed use of HCL_ASSERT with dummy_hcl
	 *  in both the server thread context and the client thread contexts.
	 *  it should be ok as assertion is only for debugging and it's operation
	 *  is thread safe. */
	hcl_t* dummy_hcl;

	hcl_tmr_t* tmr;

	hcl_errnum_t errnum;
	struct
	{
		hcl_ooch_t buf[HCL_ERRMSG_CAPA];
		hcl_oow_t len;
	} errmsg;
	int stopreq;

	struct
	{
		hcl_bitmask_t trait;
		hcl_bitmask_t logmask;
		hcl_oow_t worker_stack_size;
		hcl_oow_t worker_max_count;
		hcl_ntime_t worker_idle_timeout;
		hcl_oow_t actor_heap_size;
		hcl_ntime_t actor_max_runtime;
		hcl_ooch_t script_include_path[HCL_PATH_MAX + 1];
		void* module_inctx;
	} cfg;

	struct
	{
		int ep_fd;
		struct epoll_event ev_buf[128];
		hcl_server_listener_t* head;
		hcl_oow_t count;
	} listener;

	struct
	{
		hcl_server_worker_t* head;
		hcl_server_worker_t* tail;
		hcl_oow_t count;
	} worker_list[2];

	struct
	{
		hcl_server_wid_map_data_t* ptr;
		hcl_oow_t                  capa;
		hcl_oow_t                  free_first;
		hcl_oow_t                  free_last;
	} wid_map; /* worker's id map */

	int mux_pipe[2]; /* pipe to break the blocking multiplexer in the main server loop */

	pthread_mutex_t worker_mutex;
	pthread_mutex_t tmr_mutex;
	pthread_mutex_t log_mutex;
};

/* ========================================================================= */

#if defined(_WIN32) || defined(__OS2__) || defined(__DOS__)
#	define HCL_IS_PATH_SEP(c) ((c) == '/' || (c) == '\\')
#	define PATH_SEP_CHAR ('\\')
#else
#	define HCL_IS_PATH_SEP(c) ((c) == '/')
#	define PATH_SEP_CHAR ('/')
#endif

/* TODO: handle path with a drive letter or in the UNC notation */
#define HCL_IS_PATH_ABSOLUTE(x) HCL_IS_PATH_SEP(x[0])


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
	worker_hcl_xtn_t* xtn = (worker_hcl_xtn_t*)hcl_getxtn(hcl);
	bb_t* bb = HCL_NULL;
	hcl_server_t* server;

	server = xtn->proto->worker->server;

	if (arg->includer)
	{
		/* includee */

		/* TOOD: Do i need to skip prepending the include path if the included path is an absolute path? 
		 *       it may be good for security if i don't skip it. we can lock the included files in a given directory */
		hcl_oow_t ucslen, bcslen, parlen;
		const hcl_bch_t* fn, * fb;

	#if defined(HCL_OOCH_IS_UCH)
		if (hcl_convootobcstr(hcl, arg->name, &ucslen, HCL_NULL, &bcslen) <= -1) goto oops;
	#else
		bcslen = hcl_count_bcstr(arg->name);
	#endif

		fn = ((bb_t*)arg->includer->handle)->fn;
		if (fn[0] == '\0' && server->cfg.script_include_path[0] != '\0')
		{
		#if defined(HCL_OOCH_IS_UCH)
			if (hcl_convootobcstr(hcl, server->cfg.script_include_path, &ucslen, HCL_NULL, &parlen) <= -1) goto oops;
		#else
			parlen = hcl_count_bcstr(server->cfg.script_include_path);
		#endif
		}
		else
		{
			fb = get_base_name(fn);
			parlen = fb - fn;
		}

		bb = (bb_t*)hcl_callocmem(hcl, HCL_SIZEOF(*bb) + (HCL_SIZEOF(hcl_bch_t) * (parlen + bcslen + 2)));
		if (!bb) goto oops;

		bb->fn = (hcl_bch_t*)(bb + 1);
		if (fn[0] == '\0' && server->cfg.script_include_path[0] != '\0')
		{
		#if defined(HCL_OOCH_IS_UCH)
			hcl_convootobcstr (hcl, server->cfg.script_include_path, &ucslen, bb->fn, &parlen);
		#else
			hcl_copy_bchars (bb->fn, server->cfg.script_include_path, parlen);
		#endif
			if (!HCL_IS_PATH_SEP(bb->fn[parlen])) bb->fn[parlen++] = PATH_SEP_CHAR; /* +2 was used in hcl_callocmem() for this (+1 for this, +1 for '\0' */
		}
		else
		{
			hcl_copy_bchars (bb->fn, fn, parlen);
		}

	#if defined(HCL_OOCH_IS_UCH)
		hcl_convootobcstr (hcl, arg->name, &ucslen, &bb->fn[parlen], &bcslen);
	#else
		hcl_copy_bcstr (&bb->fn[parlen], bcslen + 1, arg->name);
	#endif
		bb->fd = open(bb->fn, O_RDONLY, 0);
	}
	else
	{
		/* main stream */
		hcl_oow_t pathlen = 0;
		bb = (bb_t*)hcl_callocmem(hcl, HCL_SIZEOF(*bb) + (HCL_SIZEOF(hcl_bch_t) * (pathlen + 1)));
		if (!bb) goto oops;

		/* copy ane empty string as a main stream's name */
		bb->fn = (hcl_bch_t*)(bb + 1);
		hcl_copy_bcstr (bb->fn, pathlen + 1, "");

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
	hcl_server_worker_t* worker;
	int x;

	bb = (bb_t*)arg->handle;
	HCL_ASSERT (hcl, bb != HCL_NULL && bb->fd >= 0);

	worker = xtn->proto->worker;

	if (bb->fd == worker->sck)
	{
		ssize_t x;
		hcl_server_t* server;

		server = worker->server;

	start_over:
		while (1)
		{
			int n;
			struct pollfd pfd;
			int tmout, actual_tmout;

			if (server->stopreq)
			{
				hcl_seterrbfmt (hcl, HCL_EGENERIC, "stop requested");
				return -1;
			}

			tmout = HCL_SECNSEC_TO_MSEC(server->cfg.worker_idle_timeout.sec, server->cfg.worker_idle_timeout.nsec);
			actual_tmout = (tmout <= 0)? 10000: tmout;

			pfd.fd = bb->fd;
			pfd.events = POLLIN | POLLERR;
			n = poll(&pfd, 1, actual_tmout);
			if (n <= -1)
			{
				if (errno == EINTR) goto start_over;
				hcl_seterrwithsyserr (hcl, 0, errno);
				return -1;
			}
			else if (n >= 1) break;

			/* timed out - no activity on the pfd */
			if (tmout > 0)
			{
				hcl_seterrbfmt (hcl, HCL_EGENERIC, "no activity on the worker socket %d", worker->sck);
				return -1;
			}
		}
		
		x = recv(bb->fd, &bb->buf[bb->len], HCL_COUNTOF(bb->buf) - bb->len, 0);
		if (x <= -1)
		{
			if (errno == EINTR) goto start_over;
			hcl_seterrwithsyserr (hcl, 0, errno);
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

static void log_write (hcl_t* hcl, hcl_bitmask_t mask, const hcl_ooch_t* msg, hcl_oow_t len)
{
	worker_hcl_xtn_t* xtn = (worker_hcl_xtn_t*)hcl_getxtn(hcl);
	hcl_server_t* server;

	server = xtn->proto->worker->server;
	pthread_mutex_lock (&server->log_mutex);
	server->prim.log_write (server, xtn->proto->worker->wid, mask, msg, len);
	pthread_mutex_unlock (&server->log_mutex);
}

static void log_write_for_dummy (hcl_t* hcl, hcl_bitmask_t mask, const hcl_ooch_t* msg, hcl_oow_t len)
{
	server_hcl_xtn_t* xtn = (server_hcl_xtn_t*)hcl_getxtn(hcl);
	hcl_server_t* server;

	server = xtn->server;
	pthread_mutex_lock (&server->log_mutex);
	server->prim.log_write (server, HCL_SERVER_WID_INVALID, mask, msg, len);
	pthread_mutex_unlock (&server->log_mutex);
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

#define SERVER_LOGMASK_INFO (HCL_LOG_INFO | HCL_LOG_APP)
#define SERVER_LOGMASK_ERROR (HCL_LOG_ERROR | HCL_LOG_APP)

hcl_server_proto_t* hcl_server_proto_open (hcl_oow_t xtnsize, hcl_server_worker_t* worker)
{
	hcl_server_proto_t* proto;
	hcl_vmprim_t vmprim;
	hcl_cb_t hclcb;
	worker_hcl_xtn_t* xtn;
	hcl_bitmask_t trait;

	HCL_MEMSET (&vmprim, 0, HCL_SIZEOF(vmprim));
	if (worker->server->cfg.trait & HCL_SERVER_TRAIT_USE_LARGE_PAGES)
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
	vmprim.vm_gettime = hcl_vmprim_vm_gettime;
	vmprim.vm_sleep = hcl_vmprim_vm_sleep;

	proto = (hcl_server_proto_t*)hcl_server_allocmem(worker->server, HCL_SIZEOF(*proto));
	if (!proto) return HCL_NULL;

	HCL_MEMSET (proto, 0, HCL_SIZEOF(*proto));
	proto->worker = worker;
	proto->exec_runtime_event_index = HCL_TMR_INVALID_INDEX;

	proto->hcl = hcl_open(hcl_server_getmmgr(proto->worker->server), HCL_SIZEOF(*xtn), worker->server->cfg.actor_heap_size, &vmprim, HCL_NULL);
	if (!proto->hcl) goto oops;

	xtn = (worker_hcl_xtn_t*)hcl_getxtn(proto->hcl);
	xtn->proto = proto;

	hcl_setoption (proto->hcl, HCL_MOD_INCTX, &proto->worker->server->cfg.module_inctx);
	hcl_setoption (proto->hcl, HCL_LOG_MASK, &proto->worker->server->cfg.logmask);
	hcl_setcmgr (proto->hcl, hcl_server_getcmgr(proto->worker->server));

	hcl_getoption (proto->hcl, HCL_TRAIT, &trait);
#if defined(HCL_BUILD_DEBUG)
	if (proto->worker->server->cfg.trait & HCL_SERVER_TRAIT_DEBUG_GC) trait |= HCL_TRAIT_DEBUG_GC;
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
		hcl_server_freemem (proto->worker->server, proto);
	}
	return HCL_NULL;
}

void hcl_server_proto_close (hcl_server_proto_t* proto)
{
	if (proto->tok.ptr) hcl_server_freemem (proto->worker->server, proto->tok.ptr);
	hcl_close (proto->hcl);
	hcl_server_freemem (proto->worker->server, proto);
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
			iov[count].iov_base = (void*)".OK\n.DATA chunked\n";
			iov[count++].iov_len = 18;
		}

		iov[count].iov_base = cl,
		iov[count++].iov_len = snprintf(cl, HCL_SIZEOF(cl), "%zu:", proto->reply.len); 
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
			HCL_LOG2 (proto->hcl, SERVER_LOGMASK_ERROR, "Unable to sendmsg on %d - %hs\n", proto->worker->sck, strerror(errno));
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
#if defined(HCL_OOCH_IS_UCH)
	hcl_oow_t bcslen, ucslen, donelen;
	int x;

	donelen = 0;
	while (donelen < len)
	{
		if (escape)
		{
			if (ptr[donelen] == '\\' || ptr[donelen] == '\"')
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
			ucslen = 1; /* i must go one by one for escaping */
		}
		else
		{
			bcslen = HCL_COUNTOF(proto->reply.buf) - proto->reply.len;
			if (bcslen < HCL_BCSIZE_MAX)
			{
				if (write_reply_chunk(proto) <=-1) return -1;
				bcslen = HCL_COUNTOF(proto->reply.buf) - proto->reply.len;
			}
			ucslen = len - donelen;
		}

		x = hcl_convootobchars(proto->hcl, &ptr[donelen], &ucslen, &proto->reply.buf[proto->reply.len], &bcslen);
		if (x <= -1 && ucslen <= 0) return -1;

		donelen += ucslen;
		proto->reply.len += bcslen;
	}

	return 0;
#else
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
#endif
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
			    hcl_server_proto_feed_reply(proto, failmsg, hcl_count_oocstr(failmsg), 1) <= -1 ||
			    hcl_server_proto_feed_reply(proto, err2, 2, 0) <= -1) return -1;

			if (write_reply_chunk(proto) <= -1) return -1;
		}
		else 
		{
			/* some chunks have beed emitted. but at the end, an error has occurred.
			 * send -1: as the last chunk. the receiver must rub out the reply
			 * buffer received so far and expect the following .ERROR response */
			static hcl_ooch_t err0[] = { '-','1',':' };
			if (proto->reply.len > 0 && write_reply_chunk(proto) <= -1) return -1;

			proto->reply.type = HCL_SERVER_PROTO_REPLY_SIMPLE; /* switch to the simple mode forcibly */
			proto->reply.nchunks = 0;
			proto->reply.len = 0;

			if (hcl_server_proto_feed_reply(proto, err0, 3, 0) <= -1) return -1;
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

static HCL_INLINE int is_alphachar (hcl_ooci_t c)
{
/* TODO: support full unicode */
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

static HCL_INLINE int is_digitchar (hcl_ooci_t c)
{
/* TODO: support full unicode */
	return (c >= '0' && c <= '9');
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

#define GET_CHAR_TO_WITH_GOTO(proto,c,oops) \
	do { \
		if (read_char(proto) <= -1) goto oops; \
		c = (proto)->lxc->c; \
	} while(0)

#define UNGET_LAST_CHAR(proto) \
	do { \
		if (unread_last_char(proto) <= -1) return -1; \
	} while (0)

#define SET_TOKEN_TYPE(proto,tv) ((proto)->tok.type = (tv))
#define ADD_TOKEN_CHAR(proto,c) \
	do { if (add_token_char(proto, c) <= -1) return -1; } while (0)


static HCL_INLINE int add_token_char (hcl_server_proto_t* proto, hcl_ooch_t c)
{
	if (proto->tok.len >= proto->tok.capa)
	{
		hcl_ooch_t* tmp;
		hcl_oow_t capa;

		capa = HCL_ALIGN_POW2(proto->tok.len + 1, HCL_SERVER_TOKEN_NAME_ALIGN);
		tmp = (hcl_ooch_t*)hcl_server_reallocmem(proto->worker->server, proto->tok.ptr, capa * HCL_SIZEOF(*tmp));
		if (!tmp) 
		{
			hcl_seterrbfmt (proto->hcl, HCL_ESYSMEM, "Out of memory in allocating token buffer");
			return -1;
		}

		proto->tok.ptr = tmp;
		proto->tok.capa = capa;
	}

	proto->tok.ptr[proto->tok.len++] = c;
	return 0;
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
		if (hcl_comp_oochars_oocstr(proto->tok.ptr, proto->tok.len, tab[i].name) == 0) 
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
	proto->tok.loc = proto->lxc->l; /* set token location */

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
				hcl_seterrbfmt (proto->hcl, HCL_EINVAL, "Alphabetic character expected after a period");
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
			if (is_digitchar(c))
			{
				SET_TOKEN_TYPE (proto, HCL_SERVER_PROTO_TOKEN_NUMBER);

				do
				{
					ADD_TOKEN_CHAR(proto, c);
					GET_CHAR_TO(proto, c);
				}
				while (is_digitchar(c));
				
				UNGET_LAST_CHAR (proto);
				break;
			}

			hcl_seterrbfmt (proto->hcl, HCL_EINVAL, "Unrecognized character - [%jc]", c);
			return -1;
	}
	return 0;
}

static void exec_runtime_handler (hcl_tmr_t* tmr, const hcl_ntime_t* now, hcl_tmr_event_t* evt)
{
	/* [NOTE] this handler is executed in the main server thread 
	 *         when it calls hcl_tmr_fire().  */

	hcl_server_proto_t* proto;
	proto = (hcl_server_proto_t*)evt->ctx;

	HCL_LOG1 (proto->worker->server->dummy_hcl, SERVER_LOGMASK_INFO, "Aborting script execution for max_actor_runtime exceeded [%zu]\n", proto->worker->wid);
	hcl_abort (proto->hcl);
}

static void exec_runtime_updater (hcl_tmr_t* tmr, hcl_tmr_index_t old_index, hcl_tmr_index_t new_index, hcl_tmr_event_t* evt)
{
	/* [NOTE] this handler is executed in the main server thread 
	 *        when it calls hcl_tmr_fire() */

	hcl_server_proto_t* proto;
	proto = (hcl_server_proto_t*)evt->ctx;
	HCL_ASSERT (proto->hcl, proto->exec_runtime_event_index == old_index);
	
	/* the event is being removed by hcl_tmr_fire() or by hcl_tmr_delete() 
	 * if new_index is HCL_TMR_INVALID_INDEX. it's being updated if not. */
	proto->exec_runtime_event_index = new_index;
}

static int insert_exec_timer (hcl_server_proto_t* proto, const hcl_ntime_t* tmout)
{
	/* [NOTE] this is executed in the worker thread */
	
	hcl_tmr_event_t event;
	hcl_tmr_index_t index;
	hcl_server_t* server;

	HCL_ASSERT (proto->hcl, proto->exec_runtime_event_index == HCL_TMR_INVALID_INDEX);

	server = proto->worker->server;

	HCL_MEMSET (&event, 0, HCL_SIZEOF(event));
	event.ctx = proto;
	proto->hcl->vmprim.vm_gettime (proto->hcl, &event.when);
	HCL_ADD_NTIME (&event.when, &event.when, tmout);
	event.handler = exec_runtime_handler;
	event.updater = exec_runtime_updater;

	pthread_mutex_lock (&server->tmr_mutex);
	index = hcl_tmr_insert(server->tmr, &event);
	proto->exec_runtime_event_index = index;
	if (index != HCL_TMR_INVALID_INDEX)
	{
		/* inform the server of timer event change */
		write (server->mux_pipe[1], "X", 1); /* don't care even if it fails */
	}
	pthread_mutex_unlock (&server->tmr_mutex);

	return (index == HCL_TMR_INVALID_INDEX)? -1: 0;
}

static void delete_exec_timer (hcl_server_proto_t* proto)
{
	/* [NOTE] this is executed in the worker thread. if the event has been fired
	 *        in the server thread, proto->exec_runtime_event_index should be 
	 *        HCL_TMR_INVALID_INDEX as set by exec_runtime_handler */
	hcl_server_t* server;

	server = proto->worker->server;

	pthread_mutex_lock (&server->tmr_mutex);
	if (proto->exec_runtime_event_index != HCL_TMR_INVALID_INDEX)
	{
		/* the event has not been fired yet. let's delete it 
		 * if it has been fired, the index it shall be HCL_TMR_INVALID_INDEX already */

		hcl_tmr_delete (server->tmr, proto->exec_runtime_event_index);
		HCL_ASSERT (proto->hcl, proto->exec_runtime_event_index == HCL_TMR_INVALID_INDEX);
		/*proto->exec_runtime_event_index = HCL_TMR_INVALID_INDEX;	*/
	}
	pthread_mutex_unlock (&server->tmr_mutex);
}

static int execute_script (hcl_server_proto_t* proto, const hcl_bch_t* trigger)
{
	hcl_oop_t obj;
	const hcl_ooch_t* failmsg = HCL_NULL;
	hcl_server_t* server;

	server = proto->worker->server;

	hcl_server_proto_start_reply (proto);
	if (server->cfg.actor_max_runtime.sec <= 0 && server->cfg.actor_max_runtime.sec <= 0)
	{
		obj = hcl_execute(proto->hcl);
		if (!obj) failmsg = hcl_geterrmsg(proto->hcl);
	}
	else
	{
		if (insert_exec_timer(proto, &server->cfg.actor_max_runtime) <= -1)
		{
			HCL_LOG0 (proto->hcl, SERVER_LOGMASK_ERROR, "Cannot start execution timer\n");
			hcl_seterrbfmt (proto->hcl, HCL_ESYSMEM, "cannot start execution timer");  /* i do this just to compose the error message  */
			failmsg = hcl_geterrmsg(proto->hcl);
		}
		else
		{
			obj = hcl_execute(proto->hcl);
			if (!obj) failmsg = hcl_geterrmsg(proto->hcl);
			delete_exec_timer (proto);
		}
	}

	if (hcl_server_proto_end_reply(proto, failmsg) <= -1) 
	{
		HCL_LOG1 (proto->hcl, SERVER_LOGMASK_ERROR, "Cannot finalize reply for %hs\n", trigger);
		return -1;
	}

	return 0;
}


static void send_error_message (hcl_server_proto_t* proto, const hcl_ooch_t* errmsg)
{
	hcl_server_proto_start_reply (proto);
	if (hcl_server_proto_end_reply(proto, errmsg) <= -1)
	{
		HCL_LOG1 (proto->hcl, SERVER_LOGMASK_ERROR, "Unable to send error message - %s\n", errmsg);
	}
}

static void show_server_workers (hcl_server_proto_t* proto)
{
	hcl_server_t* server;
	hcl_server_worker_t* w;

	server = proto->worker->server;
	pthread_mutex_lock (&server->worker_mutex);
	for (w = server->worker_list[HCL_SERVER_WORKER_STATE_ALIVE].head; w; w = w->next_worker)
	{
		/* TODO: implement this better... */
		hcl_prbfmt (proto->hcl, "%zu %d %d\n", w->wid, w->sck, 1000);
	}
	pthread_mutex_unlock (&server->worker_mutex);
}

static int kill_server_worker (hcl_server_proto_t* proto, hcl_oow_t wid)
{
	hcl_server_t* server;
	int xret = 0;

	server = proto->worker->server;
	pthread_mutex_lock (&server->worker_mutex);
	if (wid >= server->wid_map.capa)
	{
		hcl_server_seterrnum (server, HCL_ENOENT);
		xret = -1;
	}
	else
	{
		hcl_server_worker_t* worker;

		if (!server->wid_map.ptr[wid].used)
		{
			hcl_server_seterrnum (server, HCL_ENOENT);
			xret = -1;
		}
		else
		{
			worker = server->wid_map.ptr[wid].u.worker;
			if (!worker) 
			{
				hcl_server_seterrnum (server, HCL_ENOENT);
				xret = -1;
			}
			else
			{
				if (worker->sck) shutdown (worker->sck, SHUT_RDWR);
				if (worker->proto) hcl_abort (worker->proto->hcl);
			}
		}
	}
	pthread_mutex_unlock (&server->worker_mutex);
	return xret;
}

static void reformat_synerr (hcl_t* hcl)
{
	hcl_synerr_t synerr;
	const hcl_ooch_t* orgmsg;

	hcl_getsynerr (hcl, &synerr);

	orgmsg = hcl_backuperrmsg(hcl);
	hcl_seterrbfmt (
		hcl, HCL_ESYNERR,
		"%js%s%.*js at line %zu column %zu", 
		orgmsg,
		(synerr.tgt.len > 0? " near ": ""),
		synerr.tgt.len, synerr.tgt.ptr,
		synerr.loc.line, synerr.loc.colm
	);
}

int hcl_server_proto_handle_request (hcl_server_proto_t* proto)
{
	if (get_token(proto) <= -1) goto fail_with_errmsg;

	switch (proto->tok.type)
	{
		case HCL_SERVER_PROTO_TOKEN_NL:
			/* ignore new lines */
			break;

		case HCL_SERVER_PROTO_TOKEN_EOF:
			if (proto->req.state != HCL_SERVER_PROTO_REQ_IN_TOP_LEVEL)
			{
				hcl_seterrbfmt (proto->hcl, HCL_EINVAL, "Unexpected EOF without .END");
				goto fail_with_errmsg;
			}
			/* drop connection silently */
			return 0;

		case HCL_SERVER_PROTO_TOKEN_EXIT:
			if (proto->req.state != HCL_SERVER_PROTO_REQ_IN_TOP_LEVEL)
			{
				hcl_seterrbfmt (proto->hcl, HCL_EINVAL, ".EXIT allowed in the top level only");
				goto fail_with_errmsg;
			}

			if (get_token(proto) <= -1) goto fail_with_errmsg;
			if (proto->tok.type != HCL_SERVER_PROTO_TOKEN_NL)
			{
				hcl_seterrbfmt (proto->hcl, HCL_EINVAL, "No new line after .EXIT");
				goto fail_with_errmsg;
			}
			
			hcl_server_proto_start_reply (proto);
			if (hcl_server_proto_end_reply(proto, HCL_NULL) <= -1) 
			{
				HCL_LOG0 (proto->hcl, SERVER_LOGMASK_ERROR, "Unable to finalize reply for .EXIT\n");
				return -1;
			}
			return 0;

		case HCL_SERVER_PROTO_TOKEN_BEGIN:
			if (proto->req.state != HCL_SERVER_PROTO_REQ_IN_TOP_LEVEL)
			{
				hcl_seterrbfmt (proto->hcl, HCL_EINVAL, ".BEGIN not allowed to be nested");
				goto fail_with_errmsg;
			}

			if (get_token(proto) <= -1) goto fail_with_errmsg;
			if (proto->tok.type != HCL_SERVER_PROTO_TOKEN_NL)
			{
				hcl_seterrbfmt (proto->hcl, HCL_EINVAL, "No new line after .BEGIN");
				goto fail_with_errmsg;
			}

			proto->req.state = HCL_SERVER_PROTO_REQ_IN_BLOCK_LEVEL;
			hcl_reset (proto->hcl);
			break;

		case HCL_SERVER_PROTO_TOKEN_END:
			if (proto->req.state != HCL_SERVER_PROTO_REQ_IN_BLOCK_LEVEL)
			{
				hcl_seterrbfmt (proto->hcl, HCL_EINVAL, ".END without opening .BEGIN");
				goto fail_with_errmsg;
			}

			if (get_token(proto) <= -1) goto fail_with_errmsg;
			if (proto->tok.type != HCL_SERVER_PROTO_TOKEN_NL)
			{
				hcl_seterrbfmt (proto->hcl, HCL_EINVAL, "No new line after .END");
				goto fail_with_errmsg;
			}

			proto->worker->opstate = HCL_SERVER_WORKER_OPSTATE_EXECUTE;
			/* i must not jump to fail_with_errmsg when execute_script() fails.
			 * it may have produced some normal output already. so the function
			 * is supposed to handle an error in itself */
			if (execute_script(proto, ".END") <= -1) return -1; 
			proto->req.state = HCL_SERVER_PROTO_REQ_IN_TOP_LEVEL;
			break;

		case HCL_SERVER_PROTO_TOKEN_SCRIPT:
		{
			hcl_oop_t obj;
			hcl_ooci_t c;

			hcl_setinloc (proto->hcl, 1, 1);

			/* do a special check bypassing get_token(). it checks if the script contents
			 * come on the same line as .SCRIPT */
			GET_CHAR_TO_WITH_GOTO (proto, c, fail_with_errmsg);
			while (is_spacechar(c)) GET_CHAR_TO (proto, c);
			if (c == HCL_OOCI_EOF || c == '\n')
			{
				hcl_seterrbfmt (proto->hcl, HCL_EINVAL, "No contents on the .SCRIPT line");
				goto fail_with_errmsg;
			}
			UNGET_LAST_CHAR (proto);

			if (proto->req.state == HCL_SERVER_PROTO_REQ_IN_TOP_LEVEL) hcl_reset(proto->hcl);

			proto->worker->opstate = HCL_SERVER_WORKER_OPSTATE_READ;
			obj = hcl_read(proto->hcl);
			if (!obj)
			{
				if (hcl_geterrnum(proto->hcl) == HCL_ESYNERR) reformat_synerr (proto->hcl);
				goto fail_with_errmsg;
			}

			if (get_token(proto) <= -1) goto fail_with_errmsg;
			if (proto->tok.type != HCL_SERVER_PROTO_TOKEN_NL)
			{
				hcl_seterrbfmt (proto->hcl, HCL_EINVAL, "No new line after .SCRIPT contents");
				goto fail_with_errmsg;
			}

			proto->worker->opstate = HCL_SERVER_WORKER_OPSTATE_COMPILE;
			if (hcl_compile(proto->hcl, obj) <= -1)
			{
				if (hcl_geterrnum(proto->hcl) == HCL_ESYNERR) reformat_synerr (proto->hcl);
				goto fail_with_errmsg;
			}

			if (proto->req.state == HCL_SERVER_PROTO_REQ_IN_TOP_LEVEL)
			{
				proto->worker->opstate = HCL_SERVER_WORKER_OPSTATE_EXECUTE;
				if (execute_script(proto, ".SCRIPT") <= -1) return -1;
			}
			break;
		}

		case HCL_SERVER_PROTO_TOKEN_SHOW_WORKERS:
			if (proto->req.state != HCL_SERVER_PROTO_REQ_IN_TOP_LEVEL)
			{
				hcl_seterrbfmt (proto->hcl, HCL_EINVAL, ".SHOW-WORKERS not allowed to be nested");
				goto fail_with_errmsg;
			}

			if (get_token(proto) <= -1) goto fail_with_errmsg;
			if (proto->tok.type != HCL_SERVER_PROTO_TOKEN_NL)
			{
				hcl_seterrbfmt (proto->hcl, HCL_EINVAL, "No new line after .SHOW-WORKERS");
				goto fail_with_errmsg;
			}
			
			hcl_server_proto_start_reply (proto);
			proto->worker->opstate = HCL_SERVER_WORKER_OPSTATE_EXECUTE;
			show_server_workers (proto);
			if (hcl_server_proto_end_reply(proto, HCL_NULL) <= -1)
			{
				HCL_LOG0 (proto->hcl, SERVER_LOGMASK_ERROR, "Unable to finalize reply for .SHOW-WORKERS\n");
				return -1;
			}
			
			break;

		case HCL_SERVER_PROTO_TOKEN_KILL_WORKER:
		{
			int n;
			hcl_oow_t wid, wp;

			static hcl_ooch_t failmsg[] = { 'U','n','a','b','l','e',' ','t','o',' ','k','i','l','l','\0' };

			if (proto->req.state != HCL_SERVER_PROTO_REQ_IN_TOP_LEVEL)
			{
				hcl_seterrbfmt (proto->hcl, HCL_EINVAL, ".KILL-WORKER not allowed to be nested");
				goto fail_with_errmsg;
			}

			if (get_token(proto) <= -1) goto fail_with_errmsg;
			if (proto->tok.type != HCL_SERVER_PROTO_TOKEN_NUMBER)
			{
				hcl_seterrbfmt (proto->hcl, HCL_EINVAL, "No worker ID after .KILL-WORKER");
				goto fail_with_errmsg;
			}

			for (wid = 0, wp = 0; wp < proto->tok.len; wp++) wid = wid * 10 + (proto->tok.ptr[wp] - '0');

			if (get_token(proto) <= -1) goto fail_with_errmsg;
			if (proto->tok.type != HCL_SERVER_PROTO_TOKEN_NL)
			{
				hcl_seterrbfmt (proto->hcl, HCL_EINVAL, "No new line after worker ID of .KILL-WORKER");
				goto fail_with_errmsg;
			}

			hcl_server_proto_start_reply (proto);
			proto->worker->opstate = HCL_SERVER_WORKER_OPSTATE_EXECUTE;
			n = kill_server_worker(proto, wid);
			if (hcl_server_proto_end_reply(proto, (n <= -1? failmsg: HCL_NULL)) <= -1)
			{
				HCL_LOG0 (proto->hcl, SERVER_LOGMASK_ERROR, "Unable to finalize reply for .KILL-WORKER\n");
				return -1;
			}
			break;
		}

		default:
			hcl_seterrbfmt (proto->hcl, HCL_EINVAL, "Unknown token %.*js of type %d", proto->tok.len, proto->tok.ptr, (int)proto->tok.type);
			goto fail_with_errmsg;
	}

	return 1;

fail_with_errmsg:
	send_error_message (proto, hcl_geterrmsg(proto->hcl));
	HCL_LOG1 (proto->hcl, SERVER_LOGMASK_ERROR, "Unable to compile .SCRIPT contents - %js\n", hcl_geterrmsg(proto->hcl));
	return -1;
}

/* ========================================================================= */

hcl_server_t* hcl_server_open (hcl_mmgr_t* mmgr, hcl_oow_t xtnsize, hcl_server_prim_t* prim, hcl_errnum_t* errnum)
{
	hcl_server_t* server = HCL_NULL;
	hcl_t* hcl = HCL_NULL;
	hcl_vmprim_t vmprim;
	hcl_tmr_t* tmr = HCL_NULL;
	server_hcl_xtn_t* xtn;
	int pfd[2], fcv;
	hcl_bitmask_t trait;

	server = (hcl_server_t*)HCL_MMGR_ALLOC(mmgr, HCL_SIZEOF(*server) + xtnsize);
	if (!server) 
	{
		if (errnum) *errnum = HCL_ESYSMEM;
		return HCL_NULL;
	}

	HCL_MEMSET (&vmprim, 0, HCL_SIZEOF(vmprim));
	vmprim.log_write = log_write_for_dummy;
	vmprim.syserrstrb = hcl_vmprim_syserrstrb;
	vmprim.assertfail = hcl_vmprim_assertfail;
	vmprim.dl_startup = hcl_vmprim_dl_startup;
	vmprim.dl_cleanup = hcl_vmprim_dl_cleanup;
	vmprim.dl_open = hcl_vmprim_dl_open;
	vmprim.dl_close = hcl_vmprim_dl_close;
	vmprim.dl_getsym = hcl_vmprim_dl_getsym;
	vmprim.vm_gettime = hcl_vmprim_vm_gettime;
	vmprim.vm_sleep = hcl_vmprim_vm_sleep;

#if defined(USE_LTDL)
	lt_dlinit ();
#endif

	hcl = hcl_open(mmgr, HCL_SIZEOF(*xtn), 2048, &vmprim, errnum);
	if (!hcl) goto oops;

	tmr = hcl_tmr_open(hcl, 0, 1024); /* TOOD: make the timer's default size configurable */
	if (!tmr)
	{
		if (errnum) *errnum = HCL_ESYSMEM;
		goto oops;
	}

	if (pipe(pfd) <= -1)
	{
		if (errnum) *errnum = hcl_vmprim_syserrstrb(hcl, 0, errno, HCL_NULL, 0);
		goto oops;
	}

#if defined(O_NONBLOCK) || defined(O_CLOEXEC)
	fcv = fcntl(pfd[0], F_GETFD, 0);
	if (fcv >= 0) 
	{
	#if defined(O_NONBLOCK)
		fcv |= O_NONBLOCK;
	#endif
	#if defined(O_CLOEXEC)
		fcv |= O_CLOEXEC;
	#endif
		fcntl(pfd[0], F_SETFD, fcv);
	}

	fcv = fcntl(pfd[1], F_GETFD, 0);
	if (fcv >= 0) 
	{
	#if defined(O_NONBLOCK)
		fcv |= O_NONBLOCK;
	#endif
	#if defined(O_CLOEXEC)
		fcv |= O_CLOEXEC;
	#endif
		fcntl(pfd[1], F_SETFD, fcv);
	}
#endif
	
	xtn = (server_hcl_xtn_t*)hcl_getxtn(hcl);
	xtn->server = server;

	HCL_MEMSET (server, 0, HCL_SIZEOF(*server) + xtnsize);
	server->_instsize = HCL_SIZEOF(*server);
	server->_mmgr = mmgr;
	server->_cmgr = hcl_get_utf8_cmgr();
	server->prim = *prim;
	server->dummy_hcl = hcl;
	server->tmr = tmr;

	server->cfg.logmask = ~(hcl_bitmask_t)0;
	server->cfg.worker_stack_size = 512000UL; 
	server->cfg.actor_heap_size = 512000UL;

	HCL_INIT_NTIME (&server->cfg.worker_idle_timeout, 0, 0);
	HCL_INIT_NTIME (&server->cfg.actor_max_runtime, 0, 0);

	server->mux_pipe[0] = pfd[0];
	server->mux_pipe[1] = pfd[1];

	server->wid_map.free_first = HCL_SERVER_WID_INVALID;
	server->wid_map.free_last = HCL_SERVER_WID_INVALID;

	server->listener.ep_fd = -1;

	pthread_mutex_init (&server->worker_mutex, HCL_NULL);
	pthread_mutex_init (&server->tmr_mutex, HCL_NULL);
	pthread_mutex_init (&server->log_mutex, HCL_NULL);

	/* the dummy hcl is used for this server to perform primitive operations
	 * such as getting system time or logging. so the heap size doesn't 
	 * need to be changed from the tiny value set above. */
	hcl_setoption (server->dummy_hcl, HCL_LOG_MASK, &server->cfg.logmask);
	hcl_setcmgr (server->dummy_hcl, hcl_server_getcmgr(server));
	hcl_getoption (server->dummy_hcl, HCL_TRAIT, &trait);
#if defined(HCL_BUILD_DEBUG)
	if (server->cfg.trait & HCL_SERVER_TRAIT_DEBUG_GC) trait |= HCL_TRAIT_DEBUG_GC;
	if (server->cfg.trait & HCL_SERVER_TRAIT_DEBUG_BIGINT) trait |= HCL_DEBUG_BIGINT;
#endif
	hcl_setoption (server->dummy_hcl, HCL_TRAIT, &trait);

	return server;
	
oops:
	/* NOTE: pipe should be closed if jump to here is made after pipe() above */
	if (tmr) hcl_tmr_close (tmr);
	if (hcl) hcl_close (hcl);
	if (server) HCL_MMGR_FREE (mmgr, server);

	return HCL_NULL;
}

void hcl_server_close (hcl_server_t* server)
{
	HCL_ASSERT (server->dummy_hcl, server->listener.head == HCL_NULL);
	HCL_ASSERT (server->dummy_hcl, server->listener.count == 0);
	HCL_ASSERT (server->dummy_hcl, server->listener.ep_fd == -1);

	if (server->wid_map.ptr)
	{
		hcl_server_freemem(server, server->wid_map.ptr);
		server->wid_map.capa = 0;
		server->wid_map.free_first = HCL_SERVER_WID_INVALID;
		server->wid_map.free_last = HCL_SERVER_WID_INVALID;
	}
	
	pthread_mutex_destroy (&server->log_mutex);
	pthread_mutex_destroy (&server->tmr_mutex);
	pthread_mutex_destroy (&server->worker_mutex);

	close (server->mux_pipe[0]);
	close (server->mux_pipe[1]);

	hcl_tmr_close (server->tmr);
	hcl_close (server->dummy_hcl);

	HCL_MMGR_FREE (server->_mmgr, server);
}

static HCL_INLINE int prepare_to_acquire_wid (hcl_server_t* server)
{
	hcl_oow_t new_capa;
	hcl_oow_t i, j;
	hcl_server_wid_map_data_t* tmp;

	HCL_ASSERT (server->dummy_hcl, server->wid_map.free_first == HCL_SERVER_WID_INVALID);
	HCL_ASSERT (server->dummy_hcl, server->wid_map.free_last == HCL_SERVER_WID_INVALID);

	new_capa = HCL_ALIGN_POW2(server->wid_map.capa + 1, HCL_SERVER_WID_MAP_ALIGN);
	if (new_capa > HCL_SERVER_WID_MAX)
	{
		if (server->wid_map.capa >= HCL_SERVER_WID_MAX)
		{
			hcl_server_seterrnum (server, HCL_EFLOOD);
			return -1;
		}

		new_capa = HCL_SERVER_WID_MAX;
	}

	tmp = (hcl_server_wid_map_data_t*)hcl_server_reallocmem(server, server->wid_map.ptr, HCL_SIZEOF(*tmp) * new_capa);
	if (!tmp) return -1;

	server->wid_map.free_first = server->wid_map.capa;
	for (i = server->wid_map.capa, j = server->wid_map.capa + 1; j < new_capa; i++, j++)
	{
		tmp[i].used = 0;
		tmp[i].u.next = j;
	}
	tmp[i].used = 0;
	tmp[i].u.next = HCL_SERVER_WID_INVALID;
	server->wid_map.free_last = i;

	server->wid_map.ptr = tmp;
	server->wid_map.capa = new_capa;

	return 0;
}

static HCL_INLINE void acquire_wid (hcl_server_t* server, hcl_server_worker_t* worker)
{
	hcl_oow_t wid;

	wid = server->wid_map.free_first;
	worker->wid = wid;

	server->wid_map.free_first = server->wid_map.ptr[wid].u.next;
	if (server->wid_map.free_first == HCL_SERVER_WID_INVALID) server->wid_map.free_last = HCL_SERVER_WID_INVALID;

	server->wid_map.ptr[wid].used = 1;
	server->wid_map.ptr[wid].u.worker = worker;
}

static HCL_INLINE void release_wid (hcl_server_t* server, hcl_server_worker_t* worker)
{
	hcl_oow_t wid;

	wid = worker->wid;
	HCL_ASSERT (server->dummy_hcl, wid < server->wid_map.capa && wid != HCL_SERVER_WID_INVALID);

	server->wid_map.ptr[wid].used = 0;
	server->wid_map.ptr[wid].u.next = HCL_SERVER_WID_INVALID;
	if (server->wid_map.free_last == HCL_SERVER_WID_INVALID)
	{
		HCL_ASSERT (server->dummy_hcl, server->wid_map.free_first <= HCL_SERVER_WID_INVALID);
		server->wid_map.free_first = wid;
	}
	else
	{
		server->wid_map.ptr[server->wid_map.free_last].u.next = wid;
	}
	server->wid_map.free_last = wid;
	worker->wid = HCL_SERVER_WID_INVALID;
}

static hcl_server_worker_t* alloc_worker (hcl_server_t* server, int cli_sck, const hcl_sckaddr_t* peeraddr)
{
	hcl_server_worker_t* worker;

	worker = (hcl_server_worker_t*)hcl_server_allocmem(server, HCL_SIZEOF(*worker));
	if (!worker) return HCL_NULL;

	HCL_MEMSET (worker, 0, HCL_SIZEOF(*worker));
	worker->state = HCL_SERVER_WORKER_STATE_ZOMBIE;
	worker->opstate = HCL_SERVER_WORKER_OPSTATE_IDLE;
	worker->sck = cli_sck;
	worker->peeraddr = *peeraddr;
	worker->server = server;
	
	server->dummy_hcl->vmprim.vm_gettime (server->dummy_hcl, &worker->alloc_time); /* TODO: the callback may return monotonic time. find a way to guarantee it is realtime??? */

	if (server->wid_map.free_first == HCL_SERVER_WID_INVALID && prepare_to_acquire_wid(server) <= -1) 
	{
		hcl_server_freemem (server, worker);
		return HCL_NULL;
	}

	acquire_wid (server, worker);
	return worker;
}

static void close_worker_socket (hcl_server_worker_t* worker)
{
	if (worker->sck >= 0) 
	{
		if (worker->proto)
		{
			HCL_LOG2 (worker->proto->hcl, SERVER_LOGMASK_INFO, "Closing worker socket %d [%zu]\n", worker->sck, worker->wid);
		}
		else
		{
			/* this should be in the main server thread. i use dummy_hcl for logging */
			HCL_LOG2 (worker->server->dummy_hcl, SERVER_LOGMASK_INFO, "Closing worker socket %d [%zu]\n", worker->sck, worker->wid);
		}
		close (worker->sck);
		worker->sck = -1;
	}
}

static void free_worker (hcl_server_worker_t* worker)
{
	close_worker_socket (worker);
	
	if (worker->proto)
	{
		HCL_LOG1 (worker->proto->hcl, SERVER_LOGMASK_INFO, "Killing worker [%zu]\n", worker->wid);
	}
	else
	{
		/* this should be in the main server thread. i use dummy_hcl for logging */
		HCL_LOG1 (worker->server->dummy_hcl, SERVER_LOGMASK_INFO, "Killing worker [%zu]\n", worker->wid);
	}

	release_wid (worker->server, worker);
	hcl_server_freemem (worker->server, worker);
}

static void add_worker_to_server (hcl_server_t* server, hcl_server_worker_state_t wstate, hcl_server_worker_t* worker)
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

	server->worker_list[wstate].count++;
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

	HCL_ASSERT (server->dummy_hcl, server->worker_list[wstate].count > 0);
	server->worker_list[wstate].count--;
	worker->state = HCL_SERVER_WORKER_STATE_ZOMBIE;
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
	add_worker_to_server (server, HCL_SERVER_WORKER_STATE_ALIVE, worker);
	pthread_mutex_unlock (&server->worker_mutex);

	while (!server->stopreq)
	{
		worker->opstate = HCL_SERVER_WORKER_OPSTATE_WAIT;
		if (hcl_server_proto_handle_request(worker->proto) <= 0) 
		{
			worker->opstate = HCL_SERVER_WORKER_OPSTATE_ERROR;
			break;
		}
	}

	hcl_server_proto_close (worker->proto);
	worker->proto = HCL_NULL;

	pthread_mutex_lock (&server->worker_mutex);
	close_worker_socket (worker); 
	if (!worker->claimed)
	{
		zap_worker_in_server (server, worker);
		add_worker_to_server (server, HCL_SERVER_WORKER_STATE_DEAD, worker);
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

void hcl_server_logbfmt (hcl_server_t* server, hcl_bitmask_t mask, const hcl_bch_t* fmt, ...)
{
	va_list ap;
	va_start (ap, fmt);
	hcl_logbfmtv (server->dummy_hcl, mask, fmt, ap);
	va_end (ap);
}

void hcl_server_logufmt (hcl_server_t* server, hcl_bitmask_t mask, const hcl_uch_t* fmt, ...)
{
	va_list ap;
	va_start (ap, fmt);
	hcl_logufmtv (server->dummy_hcl, mask, fmt, ap);
	va_end (ap);
}

static void set_err_with_syserr (hcl_server_t* server, int syserr_type, int syserr_code, const char* bfmt, ...)
{
	hcl_t* hcl = server->dummy_hcl;
	hcl_errnum_t errnum;
	hcl_oow_t tmplen, tmplen2;
	va_list ap;
	
	static hcl_bch_t b_dash[] = { ' ', '-', ' ', '\0' };
	static hcl_uch_t u_dash[] = { ' ', '-', ' ', '\0' };

	if (hcl->shuterr) return;

	if (hcl->vmprim.syserrstrb)
	{
		errnum = hcl->vmprim.syserrstrb(hcl, syserr_type, syserr_code, hcl->errmsg.tmpbuf.bch, HCL_COUNTOF(hcl->errmsg.tmpbuf.bch));

		va_start (ap, bfmt);
		hcl_seterrbfmtv (hcl, errnum, bfmt, ap);
		va_end (ap);

	#if defined(HCL_OOCH_IS_UCH)
		hcl->errmsg.len += hcl_copy_ucstr(&hcl->errmsg.buf[hcl->errmsg.len], HCL_COUNTOF(hcl->errmsg.buf) - hcl->errmsg.len, u_dash);
		tmplen2 = HCL_COUNTOF(hcl->errmsg.buf) - hcl->errmsg.len;
		hcl_convbtoucstr (hcl, hcl->errmsg.tmpbuf.bch, &tmplen, &hcl->errmsg.buf[hcl->errmsg.len], &tmplen2);
		hcl->errmsg.len += tmplen2; /* ignore conversion errors */
	#else
		hcl->errmsg.len += hcl_copy_bcstr(&hcl->errmsg.buf[hcl->errmsg.len], HCL_COUNTOF(hcl->errmsg.buf) - hcl->errmsg.len, b_dash);
		hcl->errmsg.len += hcl_copy_bcstr(&hcl->errmsg.buf[hcl->errmsg.len], HCL_COUNTOF(hcl->errmsg.buf) - hcl->errmsg.len, hcl->errmsg.tmpbuf.bch);

	#endif
	}
	else
	{
		HCL_ASSERT (hcl, hcl->vmprim.syserrstru != HCL_NULL);

		errnum = hcl->vmprim.syserrstru(hcl, syserr_type, syserr_code, hcl->errmsg.tmpbuf.uch, HCL_COUNTOF(hcl->errmsg.tmpbuf.uch));

		va_start (ap, bfmt);
		hcl_seterrbfmtv (hcl, errnum, bfmt, ap);
		va_end (ap);

	#if defined(HCL_OOCH_IS_UCH)
		hcl->errmsg.len += hcl_copy_ucstr(&hcl->errmsg.buf[hcl->errmsg.len], HCL_COUNTOF(hcl->errmsg.buf) - hcl->errmsg.len, u_dash);
		hcl->errmsg.len += hcl_copy_ucstr(&hcl->errmsg.buf[hcl->errmsg.len], HCL_COUNTOF(hcl->errmsg.buf) - hcl->errmsg.len, hcl->errmsg.tmpbuf.uch);
	#else
		hcl->errmsg.len += hcl_copy_bcstr(&hcl->errmsg.buf[hcl->errmsg.len], HCL_COUNTOF(hcl->errmsg.buf) - hcl->errmsg.len, b_dash);
		tmplen2 = HCL_COUNTOF(hcl->errmsg.buf) - hcl->errmsg.len;
		hcl_convutobcstr (hcl, hcl->errmsg.tmpbuf.uch, &tmplen, &hcl->errmsg.buf[hcl->errmsg.len], &tmplen2);
		hcl->errmsg.len += tmplen2; /* ignore conversion errors */
	#endif
	}

	server->errnum = errnum;
	hcl_copy_oochars (server->errmsg.buf, server->dummy_hcl->errmsg.buf, HCL_COUNTOF(server->errmsg.buf));
	server->errmsg.len = server->dummy_hcl->errmsg.len;
}

static void free_all_listeners (hcl_server_t* server)
{
	hcl_server_listener_t* lp;
	struct epoll_event dummy_ev;

	epoll_ctl (server->listener.ep_fd, EPOLL_CTL_DEL, server->mux_pipe[0], &dummy_ev);

	while (server->listener.head)
	{
		lp = server->listener.head;
		server->listener.head = lp->next_listener;
		server->listener.count--;

		epoll_ctl (server->listener.ep_fd, EPOLL_CTL_DEL, lp->sck, &dummy_ev);
		close (lp->sck);
		hcl_server_freemem (server, lp);
	}

	HCL_ASSERT (server->dummy_hcl, server->listener.ep_fd >= 0);
	close (server->listener.ep_fd);
	server->listener.ep_fd = -1;
}

static int setup_listeners (hcl_server_t* server, const hcl_bch_t* addrs)
{
	const hcl_bch_t* addr_ptr, * comma;
	int ep_fd, fcv;
	struct epoll_event ev;

	ep_fd = epoll_create(1024);
	if (ep_fd <= -1)
	{
		set_err_with_syserr (server, 0, errno, "unable to create multiplexer");
		HCL_LOG1 (server->dummy_hcl, SERVER_LOGMASK_ERROR, "%js\n", hcl_server_geterrmsg(server));
		return -1;
	}

#if defined(O_CLOEXEC)
	fcv = fcntl(ep_fd, F_GETFD, 0);
	if (fcv >= 0) fcntl(ep_fd, F_SETFD, fcv | O_CLOEXEC);
#endif

	HCL_MEMSET (&ev, 0, HCL_SIZEOF(ev));
	ev.events = EPOLLIN | EPOLLHUP | EPOLLERR;
	ev.data.fd = server->mux_pipe[0];
	if (epoll_ctl(ep_fd, EPOLL_CTL_ADD, server->mux_pipe[0], &ev) <= -1)
	{
		set_err_with_syserr (server, 0, errno, "unable to register pipe %d to multiplexer", server->mux_pipe[0]);
		HCL_LOG1 (server->dummy_hcl, SERVER_LOGMASK_ERROR, "%js\n", hcl_server_geterrmsg(server));
		close (ep_fd);
		return -1;
	}

	server->listener.ep_fd = ep_fd;
	addr_ptr = addrs;
	while (1)
	{
		hcl_sckaddr_t srv_addr;
		int srv_fd, sck_fam, optval;
		hcl_scklen_t srv_len;
		hcl_oow_t addr_len;
		hcl_server_listener_t* listener;

		comma = hcl_find_bchar_in_bcstr(addr_ptr, ',');
		addr_len = comma? comma - addr_ptr: hcl_count_bcstr(addr_ptr);
		/* [NOTE] no whitespaces are allowed before and after a comma */

		sck_fam = hcl_bchars_to_sckaddr(addr_ptr, addr_len, &srv_addr, &srv_len);
		if (sck_fam <= -1) 
		{
			hcl_server_seterrbfmt (server, HCL_EINVAL, "unable to convert address - %.*hs", addr_len, addr_ptr);
			HCL_LOG1 (server->dummy_hcl, SERVER_LOGMASK_ERROR, "%js\n", hcl_server_geterrmsg(server));
			goto next_segment;
		}

		srv_fd = socket(sck_fam, SOCK_STREAM, 0);
		if (srv_fd <= -1)
		{
			set_err_with_syserr (server, 0, errno, "unable to open server socket for %.*hs", addr_len, addr_ptr);
			HCL_LOG1 (server->dummy_hcl, SERVER_LOGMASK_ERROR, "%js\n", hcl_server_geterrmsg(server));
			goto next_segment;
		}

		optval = 1;
		setsockopt (srv_fd, SOL_SOCKET, SO_REUSEADDR, &optval, HCL_SIZEOF(int));

	#if defined(O_NONBLOCK) || defined(O_CLOEXEC)
		fcv = fcntl(srv_fd, F_GETFD, 0);
		if (fcv >= 0) 
		{
		#if defined(O_NONBLOCK)
			fcv |= O_NONBLOCK;
		#endif
		#if defined(O_CLOEXEC)
			fcv |= O_CLOEXEC;
		#endif

			fcntl(srv_fd, F_SETFL, fcv);
		}
	#endif

		if (bind(srv_fd, (struct sockaddr*)&srv_addr, srv_len) == -1)
		{
			set_err_with_syserr (server, 0, errno, "unable to bind server socket %d for %.*hs", srv_fd, addr_len, addr_ptr);
			HCL_LOG1 (server->dummy_hcl, SERVER_LOGMASK_ERROR, "%js\n", hcl_server_geterrmsg(server));
			close (srv_fd);
			goto next_segment;
		}

		if (listen(srv_fd, 128) <= -1)
		{
			set_err_with_syserr (server, 0, errno, "unable to listen on server socket %d for %.*hs", srv_fd, addr_len, addr_ptr);
			HCL_LOG1 (server->dummy_hcl, SERVER_LOGMASK_ERROR, "%js\n", hcl_server_geterrmsg(server));
			close (srv_fd);
			goto next_segment;
		}

		HCL_MEMSET (&ev, 0, HCL_SIZEOF(ev));
		ev.events = EPOLLIN | EPOLLHUP | EPOLLERR;
		ev.data.fd = srv_fd;
		if (epoll_ctl(ep_fd, EPOLL_CTL_ADD, srv_fd, &ev) <= -1)
		{
			set_err_with_syserr (server, 0, errno, "unable to register server socket %d to multiplexer for %.*hs", srv_fd, addr_len, addr_ptr);
			HCL_LOG1 (server->dummy_hcl, SERVER_LOGMASK_ERROR, "%js\n", hcl_server_geterrmsg(server));
			close (srv_fd);
			goto next_segment;
		}

		listener = (hcl_server_listener_t*)hcl_server_allocmem(server, HCL_SIZEOF(*listener));
		if (!listener)
		{
			close(srv_fd);
			goto next_segment;
		}

		HCL_MEMSET (listener, 0, HCL_SIZEOF(*listener));
		listener->sck = srv_fd;
		listener->sckaddr = srv_addr;
		listener->next_listener = server->listener.head;
		server->listener.head = listener;
		server->listener.count++;

	next_segment:
		if (!comma) break;
		addr_ptr = comma + 1;
	}


	if (!server->listener.head)
	{
		/* no valid server has been configured */
		hcl_server_seterrbfmt (server, HCL_EINVAL, "unable to set up listeners with %hs", addrs);
		free_all_listeners (server);
		return -1;
	}

	return 0;
}

int hcl_server_start (hcl_server_t* server, const hcl_bch_t* addrs)
{
	int xret = 0, fcv;
	pthread_attr_t thr_attr;

	if (setup_listeners(server, addrs) <= -1) return -1;

	pthread_attr_init (&thr_attr);
	pthread_attr_setstacksize (&thr_attr, server->cfg.worker_stack_size);

	server->stopreq = 0;
	while (!server->stopreq)
	{
		hcl_sckaddr_t cli_addr;
		hcl_scklen_t cli_len;
		int cli_fd;
		pthread_t thr;
		hcl_ntime_t tmout;
		hcl_server_worker_t* worker;
		int n;

		pthread_mutex_lock (&server->tmr_mutex);
		n = hcl_tmr_gettmout(server->tmr,  HCL_NULL, &tmout);
		pthread_mutex_unlock (&server->tmr_mutex);
		if (n <= -1) HCL_INIT_NTIME (&tmout, 10, 0);

		n = epoll_wait(server->listener.ep_fd, server->listener.ev_buf, HCL_COUNTOF(server->listener.ev_buf), HCL_SECNSEC_TO_MSEC(tmout.sec, tmout.nsec));

		purge_all_workers (server, HCL_SERVER_WORKER_STATE_DEAD);
		if (n <= -1)
		{
			if (server->stopreq) break; /* normal termination requested */
			if (errno == EINTR) continue; /* interrupted but not termination requested */

			set_err_with_syserr (server, 0, errno, "unable to poll for events in server");
			xret = -1;
			break;
		}

		pthread_mutex_lock (&server->tmr_mutex);
		hcl_tmr_fire (server->tmr, HCL_NULL, HCL_NULL);
		pthread_mutex_unlock (&server->tmr_mutex);

		while (n > 0)
		{
			struct epoll_event* evp;

			--n;

			evp = &server->listener.ev_buf[n];
			if (!evp->events /*& (POLLIN | POLLHUP | POLLERR) */) continue;

			if (evp->data.fd == server->mux_pipe[0])
			{
				char tmp[128];
				while (read(server->mux_pipe[0], tmp, HCL_SIZEOF(tmp)) > 0) /* nothing */;
			}
			else
			{
				/* the reset should be the listener's socket */

				cli_len = HCL_SIZEOF(cli_addr);
				cli_fd = accept(evp->data.fd, (struct sockaddr*)&cli_addr, &cli_len);
				if (cli_fd == -1)
				{
					if (server->stopreq) break; /* normal termination requested */
					if (errno == EINTR) continue; /* interrupted but no termination requested */
				#if defined(EWOULDBLOCK) && defined(EAGAIN) && (EWOULDBLOCK != EAGAIN)
					if (errno == EWOULDBLOCK || errno == EAGAIN) continue;
				#elif defined(EWOULDBLOCK)
					if (errno == EWOULDBLOCK) continue;
				#elif defined(EAGAIN)
					if (errno == EAGAIN) continue;
				#endif

					set_err_with_syserr (server, 0, errno, "unable to accept worker on server socket %d", evp->data.fd);
					xret = -1;
					break;
				}

			#if defined(O_NONBLOCK) || defined(O_CLOEXEC)
				fcv = fcntl(cli_fd, F_GETFD, 0);
				if (fcv >= 0) 
				{
				#if defined(O_NONBLOCK)
					fcv &= ~O_NONBLOCK; // force the accepted socket to be blocking
				#endif
				#if defined(O_CLOEXEC)
					fcv |= O_CLOEXEC;
				#endif
					fcntl(cli_fd, F_SETFD, fcv);
				}
			#endif

				if (server->cfg.worker_max_count > 0)
				{
					int flood;
					pthread_mutex_lock (&server->worker_mutex);
					flood = (server->worker_list[HCL_SERVER_WORKER_STATE_ALIVE].count >= server->cfg.worker_max_count);
					pthread_mutex_unlock (&server->worker_mutex);
					if (flood) 
					{
						HCL_LOG1 (server->dummy_hcl, SERVER_LOGMASK_ERROR, "Not accepting connection for too many workers - socket %d\n", cli_fd);
						goto drop_connection;
					}
				}

				worker = alloc_worker(server, cli_fd, &cli_addr);
				if (!worker)
				{
					HCL_LOG1 (server->dummy_hcl, SERVER_LOGMASK_ERROR, "Unable to accomodate worker - socket %d\n", cli_fd);
				drop_connection:
					close (cli_fd);
				}
				else
				{
					HCL_LOG2 (server->dummy_hcl, SERVER_LOGMASK_INFO, "Accomodated worker [%zu] - socket %d\n", worker->wid, cli_fd);
					if (pthread_create(&thr, &thr_attr, worker_main, worker) != 0)
					{
						free_worker (worker);
					}
				}
			}
		}
	}

	purge_all_workers (server, HCL_SERVER_WORKER_STATE_ALIVE);
	purge_all_workers (server, HCL_SERVER_WORKER_STATE_DEAD);

	pthread_attr_destroy (&thr_attr);

	free_all_listeners (server);
	return xret;
}

void hcl_server_stop (hcl_server_t* server)
{
	server->stopreq = 1;
	write (server->mux_pipe[1], "Q", 1); /* don't care about failure */
}

int hcl_server_setoption (hcl_server_t* server, hcl_server_option_t id, const void* value)
{
	switch (id)
	{
		case HCL_SERVER_TRAIT:
			server->cfg.trait = *(const hcl_bitmask_t*)value;
			if (server->dummy_hcl)
			{
				/* setting this affects the dummy hcl immediately.
				 * existing hcl instances inside worker threads won't get 
				 * affected. new hcl instances to be created later 
				 * is supposed to use the new value */
				hcl_bitmask_t trait;

				hcl_getoption (server->dummy_hcl, HCL_TRAIT, &trait);
			#if defined(HCL_BUILD_DEBUG)
				if (server->cfg.trait & HCL_SERVER_TRAIT_DEBUG_GC) trait |= HCL_TRAIT_DEBUG_GC;
				if (server->cfg.trait & HCL_SERVER_TRAIT_DEBUG_BIGINT) trait |= HCL_DEBUG_BIGINT;
			#endif
				hcl_setoption (server->dummy_hcl, HCL_TRAIT, &trait);
			}
			return 0;

		case HCL_SERVER_LOG_MASK:
			server->cfg.logmask = *(const hcl_bitmask_t*)value;
			if (server->dummy_hcl) 
			{
				/* setting this affects the dummy hcl immediately.
				 * existing hcl instances inside worker threads won't get 
				 * affected. new hcl instances to be created later 
				 * is supposed to use the new value */
				hcl_setoption (server->dummy_hcl, HCL_LOG_MASK, value);
			}
			return 0;

		case HCL_SERVER_WORKER_MAX_COUNT:
			server->cfg.worker_max_count = *(hcl_oow_t*)value;
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

		case HCL_SERVER_SCRIPT_INCLUDE_PATH:
			hcl_copy_oocstr (server->cfg.script_include_path, HCL_COUNTOF(server->cfg.script_include_path), (const hcl_ooch_t*)value);
			return 0;

		case HCL_SERVER_MODULE_INCTX:
			server->cfg.module_inctx = *(void**)value;
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
			*(hcl_bitmask_t*)value = server->cfg.trait;
			return 0;

		case HCL_SERVER_LOG_MASK:
			*(hcl_bitmask_t*)value = server->cfg.logmask;
			return 0;

		case HCL_SERVER_WORKER_MAX_COUNT:
			*(hcl_oow_t*)value = server->cfg.worker_max_count;
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

		case HCL_SERVER_SCRIPT_INCLUDE_PATH:
			*(hcl_ooch_t**)value = server->cfg.script_include_path;
			return 0;

		case HCL_SERVER_MODULE_INCTX:
			*(void**)value = server->cfg.module_inctx;
			return 0;
	};

	hcl_server_seterrnum (server, HCL_EINVAL);
	return -1;
}

void* hcl_server_getxtn (hcl_server_t* server)
{
	return (void*)((hcl_uint8_t*)server + server->_instsize);
}

hcl_mmgr_t* hcl_server_getmmgr (hcl_server_t* server)
{
	return server->_mmgr;
}

hcl_cmgr_t* hcl_server_getcmgr (hcl_server_t* server)
{
	return server->_cmgr;
}

void hcl_server_setcmgr (hcl_server_t* server, hcl_cmgr_t* cmgr)
{
	server->_cmgr = cmgr;
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
	hcl_copy_oochars (server->errmsg.buf, server->dummy_hcl->errmsg.buf, HCL_COUNTOF(server->errmsg.buf));
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
	server->errnum = errnum;
	hcl_copy_oochars (server->errmsg.buf, server->dummy_hcl->errmsg.buf, HCL_COUNTOF(server->errmsg.buf));
	server->errmsg.len = server->dummy_hcl->errmsg.len;
}

void* hcl_server_allocmem (hcl_server_t* server, hcl_oow_t size)
{
	void* ptr;

	ptr = HCL_MMGR_ALLOC(server->_mmgr, size);
	if (!ptr) hcl_server_seterrnum (server, HCL_ESYSMEM);
	return ptr;
}

void* hcl_server_callocmem (hcl_server_t* server, hcl_oow_t size)
{
	void* ptr;

	ptr = HCL_MMGR_ALLOC(server->_mmgr, size);
	if (!ptr) hcl_server_seterrnum (server, HCL_ESYSMEM);
	else HCL_MEMSET (ptr, 0, size);
	return ptr;
}

void* hcl_server_reallocmem (hcl_server_t* server, void* ptr, hcl_oow_t size)
{
	ptr = HCL_MMGR_REALLOC(server->_mmgr, ptr, size);
	if (!ptr) hcl_server_seterrnum (server, HCL_ESYSMEM);
	return ptr;
}

void hcl_server_freemem (hcl_server_t* server, void* ptr)
{
	HCL_MMGR_FREE (server->_mmgr, ptr);
}
