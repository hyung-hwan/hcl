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
#include <hcl-utl.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#	include <windows.h>
#	include <tchar.h>
#	include <time.h>
#	include <io.h>
#	include <errno.h>

#	if defined(HCL_HAVE_CFG_H) && defined(HCL_ENABLE_LIBLTDL)
#		include <ltdl.h>
#		define USE_LTDL
#	else
#		define USE_WIN_DLL
#	endif


#	include "poll-msw.h"
#	define USE_POLL
#	define XPOLLIN POLLIN
#	define XPOLLOUT POLLOUT
#	define XPOLLERR POLLERR
#	define XPOLLHUP POLLHUP


#if !defined(SIZE_T)
#	define SIZE_T unsigned long int
#endif

#elif defined(__OS2__)
#	define INCL_DOSMODULEMGR
#	define INCL_DOSPROCESS
#	define INCL_DOSSEMAPHORES
#	define INCL_DOSEXCEPTIONS
#	define INCL_DOSMISC
#	define INCL_DOSDATETIME
#	define INCL_DOSFILEMGR
#	define INCL_DOSERRORS
#	include <os2.h>
#	include <time.h>
#	include <fcntl.h>
#	include <io.h>

#	include <errno.h>

	/* fake XPOLLXXX values */
#	define XPOLLIN  (1 << 0)
#	define XPOLLOUT (1 << 1)
#	define XPOLLERR (1 << 2)
#	define XPOLLHUP (1 << 3)

#elif defined(__DOS__)
#	include <dos.h>
#	include <time.h>
#	include <io.h>
#	include <signal.h>
#	include <errno.h>
#	include <fcntl.h>
#	include <conio.h> /* inp, outp */

#	if defined(_INTELC32_)
#		define DOS_EXIT 0x4C
#		include <i32.h>
#		include <stk.h>
#	else
#		include <dosfunc.h>
#	endif

	/* fake XPOLLXXX values */
#	define XPOLLIN  (1 << 0)
#	define XPOLLOUT (1 << 1)
#	define XPOLLERR (1 << 2)
#	define XPOLLHUP (1 << 3)

#elif defined(macintosh)
#	include <Types.h>
#	include <OSUtils.h>
#	include <Timer.h>

#	include <MacErrors.h>
#	include <Process.h>
#	include <Dialogs.h>
#	include <TextUtils.h>

#else

#	include <sys/types.h>
#	include <unistd.h>
#	include <fcntl.h>
#	include <errno.h>

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

#	if defined(USE_THREAD)
#		include <pthread.h>
#		include <sched.h>
#	endif

#	if defined(HAVE_SYS_DEVPOLL_H)
		/* solaris */
#		include <sys/devpoll.h>
#		define USE_DEVPOLL
#		define XPOLLIN POLLIN
#		define XPOLLOUT POLLOUT
#		define XPOLLERR POLLERR
#		define XPOLLHUP POLLHUP
#	elif defined(HAVE_SYS_EVENT_H) && defined(HAVE_KQUEUE)
		/* netbsd, openbsd, etc */
#		include <sys/event.h>
#		define USE_KQUEUE
		/* fake XPOLLXXX values */
#		define XPOLLIN  (1 << 0)
#		define XPOLLOUT (1 << 1)
#		define XPOLLERR (1 << 2)
#		define XPOLLHUP (1 << 3)
#	elif defined(HAVE_SYS_EPOLL_H) && defined(HAVE_EPOLL_CREATE)
		/* linux */
#		include <sys/epoll.h>
#		define USE_EPOLL
#		define XPOLLIN EPOLLIN
#		define XPOLLOUT EPOLLOUT
#		define XPOLLERR EPOLLERR
#		define XPOLLHUP EPOLLHUP
#	elif defined(HAVE_POLL_H)
#		include <poll.h>
#		define USE_POLL
#		define XPOLLIN POLLIN
#		define XPOLLOUT POLLOUT
#		define XPOLLERR POLLERR
#		define XPOLLHUP POLLHUP
#	else
#		define USE_SELECT
		/* fake XPOLLXXX values */
#		define XPOLLIN  (1 << 0)
#		define XPOLLOUT (1 << 1)
#		define XPOLLERR (1 << 2)
#		define XPOLLHUP (1 << 3)
#	endif
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

#if defined(USE_THREAD)
#	define MUTEX_INIT(x) pthread_mutex_init((x), MOO_NULL)
#	define MUTEX_DESTROY(x) pthread_mutex_destroy(x)
#	define MUTEX_LOCK(x) pthread_mutex_lock(x)
#	define MUTEX_UNLOCK(x) pthread_mutex_unlock(x)
#else
#	define MUTEX_INIT(x)
#	define MUTEX_DESTROY(x)
#	define MUTEX_LOCK(x) 
#	define MUTEX_UNLOCK(x) 
#endif


typedef struct xtn_t xtn_t;
struct xtn_t
{
	hcl_t* next;
	hcl_t* prev;

	int vm_running;
	int rcv_tick;

	hcl_cmgr_t* input_cmgr;
	hcl_cmgr_t* log_cmgr;

	struct
	{
		int fd;
		int fd_flag; /* bitwise OR'ed fo logfd_flag_t bits */

		struct
		{
			hcl_bch_t buf[4096];
			hcl_oow_t len;
		} out;
	} log;

	#if defined(_WIN32)
	HANDLE waitable_timer;
	DWORD tc_last;
	DWORD tc_overflow;
	#elif defined(__OS2__)
	ULONG tc_last;
	hcl_ntime_t tc_last_ret;
	#elif defined(__DOS__)
	clock_t tc_last;
	hcl_ntime_t tc_last_ret;
	#endif

	#if defined(USE_DEVPOLL)
	int ep; /* /dev/poll */
	#elif defined(USE_KQUEUE)
	int ep; /* kqueue */
	#elif defined(USE_EPOLL)
	int ep; /* epoll */
	#elif defined(USE_POLL)
	/* nothing */
	#elif defined(USE_SELECT)
	/* nothing */
	#endif

	#if defined(USE_THREAD)
	struct
	{
		int p[2]; /* pipe for signaling */
		pthread_t thr;
		int up;
		int abort;
	} iothr;
	#endif

	struct
	{
		int p[2]; /* pipe for signaling */
	} sigfd;

	struct
	{
	#if defined(USE_DEVPOLL)
		/*TODO: make it dynamically changeable depending on the number of
		 *      file descriptors added */
		struct pollfd buf[64]; /* buffer for reading events */
	#elif defined(USE_KQUEUE)
		struct
		{
			hcl_oow_t* ptr;
			hcl_oow_t capa;
		} reg;
		struct kevent buf[64]; 
	#elif defined(USE_EPOLL) 
		/*TODO: make it dynamically changeable depending on the number of
		 *      file descriptors added */
		struct epoll_event buf[64]; /* buffer for reading events */
	#elif defined(USE_POLL)
		struct
		{
			struct pollfd* ptr;
			hcl_oow_t capa;
			hcl_oow_t len;
		#if defined(USE_THREAD)
			pthread_mutex_t pmtx;
		#endif
		} reg; /* registry */
		struct pollfd* buf;

	#elif defined(USE_SELECT)
		struct
		{
			fd_set rfds;
			fd_set wfds;
			int maxfd;
		#if defined(USE_THREAD)
			pthread_mutex_t smtx;
		#endif
		} reg;

		struct select_fd_t buf[FD_SETSIZE];
	#endif

		hcl_oow_t len;

	#if defined(USE_THREAD)
		pthread_mutex_t mtx;
		pthread_cond_t cnd;
		pthread_cond_t cnd2;
	#endif

		int halting;
	} ev;
};

#define GET_XTN(hcl) ((xtn_t*)((hcl_uint8_t*)hcl_getxtn(hcl) - HCL_SIZEOF(xtn_t)))


/* ----------------------------------------------------------------- 
 * BASIC MEMORY MANAGER
 * ----------------------------------------------------------------- */

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

/* ----------------------------------------------------------------- 
 * LOGGING SUPPORT
 * ----------------------------------------------------------------- */

enum logfd_flag_t
{
	LOGFD_TTY = (1 << 0),
	LOGFD_OPENED_HERE = (1 << 1)
};

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
	xtn_t* xtn = GET_XTN(hcl);

	while (len > 0)
	{
		if (xtn->log.out.len > 0)
		{
			hcl_oow_t rcapa, cplen;

			rcapa = HCL_COUNTOF(xtn->log.out.buf) - xtn->log.out.len;
			cplen = (len >= rcapa)? rcapa: len;

			HCL_MEMCPY (&xtn->log.out.buf[xtn->log.out.len], ptr, cplen);
			xtn->log.out.len += cplen;
			ptr += cplen;
			len -= cplen;

			if (xtn->log.out.len >= HCL_COUNTOF(xtn->log.out.buf))
			{
				int n;
				n = write_all(fd, xtn->log.out.buf, xtn->log.out.len);
				xtn->log.out.len = 0;
				if (n <= -1) return -1;
			}
		}
		else
		{
			hcl_oow_t rcapa;

			rcapa = HCL_COUNTOF(xtn->log.out.buf);
			if (len >= rcapa)
			{
				if (write_all(fd, ptr, rcapa) <= -1) return -1;
				ptr += rcapa;
				len -= rcapa;
			}
			else
			{
				HCL_MEMCPY (xtn->log.out.buf, ptr, len);
				xtn->log.out.len += len;
				ptr += len;
				len -= len;
			}
		}
	}

	return 0;
}

static void flush_log (hcl_t* hcl, int fd)
{
	xtn_t* xtn = GET_XTN(hcl);
	if (xtn->log.out.len > 0)
	{
		write_all (fd, xtn->log.out.buf, xtn->log.out.len);
		xtn->log.out.len = 0;
	}
}


