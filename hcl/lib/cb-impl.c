#include "cb-impl.h"
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

#elif defined(__OS2__)
#	define INCL_DOSMODULEMGR
#	define INCL_DOSPROCESS
#	define INCL_DOSEXCEPTIONS
#	define INCL_DOSMISC
#	define INCL_DOSDATETIME
#	define INCL_DOSFILEMGR
#	define INCL_DOSERRORS
#	include <os2.h>
#	include <time.h>
#	include <io.h>

#elif defined(__DOS__)
#	include <dos.h>
#	include <time.h>
#	include <io.h>
#	include <signal.h>
#	include <errno.h>

#	if defined(_INTELC32_)
#		define DOS_EXIT 0x4C
#	else
#		include <dosfunc.h>
#	endif

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

hcl_errnum_t hcl_vmprim_syserrstrb (hcl_t* hcl, int syserr_type, int syserr_code, hcl_bch_t* buf, hcl_oow_t len)
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

void hcl_vmprim_assertfail (hcl_t* hcl, const hcl_bch_t* expr, const hcl_bch_t* file, hcl_oow_t line)
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
			"#%02d ip=0x%*p sp=0x%*p %s+0x%zu\n", 
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
			hcl_logbfmt(hcl, HCL_LOG_UNTYPED | HCL_LOG_DEBUG, "  %s\n", btsyms[i]);
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

void hcl_vmprim_assertfail (hcl_t* hcl, const hcl_bch_t* expr, const hcl_bch_t* file, hcl_oow_t line)
{
	hcl_logbfmt (hcl, HCL_LOG_UNTYPED | HCL_LOG_FATAL, "ASSERTION FAILURE: %s at %s:%zu\n", expr, file, line);
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

void* hcl_vmprim_alloc_heap (hcl_t* hcl, hcl_oow_t size)
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
	return HCL_MMGR_ALLOC(hcl->_mmgr, size);
#endif
}

void hcl_vmprim_free_heap (hcl_t* hcl, void* ptr)
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

void hcl_vmprim_vm_gettime (hcl_t* hcl, hcl_ntime_t* now)
{
#if defined(_WIN32)

	#if defined(_WIN64) || (defined(_WIN32_WINNT) && (_WIN32_WINNT >= 0x0600))
	hcl_uint64_t bigsec, bigmsec;
	bigmsec = GetTickCount64();
	#else
	xtn_t* xtn = (xtn_t*)hcl_getxtn(hcl);
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
	xtn_t* xtn = (xtn_t*)hcl_getxtn(hcl);
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
 * TODO: MUX
 * ----------------------------------------------------------------- */

#if 0
TODO TODO TODO 
#endif

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

int hcl_vmprim_vm_sleep (hcl_t* hcl, const hcl_ntime_t* dur)
{
#if defined(_WIN32)
	xtn_t* xtn = (xtn_t*)hcl_getxtn(hcl);
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

void hcl_vmprim_dl_startup (hcl_t* hcl)
{
#if defined(USE_LTDL)
	lt_dlinit ();
#endif
}

void hcl_vmprim_dl_cleanup (hcl_t* hcl)
{
#if defined(USE_LTDL)
	lt_dlexit ();
#endif
}

void* hcl_vmprim_dl_open (hcl_t* hcl, const hcl_ooch_t* name, int flags)
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
				HCL_DEBUG2 (hcl, "Unable to open(ext) DL %hs - %s\n", bufptr, dl_errstr);
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

void hcl_vmprim_dl_close (hcl_t* hcl, void* handle)
{
#if defined(USE_LTDL) || defined(USE_DLFCN) || defined(USE_MACH_O_DYLD)
	HCL_DEBUG1 (hcl, "Closed DL handle %p\n", handle);
	sys_dl_close (handle);

#else
	/* TODO: implemenent this */
	HCL_DEBUG1 (hcl, "Dynamic loading not implemented - cannot close handle %p\n", handle);
#endif
}

void* hcl_vmprim_dl_getsym (hcl_t* hcl, void* handle, const hcl_ooch_t* name)
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

