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

/* TODO: remove these headers after having migrated system-dependent functions of of this file */
#if defined(_WIN32)
#	include <windows.h>
#elif defined(__OS2__)
#	define INCL_DOSMISC
#	define INCL_DOSDATETIME
#	define INCL_DOSERRORS
#	include <os2.h>
#	include <time.h>
#elif defined(__MSDOS__)
#	include <time.h>
#elif defined(macintosh)
#	include <Types.h>
#	include <OSUtils.h>
#	include <Timer.h>
#else
#	if defined(HAVE_TIME_H)
#		include <time.h>
#	endif
#	if defined(HAVE_SYS_TIME_H)
#		include <sys/time.h>
#	endif
#endif

#define PROC_STATE_RUNNING 3
#define PROC_STATE_WAITING 2
#define PROC_STATE_RUNNABLE 1
#define PROC_STATE_SUSPENDED 0
#define PROC_STATE_TERMINATED -1

#define SEM_LIST_INC 256
#define SEM_HEAP_INC 256
#define SEM_LIST_MAX (SEM_LIST_INC * 1000)
#define SEM_HEAP_MAX (SEM_HEAP_INC * 1000)

#define SEM_HEAP_PARENT(x) (((x) - 1) / 2)
#define SEM_HEAP_LEFT(x)   ((x) * 2 + 1)
#define SEM_HEAP_RIGHT(x)  ((x) * 2 + 2)

#define SEM_HEAP_EARLIER_THAN(stx,x,y) ( \
	(HCL_OOP_TO_SMOOI((x)->heap_ftime_sec) < HCL_OOP_TO_SMOOI((y)->heap_ftime_sec)) || \
	(HCL_OOP_TO_SMOOI((x)->heap_ftime_sec) == HCL_OOP_TO_SMOOI((y)->heap_ftime_sec) && HCL_OOP_TO_SMOOI((x)->heap_ftime_nsec) < HCL_OOP_TO_SMOOI((y)->heap_ftime_nsec)) \
)


#define LOAD_IP(hcl, v_ctx) ((hcl)->ip = HCL_OOP_TO_SMOOI((v_ctx)->ip))
#define STORE_IP(hcl, v_ctx) ((v_ctx)->ip = HCL_SMOOI_TO_OOP((hcl)->ip))

#define LOAD_SP(hcl, v_ctx) ((hcl)->sp = HCL_OOP_TO_SMOOI((v_ctx)->sp))
#define STORE_SP(hcl, v_ctx) ((v_ctx)->sp = HCL_SMOOI_TO_OOP((hcl)->sp))

#define LOAD_ACTIVE_IP(hcl) LOAD_IP(hcl, (hcl)->active_context)
#define STORE_ACTIVE_IP(hcl) STORE_IP(hcl, (hcl)->active_context)

#define LOAD_ACTIVE_SP(hcl) LOAD_SP(hcl, (hcl)->processor->active)
#define STORE_ACTIVE_SP(hcl) STORE_SP(hcl, (hcl)->processor->active)

#define SWITCH_ACTIVE_CONTEXT(hcl,v_ctx) \
	do \
	{ \
		STORE_ACTIVE_IP (hcl); \
		(hcl)->active_context = (v_ctx); \
		LOAD_ACTIVE_IP (hcl); \
		(hcl)->processor->active->current_context = (hcl)->active_context; \
	} while (0)


#define FETCH_BYTE_CODE(hcl) ((hcl)->code.bc.arr->slot[(hcl)->ip++])
#define FETCH_BYTE_CODE_TO(hcl, v_oow) (v_oow = FETCH_BYTE_CODE(hcl))
#if (HCL_BCODE_LONG_PARAM_SIZE == 2)
#	define FETCH_PARAM_CODE_TO(hcl, v_oow) \
		do { \
			v_oow = FETCH_BYTE_CODE(hcl); \
			v_oow = (v_oow << 8) | FETCH_BYTE_CODE(hcl); \
		} while (0)
#else
#	define FETCH_PARAM_CODE_TO(hcl, v_oow) (v_oow = FETCH_BYTE_CODE(hcl))
#endif


#if defined(HCL_DEBUG_VM_EXEC)
#	define LOG_MASK_INST (HCL_LOG_IC | HCL_LOG_MNEMONIC)

#	define LOG_INST_0(hcl,fmt) HCL_LOG1(hcl, LOG_MASK_INST, "%010zd " fmt "\n", fetched_instruction_pointer)
#	define LOG_INST_1(hcl,fmt,a1) HCL_LOG2(hcl, LOG_MASK_INST, "%010zd " fmt "\n",fetched_instruction_pointer, a1)
#	define LOG_INST_2(hcl,fmt,a1,a2) HCL_LOG3(hcl, LOG_MASK_INST, "%010zd " fmt "\n", fetched_instruction_pointer, a1, a2)
#	define LOG_INST_3(hcl,fmt,a1,a2,a3) HCL_LOG4(hcl, LOG_MASK_INST, "%010zd " fmt "\n", fetched_instruction_pointer, a1, a2, a3)

#else
#	define LOG_INST_0(hcl,fmt)
#	define LOG_INST_1(hcl,fmt,a1)
#	define LOG_INST_2(hcl,fmt,a1,a2)
#	define LOG_INST_3(hcl,fmt,a1,a2,a3)
#endif

/* ------------------------------------------------------------------------- */
static HCL_INLINE void vm_gettime (hcl_t* hcl, hcl_ntime_t* now)
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
#elif defined(__MSDOS__) && defined(_INTELC32_)
	clock_t c;

/* TODO: handle overflow?? */
	c = clock ();
	now->sec = c / CLOCKS_PER_SEC;
	#if (CLOCKS_PER_SEC == 1000)
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
	HCL_SUBNTIME (now, now, &hcl->vm_time_offset); /* offset */
#else
	struct timeval tv;
	gettimeofday (&tv, HCL_NULL);
	HCL_INITNTIME(now, tv.tv_sec, HCL_USEC_TO_NSEC(tv.tv_usec));

	/* at the first call, vm_time_offset should be 0. so subtraction takes
	 * no effect. once it becomes non-zero, it offsets the actual time.
	 * this is to keep the returned time small enough to be held in a 
	 * small integer on platforms where the small integer is not large enough */
	HCL_SUBNTIME (now, now, &hcl->vm_time_offset); 
#endif
}

static HCL_INLINE void vm_sleep (hcl_t* hcl, const hcl_ntime_t* dur)
{
#if defined(_WIN32)
	if (hcl->waitable_timer)
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

#elif defined(__MSDOS__) && defined(_INTELC32_)

	clock_t c;

	c = clock ();
	c += dur->sec * CLOCKS_PER_SEC;
	#if (CLOCKS_PER_SEC == 1000)
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
	while (c > clock()) ;

#else
	struct timespec ts;
	ts.tv_sec = dur->sec;
	ts.tv_nsec = dur->nsec;
	nanosleep (&ts, HCL_NULL);
#endif
}


static void vm_startup (hcl_t* hcl)
{
	hcl_ntime_t now;

#if defined(_WIN32)
	hcl->waitable_timer = CreateWaitableTimer(HCL_NULL, TRUE, HCL_NULL);
#endif

	/* reset hcl->vm_time_offset so that vm_gettime is not affected */
	HCL_INITNTIME(&hcl->vm_time_offset, 0, 0);
	vm_gettime (hcl, &now);
	hcl->vm_time_offset = now;
}

static void vm_cleanup (hcl_t* hcl)
{
#if defined(_WIN32)
	if (hcl->waitable_timer)
	{
		CloseHandle (hcl->waitable_timer);
		hcl->waitable_timer = HCL_NULL;
	}
#endif
}

/* ------------------------------------------------------------------------- */

static HCL_INLINE hcl_oop_t make_context (hcl_t* hcl, hcl_ooi_t ntmprs)
{
	HCL_ASSERT (ntmprs >= 0);
	return hcl_allocoopobj (hcl, HCL_BRAND_CONTEXT, HCL_CONTEXT_NAMED_INSTVARS + (hcl_oow_t)ntmprs);
}

static hcl_oop_process_t make_process (hcl_t* hcl, hcl_oop_context_t c)
{
	hcl_oop_process_t proc;
	hcl_oow_t stksize;

	stksize = hcl->option.dfl_procstk_size;
	if (stksize > HCL_TYPE_MAX(hcl_oow_t) - HCL_PROCESS_NAMED_INSTVARS)
		stksize = HCL_TYPE_MAX(hcl_oow_t) - HCL_PROCESS_NAMED_INSTVARS;

	hcl_pushtmp (hcl, (hcl_oop_t*)&c);
	proc =  (hcl_oop_process_t)hcl_allocoopobj (hcl, HCL_BRAND_PROCESS, HCL_PROCESS_NAMED_INSTVARS + stksize);
	hcl_poptmp (hcl);
	if (!proc) return HCL_NULL;

	proc->state = HCL_SMOOI_TO_OOP(PROC_STATE_SUSPENDED);
	proc->initial_context = c;
	proc->current_context = c;
	proc->sp = HCL_SMOOI_TO_OOP(-1);

	HCL_ASSERT ((hcl_oop_t)c->sender == hcl->_nil);

#if defined(HCL_DEBUG_VM_PROCESSOR)
	HCL_LOG2 (hcl, HCL_LOG_IC | HCL_LOG_DEBUG, "Processor - made process %O of size %zu\n", proc, HCL_OBJ_GET_SIZE(proc));
#endif
	return proc;
}

static HCL_INLINE void sleep_active_process (hcl_t* hcl, int state)
{
#if defined(HCL_DEBUG_VM_PROCESSOR)
	HCL_LOG3 (hcl, HCL_LOG_IC | HCL_LOG_DEBUG, "Processor - put process %O context %O ip=%zd to sleep\n", hcl->processor->active, hcl->active_context, hcl->ip);
#endif

	STORE_ACTIVE_SP(hcl);

	/* store the current active context to the current process.
	 * it is the suspended context of the process to be suspended */
	HCL_ASSERT (hcl->processor->active != hcl->nil_process);
	hcl->processor->active->current_context = hcl->active_context;
	hcl->processor->active->state = HCL_SMOOI_TO_OOP(state);
}

static HCL_INLINE void wake_new_process (hcl_t* hcl, hcl_oop_process_t proc)
{
	/* activate the given process */
	proc->state = HCL_SMOOI_TO_OOP(PROC_STATE_RUNNING);
	hcl->processor->active = proc;

	LOAD_ACTIVE_SP(hcl);

	/* activate the suspended context of the new process */
	SWITCH_ACTIVE_CONTEXT (hcl, proc->current_context);

#if defined(HCL_DEBUG_VM_PROCESSOR)
	HCL_LOG3 (hcl, HCL_LOG_IC | HCL_LOG_DEBUG, "Processor - woke up process %O context %O ip=%zd\n", hcl->processor->active, hcl->active_context, hcl->ip);
#endif
}