static void log_write (hcl_t* hcl, hcl_bitmask_t mask, const hcl_ooch_t* msg, hcl_oow_t len)
{
	hcl_bch_t buf[256];
	hcl_oow_t ucslen, bcslen, msgidx;
	int n;

	xtn_t* xtn = GET_XTN(hcl);
	int logfd;

	if (mask & HCL_LOG_STDERR) logfd = 2;
	else if (mask & HCL_LOG_STDOUT) logfd = 1;
	else
	{
		logfd = xtn->log.fd;
		if (logfd <= -1) return;
	}

/* TODO: beautify the log message.
 *       do classification based on mask. */
	if (!(mask & (HCL_LOG_STDOUT | HCL_LOG_STDERR)))
	{
		time_t now;
		char ts[32];
		size_t tslen;
		struct tm tm, *tmp;

		now = time(HCL_NULL);
	#if defined(_WIN32)
		tmp = localtime(&now);
		tslen = strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S %z ", tmp);
		if (tslen == 0) 
		{
			tslen = sprintf(ts, "%04d-%02d-%02d %02d:%02d:%02d ", tmp->tm_year + 1900, tmp->tm_mon + 1, tmp->tm_mday, tmp->tm_hour, tmp->tm_min, tmp->tm_sec);
		}
	#elif defined(__OS2__)
		#if defined(__WATCOMC__)
		tmp = _localtime(&now, &tm);
		#else
		tmp = localtime(&now);
		#endif

		#if defined(__BORLANDC__)
		/* the borland compiler doesn't handle %z properly - it showed 00 all the time */
		tslen = strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S %Z ", tmp);
		#else
		tslen = strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S %z ", tmp);
		#endif
		if (tslen == 0) 
		{
			tslen = sprintf(ts, "%04d-%02d-%02d %02d:%02d:%02d ", tmp->tm_year + 1900, tmp->tm_mon + 1, tmp->tm_mday, tmp->tm_hour, tmp->tm_min, tmp->tm_sec);
		}

	#elif defined(__DOS__)
		tmp = localtime(&now);
		/* since i know that %z/%Z is not available in strftime, i switch to sprintf immediately */
		tslen = sprintf(ts, "%04d-%02d-%02d %02d:%02d:%02d ", tmp->tm_year + 1900, tmp->tm_mon + 1, tmp->tm_mday, tmp->tm_hour, tmp->tm_min, tmp->tm_sec);
	#else
		#if defined(HAVE_LOCALTIME_R)
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
			tslen = sprintf(ts, "%04d-%02d-%02d %02d:%02d:%02d ", tmp->tm_year + 1900, tmp->tm_mon + 1, tmp->tm_mday, tmp->tm_hour, tmp->tm_min, tmp->tm_sec);
		}
	#endif
		write_log (hcl, logfd, ts, tslen);
	}

	if (logfd == xtn->log.fd && (xtn->log.fd_flag & LOGFD_TTY))
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

	if (logfd == xtn->log.fd && (xtn->log.fd_flag & LOGFD_TTY))
	{
		if (mask & (HCL_LOG_FATAL | HCL_LOG_ERROR | HCL_LOG_WARN)) write_log (hcl, logfd, "\x1B[0m", 4);
	}

	flush_log (hcl, logfd);
}

/* ----------------------------------------------------------------- 
 * SYSTEM ERROR CONVERSION
 * ----------------------------------------------------------------- */
static hcl_errnum_t errno_to_errnum (int errcode)
{
	switch (errcode)
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
}

#if defined(_WIN32)
static hcl_errnum_t winerr_to_errnum (DWORD errcode)
{
	switch (errcode)
	{
		case ERROR_NOT_ENOUGH_MEMORY:
		case ERROR_OUTOFMEMORY:
			return HCL_ESYSMEM;

		case ERROR_INVALID_PARAMETER:
		case ERROR_INVALID_NAME:
			return HCL_EINVAL;

		case ERROR_INVALID_HANDLE:
			return HCL_EBADHND;

		case ERROR_ACCESS_DENIED:
		case ERROR_SHARING_VIOLATION:
			return HCL_EACCES;

		case ERROR_FILE_NOT_FOUND:
		case ERROR_PATH_NOT_FOUND:
			return HCL_ENOENT;

		case ERROR_ALREADY_EXISTS:
		case ERROR_FILE_EXISTS:
			return HCL_EEXIST;

		case ERROR_BROKEN_PIPE:
			return HCL_EPIPE;

		default:
			return HCL_ESYSERR;
	}
}
#endif

#if defined(__OS2__)
static hcl_errnum_t os2err_to_errnum (APIRET errcode)
{
	/* APIRET e */
	switch (errcode)
	{
		case ERROR_NOT_ENOUGH_MEMORY:
			return HCL_ESYSMEM;

		case ERROR_INVALID_PARAMETER: 
		case ERROR_INVALID_NAME:
			return HCL_EINVAL;

		case ERROR_INVALID_HANDLE: 
			return HCL_EBADHND;

		case ERROR_ACCESS_DENIED: 
		case ERROR_SHARING_VIOLATION:
			return HCL_EACCES;

		case ERROR_FILE_NOT_FOUND:
		case ERROR_PATH_NOT_FOUND:
			return HCL_ENOENT;

		case ERROR_ALREADY_EXISTS:
			return HCL_EEXIST;

		/*TODO: add more mappings */
		default:
			return HCL_ESYSERR;
	}
}
#endif

#if defined(macintosh)
static hcl_errnum_t macerr_to_errnum (int errcode)
{
	switch (e)
	{
		case notEnoughMemoryErr:
			return HCL_ESYSMEM;
		case paramErr:
			return HCL_EINVAL;

		case qErr: /* queue element not found during deletion */
		case fnfErr: /* file not found */
		case dirNFErr: /* direcotry not found */
		case resNotFound: /* resource not found */
		case resFNotFound: /* resource file not found */
		case nbpNotFound: /* name not found on remove */
			return HCL_ENOENT;

		/*TODO: add more mappings */
		default: 
			return HCL_ESYSERR;
	}
}
#endif

static hcl_errnum_t _syserrstrb (hcl_t* hcl, int syserr_type, int syserr_code, hcl_bch_t* buf, hcl_oow_t len)
{
	switch (syserr_type)
	{
		case 1: 
		#if defined(_WIN32)
			if (buf)
			{
				DWORD rc;
				rc = FormatMessageA (
					FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
					NULL, syserr_code, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), 
					buf, len, HCL_NULL
				);
				while (rc > 0 && buf[rc - 1] == '\r' || buf[rc - 1] == '\n') buf[--rc] = '\0';
			}
			return winerr_to_errnum(syserr_code);
		#elif defined(__OS2__)
			/* TODO: convert code to string */
			if (buf) hcl_copy_bcstr (buf, len, "system error");
			return os2err_to_errnum(syserr_code);
		#elif defined(macintosh)
			/* TODO: convert code to string */
			if (buf) hcl_copy_bcstr (buf, len, "system error");
			return os2err_to_errnum(syserr_code);
		#else
			/* in other systems, errno is still the native system error code.
			 * fall thru */
		#endif

		case 0:
		#if defined(HAVE_STRERROR_R)
			if (buf) strerror_r (syserr_code, buf, len);
		#else
			/* this is not thread safe */
			if (buf) hcl_copy_bcstr (buf, len, strerror(syserr_code));
		#endif
			return errno_to_errnum(syserr_code);
	}

	if (buf) hcl_copy_bcstr (buf, len, "system error");
	return HCL_ESYSERR;
}


/* -------------------------------------------------------------------------- 
 * ASSERTION SUPPORT
 * -------------------------------------------------------------------------- */

#if defined(HCL_BUILD_RELEASE)

static void _assertfail (hcl_t* hcl, const hcl_bch_t* expr, const hcl_bch_t* file, hcl_oow_t line)
{
	/* do nothing */
}

#else /* defined(HCL_BUILD_RELEASE) */

/* -------------------------------------------------------------------------- 
 * SYSTEM DEPENDENT HEADERS
 * -------------------------------------------------------------------------- */

#if defined(_WIN32)
#	include <windows.h>
#	include <errno.h>
#elif defined(__OS2__)

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

#if defined(HCL_ENABLE_LIBUNWIND)
#include <libunwind.h>
static void backtrace_stack_frames (hcl_t* hcl)
{
	unw_cursor_t cursor;
	unw_context_t context;
	int n;

	unw_getcontext(&context);
	unw_init_local(&cursor, &context);

	hcl_logbfmt (hcl, HCL_LOG_UNTYPED | HCL_LOG_DEBUG, "[BACKTRACE]\n");
	for (n = 0; unw_step(&cursor) > 0; n++) 
	{
		unw_word_t ip, sp, off;
		char symbol[256];

		unw_get_reg (&cursor, UNW_REG_IP, &ip);
		unw_get_reg (&cursor, UNW_REG_SP, &sp);

		if (unw_get_proc_name(&cursor, symbol, HCL_COUNTOF(symbol), &off)) 
		{
			hcl_copy_bcstr (symbol, HCL_COUNTOF(symbol), "<unknown>");
		}

		hcl_logbfmt (hcl, HCL_LOG_UNTYPED | HCL_LOG_DEBUG, 
			"#%02d ip=0x%*p sp=0x%*p %hs+0x%zu\n", 
			n, HCL_SIZEOF(void*) * 2, (void*)ip, HCL_SIZEOF(void*) * 2, (void*)sp, symbol, (hcl_oow_t)off);
	}
}
#elif defined(HAVE_BACKTRACE)
#include <execinfo.h>
static void backtrace_stack_frames (hcl_t* hcl)
{
	void* btarray[128];
	hcl_oow_t btsize;
	char** btsyms;

	btsize = backtrace (btarray, HCL_COUNTOF(btarray));
	btsyms = backtrace_symbols (btarray, btsize);
	if (btsyms)
	{
		hcl_oow_t i;
		hcl_logbfmt (hcl, HCL_LOG_UNTYPED | HCL_LOG_DEBUG, "[BACKTRACE]\n");

		for (i = 0; i < btsize; i++)
		{
			hcl_logbfmt (hcl, HCL_LOG_UNTYPED | HCL_LOG_DEBUG, "  %hs\n", btsyms[i]);
		}
		free (btsyms);
	}
}
#else
static void backtrace_stack_frames (hcl_t* hcl)
{
	/* do nothing. not supported */
}
#endif

static void _assertfail (hcl_t* hcl, const hcl_bch_t* expr, const hcl_bch_t* file, hcl_oow_t line)
{
	hcl_logbfmt (hcl, HCL_LOG_UNTYPED | HCL_LOG_FATAL, "ASSERTION FAILURE: %hs at %hs:%zu\n", expr, file, line);
	backtrace_stack_frames (hcl);

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

#endif /* defined(HCL_BUILD_RELEASE) */


/* ----------------------------------------------------------------- 
 * HEAP ALLOCATION
 * ----------------------------------------------------------------- */

static int get_huge_page_size (hcl_t* hcl, hcl_oow_t* page_size)
{
	FILE* fp;
	char buf[256];
	
	fp = fopen("/proc/meminfo", "r");
	if (!fp) return -1;

	while (!feof(fp))
	{
		if (fgets(buf, sizeof(buf) - 1, fp) == NULL) goto oops;

		if (strncmp(buf, "Hugepagesize: ", 13) == 0)
		{
			unsigned long int tmp;
			tmp = strtoul(&buf[13], NULL, 10);
			if (tmp == HCL_TYPE_MAX(unsigned long int) && errno == ERANGE) goto oops;

			*page_size = tmp * 1024; /* KBytes to Bytes */
			fclose (fp);
			return 0;
		}
	}

oops:
	fclose (fp);
	return -1;
}

static void* alloc_heap (hcl_t* hcl, hcl_oow_t* size)
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
	hcl_oow_t req_size, align, aligned_size;

	req_size = HCL_SIZEOF(hcl_oow_t) + *size;
	flags = MAP_PRIVATE | MAP_ANONYMOUS;

	#if defined(MAP_UNINITIALIZED)
	flags |= MAP_UNINITIALIZED;
	#endif

	#if defined(MAP_HUGETLB)
	if (get_huge_page_size(hcl, &align) <= -1) align = 2 * 1024 * 1024; /* default to 2MB */
	if (req_size > align / 2)
	{
		/* if the requested size is large enough, attempt HUGETLB */
		flags |= MAP_HUGETLB;
	}
	else
	{
		align = sysconf(_SC_PAGESIZE);
	}
	#else
	align = sysconf(_SC_PAGESIZE);
	#endif

	aligned_size = HCL_ALIGN_POW2(req_size, align);
	ptr = (hcl_oow_t*)mmap(NULL, aligned_size, PROT_READ | PROT_WRITE, flags, -1, 0);
	#if defined(MAP_HUGETLB)
	if (ptr == MAP_FAILED && (flags & MAP_HUGETLB)) 
	{
		flags &= ~MAP_HUGETLB;
		align = sysconf(_SC_PAGESIZE);
		aligned_size = HCL_ALIGN_POW2(req_size, align);
		ptr = (hcl_oow_t*)mmap(NULL, aligned_size, PROT_READ | PROT_WRITE, flags, -1, 0);
		if (ptr == MAP_FAILED) 
		{
			hcl_seterrwithsyserr (hcl, 0, errno);
			return HCL_NULL;
		}
	}
	#else
	if (ptr == MAP_FAILED) 
	{
		hcl_seterrwithsyserr (hcl, 0, errno);
		return HCL_NULL;
	}
	#endif

	*ptr = aligned_size;
	*size = aligned_size - HCL_SIZEOF(hcl_oow_t);

	return (void*)(ptr + 1);

#else
	return HCL_MMGR_ALLOC(hcl->_mmgr, *size);
#endif
}

