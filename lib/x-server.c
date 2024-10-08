/*
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

#include <hcl-x.h>
#include <hcl-tmr.h>
#include "hcl-prv.h"

#include <stdio.h>
#include <string.h>
#include <errno.h>

#define HCL_SERVER_TOKEN_NAME_ALIGN 64
#define HCL_SERVER_WID_MAP_ALIGN 512
#define HCL_XPROTO_REPLY_BUF_SIZE 1300

#if defined(_WIN32)
#	include <windows.h>
#	include <tchar.h>
#elif defined(__OS2__)
#	define INCL_DOSMODULEMGR
#	define INCL_DOSPROCESS
#	define INCL_DOSERRORS
#	include <os2.h>
#elif defined(__DOS__)
#	include <dos.h>
#	include <time.h>
#	include <signal.h>
#elif defined(macintosh)
#	include <Timer.h>
#else

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
#		define USE_EPOLL
#	endif

#	include <unistd.h>
#	include <fcntl.h>
#	include <sys/types.h>
#	include <sys/socket.h>
#	include <netinet/in.h>
#	include <pthread.h>
#	include <poll.h>
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

struct proto_xtn_t
{
	hcl_server_worker_t* worker;
};
typedef struct proto_xtn_t proto_xtn_t;

struct worker_hcl_xtn_t
{
	hcl_server_worker_t* worker;
	int vm_running;
};
typedef struct worker_hcl_xtn_t worker_hcl_xtn_t;

struct server_hcl_xtn_t
{
	hcl_server_t* server;
};
typedef struct server_hcl_xtn_t server_hcl_xtn_t;

/* ---------------------------------- */

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
	hcl_tmr_index_t exec_runtime_event_index;
	hcl_xproto_t* proto;
	hcl_t* hcl;

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
	#if defined(USE_EPOLL)
		int ep_fd;
		struct epoll_event ev_buf[128];
	#endif
		hcl_server_listener_t* head;
		hcl_oow_t count;
	} listener;

	struct
	{
		hcl_server_worker_t* head;
		hcl_server_worker_t* tail;
		hcl_oow_t count;
	} worker_list[2]; /* DEAD and ALIVE oly. ZOMBIEs are not chained here */

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
static int send_bytes (hcl_xproto_t* proto, hcl_xpkt_type_t xpkt_code, const hcl_bch_t* data, hcl_oow_t len);

#if defined(HCL_OOCH_IS_UCH)
static int send_chars (hcl_xproto_t* proto, hcl_xpkt_type_t xpkt_code, const hcl_ooch_t* data, hcl_oow_t len);
#else
#define send_chars(proto,xpkt_code,data,len) send_bytes(proto,xpkt_code,data,len)
#endif

/* ========================================================================= */

static HCL_INLINE int open_read_stream (hcl_t* hcl, hcl_io_cciarg_t* arg)
{
	worker_hcl_xtn_t* xtn = (worker_hcl_xtn_t*)hcl_getxtn(hcl);
	bb_t* bb = HCL_NULL;
	hcl_server_t* server;

	server = xtn->worker->server;

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
			fb = hcl_get_base_name_from_bcstr_path(fn);
			parlen = fb - fn;
		}

		bb = (bb_t*)hcl_callocmem(hcl, HCL_SIZEOF(*bb) + (HCL_SIZEOF(hcl_bch_t) * (parlen + bcslen + 2)));
		if (HCL_UNLIKELY(!bb)) goto oops;

		bb->fn = (hcl_bch_t*)(bb + 1);
		if (fn[0] == '\0' && server->cfg.script_include_path[0] != '\0')
		{
		#if defined(HCL_OOCH_IS_UCH)
			hcl_convootobcstr (hcl, server->cfg.script_include_path, &ucslen, bb->fn, &parlen);
		#else
			hcl_copy_bchars (bb->fn, server->cfg.script_include_path, parlen);
		#endif
			if (!HCL_IS_PATH_SEP(bb->fn[parlen])) bb->fn[parlen++] = HCL_DFL_PATH_SEP; /* +2 was used in hcl_callocmem() for this (+1 for this, +1 for '\0' */
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

		if (bb->fd <= -1)
		{
			hcl_seterrnum (hcl, HCL_EIOERR);
			goto oops;
		}
	}
	else
	{
		/* main stream */
		hcl_oow_t pathlen = 0;
		bb = (bb_t*)hcl_callocmem(hcl, HCL_SIZEOF(*bb) + (HCL_SIZEOF(hcl_bch_t) * (pathlen + 1)));
		if (HCL_UNLIKELY(!bb)) goto oops;

		/* copy ane empty string as a main stream's name */
		bb->fn = (hcl_bch_t*)(bb + 1);
		hcl_copy_bcstr (bb->fn, pathlen + 1, "");

		bb->fd = xtn->worker->sck;
	}

	HCL_ASSERT (hcl, bb->fd >= 0);

	arg->handle = bb;
	return 0;