static void switch_to_process (hcl_t* hcl, hcl_oop_process_t proc, int new_state_for_old_active)
{
	/* the new process must not be the currently active process */
	HCL_ASSERT (hcl->processor->active != proc);

	/* the new process must be in the runnable state */
	HCL_ASSERT (proc->state == HCL_SMOOI_TO_OOP(PROC_STATE_RUNNABLE) ||
	            proc->state == HCL_SMOOI_TO_OOP(PROC_STATE_WAITING));

	sleep_active_process (hcl, new_state_for_old_active);
	wake_new_process (hcl, proc);

	hcl->proc_switched = 1;
}

static HCL_INLINE hcl_oop_process_t find_next_runnable_process (hcl_t* hcl)
{
	hcl_oop_process_t npr;

	HCL_ASSERT (hcl->processor->active->state == HCL_SMOOI_TO_OOP(PROC_STATE_RUNNING));
	npr = hcl->processor->active->next;
	if ((hcl_oop_t)npr == hcl->_nil) npr = hcl->processor->runnable_head;
	return npr;
}

static HCL_INLINE void switch_to_next_runnable_process (hcl_t* hcl)
{
	hcl_oop_process_t nrp;

	nrp = find_next_runnable_process (hcl);
	if (nrp != hcl->processor->active) switch_to_process (hcl, nrp, PROC_STATE_RUNNABLE);
}

static HCL_INLINE int chain_into_processor (hcl_t* hcl, hcl_oop_process_t proc)
{
	/* the process is not scheduled at all. 
	 * link it to the processor's process list. */
	hcl_ooi_t tally;

	HCL_ASSERT ((hcl_oop_t)proc->prev == hcl->_nil);
	HCL_ASSERT ((hcl_oop_t)proc->next == hcl->_nil);

	HCL_ASSERT (proc->state == HCL_SMOOI_TO_OOP(PROC_STATE_SUSPENDED));

	tally = HCL_OOP_TO_SMOOI(hcl->processor->tally);

	HCL_ASSERT (tally >= 0);
	if (tally >= HCL_SMOOI_MAX)
	{
#if defined(HCL_DEBUG_VM_PROCESSOR)
		HCL_LOG0 (hcl, HCL_LOG_IC | HCL_LOG_FATAL, "Processor - too many process\n");
#endif
		hcl->errnum = HCL_EPFULL;
		return -1;
	}

	/* append to the runnable list */
	if (tally > 0)
	{
		proc->prev = hcl->processor->runnable_tail;
		hcl->processor->runnable_tail->next = proc;
	}
	else
	{
		hcl->processor->runnable_head = proc;
	}
	hcl->processor->runnable_tail = proc;

	tally++;
	hcl->processor->tally = HCL_SMOOI_TO_OOP(tally);

	return 0;
}

static HCL_INLINE void unchain_from_processor (hcl_t* hcl, hcl_oop_process_t proc, int state)
{
	hcl_ooi_t tally;

	/* the processor's process chain must be composed of running/runnable
	 * processes only */
	HCL_ASSERT (proc->state == HCL_SMOOI_TO_OOP(PROC_STATE_RUNNING) ||
	             proc->state == HCL_SMOOI_TO_OOP(PROC_STATE_RUNNABLE));

	tally = HCL_OOP_TO_SMOOI(hcl->processor->tally);
	HCL_ASSERT (tally > 0);

	if ((hcl_oop_t)proc->prev != hcl->_nil) proc->prev->next = proc->next;
	else hcl->processor->runnable_head = proc->next;
	if ((hcl_oop_t)proc->next != hcl->_nil) proc->next->prev = proc->prev;
	else hcl->processor->runnable_tail = proc->prev;

	proc->prev = (hcl_oop_process_t)hcl->_nil;
	proc->next = (hcl_oop_process_t)hcl->_nil;
	proc->state = HCL_SMOOI_TO_OOP(state);

	tally--;
	if (tally == 0) hcl->processor->active = hcl->nil_process;
	hcl->processor->tally = HCL_SMOOI_TO_OOP(tally);
}

static HCL_INLINE void chain_into_semaphore (hcl_t* hcl, hcl_oop_process_t proc, hcl_oop_semaphore_t sem)
{
	/* append a process to the process list of a semaphore*/

	HCL_ASSERT ((hcl_oop_t)proc->sem == hcl->_nil);
	HCL_ASSERT ((hcl_oop_t)proc->prev == hcl->_nil);
	HCL_ASSERT ((hcl_oop_t)proc->next == hcl->_nil);

	if ((hcl_oop_t)sem->waiting_head == hcl->_nil)
	{
		HCL_ASSERT ((hcl_oop_t)sem->waiting_tail == hcl->_nil);
		sem->waiting_head = proc;
	}
	else
	{
		proc->prev = sem->waiting_tail;
		sem->waiting_tail->next = proc;
	}
	sem->waiting_tail = proc;

	proc->sem = sem;
}

static HCL_INLINE void unchain_from_semaphore (hcl_t* hcl, hcl_oop_process_t proc)
{
	hcl_oop_semaphore_t sem;

	HCL_ASSERT ((hcl_oop_t)proc->sem != hcl->_nil);

	sem = proc->sem;
	if ((hcl_oop_t)proc->prev != hcl->_nil) proc->prev->next = proc->next;
	else sem->waiting_head = proc->next;
	if ((hcl_oop_t)proc->next != hcl->_nil) proc->next->prev = proc->prev;
	else sem->waiting_tail = proc->prev;

	proc->prev = (hcl_oop_process_t)hcl->_nil;
	proc->next = (hcl_oop_process_t)hcl->_nil;

	proc->sem = (hcl_oop_semaphore_t)hcl->_nil;
}

static void terminate_process (hcl_t* hcl, hcl_oop_process_t proc)
{
	if (proc->state == HCL_SMOOI_TO_OOP(PROC_STATE_RUNNING) ||
	    proc->state == HCL_SMOOI_TO_OOP(PROC_STATE_RUNNABLE))
	{
		/* RUNNING/RUNNABLE ---> TERMINATED */

	#if defined(HCL_DEBUG_VM_PROCESSOR)
		HCL_LOG1 (hcl, HCL_LOG_IC | HCL_LOG_DEBUG, "Processor - process %O RUNNING/RUNNABLE->TERMINATED\n", proc);
	#endif

		if (proc == hcl->processor->active)
		{
			hcl_oop_process_t nrp;

			nrp = find_next_runnable_process (hcl);

			unchain_from_processor (hcl, proc, PROC_STATE_TERMINATED);
			proc->sp = HCL_SMOOI_TO_OOP(-1); /* invalidate the process stack */
			proc->current_context = proc->initial_context; /* not needed but just in case */

			/* a runnable or running process must not be chanined to the
			 * process list of a semaphore */
			HCL_ASSERT ((hcl_oop_t)proc->sem == hcl->_nil);

			if (nrp == proc)
			{
				/* no runnable process after termination */
				HCL_ASSERT (hcl->processor->active == hcl->nil_process);
				HCL_LOG0 (hcl, HCL_LOG_IC | HCL_LOG_DEBUG, "No runnable process after process termination\n");
			}
			else
			{
				switch_to_process (hcl, nrp, PROC_STATE_TERMINATED);
			}
		}
		else
		{
			unchain_from_processor (hcl, proc, PROC_STATE_TERMINATED);
			proc->sp = HCL_SMOOI_TO_OOP(-1); /* invalidate the process stack */
		}
	}
	else if (proc->state == HCL_SMOOI_TO_OOP(PROC_STATE_SUSPENDED))
	{
		/* SUSPENDED ---> TERMINATED */
	#if defined(HCL_DEBUG_VM_PROCESSOR)
		HCL_LOG1 (hcl, HCL_LOG_IC | HCL_LOG_DEBUG, "Processor - process %O SUSPENDED->TERMINATED\n", proc);
	#endif

		proc->state = HCL_SMOOI_TO_OOP(PROC_STATE_TERMINATED);
		proc->sp = HCL_SMOOI_TO_OOP(-1); /* invalidate the proce stack */

		if ((hcl_oop_t)proc->sem != hcl->_nil)
		{
			unchain_from_semaphore (hcl, proc);
		}
	}
	else if (proc->state == HCL_SMOOI_TO_OOP(PROC_STATE_WAITING))
	{
		/* WAITING ---> TERMINATED */
		/* TODO: */
	}
}

static void resume_process (hcl_t* hcl, hcl_oop_process_t proc)
{
	if (proc->state == HCL_SMOOI_TO_OOP(PROC_STATE_SUSPENDED))
	{
		/* SUSPENED ---> RUNNING */
		HCL_ASSERT ((hcl_oop_t)proc->prev == hcl->_nil);
		HCL_ASSERT ((hcl_oop_t)proc->next == hcl->_nil);

	#if defined(HCL_DEBUG_VM_PROCESSOR)
		HCL_LOG1 (hcl, HCL_LOG_IC | HCL_LOG_DEBUG, "Processor - process %O SUSPENDED->RUNNING\n", proc);
	#endif

		chain_into_processor (hcl, proc); /* TODO: error check */

		/*proc->current_context = proc->initial_context;*/
		proc->state = HCL_SMOOI_TO_OOP(PROC_STATE_RUNNABLE);

		/* don't switch to this process. just set the state to RUNNING */
	}
#if 0
	else if (proc->state == HCL_SMOOI_TO_OOP(PROC_STATE_RUNNABLE))
	{
		/* RUNNABLE ---> RUNNING */
		/* TODO: should i allow this? */
		HCL_ASSERT (hcl->processor->active != proc);
		switch_to_process (hcl, proc, PROC_STATE_RUNNABLE);
	}
#endif
}