static void free_heap (hcl_t* hcl, void* ptr)
{
#if defined(HAVE_MMAP) && defined(HAVE_MUNMAP)
	hcl_oow_t* actual_ptr;
	actual_ptr = (hcl_oow_t*)ptr - 1;
	munmap (actual_ptr, *actual_ptr);
#else
	return HCL_MMGR_FREE(hcl->_mmgr, ptr);
#endif
}

/* ----------------------------------------------------------------- 
 * POSSIBLY MONOTONIC TIME
 * ----------------------------------------------------------------- */

void vm_gettime (hcl_t* hcl, hcl_ntime_t* now)
{
#if defined(_WIN32)

	#if defined(_WIN64) || (defined(_WIN32_WINNT) && (_WIN32_WINNT >= 0x0600))
	hcl_uint64_t bigsec, bigmsec;
	bigmsec = GetTickCount64();
	#else
	xtn_t* xtn = GET_XTN(hcl);
	hcl_uint64_t bigsec, bigmsec;
	DWORD msec;

	msec = GetTickCount(); /* this can sustain for 49.7 days */
	if (msec < xtn->tc_last)
	{
		/* i assume the difference is never bigger than 49.7 days */
		/*diff = (HCL_TYPE_MAX(DWORD) - xtn->tc_last) + 1 + msec;*/
		xtn->tc_overflow++;
		bigmsec = ((hcl_uint64_t)HCL_TYPE_MAX(DWORD) * xtn->tc_overflow) + msec;
	}
	else bigmsec = msec;
	xtn->tc_last = msec;
	#endif

	bigsec = HCL_MSEC_TO_SEC(bigmsec);
	bigmsec -= HCL_SEC_TO_MSEC(bigsec);
	HCL_INIT_NTIME(now, bigsec, HCL_MSEC_TO_NSEC(bigmsec));

#elif defined(__OS2__)
	xtn_t* xtn = GET_XTN(hcl);
	hcl_uint64_t bigsec, bigmsec;
	ULONG msec;

/* TODO: use DosTmrQueryTime() and DosTmrQueryFreq()? */
	DosQuerySysInfo (QSV_MS_COUNT, QSV_MS_COUNT, &msec, HCL_SIZEOF(msec)); /* milliseconds */
	/* it must return NO_ERROR */
	if (msec < xtn->tc_last)
	{
		xtn->tc_overflow++;
		bigmsec = ((hcl_uint64_t)HCL_TYPE_MAX(ULONG) * xtn->tc_overflow) + msec;
	}
	else bigmsec = msec;
	xtn->tc_last = msec;

	bigsec = HCL_MSEC_TO_SEC(bigmsec);
	bigmsec -= HCL_SEC_TO_MSEC(bigsec);
	HCL_INIT_NTIME (now, bigsec, HCL_MSEC_TO_NSEC(bigmsec));

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
	HCL_INIT_NTIME (now, HCL_USEC_TO_SEC(tick64), HCL_USEC_TO_NSEC(tick64));
#elif defined(HAVE_CLOCK_GETTIME) && defined(CLOCK_MONOTONIC)
	struct timespec ts;
	clock_gettime (CLOCK_MONOTONIC, &ts);
	HCL_INIT_NTIME(now, ts.tv_sec, ts.tv_nsec);
#elif defined(HAVE_CLOCK_GETTIME) && defined(CLOCK_REALTIME)
	struct timespec ts;
	clock_gettime (CLOCK_REALTIME, &ts);
	HCL_INIT_NTIME(now, ts.tv_sec, ts.tv_nsec);
#else
	struct timeval tv;
	gettimeofday (&tv, HCL_NULL);
	HCL_INIT_NTIME(now, tv.tv_sec, HCL_USEC_TO_NSEC(tv.tv_usec));
#endif
}

/* -----------------------------------------------------------------
 * IO MULTIPLEXING
 * ----------------------------------------------------------------- */

static int _add_poll_fd (hcl_t* hcl, int fd, int event_mask)
{
#if defined(USE_DEVPOLL)
	xtn_t* xtn = GET_XTN(hcl);
	struct pollfd ev;

	HCL_ASSERT (hcl, xtn->ep >= 0);
	ev.fd = fd;
	ev.events = event_mask;
	ev.revents = 0;
	if (write(xtn->ep, &ev, HCL_SIZEOF(ev)) != HCL_SIZEOF(ev))
	{
		hcl_seterrwithsyserr (hcl, 0, errno);
		HCL_DEBUG2 (hcl, "Cannot add file descriptor %d to devpoll - %hs\n", fd, strerror(errno));
		return -1;
	}

	return 0;

#elif defined(USE_KQUEUE)
	xtn_t* xtn = GET_XTN(hcl);
	struct kevent ev;
	hcl_oow_t rindex, roffset;
	hcl_oow_t rv = 0;

	rindex = (hcl_oow_t)fd / (HCL_BITSOF(hcl_oow_t) >> 1);
	roffset = ((hcl_oow_t)fd << 1) % HCL_BITSOF(hcl_oow_t);

	if (rindex >= xtn->ev.reg.capa)
	{
		hcl_oow_t* tmp;
		hcl_oow_t newcapa;

		HCL_STATIC_ASSERT (HCL_SIZEOF(*tmp) == HCL_SIZEOF(*xtn->ev.reg.ptr));

		newcapa = rindex + 1;
		newcapa = HCL_ALIGN_POW2(newcapa, 16);

		tmp = (hcl_oow_t*)hcl_reallocmem(hcl, xtn->ev.reg.ptr, newcapa * HCL_SIZEOF(*tmp));
		if (!tmp)
		{
			const hcl_ooch_t* oldmsg = hcl_backuperrmsg(hcl);
			hcl_seterrbfmt (hcl, HCL_ESYSERR, "unable to add file descriptor %d to kqueue - %js", fd, oldmsg);
			HCL_DEBUG1 (hcl, "%js", hcl_geterrmsg(hcl));
			return -1;
		}

		HCL_MEMSET (&tmp[xtn->ev.reg.capa], 0, newcapa - xtn->ev.reg.capa);
		xtn->ev.reg.ptr = tmp;
		xtn->ev.reg.capa = newcapa;
	}

	if (event_mask & XPOLLIN) 
	{
		/*EV_SET (&ev, fd, EVFILT_READ, EV_ADD | EV_CLEAR, 0, 0, 0);*/
		HCL_MEMSET (&ev, 0, HCL_SIZEOF(ev));
		ev.ident = fd;
		ev.flags = EV_ADD;
	#if defined(USE_THREAD)
		ev.flags |= EV_CLEAR; /* EV_CLEAR for edge trigger? */
	#endif
		ev.filter = EVFILT_READ;
		if (kevent(xtn->ep, &ev, 1, HCL_NULL, 0, HCL_NULL) == -1)
		{
			hcl_seterrwithsyserr (hcl, 0, errno);
			HCL_DEBUG2 (hcl, "Cannot add file descriptor %d to kqueue for read - %hs\n", fd, strerror(errno));
			return -1;
		}

		rv |= 1;
	}
	if (event_mask & XPOLLOUT)
	{
		/*EV_SET (&ev, fd, EVFILT_WRITE, EV_ADD | EV_CLEAR, 0, 0, 0);*/
		HCL_MEMSET (&ev, 0, HCL_SIZEOF(ev));
		ev.ident = fd;
		ev.flags = EV_ADD;
	#if defined(USE_THREAD)
		ev.flags |= EV_CLEAR; /* EV_CLEAR for edge trigger? */
	#endif
		ev.filter = EVFILT_WRITE;
		if (kevent(xtn->ep, &ev, 1, HCL_NULL, 0, HCL_NULL) == -1)
		{
			hcl_seterrwithsyserr (hcl, 0, errno);
			HCL_DEBUG2 (hcl, "Cannot add file descriptor %d to kqueue for write - %hs\n", fd, strerror(errno));

			if (event_mask & XPOLLIN)
			{
				HCL_MEMSET (&ev, 0, HCL_SIZEOF(ev));
				ev.ident = fd;
				ev.flags = EV_DELETE;
				ev.filter = EVFILT_READ;
				kevent(xtn->ep, &ev, 1, HCL_NULL, 0, HCL_NULL);
			}
			return -1;
		}

		rv |= 2;
	}

	HCL_SETBITS (hcl_oow_t, xtn->ev.reg.ptr[rindex], roffset, 2, rv);
	return 0;

#elif defined(USE_EPOLL)
	xtn_t* xtn = GET_XTN(hcl);
	struct epoll_event ev;

	HCL_ASSERT (hcl, xtn->ep >= 0);
	HCL_MEMSET (&ev, 0, HCL_SIZEOF(ev));
	ev.events = event_mask;
	#if defined(USE_THREAD) && defined(EPOLLET)
	/* epoll_wait may return again if the worker thread consumes events.
	 * switch to level-trigger. */
	/* TODO: verify if EPOLLLET is desired */
	ev.events |= EPOLLET/*  | EPOLLRDHUP | EPOLLHUP */;
	#endif
	/*ev.data.ptr = (void*)event_data;*/
	ev.data.fd = fd;
	if (epoll_ctl(xtn->ep, EPOLL_CTL_ADD, fd, &ev) == -1)
	{
		hcl_seterrwithsyserr (hcl, 0, errno);
		HCL_DEBUG2 (hcl, "Cannot add file descriptor %d to epoll - %hs\n", fd, strerror(errno));
		return -1;
	}
	return 0;

#elif defined(USE_POLL)
	xtn_t* xtn = GET_XTN(hcl);

	MUTEX_LOCK (&xtn->ev.reg.pmtx);
	if (xtn->ev.reg.len >= xtn->ev.reg.capa)
	{
		struct pollfd* tmp, * tmp2;
		hcl_oow_t newcapa;

		newcapa = HCL_ALIGN_POW2 (xtn->ev.reg.len + 1, 256);
		tmp = (struct pollfd*)hcl_reallocmem(hcl, xtn->ev.reg.ptr, newcapa * HCL_SIZEOF(*tmp));
		tmp2 = (struct pollfd*)hcl_reallocmem(hcl, xtn->ev.buf, newcapa * HCL_SIZEOF(*tmp2));
		if (!tmp || !tmp2)
		{
			HCL_DEBUG2 (hcl, "Cannot add file descriptor %d to poll - %hs\n", fd, strerror(errno));
			MUTEX_UNLOCK (&xtn->ev.reg.pmtx);
			if (tmp) hcl_freemem (hcl, tmp);
			return -1;
		}

		xtn->ev.reg.ptr = tmp;
		xtn->ev.reg.capa = newcapa;

		xtn->ev.buf = tmp2;
	}

	xtn->ev.reg.ptr[xtn->ev.reg.len].fd = fd;
	xtn->ev.reg.ptr[xtn->ev.reg.len].events = event_mask;
	xtn->ev.reg.ptr[xtn->ev.reg.len].revents = 0;
	xtn->ev.reg.len++;
	MUTEX_UNLOCK (&xtn->ev.reg.pmtx);

	return 0;

#elif defined(USE_SELECT)
	xtn_t* xtn = GET_XTN(hcl);

	MUTEX_LOCK (&xtn->ev.reg.smtx);
	if (event_mask & XPOLLIN) 
	{
		FD_SET (fd, &xtn->ev.reg.rfds);
		if (fd > xtn->ev.reg.maxfd) xtn->ev.reg.maxfd = fd;
	}
	if (event_mask & XPOLLOUT) 
	{
		FD_SET (fd, &xtn->ev.reg.wfds);
		if (fd > xtn->ev.reg.maxfd) xtn->ev.reg.maxfd = fd;
	}
	MUTEX_UNLOCK (&xtn->ev.reg.smtx);

	return 0;

#else

	HCL_DEBUG1 (hcl, "Cannot add file descriptor %d to poll - not implemented\n", fd);
	hcl_seterrnum (hcl, HCL_ENOIMPL);
	return -1;
#endif

}