oops:
	if (bb)
	{
		if (bb->fd >= 0 && bb->fd != xtn->worker->sck) close (bb->fd);
		hcl_freemem (hcl, bb);
	}
	return -1;
}

static HCL_INLINE int close_read_stream (hcl_t* hcl, hcl_io_cciarg_t* arg)
{
	worker_hcl_xtn_t* xtn = (worker_hcl_xtn_t*)hcl_getxtn(hcl);
	bb_t* bb;

	bb = (bb_t*)arg->handle;
	HCL_ASSERT (hcl, bb != HCL_NULL && bb->fd >= 0);

	if (bb->fd != xtn->worker->sck) close (bb->fd);
	hcl_freemem (hcl, bb);

	arg->handle = HCL_NULL;
	return 0;
}

static HCL_INLINE int read_read_stream (hcl_t* hcl, hcl_io_cciarg_t* arg)
{
	worker_hcl_xtn_t* xtn = (worker_hcl_xtn_t*)hcl_getxtn(hcl);
	bb_t* bb;
	hcl_oow_t bcslen, ucslen, remlen;
	hcl_server_worker_t* worker;
	ssize_t x;
	int y;

	bb = (bb_t*)arg->handle;
	HCL_ASSERT (hcl, bb != HCL_NULL && bb->fd >= 0);

	worker = xtn->worker;

start_over:
	if (arg->includer)
	{
		/* includee */
		if (HCL_UNLIKELY(worker->server->stopreq))
		{
			hcl_seterrbfmt (hcl, HCL_EGENERIC, "stop requested");
			return -1;
		}

		x = read(bb->fd, &bb->buf[bb->len], HCL_COUNTOF(bb->buf) - bb->len);
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
		/* main stream */
		hcl_server_t* server;

		HCL_ASSERT (hcl, bb->fd == worker->sck);
		server = worker->server;

		while (1)
		{
			int n;
			struct pollfd pfd;
			int tmout, actual_tmout;

			if (HCL_UNLIKELY(server->stopreq))
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
				hcl_seterrbfmt (hcl, HCL_EGENERIC, "no activity on the worker socket %d", bb->fd);
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

#if defined(HCL_OOCH_IS_UCH)
	bcslen = bb->len;
	ucslen = HCL_COUNTOF(arg->buf.c);
	y = hcl_convbtooochars(hcl, bb->buf, &bcslen, arg->buf.c, &ucslen);
	if (y <= -1 && ucslen <= 0)
	{
		if (y == -3 && x != 0) goto start_over; /* incomplete sequence and not EOF yet */
		return -1;
	}
	/* if ucslen is greater than 0, i see that some characters have been
	 * converted properly */
#else
	bcslen = (bb->len < HCL_COUNTOF(arg->buf.b))? bb->len: HCL_COUNTOF(arg->buf.b);
	ucslen = bcslen;
	hcl_copy_bchars (arg->buf.b, bb->buf, bcslen);
#endif

	remlen = bb->len - bcslen;
	if (remlen > 0) HCL_MEMMOVE (bb->buf, &bb->buf[bcslen], remlen);
	bb->len = remlen;

	arg->xlen = ucslen;
	return 0;
}


static int read_handler (hcl_t* hcl, hcl_io_cmd_t cmd, void* arg)
{
	switch (cmd)
	{
		case HCL_IO_OPEN:
			return open_read_stream(hcl, (hcl_io_cciarg_t*)arg);

		case HCL_IO_CLOSE:
			return close_read_stream(hcl, (hcl_io_cciarg_t*)arg);

		case HCL_IO_READ:
			return read_read_stream(hcl, (hcl_io_cciarg_t*)arg);

		default:
			hcl_seterrnum (hcl, HCL_EINTERN);
			return -1;
	}
}

static int scan_handler (hcl_t* hcl, hcl_io_cmd_t cmd, void* arg)
{
	switch (cmd)
	{
		case HCL_IO_OPEN:
			return 0;

		case HCL_IO_CLOSE:
			return 0;

		case HCL_IO_READ:
#if 0
		{
			worker_hcl_xtn_t* xtn = (worker_hcl_xtn_t*)hcl_getxtn(hcl);
			hcl_io_udiarg_t* inarg = (hcl_io_udiarg_t*)arg;

// what if it writes a request to require more input??
			if (hcl_xproto_handle_incoming(xtn->proto) <= -1)
			{
			}
		}
#else
			/* TODO: read from the input buffer or pipe*/
#endif

		default:
			hcl_seterrnum (hcl, HCL_EINTERN);
			return -1;
	}
}

static int print_handler (hcl_t* hcl, hcl_io_cmd_t cmd, void* arg)
{
	switch (cmd)
	{
		case HCL_IO_OPEN:
printf ("IO OPEN SOMETHING...........\n");
			return 0;

		case HCL_IO_CLOSE:
printf ("IO CLOSE SOMETHING...........\n");
			return 0;

		case HCL_IO_WRITE:
		{
			worker_hcl_xtn_t* xtn = (worker_hcl_xtn_t*)hcl_getxtn(hcl);
			hcl_io_udoarg_t* outarg = (hcl_io_udoarg_t*)arg;

/*printf ("IO WRITE SOMETHING...........\n");*/
			if (send_chars(xtn->worker->proto, HCL_XPKT_STDOUT, outarg->ptr, outarg->len) <= -1)
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
		case HCL_IO_WRITE_BYTES:
		{
			worker_hcl_xtn_t* xtn = (worker_hcl_xtn_t*)hcl_getxtn(hcl);
			hcl_io_udoarg_t* outarg = (hcl_io_udoarg_t*)arg;

/*printf ("IO WRITE SOMETHING BYTES...........\n");*/
			if (send_bytes(xtn->worker->proto, HCL_XPKT_STDOUT, outarg->ptr, outarg->len) <= -1)
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

		case HCL_IO_FLUSH:
/* TODO: flush data... */
			return 0;

		default:
			hcl_seterrnum (hcl, HCL_EINTERN);
			return -1;
	}
}

/* ========================================================================= */

static void server_log_write (hcl_t* hcl, hcl_bitmask_t mask, const hcl_ooch_t* msg, hcl_oow_t len)
{
	worker_hcl_xtn_t* xtn = (worker_hcl_xtn_t*)hcl_getxtn(hcl);
	hcl_server_t* server;

	server = xtn->worker->server;
	pthread_mutex_lock (&server->log_mutex);
	server->prim.log_write (server, xtn->worker->wid, mask, msg, len);
	pthread_mutex_unlock (&server->log_mutex);
}

static void server_log_write_for_dummy (hcl_t* hcl, hcl_bitmask_t mask, const hcl_ooch_t* msg, hcl_oow_t len)
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
	if (xtn->worker->server->stopreq) hcl_abort(hcl);
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


static hcl_server_worker_t* proto_to_worker (hcl_xproto_t* proto)
{
	proto_xtn_t* prtxtn;
	prtxtn = (proto_xtn_t*)hcl_xproto_getxtn(proto);
	return prtxtn->worker;
}

/* ========================================================================= */

#define SERVER_LOGMASK_INFO (HCL_LOG_INFO | HCL_LOG_APP)
#define SERVER_LOGMASK_ERROR (HCL_LOG_ERROR | HCL_LOG_APP)

static int on_fed_cnode (hcl_t* hcl, hcl_cnode_t* obj)
{
	worker_hcl_xtn_t* hcl_xtn = (worker_hcl_xtn_t*)hcl_getxtn(hcl);
	hcl_server_worker_t* worker;
	hcl_xproto_t* proto;
	int flags = 0;

	hcl_xtn = (worker_hcl_xtn_t*)hcl_getxtn(hcl);
	worker = hcl_xtn->worker;
	proto = worker->proto;

	/* the compile error must not break the input loop.
	 * this function returns 0 to go on despite a compile-time error.
	 *
	 * if a single line or continued lines contain multiple expressions,
	 * execution is delayed until the last expression is compiled. */

	if (hcl_compile(hcl, obj, flags) <= -1)
	{
		const hcl_bch_t* errmsg = hcl_geterrbmsg(hcl);
		send_bytes(proto, HCL_XPKT_ERROR, errmsg, hcl_count_bcstr(errmsg));
/* TODO: ignore the whole line??? */
	}

	return 0;
}

static void exec_runtime_handler (hcl_tmr_t* tmr, const hcl_ntime_t* now, hcl_tmr_event_t* evt)
{
	/* [NOTE] this handler is executed in the main server thread
	 *         when it calls hcl_tmr_fire().  */
	hcl_server_worker_t* worker;

	worker = proto_to_worker((hcl_xproto_t*)evt->ctx);
/* TODO: can we use worker->hcl for logging before abort?? */
	HCL_LOG1 (worker->server->dummy_hcl, SERVER_LOGMASK_INFO, "Aborting script execution for max_actor_runtime exceeded [%zu]\n", worker->wid);
	hcl_abort (worker->hcl);
}

static void exec_runtime_updater (hcl_tmr_t* tmr, hcl_tmr_index_t old_index, hcl_tmr_index_t new_index, hcl_tmr_event_t* evt)
{
	/* [NOTE] this handler is executed in the main server thread
	 *        when it calls hcl_tmr_fire() */
	hcl_xproto_t* proto;
	hcl_server_worker_t* worker;

	proto = (hcl_xproto_t*)evt->ctx;
	worker = proto_to_worker(proto);
	HCL_ASSERT (worker->hcl, worker->exec_runtime_event_index == old_index);

	/* the event is being removed by hcl_tmr_fire() or by hcl_tmr_delete()
	 * if new_index is HCL_TMR_INVALID_INDEX. it's being updated if not. */
	worker->exec_runtime_event_index = new_index;
}

static int insert_exec_timer (hcl_xproto_t* proto, const hcl_ntime_t* tmout)
{
	/* [NOTE] this is executed in the worker thread */

	hcl_tmr_event_t event;
	hcl_tmr_index_t index;
	hcl_server_worker_t* worker;
	hcl_server_t* server;

	worker = proto_to_worker(proto);
	server = worker->server;

	HCL_ASSERT (worker->hcl, worker->exec_runtime_event_index == HCL_TMR_INVALID_INDEX);

	HCL_MEMSET (&event, 0, HCL_SIZEOF(event));
	event.ctx = proto;
	worker->hcl->vmprim.vm_gettime (worker->hcl, &event.when);
	HCL_ADD_NTIME (&event.when, &event.when, tmout);
	event.handler = exec_runtime_handler;
	event.updater = exec_runtime_updater;

	pthread_mutex_lock (&server->tmr_mutex);
	index = hcl_tmr_insert(server->tmr, &event);
	worker->exec_runtime_event_index = index;
	if (index != HCL_TMR_INVALID_INDEX)
	{
		/* inform the server of timer event change */
		write (server->mux_pipe[1], "X", 1); /* don't care even if it fails */
	}
	pthread_mutex_unlock (&server->tmr_mutex);

	return (index == HCL_TMR_INVALID_INDEX)? -1: 0;
}

static void delete_exec_timer (hcl_xproto_t* proto)
{
	/* [NOTE] this is executed in the worker thread. if the event has been fired
	 *        in the server thread, worker->exec_runtime_event_index should be
	 *        HCL_TMR_INVALID_INDEX as set by exec_runtime_handler */
	hcl_server_worker_t* worker;
	hcl_server_t* server;

	worker = proto_to_worker(proto);
	server = worker->server;

	pthread_mutex_lock (&server->tmr_mutex);
	if (worker->exec_runtime_event_index != HCL_TMR_INVALID_INDEX)
	{
		/* the event has not been fired yet. let's delete it
		 * if it has been fired, the index it shall be HCL_TMR_INVALID_INDEX already */

		hcl_tmr_delete (server->tmr, worker->exec_runtime_event_index);
		HCL_ASSERT (worker->hcl, worker->exec_runtime_event_index == HCL_TMR_INVALID_INDEX);
		/*worker->exec_runtime_event_index = HCL_TMR_INVALID_INDEX;	*/
	}
	pthread_mutex_unlock (&server->tmr_mutex);
}

static int execute_script (hcl_xproto_t* proto, const hcl_bch_t* trigger)
{
	hcl_oop_t obj;
	const hcl_ooch_t* failmsg = HCL_NULL;
	hcl_server_worker_t* worker;
	hcl_server_t* server;

	worker = proto_to_worker(proto);
	server = worker->server;

#if 0
	hcl_xproto_start_reply (proto);
#endif
	if (server->cfg.actor_max_runtime.sec <= 0 && server->cfg.actor_max_runtime.sec <= 0)
	{
		obj = hcl_execute(worker->hcl);
		if (!obj) failmsg = hcl_geterrmsg(worker->hcl);
	}
	else
	{
		if (insert_exec_timer(proto, &server->cfg.actor_max_runtime) <= -1)
		{
			HCL_LOG0 (worker->hcl, SERVER_LOGMASK_ERROR, "Cannot start execution timer\n");
			hcl_seterrbfmt (worker->hcl, HCL_ESYSMEM, "cannot start execution timer");  /* i do this just to compose the error message  */
			failmsg = hcl_geterrmsg(worker->hcl);
		}
		else
		{
			obj = hcl_execute(worker->hcl);
			if (!obj) failmsg = hcl_geterrmsg(worker->hcl);
			delete_exec_timer (proto);
		}
	}

#if 0
	if (hcl_xproto_end_reply(proto, failmsg) <= -1)
	{
		HCL_LOG1 (worker->hcl, SERVER_LOGMASK_ERROR, "Cannot finalize reply for %hs\n", trigger);
		return -1;
	}
#endif

	return 0;
}


static void send_error_message (hcl_xproto_t* proto, const hcl_ooch_t* errmsg)
{
#if 0
	hcl_xproto_start_reply (proto);
	if (hcl_xproto_end_reply(proto, errmsg) <= -1)
	{
		HCL_LOG1 (proto->hcl, SERVER_LOGMASK_ERROR, "Unable to send error message - %s\n", errmsg);
	}
#endif
}

static void reformat_synerr (hcl_t* hcl)
{
	hcl_synerr_t synerr;
	const hcl_ooch_t* orgmsg;
	static hcl_ooch_t nullstr[] = { '\0' };

	hcl_getsynerr (hcl, &synerr);

	orgmsg = hcl_backuperrmsg(hcl);
	hcl_seterrbfmt (
		hcl, HCL_ESYNERR,
		"%js%hs%.*js at %js%hsline %zu column %zu",
		orgmsg,
		(synerr.tgt.len > 0? " near ": ""),
		synerr.tgt.len, synerr.tgt.val,
		(synerr.loc.file? synerr.loc.file: nullstr),
		(synerr.loc.file? " ": ""),
		synerr.loc.line, synerr.loc.colm
	);
}

static void send_proto_hcl_error (hcl_xproto_t* proto)
{
	hcl_server_worker_t* worker;
	worker = proto_to_worker(proto);
	if (HCL_ERRNUM(worker->hcl) == HCL_ESYNERR) reformat_synerr (worker->hcl);
	send_error_message (proto, hcl_geterrmsg(worker->hcl));
}

static void show_server_workers (hcl_xproto_t* proto)
{
	hcl_server_worker_t* worker, * w;
	hcl_server_t* server;

	worker = proto_to_worker(proto);
	server = worker->server;

	pthread_mutex_lock (&server->worker_mutex);
	for (w = server->worker_list[HCL_SERVER_WORKER_STATE_ALIVE].head; w; w = w->next_worker)
	{
		/* TODO: implement this better... */
		hcl_prbfmt (worker->hcl, "%zu %d %d\n", w->wid, w->sck, 1000);
	}
	pthread_mutex_unlock (&server->worker_mutex);
}

static int kill_server_worker (hcl_xproto_t* proto, hcl_oow_t wid)
{
	hcl_server_worker_t* worker;
	hcl_server_t* server;
	int xret = 0;

	worker = proto_to_worker(proto);
	server = worker->server;

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
				if (worker->hcl) hcl_abort (worker->hcl);
			}
		}
	}
	pthread_mutex_unlock (&server->worker_mutex);
	return xret;
}

static int server_on_packet (hcl_xproto_t* proto, hcl_xpkt_type_t type, const void* data, hcl_oow_t len)
{
	hcl_server_worker_t* worker;
	hcl_t* hcl;

	worker = proto_to_worker(proto);
	hcl = worker->hcl;

printf ("HANDLE PACKET TYPE => %d\n", type);
	switch (type)
	{
		case HCL_XPKT_CODE:
printf ("FEEDING [%.*s]\n", (int)len, data);
			if (hcl_feedbchars(hcl, data, len) <= -1)
			{
				/* TODO: backup error message...and create a new message */
				goto oops;
			}
			break;

		case HCL_XPKT_EXECUTE:
		{
			hcl_oop_t retv;
printf ("EXECUTING hcl_executing......\n");

			hcl_decode (hcl, hcl_getcode(hcl), 0, hcl_getbclen(hcl));
			if (hcl_feedpending(hcl))
			{
				/* TODO: change the message */
				if (send_bytes(proto, HCL_XPKT_ERROR, "feed more",  9) <=-1)
				{
					/* TODO: error handling */
				}
				break;
			}

			retv = hcl_execute(hcl);
			if (HCL_UNLIKELY(!retv))
			{
				hcl_bch_t errmsg[512];
				hcl_oow_t errlen;

				/* TODO: backup error message...and create a new message */
				/* save error message before other calls override erro info */
				errlen = hcl_copyerrbmsg(hcl, errmsg, HCL_COUNTOF(errmsg));

				hcl_flushudio (hcl);
				hcl_clearcode (hcl);

				if (send_bytes(proto, HCL_XPKT_ERROR, errmsg, errlen) <= -1)
				{
					/* TODO: error handling */
				}

				goto oops;
			}
			else
			{
				hcl_bch_t rvbuf[512]; /* TODO make this dynamic in side? */
				hcl_oow_t rvlen;

				hcl_flushudio (hcl);
				hcl_clearcode (hcl);

				/* TODO or make hcl_fmtXXXX  that accepts the output function */
				rvlen = hcl_fmttobcstr(hcl, rvbuf, HCL_COUNTOF(rvbuf), "[%O]", retv);
				if (send_bytes(proto, HCL_XPKT_RETVAL, rvbuf, rvlen) <= -1)
				{
				}
			}


			break;
		}

		case HCL_XPKT_STDIN:
			/* store ... push stdin pipe... */
			/*if (hcl_feedstdin() <= -1) */
			break;

		case HCL_XPKT_LIST_WORKERS:
			break;

		case HCL_XPKT_KILL_WORKER:
			break;

		case HCL_XPKT_DISCONNECT:
			return 0; /* disconnect received */

		default:
			/* unknown packet type */
			/* TODO: proper error message */
			goto oops;
	}

	return 1;


oops:
	return -1;
}

static int send_bytes (hcl_xproto_t* proto, hcl_xpkt_type_t xpkt_type, const hcl_bch_t* data, hcl_oow_t len)
{
	hcl_server_worker_t* worker;
	hcl_xpkt_hdr_t hdr;
	struct iovec iov[2];
	const hcl_bch_t* ptr, * cur, * end;
	hcl_uint16_t seglen;

	worker = proto_to_worker(proto);

	ptr = cur = data;
	end = data + len;

/*printf ("SENDING BYTES [%.*s]\n", (int)len, data);*/
	do
	{
		int nv;

		while (cur != end && cur - ptr < HCL_XPKT_MAX_PLD_LEN) cur++;

		seglen = cur - ptr;

		hdr.id = 1; /* TODO: */
		hdr.type = xpkt_type | (((seglen >> 8) & 0x0F) << 4);
		hdr.len = seglen & 0xFF;

		nv = 0;
		iov[nv].iov_base = &hdr;
		iov[nv++].iov_len = HCL_SIZEOF(hdr);
		if (seglen > 0)
		{
			iov[nv].iov_base = ptr;
			iov[nv++].iov_len = seglen;
		}

		if (hcl_sys_send_iov(worker->sck, iov, nv) <= -1)
		{
			/* TODO: error message */
fprintf (stderr, "Unable to sendmsg on %d - %s\n", worker->sck, strerror(errno));
			return -1;
		}

		ptr = cur;
	}
	while (ptr < end);

	return 0;
}

#if defined(HCL_OOCH_IS_UCH)
static int send_chars (hcl_xproto_t* proto, hcl_xpkt_type_t xpkt_type, const hcl_ooch_t* data, hcl_oow_t len)
{
	hcl_server_worker_t* worker;
	const hcl_ooch_t* ptr, * end;
	hcl_bch_t tmp[256];
	hcl_oow_t tln, pln;
	int n;

	worker = proto_to_worker(proto);

	ptr = data;
	end = data + len;

	while (ptr < end)
	{
		pln = end - ptr;
		tln = HCL_COUNTOF(tmp);
		n = hcl_convutobchars(worker->hcl, ptr, &pln, tmp, &tln);
		if (n <= -1 && n != -2) return -1;

		if (send_bytes(proto, xpkt_type, tmp, tln) <= -1) return -1;
		ptr += pln;
	}

	return 0;

}
#endif

/* ========================================================================= */

hcl_server_t* hcl_server_open (hcl_mmgr_t* mmgr, hcl_oow_t xtnsize, hcl_server_prim_t* prim, hcl_errnum_t* errnum)
{
	hcl_server_t* server = HCL_NULL;
	hcl_t* hcl = HCL_NULL;
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

	hcl = hcl_openstdwithmmgr(mmgr, HCL_SIZEOF(*xtn), errnum);
	if (!hcl) goto oops;

	/* replace the vmprim.log_write function */
	hcl->vmprim.log_write = server_log_write_for_dummy;

	tmr = hcl_tmr_open(hcl, 0, 1024); /* TOOD: make the timer's default size configurable */
	if (!tmr)
	{
		if (errnum) *errnum = HCL_ESYSMEM;
		goto oops;
	}

	if (hcl_sys_open_pipes(pfd, 1) <= -1)
	{
		if (errnum) *errnum = hcl->vmprim.syserrstrb(hcl, 0, errno, HCL_NULL, 0);
		goto oops;
	}

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
	if (server->cfg.trait & HCL_SERVER_TRAIT_DEBUG_BIGINT) trait |= HCL_TRAIT_DEBUG_BIGINT;
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

	hcl_sys_close_pipes (server->mux_pipe);

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
	worker->exec_runtime_event_index = HCL_TMR_INVALID_INDEX;

	server->dummy_hcl->vmprim.vm_gettime (server->dummy_hcl, &worker->alloc_time); /* TODO: the callback may return monotonic time. find a way to guarantee it is realtime??? */

	if (server->wid_map.free_first == HCL_SERVER_WID_INVALID && prepare_to_acquire_wid(server) <= -1)
	{
		hcl_server_freemem (server, worker);
		return HCL_NULL;
	}

	acquire_wid (server, worker);
	return worker;
}

static void fini_worker_socket (hcl_server_worker_t* worker)
{
	if (worker->sck >= 0)
	{
		if (worker->hcl)
		{
			HCL_LOG2 (worker->hcl, SERVER_LOGMASK_INFO, "Closing worker socket %d [%zu]\n", worker->sck, worker->wid);
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
	fini_worker_socket (worker);

	if (worker->hcl)
	{
		HCL_LOG1 (worker->hcl, SERVER_LOGMASK_INFO, "Killing worker [%zu]\n", worker->wid);
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

static int worker_step (hcl_server_worker_t* worker)
{
	hcl_xproto_t* proto = worker->proto;
	hcl_server_t* server = worker->server;
	hcl_t* hcl = worker->hcl;
	struct pollfd pfd;
	int tmout, actual_tmout;
	ssize_t x;
	int n;

	//HCL_ASSERT (hcl, proto->rcv.len < proto->rcv.len_needed);

	if (HCL_UNLIKELY(hcl_xproto_geteof(proto)))
	{
// TODO: may not be an error if writable needs to be checked...
		hcl_seterrbfmt (hcl, HCL_EGENERIC, "connection closed");
		return -1;
	}

	tmout = HCL_SECNSEC_TO_MSEC(server->cfg.worker_idle_timeout.sec, server->cfg.worker_idle_timeout.nsec);
	actual_tmout = (tmout <= 0)? 10000: tmout;

	pfd.fd = worker->sck;
	pfd.events = 0;
	if (!hcl_xproto_ready(proto)) pfd.events |= POLLIN;
	//if (proto->snd.len > 0) pfd.events |= POLLOUT;

	if (pfd.events)
	{
		n = poll(&pfd, 1, actual_tmout);
		if (n <= -1)
		{
			if (errno == EINTR) return 0;
			hcl_seterrwithsyserr (hcl, 0, errno);
			return -1;
		}
		else if (n == 0)
		{
			/* timed out - no activity on the pfd */
			if (tmout > 0)
			{
				/* timeout explicity set. no activity for that duration. considered idle */
				hcl_seterrbfmt (hcl, HCL_EGENERIC, "no activity on the worker socket %d", worker->sck);
				return -1;
			}

			goto carry_on;
		}

		if (pfd.revents & POLLERR)
		{
			hcl_seterrbfmt (hcl, HCL_EGENERIC, "error condition detected on workder socket %d", worker->sck);
			return -1;
		}

		if (pfd.revents & POLLOUT)
		{
		}

		if (pfd.revents & POLLIN)
		{
			hcl_oow_t bcap;
			hcl_uint8_t* bptr;

			bptr = hcl_xproto_getbuf(proto, &bcap);;
			x = recv(worker->sck, bptr, bcap, 0);
			if (x <= -1)
			{
				if (errno == EINTR) goto carry_on; /* didn't read read */
				hcl_seterrwithsyserr (hcl, 0, errno);
				return -1;
			}

			if (x == 0) hcl_xproto_seteof(proto, 1);
			hcl_xproto_advbuf (proto, x);
		}
	}

	/* the receiver buffer has enough data */
	while (hcl_xproto_ready(worker->proto))
	{
		if ((n = hcl_xproto_process(worker->proto)) <= -1)
		{
			/* TODO: proper error message */
			return -1;
		}
		if (n == 0)
		{
			/* TODO: chceck if there is remaining data in the buffer...?? */
			return 0; /* tell the caller to break the step loop */
		}
	}

carry_on:
	return 1; /* carry on */
}

static int init_worker_hcl (hcl_server_worker_t* worker)
{
	hcl_server_t* server = worker->server;
	hcl_t* hcl;
	worker_hcl_xtn_t* xtn;
	hcl_bitmask_t trait;
	hcl_cb_t hclcb;

	hcl = hcl_openstdwithmmgr(hcl_server_getmmgr(server), HCL_SIZEOF(*xtn), HCL_NULL);
	if (HCL_UNLIKELY(!hcl)) goto oops;

	/* replace the vmprim.log_write function */
	hcl->vmprim.log_write = server_log_write;

	xtn = (worker_hcl_xtn_t*)hcl_getxtn(hcl);
	xtn->worker = worker;

	hcl_setoption (hcl, HCL_MOD_INCTX, &server->cfg.module_inctx);
	hcl_setoption (hcl, HCL_LOG_MASK, &server->cfg.logmask);
	hcl_setcmgr (hcl, hcl_server_getcmgr(server));

	hcl_getoption (hcl, HCL_TRAIT, &trait);
#if defined(HCL_BUILD_DEBUG)
	if (server->cfg.trait & HCL_SERVER_TRAIT_DEBUG_GC) trait |= HCL_TRAIT_DEBUG_GC;
	if (server->cfg.trait & HCL_SERVER_TRAIT_DEBUG_BIGINT) trait |= HCL_TRAIT_DEBUG_BIGINT;
#endif
	trait |= HCL_TRAIT_LANG_ENABLE_EOL;
	hcl_setoption (hcl, HCL_TRAIT, &trait);

	HCL_MEMSET (&hclcb, 0, HCL_SIZEOF(hclcb));
	/*hclcb.fini = fini_hcl;
	hclcb.gc = gc_hcl;*/
	hclcb.vm_startup =  vm_startup;
	hclcb.vm_cleanup = vm_cleanup;
	hclcb.vm_checkbc = vm_checkbc;
	hcl_regcb (hcl, &hclcb);

	if (hcl_ignite(hcl, server->cfg.actor_heap_size) <= -1) goto oops;
	if (hcl_addbuiltinprims(hcl) <= -1) goto oops;

	if (hcl_attachccio(hcl, read_handler) <= -1) goto oops;
	if (hcl_attachudio(hcl, scan_handler, print_handler) <= -1) goto oops;

	if (hcl_beginfeed(hcl, on_fed_cnode) <= -1) goto oops;

	worker->hcl = hcl;
	return 0;

oops:
	if (hcl) hcl_close (hcl);
	return -1;
}


static void fini_worker_hcl (hcl_server_worker_t* worker)
{
	if (HCL_LIKELY(worker->hcl))
	{
		hcl_endfeed (worker->hcl);
		hcl_close (worker->hcl);
		worker->hcl = HCL_NULL;
	}
}


static int init_worker_proto (hcl_server_worker_t* worker)
{
	hcl_xproto_t* proto;
	proto_xtn_t* xtn;
	hcl_xproto_cb_t cb;

	HCL_MEMSET (&cb, 0, HCL_SIZEOF(cb));
	cb.on_packet = server_on_packet;

	proto = hcl_xproto_open(hcl_server_getmmgr(worker->server), &cb, HCL_SIZEOF(*xtn));
	if (HCL_UNLIKELY(!proto)) return -1;

	xtn = hcl_xproto_getxtn(proto);
	xtn->worker = worker;

	worker->proto = proto;
	return 0;
}

static void fini_worker_proto (hcl_server_worker_t* worker)
{
	if (HCL_LIKELY(worker->proto))
	{
		hcl_xproto_close (worker->proto);
		worker->proto = HCL_NULL;
	}
}

static void* worker_main (void* ctx)
{
	hcl_server_worker_t* worker = (hcl_server_worker_t*)ctx;
	hcl_server_t* server = worker->server;
	sigset_t set;
	int n;

	sigfillset (&set);
	pthread_sigmask (SIG_BLOCK, &set, HCL_NULL);

	worker->thr = pthread_self();

	n = init_worker_hcl(worker);
	if (HCL_UNLIKELY(n <= -1))
	{
		/* TODO: capture error ... */
		return HCL_NULL;
	}

	n = init_worker_proto(worker);
	if (HCL_UNLIKELY(n <= -1))
	{
		fini_worker_hcl (worker);
		return HCL_NULL;
	}

	pthread_mutex_lock (&server->worker_mutex);
	add_worker_to_server (server, HCL_SERVER_WORKER_STATE_ALIVE, worker);
	pthread_mutex_unlock (&server->worker_mutex);

	/* the worker loop */
	while (!server->stopreq)
	{
		int n;
		worker->opstate = HCL_SERVER_WORKER_OPSTATE_WAIT;

		if ((n = worker_step(worker)) <= 0)
		{
			worker->opstate = (n <= -1)? HCL_SERVER_WORKER_OPSTATE_ERROR: HCL_SERVER_WORKER_OPSTATE_IDLE;
			break;
		}
	}

	hcl_xproto_close (worker->proto);
	worker->proto = HCL_NULL;

	fini_worker_hcl (worker);

	pthread_mutex_lock (&server->worker_mutex);
	fini_worker_socket (worker);
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
#if defined(USE_EPOLL)
	struct epoll_event dummy_ev;

	epoll_ctl (server->listener.ep_fd, EPOLL_CTL_DEL, server->mux_pipe[0], &dummy_ev);
#endif

	while (server->listener.head)
	{
		lp = server->listener.head;
		server->listener.head = lp->next_listener;
		server->listener.count--;

#if defined(USE_EPOLL)
		epoll_ctl (server->listener.ep_fd, EPOLL_CTL_DEL, lp->sck, &dummy_ev);
#endif
		close (lp->sck);
		hcl_server_freemem (server, lp);
	}

#if defined(USE_EPOLL)
	HCL_ASSERT (server->dummy_hcl, server->listener.ep_fd >= 0);
	close (server->listener.ep_fd);
	server->listener.ep_fd = -1;
#endif
}

static int setup_listeners (hcl_server_t* server, const hcl_bch_t* addrs)
{
	const hcl_bch_t* addr_ptr, * comma;
	int ep_fd, fcv;
#if defined(USE_EPOLL)
	struct epoll_event ev;

	ep_fd = epoll_create(1024);
	if (ep_fd <= -1)
	{
		set_err_with_syserr (server, 0, errno, "unable to create multiplexer");
		HCL_LOG1 (server->dummy_hcl, SERVER_LOGMASK_ERROR, "%js\n", hcl_server_geterrmsg(server));
		return -1;
	}

	hcl_sys_set_cloexec(ep_fd, 1);

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
#endif
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
		hcl_sys_set_nonblock (srv_fd, 1); /* the listening socket is non-blocking unlike accepted sockets */
		hcl_sys_set_cloexec (srv_fd, 1);

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


#if defined(USE_EPOLL)
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
#endif

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

#if defined(USE_EPOLL)
		n = epoll_wait(server->listener.ep_fd, server->listener.ev_buf, HCL_COUNTOF(server->listener.ev_buf), HCL_SECNSEC_TO_MSEC(tmout.sec, tmout.nsec));
#else
		n = poll(); /* TODO: */
#endif

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
#if defined(USE_EPOLL)
			struct epoll_event* evp;
#endif

			--n;

#if defined(USE_EPOLL)
			evp = &server->listener.ev_buf[n];
			if (!evp->events /*& (POLLIN | POLLHUP | POLLERR) */) continue;
#else

			/* TODO: */
#endif

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
					if (hcl_sys_is_errno_wb(errno)) continue;
					set_err_with_syserr (server, 0, errno, "unable to accept worker on server socket %d", evp->data.fd);
					xret = -1;
					break;
				}

				hcl_sys_set_nonblock (cli_fd, 0); /* force the accepted socket to be blocking */
				hcl_sys_set_cloexec (cli_fd, 1);

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
				if (server->cfg.trait & HCL_SERVER_TRAIT_DEBUG_BIGINT) trait |= HCL_TRAIT_DEBUG_BIGINT;
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