static void suspend_process (hcl_t* hcl, hcl_oop_process_t proc)
{
	if (proc->state == HCL_SMOOI_TO_OOP(PROC_STATE_RUNNING) ||
	    proc->state == HCL_SMOOI_TO_OOP(PROC_STATE_RUNNABLE))
	{
		/* RUNNING/RUNNABLE ---> SUSPENDED */

	#if defined(HCL_DEBUG_VM_PROCESSOR)
		HCL_LOG1 (hcl, HCL_LOG_IC | HCL_LOG_DEBUG, "Processor - process %O RUNNING/RUNNABLE->SUSPENDED\n", proc);
	#endif

		if (proc == hcl->processor->active)
		{
			hcl_oop_process_t nrp;

			nrp = find_next_runnable_process (hcl);

			if (nrp == proc)
			{
				/* no runnable process after suspension */
				sleep_active_process (hcl, PROC_STATE_RUNNABLE);
				unchain_from_processor (hcl, proc, PROC_STATE_SUSPENDED);

				/* the last running/runnable process has been unchained 
				 * from the processor and set to SUSPENDED. the active
				 * process must be the nil process */
				HCL_ASSERT (hcl->processor->active == hcl->nil_process);
			}
			else
			{
				/* keep the unchained process at the runnable state for
				 * the immediate call to switch_to_process() below */
				unchain_from_processor (hcl, proc, PROC_STATE_RUNNABLE);
				/* unchain_from_processor() leaves the active process
				 * untouched unless the unchained process is the last
				 * running/runnable process. so calling switch_to_process()
				 * which expects the active process to be valid is safe */
				HCL_ASSERT (hcl->processor->active != hcl->nil_process);
				switch_to_process (hcl, nrp, PROC_STATE_SUSPENDED);
			}
		}
		else
		{
			unchain_from_processor (hcl, proc, PROC_STATE_SUSPENDED);
		}
	}
}

static void yield_process (hcl_t* hcl, hcl_oop_process_t proc)
{
	if (proc->state == HCL_SMOOI_TO_OOP(PROC_STATE_RUNNING))
	{
		/* RUNNING --> RUNNABLE */

		hcl_oop_process_t nrp;

		HCL_ASSERT (proc == hcl->processor->active);

		nrp = find_next_runnable_process (hcl); 
		/* if there are more than 1 runnable processes, the next
		 * runnable process must be different from proc */
		if (nrp != proc) 
		{
		#if defined(HCL_DEBUG_VM_PROCESSOR)
			HCL_LOG1 (hcl, HCL_LOG_IC | HCL_LOG_DEBUG, "Processor - process %O RUNNING->RUNNABLE\n", proc);
		#endif
			switch_to_process (hcl, nrp, PROC_STATE_RUNNABLE);
		}
	}
}

static int async_signal_semaphore (hcl_t* hcl, hcl_oop_semaphore_t sem)
{
	if (hcl->sem_list_count >= SEM_LIST_MAX)
	{
		hcl->errnum = HCL_ESLFULL;
		return -1;
	}

	if (hcl->sem_list_count >= hcl->sem_list_capa)
	{
		hcl_oow_t new_capa;
		hcl_oop_semaphore_t* tmp;

		new_capa = hcl->sem_list_capa + SEM_LIST_INC; /* TODO: overflow check.. */
		tmp = hcl_reallocmem (hcl, hcl->sem_list, HCL_SIZEOF(hcl_oop_semaphore_t) * new_capa);
		if (!tmp) return -1;

		hcl->sem_list = tmp;
		hcl->sem_list_capa = new_capa;
	}

	hcl->sem_list[hcl->sem_list_count] = sem;
	hcl->sem_list_count++;
	return 0;
}

static hcl_oop_process_t signal_semaphore (hcl_t* hcl, hcl_oop_semaphore_t sem)
{
	hcl_oop_process_t proc;
	hcl_ooi_t count;

	if ((hcl_oop_t)sem->waiting_head == hcl->_nil)
	{
		/* no process is waiting on this semaphore */
		count = HCL_OOP_TO_SMOOI(sem->count);
		count++;
		sem->count = HCL_SMOOI_TO_OOP(count);

		/* no process has been resumed */
		return (hcl_oop_process_t)hcl->_nil;
	}
	else
	{
		proc = sem->waiting_head;

		/* [NOTE] no GC must occur as 'proc' isn't protected with hcl_pushtmp(). */

		unchain_from_semaphore (hcl, proc);
		resume_process (hcl, proc); /* TODO: error check */

		/* return the resumed process */
		return proc;
	}
}

static void await_semaphore (hcl_t* hcl, hcl_oop_semaphore_t sem)
{
	hcl_oop_process_t proc;
	hcl_ooi_t count;

	count = HCL_OOP_TO_SMOOI(sem->count);
	if (count > 0)
	{
		/* it's already signalled */
		count--;
		sem->count = HCL_SMOOI_TO_OOP(count);
	}
	else
	{
		/* not signaled. need to wait */
		proc = hcl->processor->active;

		/* suspend the active process */
		suspend_process (hcl, proc); 

		/* link the suspended process to the semaphore's process list */
		chain_into_semaphore (hcl, proc, sem); 

		HCL_ASSERT (sem->waiting_tail == proc);

		HCL_ASSERT (hcl->processor->active != proc);
	}
}

static void sift_up_sem_heap (hcl_t* hcl, hcl_ooi_t index)
{
	if (index > 0)
	{
		hcl_ooi_t parent;
		hcl_oop_semaphore_t sem, parsem;

		parent = SEM_HEAP_PARENT(index);
		sem = hcl->sem_heap[index];
		parsem = hcl->sem_heap[parent];
		if (SEM_HEAP_EARLIER_THAN(hcl, sem, parsem))
		{
			do
			{
				/* move down the parent to the current position */
				parsem->heap_index = HCL_SMOOI_TO_OOP(index);
				hcl->sem_heap[index] = parsem;

				/* traverse up */
				index = parent;
				if (index <= 0) break;

				parent = SEM_HEAP_PARENT(parent);
				parsem = hcl->sem_heap[parent];
			}
			while (SEM_HEAP_EARLIER_THAN(hcl, sem, parsem));

			sem->heap_index = HCL_SMOOI_TO_OOP(index);
			hcl->sem_heap[index] = sem;
		}
	}
}

static void sift_down_sem_heap (hcl_t* hcl, hcl_ooi_t index)
{
	hcl_ooi_t base = hcl->sem_heap_count / 2;

	if (index < base) /* at least 1 child is under the 'index' position */
	{
		hcl_ooi_t left, right, child;
		hcl_oop_semaphore_t sem, chisem;

		sem = hcl->sem_heap[index];
		do
		{
			left = SEM_HEAP_LEFT(index);
			right = SEM_HEAP_RIGHT(index);

			if (right < hcl->sem_heap_count && SEM_HEAP_EARLIER_THAN(hcl, hcl->sem_heap[left], hcl->sem_heap[right]))
			{
				child = right;
			}
			else
			{
				child = left;
			}

			chisem = hcl->sem_heap[child];
			if (SEM_HEAP_EARLIER_THAN(hcl, sem, chisem)) break;

			chisem->heap_index = HCL_SMOOI_TO_OOP(index);
			hcl->sem_heap[index ] = chisem;

			index = child;
		}
		while (index < base);

		sem->heap_index = HCL_SMOOI_TO_OOP(index);
		hcl->sem_heap[index] = sem;
	}
}

static int add_to_sem_heap (hcl_t* hcl, hcl_oop_semaphore_t sem)
{
	hcl_ooi_t index;

	if (hcl->sem_heap_count >= SEM_HEAP_MAX)
	{
		hcl->errnum = HCL_ESHFULL;
		return -1;
	}

	if (hcl->sem_heap_count >= hcl->sem_heap_capa)
	{
		hcl_oow_t new_capa;
		hcl_oop_semaphore_t* tmp;

		/* no overflow check when calculating the new capacity
		 * owing to SEM_HEAP_MAX check above */
		new_capa = hcl->sem_heap_capa + SEM_HEAP_INC;
		tmp = hcl_reallocmem (hcl, hcl->sem_heap, HCL_SIZEOF(hcl_oop_semaphore_t) * new_capa);
		if (!tmp) return -1;

		hcl->sem_heap = tmp;
		hcl->sem_heap_capa = new_capa;
	}

	HCL_ASSERT (hcl->sem_heap_count <= HCL_SMOOI_MAX);

	index = hcl->sem_heap_count;
	hcl->sem_heap[index] = sem;
	sem->heap_index = HCL_SMOOI_TO_OOP(index);
	hcl->sem_heap_count++;

	sift_up_sem_heap (hcl, index);
	return 0;
}

static void delete_from_sem_heap (hcl_t* hcl, hcl_ooi_t index)
{
	hcl_oop_semaphore_t sem, lastsem;

	sem = hcl->sem_heap[index];
	sem->heap_index = HCL_SMOOI_TO_OOP(-1);

	hcl->sem_heap_count--;
	if (hcl->sem_heap_count > 0 && index != hcl->sem_heap_count)
	{
		/* move the last item to the deletion position */
		lastsem = hcl->sem_heap[hcl->sem_heap_count];
		lastsem->heap_index = HCL_SMOOI_TO_OOP(index);
		hcl->sem_heap[index] = lastsem;

		if (SEM_HEAP_EARLIER_THAN(hcl, lastsem, sem)) 
			sift_up_sem_heap (hcl, index);
		else
			sift_down_sem_heap (hcl, index);
	}
}

static void update_sem_heap (hcl_t* hcl, hcl_ooi_t index, hcl_oop_semaphore_t newsem)
{
	hcl_oop_semaphore_t sem;

	sem = hcl->sem_heap[index];
	sem->heap_index = HCL_SMOOI_TO_OOP(-1);

	newsem->heap_index = HCL_SMOOI_TO_OOP(index);
	hcl->sem_heap[index] = newsem;

	if (SEM_HEAP_EARLIER_THAN(hcl, newsem, sem))
		sift_up_sem_heap (hcl, index);
	else
		sift_down_sem_heap (hcl, index);
}
/* ------------------------------------------------------------------------- */