static int _del_poll_fd (hcl_t* hcl, int fd)
{

#if defined(USE_DEVPOLL)
	xtn_t* xtn = GET_XTN(hcl);
	struct pollfd ev;

	HCL_ASSERT (hcl, xtn->ep >= 0);
	ev.fd = fd;
	ev.events = POLLREMOVE;
	ev.revents = 0;
	if (write(xtn->ep, &ev, HCL_SIZEOF(ev)) != HCL_SIZEOF(ev))
	{
		hcl_seterrwithsyserr (hcl, 0, errno);
		HCL_DEBUG2 (hcl, "Cannot remove file descriptor %d from devpoll - %hs\n", fd, strerror(errno));
		return -1;
	}

	return 0;

#elif defined(USE_KQUEUE)
	xtn_t* xtn = GET_XTN(hcl);
	hcl_oow_t rindex, roffset;
	int rv;
	struct kevent ev;

	rindex = (hcl_oow_t)fd / (HCL_BITSOF(hcl_oow_t) >> 1);
	roffset = ((hcl_oow_t)fd << 1) % HCL_BITSOF(hcl_oow_t);

	if (rindex >= xtn->ev.reg.capa)
	{
		hcl_seterrbfmt (hcl, HCL_EINVAL, "unknown file descriptor %d", fd);
		HCL_DEBUG2 (hcl, "Cannot remove file descriptor %d from kqueue - %js\n", fd, hcl_geterrmsg(hcl));
		return -1;
	};

	rv = HCL_GETBITS (hcl_oow_t, xtn->ev.reg.ptr[rindex], roffset, 2);

	if (rv & 1)
	{
		/*EV_SET (&ev, fd, EVFILT_READ, EV_DELETE, 0, 0, 0);*/
		HCL_MEMSET (&ev, 0, HCL_SIZEOF(ev));
		ev.ident = fd;
		ev.flags = EV_DELETE;
		ev.filter = EVFILT_READ;
		kevent(xtn->ep, &ev, 1, HCL_NULL, 0, HCL_NULL);
		/* no error check for now */
	}

	if (rv & 2)
	{
		/*EV_SET (&ev, fd, EVFILT_WRITE, EV_DELETE, 0, 0, 0);*/
		HCL_MEMSET (&ev, 0, HCL_SIZEOF(ev));
		ev.ident = fd;
		ev.flags = EV_DELETE;
		ev.filter = EVFILT_WRITE;
		kevent(xtn->ep, &ev, 1, HCL_NULL, 0, HCL_NULL);
		/* no error check for now */
	}

	HCL_SETBITS (hcl_oow_t, xtn->ev.reg.ptr[rindex], roffset, 2, 0);
	return 0;

#elif defined(USE_EPOLL)
	xtn_t* xtn = GET_XTN(hcl);
	struct epoll_event ev;

	HCL_ASSERT (hcl, xtn->ep >= 0);
	HCL_MEMSET (&ev, 0, HCL_SIZEOF(ev));
	if (epoll_ctl(xtn->ep, EPOLL_CTL_DEL, fd, &ev) == -1)
	{
		hcl_seterrwithsyserr (hcl, 0, errno);
		HCL_DEBUG2 (hcl, "Cannot remove file descriptor %d from epoll - %hs\n", fd, strerror(errno));
		return -1;
	}
	return 0;

#elif defined(USE_POLL)
	xtn_t* xtn = GET_XTN(hcl);
	hcl_oow_t i;

	/* TODO: performance boost. no linear search */
	MUTEX_LOCK (&xtn->ev.reg.pmtx);
	for (i = 0; i < xtn->ev.reg.len; i++)
	{
		if (xtn->ev.reg.ptr[i].fd == fd)
		{
			xtn->ev.reg.len--;
			HCL_MEMMOVE (&xtn->ev.reg.ptr[i], &xtn->ev.reg.ptr[i+1], (xtn->ev.reg.len - i) * HCL_SIZEOF(*xtn->ev.reg.ptr));
			MUTEX_UNLOCK (&xtn->ev.reg.pmtx);
			return 0;
		}
	}
	MUTEX_UNLOCK (&xtn->ev.reg.pmtx);


	HCL_DEBUG1 (hcl, "Cannot remove file descriptor %d from poll - not found\n", fd);
	hcl_seterrnum (hcl, HCL_ENOENT);
	return -1;

#elif defined(USE_SELECT)
	xtn_t* xtn = GET_XTN(hcl);

	MUTEX_LOCK (&xtn->ev.reg.smtx);
	FD_CLR (fd, &xtn->ev.reg.rfds);
	FD_CLR (fd, &xtn->ev.reg.wfds);
	if (fd >= xtn->ev.reg.maxfd)
	{
		int i;
		/* TODO: any way to make this search faster or to do without the search like this */
		for (i = fd - 1; i >= 0; i--)
		{
			if (FD_ISSET(i, &xtn->ev.reg.rfds) || FD_ISSET(i, &xtn->ev.reg.wfds)) break;
		}
		xtn->ev.reg.maxfd = i;
	}
	MUTEX_UNLOCK (&xtn->ev.reg.smtx);

	return 0;

#else

	HCL_DEBUG1 (hcl, "Cannot remove file descriptor %d from poll - not implemented\n", fd);
	hcl_seterrnum (hcl, HCL_ENOIMPL);
	return -1;
#endif
}


static int _mod_poll_fd (hcl_t* hcl, int fd, int event_mask)
{
#if defined(USE_DEVPOLL)

	if (_del_poll_fd (hcl, fd) <= -1) return -1;

	if (_add_poll_fd (hcl, fd, event_mask) <= -1) 
	{
		/* TODO: any good way to rollback successful deletion? */
		return -1;
	}

	return 0;
#elif defined(USE_KQUEUE)
	xtn_t* xtn = GET_XTN(hcl);
	hcl_oow_t rindex, roffset;
	int rv, newrv = 0;
	struct kevent ev;

	rindex = (hcl_oow_t)fd / (HCL_BITSOF(hcl_oow_t) >> 1);
	roffset = ((hcl_oow_t)fd << 1) % HCL_BITSOF(hcl_oow_t);

	if (rindex >= xtn->ev.reg.capa)
	{
		hcl_seterrbfmt (hcl, HCL_EINVAL, "unknown file descriptor %d", fd);
		HCL_DEBUG2 (hcl, "Cannot modify file descriptor %d in kqueue - %js\n", fd, hcl_geterrmsg(hcl));
		return -1;
	};

	rv = HCL_GETBITS(hcl_oow_t, xtn->ev.reg.ptr[rindex], roffset, 2);

	if (rv & 1)
	{
		if (!(event_mask & XPOLLIN))
		{
			HCL_MEMSET (&ev, 0, HCL_SIZEOF(ev));
			ev.ident = fd;
			ev.flags = EV_DELETE;
			ev.filter = EVFILT_READ;
			if (kevent(xtn->ep, &ev, 1, HCL_NULL, 0, HCL_NULL) == -1) goto kqueue_syserr;

			newrv &= ~1;
		}
	}
	else
	{
		if (event_mask & XPOLLIN)
		{
			HCL_MEMSET (&ev, 0, HCL_SIZEOF(ev));
			ev.ident = fd;
			ev.flags = EV_ADD;
		#if defined(USE_THREAD)
			ev.flags |= EV_CLEAR; /* EV_CLEAR for edge trigger? */
		#endif
			ev.filter = EVFILT_READ;
			if (kevent(xtn->ep, &ev, 1, HCL_NULL, 0, HCL_NULL) == -1) goto kqueue_syserr;

			newrv |= 1;
		}
	}

	if (rv & 2)
	{
		if (!(event_mask & XPOLLOUT))
		{
			HCL_MEMSET (&ev, 0, HCL_SIZEOF(ev));
			ev.ident = fd;
			ev.flags = EV_DELETE;
			ev.filter = EVFILT_WRITE;
			/* there is no operation rollback for the (rv & 1) case.
			 * the rollback action may fail again even if i try it */
			if (kevent(xtn->ep, &ev, 1, HCL_NULL, 0, HCL_NULL) == -1) goto kqueue_syserr;

			newrv &= ~2;
		}
	}
	else
	{
		if (event_mask & XPOLLOUT)
		{
			HCL_MEMSET (&ev, 0, HCL_SIZEOF(ev));
			ev.ident = fd;
			ev.flags = EV_ADD;
		#if defined(USE_THREAD)
			ev.flags |= EV_CLEAR; /* EV_CLEAR for edge trigger? */
		#endif
			ev.filter = EVFILT_WRITE;

			/* there is no operation rollback for the (rv & 1) case.
			 * the rollback action may fail again even if i try it */
			if (kevent(xtn->ep, &ev, 1, HCL_NULL, 0, HCL_NULL) == -1) goto kqueue_syserr;

			newrv |= 2;
		}
	}

	HCL_SETBITS (hcl_oow_t, xtn->ev.reg.ptr[rindex], roffset, 2, newrv);
	return 0;

kqueue_syserr:
	hcl_seterrwithsyserr (hcl, 0, errno);
	HCL_DEBUG2 (hcl, "Cannot modify file descriptor %d in kqueue - %hs\n", fd, strerror(errno));
	return -1;

#elif defined(USE_EPOLL)
	xtn_t* xtn = GET_XTN(hcl);
	struct epoll_event ev;

	HCL_ASSERT (hcl, xtn->ep >= 0);
	HCL_MEMSET (&ev, 0, HCL_SIZEOF(ev));
	ev.events = event_mask;
	#if defined(USE_THREAD) && defined(EPOLLET)
	/* epoll_wait may return again if the worker thread consumes events.
	 * switch to level-trigger. */
	/* TODO: verify if EPOLLLET is desired */
	ev.events |= EPOLLET;
	#endif
	ev.data.fd = fd;
	if (epoll_ctl(xtn->ep, EPOLL_CTL_MOD, fd, &ev) == -1)
	{
		hcl_seterrwithsyserr (hcl, 0, errno);
		HCL_DEBUG2 (hcl, "Cannot modify file descriptor %d in epoll - %hs\n", fd, strerror(errno));
		return -1;
	}

	return 0;

#elif defined(USE_POLL)

	xtn_t* xtn = GET_XTN(hcl);
	hcl_oow_t i;

	MUTEX_LOCK (&xtn->ev.reg.pmtx);
	for (i = 0; i < xtn->ev.reg.len; i++)
	{
		if (xtn->ev.reg.ptr[i].fd == fd)
		{
			HCL_MEMMOVE (&xtn->ev.reg.ptr[i], &xtn->ev.reg.ptr[i+1], (xtn->ev.reg.len - i - 1) * HCL_SIZEOF(*xtn->ev.reg.ptr));
			xtn->ev.reg.ptr[i].fd = fd;
			xtn->ev.reg.ptr[i].events = event_mask;
			xtn->ev.reg.ptr[i].revents = 0;
			MUTEX_UNLOCK (&xtn->ev.reg.pmtx);

			return 0;
		}
	}
	MUTEX_UNLOCK (&xtn->ev.reg.pmtx);

	HCL_DEBUG1 (hcl, "Cannot modify file descriptor %d in poll - not found\n", fd);
	hcl_seterrnum (hcl, HCL_ENOENT);
	return -1;

#elif defined(USE_SELECT)

	xtn_t* xtn = GET_XTN(hcl);

	MUTEX_LOCK (&xtn->ev.reg.smtx);
	HCL_ASSERT (hcl, fd <= xtn->ev.reg.maxfd);

	if (event_mask & XPOLLIN) 
		FD_SET (fd, &xtn->ev.reg.rfds);
	else 
		FD_CLR (fd, &xtn->ev.reg.rfds);

	if (event_mask & XPOLLOUT) 
		FD_SET (fd, &xtn->ev.reg.wfds);
	else
		FD_CLR (fd, &xtn->ev.reg.wfds);
	MUTEX_UNLOCK (&xtn->ev.reg.smtx);

	return 0;

#else
	HCL_DEBUG1 (hcl, "Cannot modify file descriptor %d in poll - not implemented\n", fd);
	hcl_seterrnum (hcl, HCL_ENOIMPL);
	return -1;
#endif
}