static int __activate_context (hcl_t* hcl, hcl_oop_context_t rcv_blkctx, hcl_ooi_t nargs, hcl_oop_context_t* pblkctx)
{
	/* prepare a new block context for activation.
	 * the receiver must be a block context which becomes the base
	 * for a new block context. */

	hcl_oop_context_t blkctx;
	hcl_ooi_t local_ntmprs, i;

	/* TODO: find a better way to support a reentrant block context. */

	/* | sum |
	 * sum := [ :n | (n < 2) ifTrue: [1] ifFalse: [ n + (sum value: (n - 1))] ].
	 * (sum value: 10).
	 * 
	 * For the code above, sum is a block context and it is sent value: inside
	 * itself. Let me simply clone a block context to allow reentrancy like this
	 * while the block context is active
	 */

	/* the receiver must be a block context */
	//HCL_ASSERT (HCL_CLASSOF(hcl, rcv_blkctx) == hcl->_block_context);
	HCL_ASSERT (HCL_IS_CONTEXT (hcl, rcv_blkctx));
	if (rcv_blkctx->receiver_or_source != hcl->_nil)
	{
		/* the 'source' field is not nil.
		 * this block context has already been activated once.
		 * you can't send 'value' again to reactivate it.
		 * For example, [thisContext value] value. */
		HCL_ASSERT (HCL_OBJ_GET_SIZE(rcv_blkctx) > HCL_CONTEXT_NAMED_INSTVARS);
		HCL_LOG1 (hcl, HCL_LOG_IC | HCL_LOG_ERROR, 
			"Error - re-valuing of a block context - %O\n", rcv_blkctx);
		hcl->errnum = HCL_ERECALL;
		return -1;
	}
	HCL_ASSERT (HCL_OBJ_GET_SIZE(rcv_blkctx) == HCL_CONTEXT_NAMED_INSTVARS);

	if (HCL_OOP_TO_SMOOI(rcv_blkctx->method_or_nargs) != nargs)
	{
		HCL_LOG3 (hcl, HCL_LOG_IC | HCL_LOG_ERROR, 
			"Error - wrong number of arguments to a block context %O - expecting %zd, got %zd\n",
			rcv_blkctx, HCL_OOP_TO_SMOOI(rcv_blkctx->method_or_nargs), nargs);
		hcl->errnum = HCL_ECALLARG;
		return -1;
	}

	/* the number of temporaries stored in the block context
	 * accumulates the number of temporaries starting from the origin.
	 * simple calculation is needed to find the number of local temporaries */
	local_ntmprs = HCL_OOP_TO_SMOOI(rcv_blkctx->ntmprs) -
	               HCL_OOP_TO_SMOOI(((hcl_oop_context_t)rcv_blkctx->home)->ntmprs);
	HCL_ASSERT (local_ntmprs >= nargs);

	/* create a new block context to clone rcv_blkctx */
	hcl_pushtmp (hcl, (hcl_oop_t*)&rcv_blkctx);
	blkctx = (hcl_oop_context_t) make_context (hcl, local_ntmprs); 
	hcl_poptmp (hcl);
	if (!blkctx) return -1;

#if 0
	/* shallow-copy the named part including home, origin, etc. */
	for (i = 0; i < HCL_CONTEXT_NAMED_INSTVARS; i++)
	{
		((hcl_oop_oop_t)blkctx)->slot[i] = ((hcl_oop_oop_t)rcv_blkctx)->slot[i];
	}
#else
	blkctx->ip = rcv_blkctx->ip;
	blkctx->ntmprs = rcv_blkctx->ntmprs;
	blkctx->method_or_nargs = rcv_blkctx->method_or_nargs;
	blkctx->receiver_or_source = (hcl_oop_t)rcv_blkctx;
	blkctx->home = rcv_blkctx->home;
	blkctx->origin = rcv_blkctx->origin;
#endif

/* TODO: check the stack size of a block context to see if it's large enough to hold arguments */
	/* copy the arguments to the stack */
	for (i = 0; i < nargs; i++)
	{
		blkctx->slot[i] = HCL_STACK_GETARG(hcl, nargs, i);
	}

	HCL_STACK_POPS (hcl, nargs + 1); /* pop arguments and receiver */

	HCL_ASSERT (blkctx->home != hcl->_nil);
	blkctx->sp = HCL_SMOOI_TO_OOP(-1); /* not important at all */
	blkctx->sender = hcl->active_context;

	*pblkctx = blkctx;
	return 0;
}

static HCL_INLINE int activate_context (hcl_t* hcl, hcl_ooi_t nargs)
{
	int x;
	hcl_oop_context_t rcv, blkctx;

	rcv = (hcl_oop_context_t)HCL_STACK_GETRCV(hcl, nargs);
	HCL_ASSERT (HCL_IS_CONTEXT (hcl, rcv));

	x = __activate_context (hcl, rcv, nargs, &blkctx);
	if (x <= -1) return -1;

	SWITCH_ACTIVE_CONTEXT (hcl, (hcl_oop_context_t)blkctx);
	return 0;
}

/* ------------------------------------------------------------------------- */
static HCL_INLINE int call_primitive (hcl_t* hcl, hcl_ooi_t nargs)
{
	hcl_oop_word_t rcv;

	rcv = (hcl_oop_word_t)HCL_STACK_GETRCV(hcl, nargs);
	HCL_ASSERT (HCL_IS_PRIM (hcl, rcv));

	if (nargs < rcv->slot[1] && nargs > rcv->slot[2])
	{
/* TODO: include a primitive name... */
		HCL_LOG3 (hcl, HCL_LOG_IC | HCL_LOG_ERROR, 
			"Error - wrong number of arguments to a primitive - expecting %zd-%zd, got %zd\n",
			rcv->slot[1], rcv->slot[2], nargs);
		hcl->errnum = HCL_ECALLARG;
		return -1;
	}

	return ((hcl_prim_impl_t)rcv->slot[0]) (hcl, nargs);
}

/* ------------------------------------------------------------------------- */
static hcl_oop_process_t start_initial_process (hcl_t* hcl, hcl_oop_context_t ctx)
{
	hcl_oop_process_t proc;

	/* there must be no active process when this function is called */
	HCL_ASSERT (hcl->processor->tally == HCL_SMOOI_TO_OOP(0));
	HCL_ASSERT (hcl->processor->active == hcl->nil_process);

	proc = make_process (hcl, ctx);
	if (!proc) return HCL_NULL;

	if (chain_into_processor (hcl, proc) <= -1) return HCL_NULL;
	proc->state = HCL_SMOOI_TO_OOP(PROC_STATE_RUNNING); /* skip RUNNABLE and go to RUNNING */
	hcl->processor->active = proc;

	/* do something that resume_process() would do with less overhead */
	HCL_ASSERT ((hcl_oop_t)proc->current_context != hcl->_nil);
	HCL_ASSERT (proc->current_context == proc->initial_context);
	SWITCH_ACTIVE_CONTEXT (hcl, proc->current_context);

	return proc;
}

static int start_initial_process_and_context (hcl_t* hcl)
{
	hcl_oop_context_t ctx;
	hcl_oop_process_t proc;

	/* create a fake initial context. */
	ctx = (hcl_oop_context_t)make_context (hcl, 0); /* no temporary variables */
	if (!ctx) return -1;

	/* the initial context starts the life of the entire VM
	 * and is not really worked on except that it is used to call the
	 * initial method. so it doesn't really require any extra stack space. */
/* TODO: verify this theory of mine. */
	hcl->ip = 0;
	hcl->sp = -1;

	ctx->ip = HCL_SMOOI_TO_OOP(0); /* point to the beginning */
	ctx->sp = HCL_SMOOI_TO_OOP(-1); /* pointer to -1 below the bottom */
	ctx->origin = ctx; /* point to self */
	/*ctx->method_or_nargs = (hcl_oop_t)mth;*/ /* fake. help SWITCH_ACTIVE_CONTEXT() not fail. */
	ctx->method_or_nargs = HCL_SMOOI_TO_OOP(0);
/* TODO: XXXXX */
	ctx->ntmprs = HCL_SMOOI_TO_OOP(0);
	ctx->home = ctx; // is this correct???
/* END XXXXX */

	/* [NOTE]
	 *  the receiver field and the sender field of ctx are nils.
	 *  especially, the fact that the sender field is nil is used by 
	 *  the main execution loop for breaking out of the loop */

	HCL_ASSERT (hcl->active_context == HCL_NULL);

	/* hcl_gc() uses hcl->processor when hcl->active_context
	 * is not NULL. at this poinst, hcl->processor should point to
	 * an instance of ProcessScheduler. */
	HCL_ASSERT ((hcl_oop_t)hcl->processor != hcl->_nil);
	HCL_ASSERT (hcl->processor->tally == HCL_SMOOI_TO_OOP(0));

	/* start_initial_process() calls the SWITCH_ACTIVE_CONTEXT() macro.
	 * the macro assumes a non-null value in hcl->active_context.
	 * let's force set active_context to ctx directly. */
	hcl->active_context = ctx;

	hcl_pushtmp (hcl, (hcl_oop_t*)&ctx);
	proc = start_initial_process (hcl, ctx); 
	hcl_poptmp (hcl);
	if (!proc) return -1;

	HCL_STACK_PUSH (hcl, (hcl_oop_t)ctx);
	STORE_ACTIVE_SP (hcl); /* hcl->active_context->sp = HCL_SMOOI_TO_OOP(hcl->sp) */

	return activate_context (hcl, 0);
}

/* ------------------------------------------------------------------------- */