static int vm_muxadd (hcl_t* hcl, hcl_ooi_t io_handle, hcl_ooi_t mask)
{
	int event_mask;

	event_mask = 0;
	if (mask & HCL_SEMAPHORE_IO_MASK_INPUT) event_mask |= XPOLLIN; 
	if (mask & HCL_SEMAPHORE_IO_MASK_OUTPUT) event_mask |= XPOLLOUT;

	if (event_mask == 0)
	{
		HCL_DEBUG2 (hcl, "<vm_muxadd> Invalid semaphore mask %zd on handle %zd\n", mask, io_handle);
		hcl_seterrbfmt (hcl, HCL_EINVAL, "invalid semaphore mask %zd on handle %zd", mask, io_handle);
		return -1;
	}

	return _add_poll_fd(hcl, io_handle, event_mask);
}

static int vm_muxmod (hcl_t* hcl, hcl_ooi_t io_handle, hcl_ooi_t mask)
{
	int event_mask;

	event_mask = 0;
	if (mask & HCL_SEMAPHORE_IO_MASK_INPUT) event_mask |= XPOLLIN; 
	if (mask & HCL_SEMAPHORE_IO_MASK_OUTPUT) event_mask |= XPOLLOUT;

	if (event_mask == 0)
	{
		HCL_DEBUG2 (hcl, "<vm_muxadd> Invalid semaphore mask %zd on handle %zd\n", mask, io_handle);
		hcl_seterrbfmt (hcl, HCL_EINVAL, "invalid semaphore mask %zd on handle %zd", mask, io_handle);
		return -1;
	}

	return _mod_poll_fd(hcl, io_handle, event_mask);
}

static int vm_muxdel (hcl_t* hcl, hcl_ooi_t io_handle)
{
	return _del_poll_fd(hcl, io_handle);
}

#if defined(USE_THREAD)
static void* iothr_main (void* arg)
{
	hcl_t* hcl = (hcl_t*)arg;
	xtn_t* xtn = GET_XTN(hcl);

	/*while (!hcl->abort_req)*/
	while (!xtn->iothr.abort)
	{
		if (xtn->ev.len <= 0) /* TODO: no mutex needed for this check? */
		{
			int n;
		#if defined(USE_DEVPOLL)
			struct dvpoll dvp;
		#elif defined(USE_KQUEUE)
			struct timespec ts;
		#elif defined(USE_POLL)
			hcl_oow_t nfds;
		#elif defined(USE_SELECT)
			struct timeval tv;
			fd_set rfds;
			fd_set wfds;
			int maxfd;
		#endif

		poll_for_event:
		
		#if defined(USE_DEVPOLL)
			dvp.dp_timeout = 10000; /* milliseconds */
			dvp.dp_fds = xtn->ev.buf;
			dvp.dp_nfds = HCL_COUNTOF(xtn->ev.buf);
			n = ioctl (xtn->ep, DP_POLL, &dvp);
		#elif defined(USE_KQUEUE)
			ts.tv_sec = 10;
			ts.tv_nsec = 0;
			n = kevent(xtn->ep, HCL_NULL, 0, xtn->ev.buf, HCL_COUNTOF(xtn->ev.buf), &ts);
			/* n == 0: timeout
			 * n == -1: error */
		#elif defined(USE_EPOLL)
			n = epoll_wait(xtn->ep, xtn->ev.buf, HCL_COUNTOF(xtn->ev.buf), 10000); /* TODO: make this timeout value in the io thread */
		#elif defined(USE_POLL)
			MUTEX_LOCK (&xtn->ev.reg.pmtx);
			HCL_MEMCPY (xtn->ev.buf, xtn->ev.reg.ptr, xtn->ev.reg.len * HCL_SIZEOF(*xtn->ev.buf));
			nfds = xtn->ev.reg.len;
			MUTEX_UNLOCK (&xtn->ev.reg.pmtx);
			n = poll(xtn->ev.buf, nfds, 10000);
			if (n > 0) 
			{
				/* compact the return buffer as poll() doesn't */
				hcl_oow_t i, j;
				for (i = 0, j = 0; i < nfds && j < n; i++)
				{
					if (xtn->ev.buf[i].revents)
					{
						if (j != i) xtn->ev.buf[j] = xtn->ev.buf[i];
						j++;
					}
				}
				n = j;
			}
		#elif defined(USE_SELECT)
			tv.tv_sec = 10;
			tv.tv_usec = 0;
			MUTEX_LOCK (&xtn->ev.reg.smtx);
			maxfd = xtn->ev.reg.maxfd;
			HCL_MEMCPY (&rfds, &xtn->ev.reg.rfds, HCL_SIZEOF(rfds));
			HCL_MEMCPY (&wfds, &xtn->ev.reg.wfds, HCL_SIZEOF(wfds));
			MUTEX_UNLOCK (&xtn->ev.reg.smtx);
			n = select (maxfd + 1, &rfds, &wfds, HCL_NULL, &tv);
			if (n > 0)
			{
				int fd, count = 0;
				for (fd = 0;  fd <= maxfd; fd++)
				{
					int events = 0;
					if (FD_ISSET(fd, &rfds)) events |= XPOLLIN;
					if (FD_ISSET(fd, &wfds)) events |= XPOLLOUT;

					if (events)
					{
						HCL_ASSERT (hcl, count < HCL_COUNTOF(xtn->ev.buf));
						xtn->ev.buf[count].fd = fd;
						xtn->ev.buf[count].events = events;
						count++;
					}
				}

				n = count;
				HCL_ASSERT (hcl, n > 0);
			}
		#endif

			pthread_mutex_lock (&xtn->ev.mtx);
			if (n <= -1)
			{
				/* TODO: don't use HCL_DEBUG2. it's not thread safe... */
				/* the following call has a race-condition issue when called in this separate thread */
				/*HCL_DEBUG2 (hcl, "Warning: multiplexer wait failure - %d, %hs\n", errno, strerror(errno));*/
			}
			else if (n > 0)
			{
				xtn->ev.len = n;
			}
			pthread_cond_signal (&xtn->ev.cnd2);
			pthread_mutex_unlock (&xtn->ev.mtx);
		}
		else
		{
			/* the event buffer has not been emptied yet */
			struct timespec ts;

			pthread_mutex_lock (&xtn->ev.mtx);
			if (xtn->ev.len <= 0)
			{
				/* it got emptied between the if check and pthread_mutex_lock() above */
				pthread_mutex_unlock (&xtn->ev.mtx);
				goto poll_for_event;
			}

		#if defined(HAVE_CLOCK_GETTIME) && defined(CLOCK_REALTIME)
			clock_gettime (CLOCK_REALTIME, &ts);
		#else
			{
				struct timeval tv;
				gettimeofday (&tv, HCL_NULL);
				ts.tv_sec = tv.tv_sec;
				ts.tv_nsec = HCL_USEC_TO_NSEC(tv.tv_usec);
			}
		#endif
			ts.tv_sec += 10;
			pthread_cond_timedwait (&xtn->ev.cnd, &xtn->ev.mtx, &ts);
			pthread_mutex_unlock (&xtn->ev.mtx);
		}

		/*sched_yield ();*/
	}

	return HCL_NULL;
}
#endif