static int execute (hcl_t* hcl)
{
	hcl_oob_t bcode;
	hcl_oow_t b1, b2;
	hcl_oop_t return_value;
	int unwind_protect;
	hcl_oop_context_t unwind_start;
	hcl_oop_context_t unwind_stop;

#if defined(HCL_PROFILE_VM)
	hcl_uintmax_t inst_counter = 0;
#endif

#if defined(HCL_DEBUG_VM_EXEC)
	hcl_ooi_t fetched_instruction_pointer;
#endif

	HCL_ASSERT (hcl->active_context != HCL_NULL);

	vm_startup (hcl);
	hcl->proc_switched = 0;

	while (1)
	{
		if (hcl->sem_heap_count > 0)
		{
			hcl_ntime_t ft, now;
			vm_gettime (hcl, &now);

			do
			{
				HCL_ASSERT (HCL_OOP_IS_SMOOI(hcl->sem_heap[0]->heap_ftime_sec));
				HCL_ASSERT (HCL_OOP_IS_SMOOI(hcl->sem_heap[0]->heap_ftime_nsec));

				HCL_INITNTIME (&ft,
					HCL_OOP_TO_SMOOI(hcl->sem_heap[0]->heap_ftime_sec),
					HCL_OOP_TO_SMOOI(hcl->sem_heap[0]->heap_ftime_nsec)
				);

				if (HCL_CMPNTIME(&ft, (hcl_ntime_t*)&now) <= 0)
				{
					hcl_oop_process_t proc;

					/* waited long enough. signal the semaphore */

					proc = signal_semaphore (hcl, hcl->sem_heap[0]);
					/* [NOTE] no hcl_pushtmp() on proc. no GC must occur
					 *        in the following line until it's used for
					 *        wake_new_process() below. */
					delete_from_sem_heap (hcl, 0);

					/* if no process is waiting on the semaphore, 
					 * signal_semaphore() returns hcl->_nil. */

					if (hcl->processor->active == hcl->nil_process && (hcl_oop_t)proc != hcl->_nil)
					{
						/* this is the only runnable process. 
						 * switch the process to the running state.
						 * it uses wake_new_process() instead of
						 * switch_to_process() as there is no running 
						 * process at this moment */
						HCL_ASSERT (proc->state == HCL_SMOOI_TO_OOP(PROC_STATE_RUNNABLE));
						HCL_ASSERT (proc == hcl->processor->runnable_head);

						wake_new_process (hcl, proc);
						hcl->proc_switched = 1;
					}
				}
				else if (hcl->processor->active == hcl->nil_process)
				{
					HCL_SUBNTIME (&ft, &ft, (hcl_ntime_t*)&now);
					vm_sleep (hcl, &ft); /* TODO: change this to i/o multiplexer??? */
					vm_gettime (hcl, &now);
				}
				else 
				{
					break;
				}
			} 
			while (hcl->sem_heap_count > 0);
		}

		if (hcl->processor->active == hcl->nil_process) 
		{
			/* no more waiting semaphore and no more process */
			HCL_ASSERT (hcl->processor->tally = HCL_SMOOI_TO_OOP(0));
			HCL_LOG0 (hcl, HCL_LOG_IC | HCL_LOG_DEBUG, "No more runnable process\n");

			#if 0
			if (there is semaphore awaited.... )
			{
			/* DO SOMETHING */
			}
			#endif

			break;
		}

		while (hcl->sem_list_count > 0)
		{
			/* handle async signals */
			--hcl->sem_list_count;
			signal_semaphore (hcl, hcl->sem_list[hcl->sem_list_count]);
		}
		/*
		if (semaphore heap has pending request)
		{
			signal them...
		}*/

		/* TODO: implement different process switching scheme - time-slice or clock based??? */
#if defined(HCL_EXTERNAL_PROCESS_SWITCH)
		if (!hcl->proc_switched && hcl->switch_proc) { switch_to_next_runnable_process (hcl); }
		hcl->switch_proc = 0;
#else
		if (!hcl->proc_switched) { switch_to_next_runnable_process (hcl); }
#endif

		hcl->proc_switched = 0;

		if (hcl->ip >= hcl->code.bc.len) 
		{
			HCL_DEBUG0 (hcl, "IP reached the end of bytecode. Stopping execution\n");
			break;
		}

#if defined(HCL_DEBUG_VM_EXEC)
		fetched_instruction_pointer = hcl->ip;
#endif
		FETCH_BYTE_CODE_TO (hcl, bcode);
		/*while (bcode == HCL_CODE_NOOP) FETCH_BYTE_CODE_TO (hcl, bcode);*/

#if defined(HCL_PROFILE_VM)
		inst_counter++;
#endif

		switch (bcode)
		{
			/* ------------------------------------------------- */

#if 0
			case BCODE_PUSH_INSTVAR_X:
				FETCH_PARAM_CODE_TO (hcl, b1);
				goto push_instvar;
			case BCODE_PUSH_INSTVAR_0:
			case BCODE_PUSH_INSTVAR_1:
			case BCODE_PUSH_INSTVAR_2:
			case BCODE_PUSH_INSTVAR_3:
			case BCODE_PUSH_INSTVAR_4:
			case BCODE_PUSH_INSTVAR_5:
			case BCODE_PUSH_INSTVAR_6:
			case BCODE_PUSH_INSTVAR_7:
				b1 = bcode & 0x7; /* low 3 bits */
			push_instvar:
				LOG_INST_1 (hcl, "push_instvar %zu", b1);
				HCL_ASSERT (HCL_OBJ_GET_FLAGS_TYPE(hcl->active_context->origin->receiver_or_source) == HCL_OBJ_TYPE_OOP);
				HCL_STACK_PUSH (hcl, ((hcl_oop_oop_t)hcl->active_context->origin->receiver_or_source)->slot[b1]);
				break;

			/* ------------------------------------------------- */

			case BCODE_STORE_INTO_INSTVAR_X:
				FETCH_PARAM_CODE_TO (hcl, b1);
				goto store_instvar;
			case BCODE_STORE_INTO_INSTVAR_0:
			case BCODE_STORE_INTO_INSTVAR_1:
			case BCODE_STORE_INTO_INSTVAR_2:
			case BCODE_STORE_INTO_INSTVAR_3:
			case BCODE_STORE_INTO_INSTVAR_4:
			case BCODE_STORE_INTO_INSTVAR_5:
			case BCODE_STORE_INTO_INSTVAR_6:
			case BCODE_STORE_INTO_INSTVAR_7:
				b1 = bcode & 0x7; /* low 3 bits */
			store_instvar:
				LOG_INST_1 (hcl, "store_into_instvar %zu", b1);
				HCL_ASSERT (HCL_OBJ_GET_FLAGS_TYPE(hcl->active_context->receiver_or_source) == HCL_OBJ_TYPE_OOP);
				((hcl_oop_oop_t)hcl->active_context->origin->receiver_or_source)->slot[b1] = HCL_STACK_GETTOP(hcl);
				break;

			/* ------------------------------------------------- */
			case BCODE_POP_INTO_INSTVAR_X:
				FETCH_PARAM_CODE_TO (hcl, b1);
				goto pop_into_instvar;
			case BCODE_POP_INTO_INSTVAR_0:
			case BCODE_POP_INTO_INSTVAR_1:
			case BCODE_POP_INTO_INSTVAR_2:
			case BCODE_POP_INTO_INSTVAR_3:
			case BCODE_POP_INTO_INSTVAR_4:
			case BCODE_POP_INTO_INSTVAR_5:
			case BCODE_POP_INTO_INSTVAR_6:
			case BCODE_POP_INTO_INSTVAR_7:
				b1 = bcode & 0x7; /* low 3 bits */
			pop_into_instvar:
				LOG_INST_1 (hcl, "pop_into_instvar %zu", b1);
				HCL_ASSERT (HCL_OBJ_GET_FLAGS_TYPE(hcl->active_context->receiver_or_source) == HCL_OBJ_TYPE_OOP);
				((hcl_oop_oop_t)hcl->active_context->origin->receiver_or_source)->slot[b1] = HCL_STACK_GETTOP(hcl);
				HCL_STACK_POP (hcl);
				break;
#endif

			/* ------------------------------------------------- */
			case HCL_CODE_PUSH_TEMPVAR_X:
			case HCL_CODE_STORE_INTO_TEMPVAR_X:
			case BCODE_POP_INTO_TEMPVAR_X:
				FETCH_PARAM_CODE_TO (hcl, b1);
				goto handle_tempvar;

			case HCL_CODE_PUSH_TEMPVAR_0:
			case HCL_CODE_PUSH_TEMPVAR_1:
			case HCL_CODE_PUSH_TEMPVAR_2:
			case HCL_CODE_PUSH_TEMPVAR_3:
			case HCL_CODE_PUSH_TEMPVAR_4:
			case HCL_CODE_PUSH_TEMPVAR_5:
			case HCL_CODE_PUSH_TEMPVAR_6:
			case HCL_CODE_PUSH_TEMPVAR_7:
			case HCL_CODE_STORE_INTO_TEMPVAR_0:
			case HCL_CODE_STORE_INTO_TEMPVAR_1:
			case HCL_CODE_STORE_INTO_TEMPVAR_2:
			case HCL_CODE_STORE_INTO_TEMPVAR_3:
			case HCL_CODE_STORE_INTO_TEMPVAR_4:
			case HCL_CODE_STORE_INTO_TEMPVAR_5:
			case HCL_CODE_STORE_INTO_TEMPVAR_6:
			case HCL_CODE_STORE_INTO_TEMPVAR_7:
			case BCODE_POP_INTO_TEMPVAR_0:
			case BCODE_POP_INTO_TEMPVAR_1:
			case BCODE_POP_INTO_TEMPVAR_2:
			case BCODE_POP_INTO_TEMPVAR_3:
			case BCODE_POP_INTO_TEMPVAR_4:
			case BCODE_POP_INTO_TEMPVAR_5:
			case BCODE_POP_INTO_TEMPVAR_6:
			case BCODE_POP_INTO_TEMPVAR_7:
			{
				hcl_oop_context_t ctx;
				hcl_ooi_t bx;

				b1 = bcode & 0x7; /* low 3 bits */
			handle_tempvar:

			#if defined(HCL_USE_CTXTEMPVAR)
				/* when CTXTEMPVAR inststructions are used, the above 
				 * instructions are used only for temporary access 
				 * outside a block. i can assume that the temporary
				 * variable index is pointing to one of temporaries
				 * in the relevant method context */
				ctx = hcl->active_context->origin;
				bx = b1;
				HCL_ASSERT (HCL_IS_CONTEXT(hcl, ctx));
			#else
				/* otherwise, the index may point to a temporaries
				 * declared inside a block */

				if (hcl->active_context->home != hcl->_nil)
				{
					/* this code assumes that the method context and
					 * the block context place some key fields in the
					 * same offset. such fields include 'home', 'ntmprs' */
					hcl_oop_t home;
					hcl_ooi_t home_ntmprs;

					ctx = hcl->active_context;
					home = ctx->home;

					do
					{
						/* ntmprs contains the number of defined temporaries 
						 * including those defined in the home context */
						home_ntmprs = HCL_OOP_TO_SMOOI(((hcl_oop_context_t)home)->ntmprs);
						if (b1 >= home_ntmprs) break;

						ctx = (hcl_oop_context_t)home;
						home = ((hcl_oop_context_t)home)->home;
						if (home == hcl->_nil)
						{
							home_ntmprs = 0;
							break;
						}
					}
					while (1);

					/* bx is the actual index within the actual context 
					 * containing the temporary */
					bx = b1 - home_ntmprs;
				}
				else
				{
					ctx = hcl->active_context;
					bx = b1;
				}
			#endif

				if ((bcode >> 4) & 1)
				{
					/* push - bit 4 on */
					LOG_INST_1 (hcl, "push_tempvar %zu", b1);
					HCL_STACK_PUSH (hcl, ctx->slot[bx]);
				}
				else
				{
					/* store or pop - bit 5 off */
					ctx->slot[bx] = HCL_STACK_GETTOP(hcl);

					if ((bcode >> 3) & 1)
					{
						/* pop - bit 3 on */
						LOG_INST_1 (hcl, "pop_into_tempvar %zu", b1);
						HCL_STACK_POP (hcl);
					}
					else
					{
						LOG_INST_1 (hcl, "store_into_tempvar %zu", b1);
					}
				}

				break;
			}

			/* ------------------------------------------------- */
			case HCL_CODE_PUSH_LITERAL_X2:
				FETCH_PARAM_CODE_TO (hcl, b1);
				FETCH_PARAM_CODE_TO (hcl, b2);
		#if (HCL_BCODE_LONG_PARAM_SIZE == 2)
				b1 = (b1 << 16) | b2;
		#else
				b1 = (b1 << 8) | b2;
		#endif
				goto push_literal;

			case HCL_CODE_PUSH_LITERAL_X:
				FETCH_PARAM_CODE_TO (hcl, b1);
				goto push_literal;

			case HCL_CODE_PUSH_LITERAL_0:
			case HCL_CODE_PUSH_LITERAL_1:
			case HCL_CODE_PUSH_LITERAL_2:
			case HCL_CODE_PUSH_LITERAL_3:
			case HCL_CODE_PUSH_LITERAL_4:
			case HCL_CODE_PUSH_LITERAL_5:
			case HCL_CODE_PUSH_LITERAL_6:
			case HCL_CODE_PUSH_LITERAL_7:
				b1 = bcode & 0x7; /* low 3 bits */
			push_literal:
				LOG_INST_1 (hcl, "push_literal @%zu", b1);
				HCL_STACK_PUSH (hcl, hcl->code.lit.arr->slot[b1]);
				break;

			/* ------------------------------------------------- */
			case HCL_CODE_PUSH_OBJECT_X:
			case HCL_CODE_STORE_INTO_OBJECT_X:
			case BCODE_POP_INTO_OBJECT_X:
				FETCH_PARAM_CODE_TO (hcl, b1);
				goto handle_object;

			case HCL_CODE_PUSH_OBJECT_0:
			case HCL_CODE_PUSH_OBJECT_1:
			case HCL_CODE_PUSH_OBJECT_2:
			case HCL_CODE_PUSH_OBJECT_3:
			case HCL_CODE_STORE_INTO_OBJECT_0:
			case HCL_CODE_STORE_INTO_OBJECT_1:
			case HCL_CODE_STORE_INTO_OBJECT_2:
			case HCL_CODE_STORE_INTO_OBJECT_3:
			case BCODE_POP_INTO_OBJECT_0:
			case BCODE_POP_INTO_OBJECT_1:
			case BCODE_POP_INTO_OBJECT_2:
			case BCODE_POP_INTO_OBJECT_3:
			{
				hcl_oop_cons_t ass;

				b1 = bcode & 0x3; /* low 2 bits */
			handle_object:
				ass = (hcl_oop_cons_t)hcl->code.lit.arr->slot[b1];
				HCL_ASSERT (HCL_IS_CONS(hcl, ass));

				if ((bcode >> 3) & 1)
				{
					/* store or pop */
					ass->cdr = HCL_STACK_GETTOP(hcl);

					if ((bcode >> 2) & 1)
					{
						/* pop */
						LOG_INST_1 (hcl, "pop_into_object @%zu", b1);
						HCL_STACK_POP (hcl);
					}
					else
					{
						LOG_INST_1 (hcl, "store_into_object @%zu", b1);
					}
				}
				else
				{
					/* push */
					LOG_INST_1 (hcl, "push_object @%zu", b1);
					HCL_STACK_PUSH (hcl, ass->cdr);
				}
				break;
			}

			/* -------------------------------------------------------- */

			case HCL_CODE_JUMP_FORWARD_X:
				FETCH_PARAM_CODE_TO (hcl, b1);
				LOG_INST_1 (hcl, "jump_forward %zu", b1);
				hcl->ip += b1;
				break;

			case HCL_CODE_JUMP_FORWARD_0:
			case HCL_CODE_JUMP_FORWARD_1:
			case HCL_CODE_JUMP_FORWARD_2:
			case HCL_CODE_JUMP_FORWARD_3:
				LOG_INST_1 (hcl, "jump_forward %zu", (hcl_oow_t)(bcode & 0x3));
				hcl->ip += (bcode & 0x3); /* low 2 bits */
				break;

			case HCL_CODE_JUMP_BACKWARD_X:
				FETCH_PARAM_CODE_TO (hcl, b1);
				LOG_INST_1 (hcl, "jump_backward %zu", b1);
				hcl->ip -= b1;
				break;

			case HCL_CODE_JUMP_BACKWARD_0:
			case HCL_CODE_JUMP_BACKWARD_1:
			case HCL_CODE_JUMP_BACKWARD_2:
			case HCL_CODE_JUMP_BACKWARD_3:
				LOG_INST_1 (hcl, "jump_backward %zu", (hcl_oow_t)(bcode & 0x3));
				hcl->ip -= (bcode & 0x3); /* low 2 bits */
				break;

			case HCL_CODE_JUMP_FORWARD_IF_TRUE:
				FETCH_PARAM_CODE_TO (hcl, b1);
				LOG_INST_1 (hcl, "jump_forward_if_true %zu", b1);
				if (HCL_STACK_GETTOP(hcl) == hcl->_true) hcl->ip += b1;
				break;

			case HCL_CODE_JUMP2_FORWARD_IF_TRUE:
				FETCH_PARAM_CODE_TO (hcl, b1);
				LOG_INST_1 (hcl, "jump2_forward_if_true %zu", b1);
				if (HCL_STACK_GETTOP(hcl) == hcl->_true) hcl->ip += MAX_CODE_JUMP + b1;
				break;

			case HCL_CODE_JUMP_FORWARD_IF_FALSE:
				FETCH_PARAM_CODE_TO (hcl, b1);
				LOG_INST_1 (hcl, "jump_forward_if_false %zu", b1);
				if (HCL_STACK_GETTOP(hcl) == hcl->_false) hcl->ip += b1;
				break;

			case HCL_CODE_JUMP2_FORWARD_IF_FALSE:
				FETCH_PARAM_CODE_TO (hcl, b1);
				LOG_INST_1 (hcl, "jump2_forward_if_false %zu", b1);
				if (HCL_STACK_GETTOP(hcl) == hcl->_false) hcl->ip += MAX_CODE_JUMP + b1;
				break;

			case HCL_CODE_JUMP2_FORWARD:
				FETCH_PARAM_CODE_TO (hcl, b1);
				LOG_INST_1 (hcl, "jump2_forward %zu", b1);
				hcl->ip += MAX_CODE_JUMP + b1;
				break;

			case HCL_CODE_JUMP2_BACKWARD:
				FETCH_PARAM_CODE_TO (hcl, b1);
				LOG_INST_1 (hcl, "jump2_backward %zu", b1);
				hcl->ip -= MAX_CODE_JUMP + b1;
				break;

			/* -------------------------------------------------------- */

			case HCL_CODE_CALL_X:
				FETCH_PARAM_CODE_TO (hcl, b1);
				goto handle_call;
			case HCL_CODE_CALL_0:
			case HCL_CODE_CALL_1:
			case HCL_CODE_CALL_2:
			case HCL_CODE_CALL_3:
			{
				hcl_oop_t rcv;

				b1 = bcode & 0x3; /* low 2 bits */
			handle_call:
				LOG_INST_1 (hcl, "call %zu", b1);

				rcv = HCL_STACK_GETRCV (hcl, b1);
				if (HCL_OOP_IS_POINTER(rcv))
				{
					switch (HCL_OBJ_GET_FLAGS_BRAND(rcv))
					{
						case HCL_BRAND_CONTEXT:
							if (activate_context (hcl, b1) <= -1) return -1;
							break;
						case HCL_BRAND_PRIM:
							if (call_primitive (hcl, b1) <= -1) return -1;
							break;
						default:
							goto cannot_call;
					}
				}
				else
				{
				cannot_call:
					/* run time error */
					HCL_LOG1 (hcl, HCL_LOG_IC | HCL_LOG_ERROR, "Error - cannot call %O\n", rcv);
					hcl->errnum = HCL_ECALL;
					return -1;
				}
				break;
			}
			
			/* -------------------------------------------------------- */

			case HCL_CODE_PUSH_CTXTEMPVAR_X:
			case HCL_CODE_STORE_INTO_CTXTEMPVAR_X:
			case BCODE_POP_INTO_CTXTEMPVAR_X:
				FETCH_PARAM_CODE_TO (hcl, b1);
				FETCH_PARAM_CODE_TO (hcl, b2);
				goto handle_ctxtempvar;
			case HCL_CODE_PUSH_CTXTEMPVAR_0:
			case HCL_CODE_PUSH_CTXTEMPVAR_1:
			case HCL_CODE_PUSH_CTXTEMPVAR_2:
			case HCL_CODE_PUSH_CTXTEMPVAR_3:
			case HCL_CODE_STORE_INTO_CTXTEMPVAR_0:
			case HCL_CODE_STORE_INTO_CTXTEMPVAR_1:
			case HCL_CODE_STORE_INTO_CTXTEMPVAR_2:
			case HCL_CODE_STORE_INTO_CTXTEMPVAR_3:
			case BCODE_POP_INTO_CTXTEMPVAR_0:
			case BCODE_POP_INTO_CTXTEMPVAR_1:
			case BCODE_POP_INTO_CTXTEMPVAR_2:
			case BCODE_POP_INTO_CTXTEMPVAR_3:
			{
				hcl_ooi_t i;
				hcl_oop_context_t ctx;

				b1 = bcode & 0x3; /* low 2 bits */
				FETCH_BYTE_CODE_TO (hcl, b2);

			handle_ctxtempvar:

				ctx = hcl->active_context;
				HCL_ASSERT ((hcl_oop_t)ctx != hcl->_nil);
				for (i = 0; i < b1; i++)
				{
					ctx = (hcl_oop_context_t)ctx->home;
				}

				if ((bcode >> 3) & 1)
				{
					/* store or pop */
					ctx->slot[b2] = HCL_STACK_GETTOP(hcl);

					if ((bcode >> 2) & 1)
					{
						/* pop */
						HCL_STACK_POP (hcl);
						LOG_INST_2 (hcl, "pop_into_ctxtempvar %zu %zu", b1, b2);
					}
					else
					{
						LOG_INST_2 (hcl, "store_into_ctxtempvar %zu %zu", b1, b2);
					}
				}
				else
				{
					/* push */
					HCL_STACK_PUSH (hcl, ctx->slot[b2]);
					LOG_INST_2 (hcl, "push_ctxtempvar %zu %zu", b1, b2);
				}

				break;
			}
			/* -------------------------------------------------------- */

			case BCODE_PUSH_OBJVAR_X:
			case BCODE_STORE_INTO_OBJVAR_X:
			case BCODE_POP_INTO_OBJVAR_X:
				FETCH_PARAM_CODE_TO (hcl, b1);
				FETCH_PARAM_CODE_TO (hcl, b2);
				goto handle_objvar;

			case BCODE_PUSH_OBJVAR_0:
			case BCODE_PUSH_OBJVAR_1:
			case BCODE_PUSH_OBJVAR_2:
			case BCODE_PUSH_OBJVAR_3:
			case BCODE_STORE_INTO_OBJVAR_0:
			case BCODE_STORE_INTO_OBJVAR_1:
			case BCODE_STORE_INTO_OBJVAR_2:
			case BCODE_STORE_INTO_OBJVAR_3:
			case BCODE_POP_INTO_OBJVAR_0:
			case BCODE_POP_INTO_OBJVAR_1:
			case BCODE_POP_INTO_OBJVAR_2:
			case BCODE_POP_INTO_OBJVAR_3:
			{
				hcl_oop_oop_t t;

				/* b1 -> variable index to the object indicated by b2.
				 * b2 -> object index stored in the literal frame. */
				b1 = bcode & 0x3; /* low 2 bits */
				FETCH_BYTE_CODE_TO (hcl, b2);

			handle_objvar:
				t = (hcl_oop_oop_t)hcl->code.lit.arr->slot[b2];
				HCL_ASSERT (HCL_OBJ_GET_FLAGS_TYPE(t) == HCL_OBJ_TYPE_OOP);
				HCL_ASSERT (b1 < HCL_OBJ_GET_SIZE(t));

				if ((bcode >> 3) & 1)
				{
					/* store or pop */

					t->slot[b1] = HCL_STACK_GETTOP(hcl);

					if ((bcode >> 2) & 1)
					{
						/* pop */
						HCL_STACK_POP (hcl);
						LOG_INST_2 (hcl, "pop_into_objvar %zu %zu", b1, b2);
					}
					else
					{
						LOG_INST_2 (hcl, "store_into_objvar %zu %zu", b1, b2);
					}
				}
				else
				{
					/* push */
					LOG_INST_2 (hcl, "push_objvar %zu %zu", b1, b2);
					HCL_STACK_PUSH (hcl, t->slot[b1]);
				}
				break;
			}

			/* -------------------------------------------------------- */
#if 0
			case BCODE_SEND_MESSAGE_X:
			case BCODE_SEND_MESSAGE_TO_SUPER_X:
				/* b1 -> number of arguments 
				 * b2 -> selector index stored in the literal frame */
				FETCH_PARAM_CODE_TO (hcl, b1);
				FETCH_PARAM_CODE_TO (hcl, b2);
				goto handle_send_message;

			case BCODE_SEND_MESSAGE_0:
			case BCODE_SEND_MESSAGE_1:
			case BCODE_SEND_MESSAGE_2:
			case BCODE_SEND_MESSAGE_3:
			case BCODE_SEND_MESSAGE_TO_SUPER_0:
			case BCODE_SEND_MESSAGE_TO_SUPER_1:
			case BCODE_SEND_MESSAGE_TO_SUPER_2:
			case BCODE_SEND_MESSAGE_TO_SUPER_3:
			{
				hcl_oop_char_t selector;

				b1 = bcode & 0x3; /* low 2 bits */
				FETCH_BYTE_CODE_TO (hcl, b2);

			handle_send_message:
				/* get the selector from the literal frame */
				selector = (hcl_oop_char_t)hcl->active_method->slot[b2];

				LOG_INST_3 (hcl, "send_message%hs %zu @%zu", (((bcode >> 2) & 1)? "_to_super": ""), b1, b2);

				if (send_message (hcl, selector, ((bcode >> 2) & 1), b1) <= -1) goto oops;
				break; /* CMD_SEND_MESSAGE */
			}
#endif
			/* -------------------------------------------------------- */

			case BCODE_PUSH_RECEIVER:
				LOG_INST_0 (hcl, "push_receiver");
				HCL_STACK_PUSH (hcl, hcl->active_context->origin->receiver_or_source);
				break;

			case HCL_CODE_PUSH_NIL:
				LOG_INST_0 (hcl, "push_nil");
				HCL_STACK_PUSH (hcl, hcl->_nil);
				break;

			case HCL_CODE_PUSH_TRUE:
				LOG_INST_0 (hcl, "push_true");
				HCL_STACK_PUSH (hcl, hcl->_true);
				break;

			case HCL_CODE_PUSH_FALSE:
				LOG_INST_0 (hcl, "push_false");
				HCL_STACK_PUSH (hcl, hcl->_false);
				break;

			case BCODE_PUSH_CONTEXT:
				LOG_INST_0 (hcl, "push_context");
				HCL_STACK_PUSH (hcl, (hcl_oop_t)hcl->active_context);
				break;

			case BCODE_PUSH_PROCESS:
				LOG_INST_0 (hcl, "push_process");
				HCL_STACK_PUSH (hcl, (hcl_oop_t)hcl->processor->active);
				break;

			case HCL_CODE_PUSH_NEGONE:
				LOG_INST_0 (hcl, "push_negone");
				HCL_STACK_PUSH (hcl, HCL_SMOOI_TO_OOP(-1));
				break;

			case HCL_CODE_PUSH_ZERO:
				LOG_INST_0 (hcl, "push_zero");
				HCL_STACK_PUSH (hcl, HCL_SMOOI_TO_OOP(0));
				break;

			case HCL_CODE_PUSH_ONE:
				LOG_INST_0 (hcl, "push_one");
				HCL_STACK_PUSH (hcl, HCL_SMOOI_TO_OOP(1));
				break;

			case HCL_CODE_PUSH_TWO:
				LOG_INST_0 (hcl, "push_two");
				HCL_STACK_PUSH (hcl, HCL_SMOOI_TO_OOP(2));
				break;

			case HCL_CODE_PUSH_INTLIT:
				FETCH_PARAM_CODE_TO (hcl, b1);
				LOG_INST_1 (hcl, "push_intlit %zu", b1);
				HCL_STACK_PUSH (hcl, HCL_SMOOI_TO_OOP(b1));
				break;

			case HCL_CODE_PUSH_NEGINTLIT:
			{
				hcl_ooi_t num;
				FETCH_PARAM_CODE_TO (hcl, b1);
				num = b1;
				LOG_INST_1 (hcl, "push_negintlit %zu", b1);
				HCL_STACK_PUSH (hcl, HCL_SMOOI_TO_OOP(-num));
				break;
			}

			case HCL_CODE_PUSH_CHARLIT:
				FETCH_PARAM_CODE_TO (hcl, b1);
				LOG_INST_1 (hcl, "push_charlit %zu", b1);
				HCL_STACK_PUSH (hcl, HCL_CHAR_TO_OOP(b1));
				break;
			/* -------------------------------------------------------- */

			case BCODE_DUP_STACKTOP:
			{
				hcl_oop_t t;
				LOG_INST_0 (hcl, "dup_stacktop");
				HCL_ASSERT (!HCL_STACK_ISEMPTY(hcl));
				t = HCL_STACK_GETTOP(hcl);
				HCL_STACK_PUSH (hcl, t);
				break;
			}

			case HCL_CODE_POP_STACKTOP:
				LOG_INST_0 (hcl, "pop_stacktop");
				HCL_ASSERT (!HCL_STACK_ISEMPTY(hcl));
				HCL_STACK_POP (hcl);
				break;

			case BCODE_RETURN_STACKTOP:
				LOG_INST_0 (hcl, "return_stacktop");
				return_value = HCL_STACK_GETTOP(hcl);
				HCL_STACK_POP (hcl);
				goto handle_return;

			case BCODE_RETURN_RECEIVER:
				LOG_INST_0 (hcl, "return_receiver");
				return_value = hcl->active_context->origin->receiver_or_source;

			handle_return:
				if (hcl->active_context->origin == hcl->processor->active->initial_context->origin)
				{
					/* method return from a processified block
					 * 
 					 * #method(#class) main
					 * {
					 *    [^100] newProcess resume.
					 *    '1111' dump.
					 *    '1111' dump.
					 *    '1111' dump.
					 *    ^300.
					 * }
					 * 
					 * ^100 doesn't terminate a main process as the block
					 * has been processified. on the other hand, ^100
					 * in the following program causes main to exit.
					 * 
					 * #method(#class) main
					 * {
					 *    [^100] value.
					 *    '1111' dump.
					 *    '1111' dump.
					 *    '1111' dump.
					 *    ^300.
					 * }
					 */

//					HCL_ASSERT (HCL_CLASSOF(hcl, hcl->active_context) == hcl->_block_context);
//					HCL_ASSERT (HCL_CLASSOF(hcl, hcl->processor->active->initial_context) == hcl->_block_context);

					/* decrement the instruction pointer back to the return instruction.
					 * even if the context is reentered, it will just return.
					 *hcl->ip--;*/

					terminate_process (hcl, hcl->processor->active);
				}
				else 
				{
					unwind_protect = 0;

					/* set the instruction pointer to an invalid value.
					 * this is stored into the current method context
					 * before context switching and marks a dead context */
					if (hcl->active_context->origin == hcl->active_context)
					{
						/* returning from a method */
//						HCL_ASSERT (HCL_CLASSOF(hcl, hcl->active_context) == hcl->_method_context);
						hcl->ip = -1;
					}
					else
					{
						hcl_oop_context_t ctx;

						/* method return from within a block(including a non-local return) */
//						HCL_ASSERT (HCL_CLASSOF(hcl, hcl->active_context) == hcl->_block_context);

						ctx = hcl->active_context;
						while ((hcl_oop_t)ctx != hcl->_nil)
						{
						#if 0
//							/* TODO: XXXXXXXXXXXXXX for STACK UNWINDING... */
							if (HCL_CLASSOF(hcl, ctx) == hcl->_method_context)
							{
								hcl_ooi_t preamble;
								preamble = HCL_OOP_TO_SMOOI(((hcl_oop_method_t)ctx->method_or_nargs)->preamble);
								if (HCL_METHOD_GET_PREAMBLE_CODE(preamble) == HCL_METHOD_PREAMBLE_ENSURE)
								{
									if (!unwind_protect)
									{
										unwind_protect = 1;
										unwind_start = ctx;
									}
									unwind_stop = ctx;
								}
							}
						#endif
							if (ctx == hcl->active_context->origin) goto non_local_return_ok;
							ctx = ctx->sender;
						}

						/* cannot return from a method that has returned already */
//						HCL_ASSERT (HCL_CLASSOF(hcl, hcl->active_context->origin) == hcl->_method_context);
						HCL_ASSERT (hcl->active_context->origin->ip == HCL_SMOOI_TO_OOP(-1));

						HCL_LOG0 (hcl, HCL_LOG_IC | HCL_LOG_ERROR, "Error - cannot return from dead context\n");
						hcl->errnum = HCL_EINTERN; /* TODO: can i make this error catchable at the hcl level? */
						return -1;

					non_local_return_ok:
/*HCL_DEBUG2 (hcl, "NON_LOCAL RETURN OK TO... %p %p\n", hcl->active_context->origin, hcl->active_context->origin->sender);*/
						hcl->active_context->origin->ip = HCL_SMOOI_TO_OOP(-1);
					}

//					HCL_ASSERT (HCL_CLASSOF(hcl, hcl->active_context->origin) == hcl->_method_context);
					/* restore the stack pointer */
					hcl->sp = HCL_OOP_TO_SMOOI(hcl->active_context->origin->sp);
					SWITCH_ACTIVE_CONTEXT (hcl, hcl->active_context->origin->sender);

					if (unwind_protect)
					{
						static hcl_ooch_t fbm[] = { 
							'u', 'n', 'w', 'i', 'n', 'd', 'T', 'o', ':', 
							'r', 'e', 't', 'u', 'r', 'n', ':'
						};

						HCL_STACK_PUSH (hcl, (hcl_oop_t)unwind_start);
						HCL_STACK_PUSH (hcl, (hcl_oop_t)unwind_stop);
						HCL_STACK_PUSH (hcl, (hcl_oop_t)return_value);

						if (send_private_message (hcl, fbm, 16, 0, 2) <= -1) return -1;
					}
					else
					{
						/* push the return value to the stack of the new active context */
						HCL_STACK_PUSH (hcl, return_value);

						if (hcl->active_context == hcl->initial_context)
						{
							/* the new active context is the fake initial context.
							 * this context can't get executed further. */
							HCL_ASSERT ((hcl_oop_t)hcl->active_context->sender == hcl->_nil);
//							HCL_ASSERT (HCL_CLASSOF(hcl, hcl->active_context) == hcl->_method_context);
							HCL_ASSERT (hcl->active_context->receiver_or_source == hcl->_nil);
							HCL_ASSERT (hcl->active_context == hcl->processor->active->initial_context);
							HCL_ASSERT (hcl->active_context->origin == hcl->processor->active->initial_context->origin);
							HCL_ASSERT (hcl->active_context->origin == hcl->active_context);

							/* NOTE: this condition is true for the processified block context also.
							 *   hcl->active_context->origin == hcl->processor->active->initial_context->origin
							 *   however, the check here is done after context switching and the
							 *   processified block check has been done against the context before switching */

							/* the stack contains the final return value so the stack pointer must be 0. */
							HCL_ASSERT (hcl->sp == 0); 

							if (hcl->option.trait & HCL_AWAIT_PROCS)
								terminate_process (hcl, hcl->processor->active);
							else
								goto done;

							/* TODO: store the return value to the VM register.
							 * the caller to hcl_execute() can fetch it to return it to the system */
						}
					}
				}

				break;

			case HCL_CODE_RETURN_FROM_BLOCK:
				LOG_INST_0 (hcl, "return_from_block");

//				HCL_ASSERT(HCL_CLASSOF(hcl, hcl->active_context) == hcl->_block_context);

				if (hcl->active_context == hcl->processor->active->initial_context)
				{
					/* the active context to return from is an initial context of
					 * the active process. this process must have been created 
					 * over a block using the newProcess method. let's terminate
					 * the process. */

					HCL_ASSERT ((hcl_oop_t)hcl->active_context->sender == hcl->_nil);
					terminate_process (hcl, hcl->processor->active);
				}
				else
				{
					/* it is a normal block return as the active block context 
					 * is not the initial context of a process */

					/* the process stack is shared. the return value 
					 * doesn't need to get moved. */
					//XXX SWITCH_ACTIVE_CONTEXT (hcl, (hcl_oop_context_t)hcl->active_context->sender);
					if (hcl->active_context->sender == hcl->processor->active->initial_context)
					{
						terminate_process (hcl, hcl->processor->active);
					}
					else
					{
						SWITCH_ACTIVE_CONTEXT (hcl, (hcl_oop_context_t)hcl->active_context->sender);
					}
				}

				break;

			case HCL_CODE_MAKE_BLOCK:
			{
				hcl_oop_context_t blkctx;

				/* b1 - number of block arguments
				 * b2 - number of block temporaries */
				FETCH_PARAM_CODE_TO (hcl, b1);
				FETCH_PARAM_CODE_TO (hcl, b2);

				LOG_INST_2 (hcl, "make_block %zu %zu", b1, b2);

				HCL_ASSERT (b1 >= 0);
				HCL_ASSERT (b2 >= b1);

				/* the block context object created here is used as a base
				 * object for block context activation. activate_context()
				 * clones a block context and activates the cloned context.
				 * this base block context is created with no temporaries
				 * for this reason */
				blkctx = (hcl_oop_context_t)make_context (hcl, 0);
				if (!blkctx) return -1;

				/* the long forward jump instruction has the format of 
				 *   11000100 KKKKKKKK or 11000100 KKKKKKKK KKKKKKKK 
				 * depending on HCL_BCODE_LONG_PARAM_SIZE. change 'ip' to point to
				 * the instruction after the jump. */
				blkctx->ip = HCL_SMOOI_TO_OOP(hcl->ip + HCL_BCODE_LONG_PARAM_SIZE + 1);
				/* stack pointer below the bottom. this base block context
				 * has an empty stack anyway. */
				blkctx->sp = HCL_SMOOI_TO_OOP(-1);
				/* the number of arguments for a block context is local to the block */
				blkctx->method_or_nargs = HCL_SMOOI_TO_OOP(b1);
				/* the number of temporaries here is an accumulated count including
				 * the number of temporaries of a home context */
				blkctx->ntmprs = HCL_SMOOI_TO_OOP(b2);

				/* set the home context where it's defined */
				blkctx->home = (hcl_oop_t)hcl->active_context; 
				/* no source for a base block context. */
				blkctx->receiver_or_source = hcl->_nil; 

				blkctx->origin = hcl->active_context->origin;

				/* push the new block context to the stack of the active context */
				HCL_STACK_PUSH (hcl, (hcl_oop_t)blkctx);
				break;
			}

			case BCODE_SEND_BLOCK_COPY:
			{
				hcl_ooi_t nargs, ntmprs;
				hcl_oop_context_t rctx;
				hcl_oop_context_t blkctx;

				LOG_INST_0 (hcl, "send_block_copy");

				/* it emulates thisContext blockCopy: nargs ofTmprCount: ntmprs */
				HCL_ASSERT (hcl->sp >= 2);

				HCL_ASSERT (HCL_CLASSOF(hcl, HCL_STACK_GETTOP(hcl)) == hcl->_small_integer);
				ntmprs = HCL_OOP_TO_SMOOI(HCL_STACK_GETTOP(hcl));
				HCL_STACK_POP (hcl);

				HCL_ASSERT (HCL_CLASSOF(hcl, HCL_STACK_GETTOP(hcl)) == hcl->_small_integer);
				nargs = HCL_OOP_TO_SMOOI(HCL_STACK_GETTOP(hcl));
				HCL_STACK_POP (hcl);

				HCL_ASSERT (nargs >= 0);
				HCL_ASSERT (ntmprs >= nargs);

				/* the block context object created here is used
				 * as a base object for block context activation.
				 * prim_block_value() clones a block 
				 * context and activates the cloned context.
				 * this base block context is created with no 
				 * stack for this reason. */
				//blkctx = (hcl_oop_context_t)hcl_instantiate (hcl, hcl->_block_context, HCL_NULL, 0); 
				blkctx = (hcl_oop_context_t)make_context (hcl, 0);
				if (!blkctx) return -1;

				/* get the receiver to the block copy message after block context instantiation
				 * not to get affected by potential GC */
				rctx = (hcl_oop_context_t)HCL_STACK_GETTOP(hcl);
				HCL_ASSERT (rctx == hcl->active_context);

				/* [NOTE]
				 *  blkctx->sender is left to nil. it is set to the 
				 *  active context before it gets activated. see
				 *  prim_block_value().
				 *
				 *  blkctx->home is set here to the active context.
				 *  it's redundant to have them pushed to the stack
				 *  though it is to emulate the message sending of
				 *  blockCopy:withNtmprs:. BCODE_MAKE_BLOCK has been
				 *  added to replace BCODE_SEND_BLOCK_COPY and pusing
				 *  arguments to the stack.
				 *
				 *  blkctx->origin is set here by copying the origin
				 *  of the active context.
				 */

				/* the extended jump instruction has the format of 
				 *   0000XXXX KKKKKKKK or 0000XXXX KKKKKKKK KKKKKKKK 
				 * depending on HCL_BCODE_LONG_PARAM_SIZE. change 'ip' to point to
				 * the instruction after the jump. */
				blkctx->ip = HCL_SMOOI_TO_OOP(hcl->ip + HCL_BCODE_LONG_PARAM_SIZE + 1);
				blkctx->sp = HCL_SMOOI_TO_OOP(-1);
				/* the number of arguments for a block context is local to the block */
				blkctx->method_or_nargs = HCL_SMOOI_TO_OOP(nargs);
				/* the number of temporaries here is an accumulated count including
				 * the number of temporaries of a home context */
				blkctx->ntmprs = HCL_SMOOI_TO_OOP(ntmprs);

				blkctx->home = (hcl_oop_t)rctx;
				blkctx->receiver_or_source = hcl->_nil;


				/* [NOTE]
				 * the origin of a method context is set to itself
				 * when it's created. so it's safe to simply copy
				 * the origin field this way.
				 */
				blkctx->origin = rctx->origin;

				HCL_STACK_SETTOP (hcl, (hcl_oop_t)blkctx);
				break;
			}

			case HCL_CODE_NOOP:
				/* do nothing */
				LOG_INST_0 (hcl, "noop");
				break;


			default:
				HCL_LOG1 (hcl, HCL_LOG_IC | HCL_LOG_FATAL, "Fatal error - unknown byte code 0x%zx\n", bcode);
				hcl->errnum = HCL_EINTERN;
				goto oops;
		}
	}

done:
	vm_cleanup (hcl);
#if defined(HCL_PROFILE_VM)
	HCL_LOG1 (hcl, HCL_LOG_IC | HCL_LOG_INFO, "TOTAL_INST_COUTNER = %zu\n", inst_counter);
#endif
	return 0;

oops:
	/* TODO: anything to do here? */
	return -1;
}

int hcl_execute (hcl_t* hcl)
{
	int n;

	HCL_ASSERT (hcl->initial_context == HCL_NULL);
	HCL_ASSERT (hcl->active_context == HCL_NULL);

	if (start_initial_process_and_context (hcl) <= -1) return -1;
	hcl->initial_context = hcl->processor->active->initial_context;

	n = execute (hcl);

/* TODO: reset processor fields. set processor->tally to zero. processor->active to nil_process... */
	hcl->initial_context = HCL_NULL;
	hcl->active_context = HCL_NULL;
	return n;
}