static void vm_muxwait (hcl_t* hcl, const hcl_ntime_t* dur, hcl_vmprim_muxwait_cb_t muxwcb)
{
	xtn_t* xtn = GET_XTN(hcl);

#if defined(USE_THREAD)
	int n;

	/* create a thread if mux wait is started at least once. */
	if (!xtn->iothr.up) 
	{
		xtn->iothr.up = 1;
		if (pthread_create(&xtn->iothr.thr, HCL_NULL, iothr_main, hcl) != 0)
		{
			HCL_LOG2 (hcl, HCL_LOG_WARN, "Warning: pthread_create failure - %d, %hs\n", errno, strerror(errno));
			xtn->iothr.up = 0;
/* TODO: switch to the non-threaded mode? */
		}
	}

	if (xtn->iothr.abort) return;

	if (xtn->ev.len <= 0) 
	{
		struct timespec ts;
		hcl_ntime_t ns;

		if (!dur) return; /* immediate check is requested. and there is no event */

	#if defined(HAVE_CLOCK_GETTIME) && defined(CLOCK_REALTIME)
		clock_gettime (CLOCK_REALTIME, &ts);
		ns.sec = ts.tv_sec;
		ns.nsec = ts.tv_nsec;
	#else
		{
			struct timeval tv;
			gettimeofday (&tv, HCL_NULL);
			ns.sec = tv.tv_sec;
			ns.nsec = HCL_USEC_TO_NSEC(tv.tv_usec);
		}
	#endif
		HCL_ADD_NTIME (&ns, &ns, dur);
		ts.tv_sec = ns.sec;
		ts.tv_nsec = ns.nsec;

		pthread_mutex_lock (&xtn->ev.mtx);
		if (xtn->ev.len <= 0)
		{
			/* the event buffer is still empty */
			pthread_cond_timedwait (&xtn->ev.cnd2, &xtn->ev.mtx, &ts);
		}
		pthread_mutex_unlock (&xtn->ev.mtx);
	}

	n = xtn->ev.len;

	if (n > 0)
	{
		do
		{
			--n;

		#if defined(USE_DEVPOLL)
			if (xtn->ev.buf[n].fd == xtn->iothr.p[0])
		#elif defined(USE_KQUEUE)
			if (xtn->ev.buf[n].ident == xtn->iothr.p[0])
		#elif defined(USE_EPOLL)
			/*if (xtn->ev.buf[n].data.ptr == (void*)HCL_TYPE_MAX(hcl_oow_t))*/
			if (xtn->ev.buf[n].data.fd == xtn->iothr.p[0])
		#elif defined(USE_POLL)
			if (xtn->ev.buf[n].fd == xtn->iothr.p[0])
		#elif defined(USE_SELECT)
			if (xtn->ev.buf[n].fd == xtn->iothr.p[0])
		#else
		#	error UNSUPPORTED
		#endif
			{
				hcl_uint8_t u8;
				while (read(xtn->iothr.p[0], &u8, HCL_SIZEOF(u8)) > 0) 
				{
					/* consume as much as possible */;
					if (u8 == 'Q') xtn->iothr.abort = 1;
				}
			}
			else if (muxwcb)
			{
				int revents;
				hcl_ooi_t mask;

			#if defined(USE_DEVPOLL)
				revents = xtn->ev.buf[n].revents;
			#elif defined(USE_KQUEUE)
				if (xtn->ev.buf[n].filter == EVFILT_READ) mask = HCL_SEMAPHORE_IO_MASK_INPUT;
				else if (xtn->ev.buf[n].filter == EVFILT_WRITE) mask = HCL_SEMAPHORE_IO_MASK_OUTPUT;
				else mask = 0;
				goto call_muxwcb_kqueue;
			#elif defined(USE_EPOLL)
				revents = xtn->ev.buf[n].events;
			#elif defined(USE_POLL)
				revents = xtn->ev.buf[n].revents;
			#elif defined(USE_SELECT)
				revents = xtn->ev.buf[n].events;
			#endif

				mask = 0;
				if (revents & XPOLLIN) mask |= HCL_SEMAPHORE_IO_MASK_INPUT;
				if (revents & XPOLLOUT) mask |= HCL_SEMAPHORE_IO_MASK_OUTPUT;
				if (revents & XPOLLERR) mask |= HCL_SEMAPHORE_IO_MASK_ERROR;
				if (revents & XPOLLHUP) mask |= HCL_SEMAPHORE_IO_MASK_HANGUP;

			#if defined(USE_DEVPOLL)
				muxwcb (hcl, xtn->ev.buf[n].fd, mask);
			#elif defined(USE_KQUEUE)
			call_muxwcb_kqueue:
				muxwcb (hcl, xtn->ev.buf[n].ident, mask);
			#elif defined(USE_EPOLL)
				muxwcb (hcl, xtn->ev.buf[n].data.fd, mask);
			#elif defined(USE_POLL)
				muxwcb (hcl, xtn->ev.buf[n].fd, mask);
			#elif defined(USE_SELECT)
				muxwcb (hcl, xtn->ev.buf[n].fd, mask);
			#else
			#	error UNSUPPORTED
			#endif
			}
		}
		while (n > 0);

		pthread_mutex_lock (&xtn->ev.mtx);
		xtn->ev.len = 0;
		pthread_cond_signal (&xtn->ev.cnd);
		pthread_mutex_unlock (&xtn->ev.mtx);
	}

#else /* USE_THREAD */
	int n;
	#if defined(USE_DEVPOLL)
	int tmout;
	struct dvpoll dvp;
	#elif defined(USE_KQUEUE)
	struct timespec ts;
	#elif defined(USE_EPOLL)
	int tmout;
	#elif defined(USE_POLL)
	int tmout;
	#elif defined(USE_SELECT)
	struct timeval tv;
	fd_set rfds, wfds;
	int maxfd;
	#endif


	#if defined(USE_DEVPOLL)
	tmout = dur? HCL_SECNSEC_TO_MSEC(dur->sec, dur->nsec): 0;

	dvp.dp_timeout = tmout; /* milliseconds */
	dvp.dp_fds = xtn->ev.buf;
	dvp.dp_nfds = HCL_COUNTOF(xtn->ev.buf);
	n = ioctl(xtn->ep, DP_POLL, &dvp);

	#elif defined(USE_KQUEUE)
	
	if (dur)
	{
		ts.tv_sec = dur->sec;
		ts.tv_nsec = dur->nsec; 
	}
	else
	{
		ts.tv_sec = 0;
		ts.tv_nsec = 0;
	}

	n = kevent(xtn->ep, HCL_NULL, 0, xtn->ev.buf, HCL_COUNTOF(xtn->ev.buf), &ts);
	/* n == 0: timeout
	 * n == -1: error */

	#elif defined(USE_EPOLL)
	tmout = dur? HCL_SECNSEC_TO_MSEC(dur->sec, dur->nsec): 0;
	n = epoll_wait(xtn->ep, xtn->ev.buf, HCL_COUNTOF(xtn->ev.buf), tmout);

	#elif defined(USE_POLL)
	tmout = dur? HCL_SECNSEC_TO_MSEC(dur->sec, dur->nsec): 0;
	HCL_MEMCPY (xtn->ev.buf, xtn->ev.reg.ptr, xtn->ev.reg.len * HCL_SIZEOF(*xtn->ev.buf));
	n = poll(xtn->ev.buf, xtn->ev.reg.len, tmout);
	if (n > 0) 
	{
		/* compact the return buffer as poll() doesn't */
		hcl_oow_t i, j;
		for (i = 0, j = 0; i < xtn->ev.reg.len && j < n; i++)
		{
			if (xtn->ev.buf[i].revents)
			{
				if (j != i) xtn->ev.buf[j] = xtn->ev.buf[i];
				j++;
			}
		}
		n = j;
	}
	#elif defined(USE_SELECT)
	if (dur)
	{
		tv.tv_sec = dur->sec;
		tv.tv_usec = HCL_NSEC_TO_USEC(dur->nsec); 
	}
	else
	{
		tv.tv_sec = 0;
		tv.tv_usec = 0;
	}
	maxfd = xtn->ev.reg.maxfd;
	HCL_MEMCPY (&rfds, &xtn->ev.reg.rfds, HCL_SIZEOF(rfds));
	HCL_MEMCPY (&wfds, &xtn->ev.reg.wfds, HCL_SIZEOF(wfds));
	n = select(maxfd + 1, &rfds, &wfds, HCL_NULL, &tv);
	if (n > 0)
	{
		int fd, count = 0;
		for (fd = 0; fd <= maxfd; fd++)
		{
			int events = 0;
			if (FD_ISSET(fd, &rfds)) events |= XPOLLIN;
			if (FD_ISSET(fd, &wfds)) events |= XPOLLOUT;

			if (events)
			{
				HCL_ASSERT (hcl, count < HCL_COUNTOF(xtn->ev.buf));
				xtn->ev.buf[count].fd = fd;
				xtn->ev.buf[count].events = events;
				count++;
			}
		}

		n = count;
		HCL_ASSERT (hcl, n > 0);
	}
	#endif

	if (n <= -1)
	{
		hcl_seterrwithsyserr (hcl, 0, errno);
		HCL_DEBUG2 (hcl, "Warning: multiplexer wait failure - %d, %js\n", errno, hcl_geterrmsg(hcl));
	}
	else
	{
		xtn->ev.len = n;
	}

	/* the muxwcb must be valid all the time in a non-threaded mode */
	HCL_ASSERT (hcl, muxwcb != HCL_NULL);

	while (n > 0)
	{
		int revents;
		hcl_ooi_t mask;

		--n;

	#if defined(USE_DEVPOLL)
		revents = xtn->ev.buf[n].revents;
	#elif defined(USE_KQUEUE)
		if (xtn->ev.buf[n].filter == EVFILT_READ) mask = HCL_SEMAPHORE_IO_MASK_INPUT;
		else if (xtn->ev.buf[n].filter == EVFILT_WRITE) mask = HCL_SEMAPHORE_IO_MASK_OUTPUT;
		else mask = 0;
		goto call_muxwcb_kqueue;
	#elif defined(USE_EPOLL)
		revents = xtn->ev.buf[n].events;
	#elif defined(USE_POLL)
		revents = xtn->ev.buf[n].revents;
	#elif defined(USE_SELECT)
		revents = xtn->ev.buf[n].events;
	#else
		revents = 0; /* TODO: fake. unsupported but to compile on such an unsupported system.*/
	#endif

		mask = 0;
		if (revents & XPOLLIN) mask |= HCL_SEMAPHORE_IO_MASK_INPUT;
		if (revents & XPOLLOUT) mask |= HCL_SEMAPHORE_IO_MASK_OUTPUT;
		if (revents & XPOLLERR) mask |= HCL_SEMAPHORE_IO_MASK_ERROR;
		if (revents & XPOLLHUP) mask |= HCL_SEMAPHORE_IO_MASK_HANGUP;

	#if defined(USE_DEVPOLL)
		muxwcb (hcl, xtn->ev.buf[n].fd, mask);
	#elif defined(USE_KQUEUE)
	call_muxwcb_kqueue:
		muxwcb (hcl, xtn->ev.buf[n].ident, mask);
	#elif defined(USE_EPOLL)
		muxwcb (hcl, xtn->ev.buf[n].data.fd, mask);
	#elif defined(USE_POLL)
		muxwcb (hcl, xtn->ev.buf[n].fd, mask);
	#elif defined(USE_SELECT)
		muxwcb (hcl, xtn->ev.buf[n].fd, mask);
	#endif
	}

	xtn->ev.len = 0;
#endif  /* USE_THREAD */
}

/* ----------------------------------------------------------------- 
 * SLEEPING
 * ----------------------------------------------------------------- */

#if defined(__DOS__) 
#	if defined(_INTELC32_) 
	void _halt_cpu (void);
#	elif defined(__WATCOMC__)
	void _halt_cpu (void);
#	pragma aux _halt_cpu = "hlt"
#	endif
#endif

static int vm_sleep (hcl_t* hcl, const hcl_ntime_t* dur)
{
#if defined(_WIN32)
	xtn_t* xtn = GET_XTN(hcl);
	if (xtn->waitable_timer)
	{
		LARGE_INTEGER li;
		li.QuadPart = -(HCL_SECNSEC_TO_NSEC(dur->sec, dur->nsec) / 100); /* in 100 nanoseconds */
		if(SetWaitableTimer(xtn->waitable_timer, &li, 0, HCL_NULL, HCL_NULL, FALSE) == FALSE) goto normal_sleep;
		WaitForSingleObject(xtn->waitable_timer, INFINITE);
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
#endif

	return 0;
}

/* ----------------------------------------------------------------- 
 * SHARED LIBRARY HANDLING
 * ----------------------------------------------------------------- */
 
#if defined(USE_LTDL)
#	define sys_dl_error() lt_dlerror()
#	define sys_dl_open(x) lt_dlopen(x)
#	define sys_dl_openext(x) lt_dlopenext(x)
#	define sys_dl_close(x) lt_dlclose(x)
#	define sys_dl_getsym(x,n) lt_dlsym(x,n)

#elif defined(USE_DLFCN)
#	define sys_dl_error() dlerror()
#	define sys_dl_open(x) dlopen(x,RTLD_NOW)
#	define sys_dl_openext(x) dlopen(x,RTLD_NOW)
#	define sys_dl_close(x) dlclose(x)
#	define sys_dl_getsym(x,n) dlsym(x,n)

#elif defined(USE_WIN_DLL)
#	define sys_dl_error() win_dlerror()
#	define sys_dl_open(x) LoadLibraryExA(x, HCL_NULL, 0)
#	define sys_dl_openext(x) LoadLibraryExA(x, HCL_NULL, 0)
#	define sys_dl_close(x) FreeLibrary(x)
#	define sys_dl_getsym(x,n) GetProcAddress(x,n)

#elif defined(USE_MACH_O_DYLD)
#	define sys_dl_error() mach_dlerror()
#	define sys_dl_open(x) mach_dlopen(x)
#	define sys_dl_openext(x) mach_dlopen(x)
#	define sys_dl_close(x) mach_dlclose(x)
#	define sys_dl_getsym(x,n) mach_dlsym(x,n)
#endif

#if defined(USE_WIN_DLL)

static const char* win_dlerror (void)
{
	/* TODO: handle wchar_t, hcl_ooch_t etc? */
	static char buf[256];
	DWORD rc;

	rc = FormatMessageA (
		FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL, GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), 
		buf, HCL_COUNTOF(buf), HCL_NULL
	);
	while (rc > 0 && buf[rc - 1] == '\r' || buf[rc - 1] == '\n') 
	{
		buf[--rc] = '\0';
	}
	return buf;
}

#elif defined(USE_MACH_O_DYLD)
static const char* mach_dlerror_str = "";

static void* mach_dlopen (const char* path)
{
	NSObjectFileImage image;
	NSObjectFileImageReturnCode rc;
	void* handle;

	mach_dlerror_str = "";
	if ((rc = NSCreateObjectFileImageFromFile(path, &image)) != NSObjectFileImageSuccess) 
	{
		switch (rc)
		{
			case NSObjectFileImageFailure:
			case NSObjectFileImageFormat:
				mach_dlerror_str = "unable to crate object file image";
				break;

			case NSObjectFileImageInappropriateFile:
				mach_dlerror_str = "inappropriate file";
				break;

			case NSObjectFileImageArch:
				mach_dlerror_str = "incompatible architecture";
				break;

			case NSObjectFileImageAccess:
				mach_dlerror_str = "inaccessible file";
				break;
				
			default:
				mach_dlerror_str = "unknown error";
				break;
		}
		return HCL_NULL;
	}
	handle = (void*)NSLinkModule(image, path, NSLINKMODULE_OPTION_PRIVATE | NSLINKMODULE_OPTION_RETURN_ON_ERROR);
	NSDestroyObjectFileImage (image);
	return handle;
}

static HCL_INLINE void mach_dlclose (void* handle)
{
	mach_dlerror_str = "";
	NSUnLinkModule (handle, NSUNLINKMODULE_OPTION_NONE);
}

static HCL_INLINE void* mach_dlsym (void* handle, const char* name)
{
	mach_dlerror_str = "";
	return (void*)NSAddressOfSymbol(NSLookupSymbolInModule(handle, name));
}

static const char* mach_dlerror (void)
{
	int err_no;
	const char* err_file;
	NSLinkEditErrors err;

	if (mach_dlerror_str[0] == '\0')
		NSLinkEditError (&err, &err_no, &err_file, &mach_dlerror_str);

	return mach_dlerror_str;
}
#endif

static void dl_startup (hcl_t* hcl)
{
#if defined(USE_LTDL)
	lt_dlinit ();
#endif
}

static void dl_cleanup (hcl_t* hcl)
{
#if defined(USE_LTDL)
	lt_dlexit ();
#endif
}

static void* dl_open (hcl_t* hcl, const hcl_ooch_t* name, int flags)
{
#if defined(USE_LTDL) || defined(USE_DLFCN) || defined(USE_MACH_O_DYLD)
	hcl_bch_t stabuf[128], * bufptr;
	hcl_oow_t ucslen, bcslen, bufcapa;
	void* handle;

	#if defined(HCL_OOCH_IS_UCH)
	if (hcl_convootobcstr(hcl, name, &ucslen, HCL_NULL, &bufcapa) <= -1) return HCL_NULL;
	/* +1 for terminating null. but it's not needed because HCL_COUNTOF(HCL_DEFAULT_PFMODPREFIX)
	 * and HCL_COUNTOF(HCL_DEFAULT_PFMODPOSTIFX) include the terminating nulls. Never mind about
	 * the extra 2 characters. */
	#else
	bufcapa = hcl_count_bcstr(name);
	#endif
	bufcapa += HCL_COUNTOF(HCL_DEFAULT_PFMODDIR) + HCL_COUNTOF(HCL_DEFAULT_PFMODPREFIX) + HCL_COUNTOF(HCL_DEFAULT_PFMODPOSTFIX) + 1; 

	if (bufcapa <= HCL_COUNTOF(stabuf)) bufptr = stabuf;
	else
	{
		bufptr = (hcl_bch_t*)hcl_allocmem(hcl, bufcapa * HCL_SIZEOF(*bufptr));
		if (!bufptr) return HCL_NULL;
	}

	if (flags & HCL_VMPRIM_DLOPEN_PFMOD)
	{
		hcl_oow_t len, i, xlen, dlen;

		/* opening a primitive function module - mostly libhcl-xxxx.
		 * if PFMODPREFIX is absolute, never use PFMODDIR */
		dlen = HCL_IS_PATH_ABSOLUTE(HCL_DEFAULT_PFMODPREFIX)? 
			0: hcl_copy_bcstr(bufptr, bufcapa, HCL_DEFAULT_PFMODDIR);
		len = hcl_copy_bcstr(bufptr, bufcapa, HCL_DEFAULT_PFMODPREFIX);
		len += dlen;

		bcslen = bufcapa - len;
	#if defined(HCL_OOCH_IS_UCH)
		hcl_convootobcstr(hcl, name, &ucslen, &bufptr[len], &bcslen);
	#else
		bcslen = hcl_copy_bcstr(&bufptr[len], bcslen, name);
	#endif

		/* length including the directory, the prefix and the name. but excluding the postfix */
		xlen  = len + bcslen; 

		for (i = len; i < xlen; i++) 
		{
			/* convert a period(.) to a dash(-) */
			if (bufptr[i] == '.') bufptr[i] = '-';
		}
 
	retry:
		hcl_copy_bcstr (&bufptr[xlen], bufcapa - xlen, HCL_DEFAULT_PFMODPOSTFIX);

		/* both prefix and postfix attached. for instance, libhcl-xxx */
		handle = sys_dl_openext(bufptr);
		if (!handle) 
		{
			HCL_DEBUG3 (hcl, "Unable to open(ext) PFMOD %hs[%js] - %hs\n", &bufptr[dlen], name, sys_dl_error());

			if (dlen > 0)
			{
				handle = sys_dl_openext(&bufptr[0]);
				if (handle) goto pfmod_open_ok;
				HCL_DEBUG3 (hcl, "Unable to open(ext) PFMOD %hs[%js] - %hs\n", &bufptr[0], name, sys_dl_error());
			}

			/* try without prefix and postfix */
			bufptr[xlen] = '\0';
			handle = sys_dl_openext(&bufptr[len]);
			if (!handle) 
			{
				hcl_bch_t* dash;
				const hcl_bch_t* dl_errstr;
				dl_errstr = sys_dl_error();
				HCL_DEBUG3 (hcl, "Unable to open(ext) PFMOD %hs[%js] - %hs\n", &bufptr[len], name, dl_errstr);
				hcl_seterrbfmt (hcl, HCL_ESYSERR, "unable to open(ext) PFMOD %js - %hs", name, dl_errstr);

				dash = hcl_rfind_bchar(bufptr, hcl_count_bcstr(bufptr), '-');
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
				HCL_DEBUG3 (hcl, "Opened(ext) PFMOD %hs[%js] handle %p\n", &bufptr[len], name, handle);
			}
		}
		else
		{
		pfmod_open_ok:
			HCL_DEBUG3 (hcl, "Opened(ext) PFMOD %hs[%js] handle %p\n", &bufptr[dlen], name, handle);
		}
	}
	else
	{
		/* opening a raw shared object without a prefix and/or a postfix */
	#if defined(HCL_OOCH_IS_UCH)
		bcslen = bufcapa;
		hcl_convootobcstr(hcl, name, &ucslen, bufptr, &bcslen);
	#else
		bcslen = hcl_copy_bcstr(bufptr, bufcapa, name);
	#endif

		if (hcl_find_bchar(bufptr, bcslen, '.'))
		{
			handle = sys_dl_open(bufptr);
			if (!handle) 
			{
				const hcl_bch_t* dl_errstr;
				dl_errstr = sys_dl_error();
				HCL_DEBUG2 (hcl, "Unable to open DL %hs - %hs\n", bufptr, dl_errstr);
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
				HCL_DEBUG2 (hcl, "Unable to open(ext) DL %hs - %hs\n", bufptr, dl_errstr);
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
	hcl_seterrbfmt (hcl, HCL_ENOIMPL, "dynamic loading not implemented - cannot open %js", name);
	return HCL_NULL;
#endif
}

static void dl_close (hcl_t* hcl, void* handle)
{
#if defined(USE_LTDL) || defined(USE_DLFCN) || defined(USE_MACH_O_DYLD)
	HCL_DEBUG1 (hcl, "Closed DL handle %p\n", handle);
	sys_dl_close (handle);

#else
	/* TODO: implemenent this */
	HCL_DEBUG1 (hcl, "Dynamic loading not implemented - cannot close handle %p\n", handle);
#endif
}

static void* dl_getsym (hcl_t* hcl, void* handle, const hcl_ooch_t* name)
{
#if defined(USE_LTDL) || defined(USE_DLFCN) || defined(USE_MACH_O_DYLD)
	hcl_bch_t stabuf[64], * bufptr;
	hcl_oow_t bufcapa, ucslen, bcslen, i;
	const hcl_bch_t* symname;
	void* sym;

	#if defined(HCL_OOCH_IS_UCH)
	if (hcl_convootobcstr(hcl, name, &ucslen, HCL_NULL, &bcslen) <= -1) return HCL_NULL;
	#else
	bcslen = hcl_count_bcstr (name);
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
	bcslen = hcl_copy_bcstr(&bufptr[1], bcslen, name);
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

/* ----------------------------------------------------------------- 
 * EVENT CALLBACKS
 * ----------------------------------------------------------------- */

#define ENABLE_LOG_INITIALLY

static HCL_INLINE void reset_log_to_default (xtn_t* xtn)
{
#if defined(ENABLE_LOG_INITIALLY)
	xtn->log.fd = 2;
	xtn->log.fd_flag = 0;
	#if defined(HAVE_ISATTY)
	if (isatty(xtn->log.fd)) xtn->log.fd_flag |= LOGFD_TTY;
	#endif
#else
	xtn->log.fd = -1;
	xtn->log.fd_flag = 0;
#endif
}

static void cb_fini (hcl_t* hcl)
{
	xtn_t* xtn = GET_XTN(hcl);
	if ((xtn->log.fd_flag & LOGFD_OPENED_HERE) && xtn->log.fd >= 0) close (xtn->log.fd);
	reset_log_to_default (xtn);
}

static void cb_opt_set (hcl_t* hcl, hcl_option_t id, const void* value)
{
	xtn_t* xtn = GET_XTN(hcl);
	int fd;

	if (id != HCL_LOG_TARGET) return; /* return success. not interested */

#if defined(_WIN32)
	fd = _open(hcl->option.log_target, _O_CREAT | _O_WRONLY | _O_APPEND | _O_BINARY , 0644);
#else
	#if defined(HCL_OOCH_IS_UCH)
	fd = open(hcl->option.log_targetx, O_CREAT | O_WRONLY | O_APPEND , 0644);
	#else
	fd = open(hcl->option.log_target, O_CREAT | O_WRONLY | O_APPEND , 0644);
	#endif
#endif
	if (fd == -1)
	{
		/* TODO: any warning that log file not changed? */
	}
	else
	{
		if ((xtn->log.fd_flag & LOGFD_OPENED_HERE) && xtn->log.fd >= 0) close (xtn->log.fd);

		xtn->log.fd = fd;
		xtn->log.fd_flag = LOGFD_OPENED_HERE;
	#if defined(HAVE_ISATTY)
		if (isatty(xtn->log.fd)) xtn->log.fd_flag |= LOGFD_TTY;
	#endif
	}
}

static int open_pipes (hcl_t* hcl, int p[2])
{
	int flags;

#if defined(_WIN32)
	if (_pipe(p, 256, _O_BINARY | _O_NOINHERIT) == -1)
#elif defined(HAVE_PIPE2) && defined(O_CLOEXEC) && defined(O_NONBLOCK)
	if (pipe2(p, O_CLOEXEC | O_NONBLOCK) == -1)
#else
	if (pipe(p) == -1)
#endif
	{
		hcl_seterrbfmtwithsyserr (hcl, 0, errno, "unable to create pipes for iothr management");
		return -1;
	}

#if defined(HAVE_PIPE2) && defined(O_CLOEXEC) && defined(O_NONBLOCK)
		/* do nothing */
#else
	#if defined(FD_CLOEXEC)
	flags = fcntl(p[0], F_GETFD);
	if (flags >= 0) fcntl (p[0], F_SETFD, flags | FD_CLOEXEC);
	flags = fcntl(p[1], F_GETFD);
	if (flags >= 0) fcntl (p[1], F_SETFD, flags | FD_CLOEXEC);
	#endif
	#if defined(O_NONBLOCK)
	flags = fcntl(p[0], F_GETFL);
	if (flags >= 0) fcntl (p[0], F_SETFL, flags | O_NONBLOCK);
	flags = fcntl(p[1], F_GETFL);
	if (flags >= 0) fcntl (p[1], F_SETFL, flags | O_NONBLOCK);
	#endif
#endif

	return 0;
}
static void close_pipes (hcl_t* hcl, int p[2])
{
#if defined(_WIN32)
	_close (p[0]);
	_close (p[1]);
#else
	close (p[0]);
	close (p[1]);
#endif
	p[0] = -1;
	p[1] = -1;
}

static int cb_vm_startup (hcl_t* hcl)
{
	xtn_t* xtn = GET_XTN(hcl);
	int sigfd_pcount = 0;
	int iothr_pcount = 0, flags;

#if defined(_WIN32)
	xtn->waitable_timer = CreateWaitableTimer(HCL_NULL, TRUE, HCL_NULL);
#endif

#if defined(USE_DEVPOLL)
	xtn->ep = open("/dev/poll", O_RDWR);
	if (xtn->ep == -1) 
	{
		hcl_seterrwithsyserr (hcl, 0, errno);
		HCL_DEBUG1 (hcl, "Cannot create devpoll - %hs\n", strerror(errno));
		goto oops;
	}

	#if defined(FD_CLOEXEC)
	flags = fcntl(xtn->ep, F_GETFD);
	if (flags >= 0) fcntl (xtn->ep, F_SETFD, flags | FD_CLOEXEC);
	#endif

#elif defined(USE_KQUEUE)
	#if defined(HAVE_KQUEUE1) && defined(O_CLOEXEC)
	xtn->ep = kqueue1(O_CLOEXEC);
	if (xtn->ep == -1) xtn->ep = kqueue();
	#else
	xtn->ep = kqueue();
	#endif
	if (xtn->ep == -1)
	{
		hcl_seterrwithsyserr (hcl, 0, errno);
		HCL_DEBUG1 (hcl, "Cannot create kqueue - %hs\n", strerror(errno));
		goto oops;
	}

	#if defined(FD_CLOEXEC)
	flags = fcntl(xtn->ep, F_GETFD);
	if (flags >= 0 && !(flags & FD_CLOEXEC)) fcntl (xtn->ep, F_SETFD, flags | FD_CLOEXEC);
	#endif

#elif defined(USE_EPOLL)
	#if defined(HAVE_EPOLL_CREATE1) && defined(EPOLL_CLOEXEC)
	xtn->ep = epoll_create1(EPOLL_CLOEXEC);
	if (xtn->ep == -1) xtn->ep = epoll_create(1024); 
	#else
	xtn->ep = epoll_create(1024);
	#endif
	if (xtn->ep == -1) 
	{
		hcl_seterrwithsyserr (hcl, 0, errno);
		HCL_DEBUG1 (hcl, "Cannot create epoll - %hs\n", strerror(errno));
		goto oops;
	}

	#if defined(FD_CLOEXEC)
	flags = fcntl(xtn->ep, F_GETFD);
	if (flags >= 0 && !(flags & FD_CLOEXEC)) fcntl (xtn->ep, F_SETFD, flags | FD_CLOEXEC);
	#endif

#elif defined(USE_POLL)

	MUTEX_INIT (&xtn->ev.reg.pmtx);

#elif defined(USE_SELECT)
	FD_ZERO (&xtn->ev.reg.rfds);
	FD_ZERO (&xtn->ev.reg.wfds);
	xtn->ev.reg.maxfd = -1;
	MUTEX_INIT (&xtn->ev.reg.smtx);
#endif /* USE_DEVPOLL */

	if (open_pipes(hcl, xtn->sigfd.p) <= -1) goto oops;
	sigfd_pcount = 2;

#if defined(USE_THREAD)
	if (open_pipes(hcl, xtn->iothr.p) <= -1) goto oops;
	iothr_pcount = 2;

	if (_add_poll_fd(hcl, xtn->iothr.p[0], XPOLLIN) <= -1) goto oops;

	pthread_mutex_init (&xtn->ev.mtx, HCL_NULL);
	pthread_cond_init (&xtn->ev.cnd, HCL_NULL);
	pthread_cond_init (&xtn->ev.cnd2, HCL_NULL);
	xtn->ev.halting = 0;

	xtn->iothr.abort = 0;
	xtn->iothr.up = 0;
	/*pthread_create (&xtn->iothr, HCL_NULL, iothr_main, hcl);*/

#endif /* USE_THREAD */

	xtn->vm_running = 1;
	return 0;

oops:
#if defined(USE_THREAD)
	if (iothr_pcount > 0)
	{
		close (xtn->iothr.p[0]);
		close (xtn->iothr.p[1]);
	}
#endif

	if (sigfd_pcount > 0)
	{
		close (xtn->sigfd.p[0]);
		close (xtn->sigfd.p[1]);
	}

#if defined(USE_DEVPOLL) || defined(USE_EPOLL)
	if (xtn->ep >= 0)
	{
		close (xtn->ep);
		xtn->ep = -1;
	}
#endif

	return -1;
}

static void cb_vm_cleanup (hcl_t* hcl)
{
	xtn_t* xtn = GET_XTN(hcl);

	xtn->vm_running = 0;

#if defined(_WIN32)
	if (xtn->waitable_timer)
	{
		CloseHandle (xtn->waitable_timer);
		xtn->waitable_timer = HCL_NULL;
	}
#endif

#if defined(USE_THREAD)
	if (xtn->iothr.up)
	{
		xtn->iothr.abort = 1;
		write (xtn->iothr.p[1], "Q", 1);
		pthread_cond_signal (&xtn->ev.cnd);
		pthread_join (xtn->iothr.thr, HCL_NULL);
		xtn->iothr.up = 0;
	}
	pthread_cond_destroy (&xtn->ev.cnd);
	pthread_cond_destroy (&xtn->ev.cnd2);
	pthread_mutex_destroy (&xtn->ev.mtx);

	_del_poll_fd (hcl, xtn->iothr.p[0]);
	close_pipes (hcl, xtn->iothr.p);
#endif /* USE_THREAD */

	close_pipes (hcl, xtn->sigfd.p);

#if defined(USE_DEVPOLL) 
	if (xtn->ep >= 0)
	{
		close (xtn->ep);
		xtn->ep = -1;
	}
	/*destroy_poll_data_space (hcl);*/
#elif defined(USE_KQUEUE)
	if (xtn->ep >= 0)
	{
		close (xtn->ep);
		xtn->ep = -1;
	}
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
}

/* ----------------------------------------------------------------- 
 * STANDARD HCL
 * ----------------------------------------------------------------- */

hcl_t* hcl_openstdwithmmgr (hcl_mmgr_t* mmgr, hcl_oow_t xtnsize, hcl_errnum_t* errnum)
{
	hcl_t* hcl;
	hcl_vmprim_t vmprim;
	hcl_cb_t cb;

	HCL_MEMSET (&vmprim, 0, HCL_SIZEOF(vmprim));
	vmprim.alloc_heap = alloc_heap;
	vmprim.free_heap = free_heap;
	vmprim.log_write = log_write;
	vmprim.syserrstrb = _syserrstrb;
	vmprim.assertfail = _assertfail;
	vmprim.dl_startup = dl_startup;
	vmprim.dl_cleanup = dl_cleanup;
	vmprim.dl_open = dl_open;
	vmprim.dl_close = dl_close;
	vmprim.dl_getsym = dl_getsym;
	vmprim.vm_gettime = vm_gettime;
	vmprim.vm_muxadd = vm_muxadd;
	vmprim.vm_muxdel = vm_muxdel;
	vmprim.vm_muxmod = vm_muxmod;
	vmprim.vm_muxwait = vm_muxwait;
	vmprim.vm_sleep = vm_sleep;

	hcl = hcl_open(mmgr, HCL_SIZEOF(xtn_t) + xtnsize, &vmprim, errnum);
	if (HCL_UNLIKELY(!hcl)) return HCL_NULL;

	/* adjust the object size by the sizeof xtn_t so that moo_getxtn() returns the right pointer. */
	hcl->_instsize += HCL_SIZEOF(xtn_t);

	reset_log_to_default (GET_XTN(hcl));

	HCL_MEMSET (&cb, 0, HCL_SIZEOF(cb));
	cb.fini = cb_fini;
	cb.opt_set = cb_opt_set;
	cb.vm_startup = cb_vm_startup;
	cb.vm_cleanup = cb_vm_cleanup;
	if (hcl_regcb(hcl, &cb) == HCL_NULL)
	{
		if (errnum) *errnum = hcl_geterrnum(hcl);
		hcl_close (hcl);
		return HCL_NULL;
	}

	return hcl;
}

hcl_t* hcl_openstd (hcl_oow_t xtnsize, hcl_errnum_t* errnum)
{
	return hcl_openstdwithmmgr(&sys_mmgr, xtnsize, errnum);
}
