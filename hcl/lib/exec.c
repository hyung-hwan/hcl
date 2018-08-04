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

#define PROC_STATE_RUNNING 3
#define PROC_STATE_WAITING 2
#define PROC_STATE_RUNNABLE 1
#define PROC_STATE_SUSPENDED 0
#define PROC_STATE_TERMINATED -1


static HCL_INLINE const char* proc_state_to_string (int state)
{
	static const hcl_bch_t* str[] = 
	{
		"TERMINATED",
		"SUSPENDED",
		"RUNNABLE",
		"WAITING",
		"RUNNING"
	};

	return str[state + 1];
}

#define PROC_MAP_INC 64

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
#	define LOG_MASK_INST (HCL_LOG_IC | HCL_LOG_MNEMONIC | HCL_LOG_INFO)

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

static int vm_startup (hcl_t* hcl)
{
	hcl_cb_t* cb;
	
	HCL_DEBUG1 (hcl, "VM started up at IP %zd\n", hcl->ip);

	for (cb = hcl->cblist; cb; cb = cb->next)
	{
		if (cb->vm_startup && cb->vm_startup(hcl) <= -1) 
		{
			for (cb = cb->prev; cb; cb = cb->prev)
			{
				if (cb->vm_cleanup) cb->vm_cleanup (hcl);
			}
			return -1;
		}
	}
	hcl->vmprim.vm_gettime (hcl, &hcl->exec_start_time); /* raw time. no adjustment */

	return 0;
}

static void vm_cleanup (hcl_t* hcl)
{
	hcl_cb_t* cb;
	hcl->vmprim.vm_gettime (hcl, &hcl->exec_end_time); /* raw time. no adjustment */
	for (cb = hcl->cblist; cb; cb = cb->next)
	{
		if (cb->vm_cleanup) cb->vm_cleanup(hcl);
	}
	HCL_DEBUG1 (hcl, "VM cleaned up at IP %zd\n", hcl->ip);
}

static void vm_checkbc (hcl_t* hcl, hcl_oob_t bcode)
{
	hcl_cb_t* cb;
	for (cb = hcl->cblist; cb; cb = cb->next)
	{
		if (cb->vm_checkbc) cb->vm_checkbc(hcl, bcode);
	}
}
/* ------------------------------------------------------------------------- */

static HCL_INLINE hcl_oop_t make_context (hcl_t* hcl, hcl_ooi_t ntmprs)
{
	HCL_ASSERT (hcl, ntmprs >= 0);
	return hcl_allocoopobj (hcl, HCL_BRAND_CONTEXT, HCL_CONTEXT_NAMED_INSTVARS + (hcl_oow_t)ntmprs);
}


static HCL_INLINE int prepare_to_alloc_pid (hcl_t* hcl)
{
	hcl_oow_t new_capa;
	hcl_ooi_t i, j;
	hcl_oop_t* tmp;

	HCL_ASSERT (hcl, hcl->proc_map_free_first <= -1);
	HCL_ASSERT (hcl, hcl->proc_map_free_last <= -1);

	new_capa = hcl->proc_map_capa + PROC_MAP_INC;
	if (new_capa > HCL_SMOOI_MAX)
	{
		if (hcl->proc_map_capa >= HCL_SMOOI_MAX)
		{
		#if defined(HCL_DEBUG_VM_PROCESSOR)
			HCL_LOG0 (hcl, HCL_LOG_IC | HCL_LOG_FATAL, "Processor - too many processes\n");
		#endif
			hcl_seterrbfmt (hcl, HCL_EPFULL, "maximum number(%zd) of processes reached", HCL_SMOOI_MAX);
			return -1;
		}

		new_capa = HCL_SMOOI_MAX;
	}

	tmp = (hcl_oop_t*)hcl_reallocmem (hcl, hcl->proc_map, HCL_SIZEOF(hcl_oop_t) * new_capa);
	if (!tmp) return -1;

	hcl->proc_map_free_first = hcl->proc_map_capa;
	for (i = hcl->proc_map_capa, j = hcl->proc_map_capa + 1; j < new_capa; i++, j++)
	{
		tmp[i] = HCL_SMOOI_TO_OOP(j);
	}
	tmp[i] = HCL_SMOOI_TO_OOP(-1);
	hcl->proc_map_free_last = i;

	hcl->proc_map = tmp;
	hcl->proc_map_capa = new_capa;

	return 0;
}

static HCL_INLINE void alloc_pid (hcl_t* hcl, hcl_oop_process_t proc)
{
	hcl_ooi_t pid;

	pid = hcl->proc_map_free_first;
	proc->id = HCL_SMOOI_TO_OOP(pid);
	HCL_ASSERT (hcl, HCL_OOP_IS_SMOOI(hcl->proc_map[pid]));
	hcl->proc_map_free_first = HCL_OOP_TO_SMOOI(hcl->proc_map[pid]);
	if (hcl->proc_map_free_first <= -1) hcl->proc_map_free_last = -1;
	hcl->proc_map[pid] = (hcl_oop_t)proc;
}

static HCL_INLINE void free_pid (hcl_t* hcl, hcl_oop_process_t proc)
{
	hcl_ooi_t pid;

	pid = HCL_OOP_TO_SMOOI(proc->id);
	HCL_ASSERT (hcl, pid < hcl->proc_map_capa);

	hcl->proc_map[pid] = HCL_SMOOI_TO_OOP(-1);
	if (hcl->proc_map_free_last <= -1)
	{
		HCL_ASSERT (hcl, hcl->proc_map_free_first <= -1);
		hcl->proc_map_free_first = pid;
	}
	else
	{
		hcl->proc_map[hcl->proc_map_free_last] = HCL_SMOOI_TO_OOP(pid);
	}
	hcl->proc_map_free_last = pid;
}


static hcl_oop_process_t make_process (hcl_t* hcl, hcl_oop_context_t c)
{
	hcl_oop_process_t proc;
	hcl_oow_t stksize;

	if (hcl->proc_map_free_first <= -1 && prepare_to_alloc_pid(hcl) <= -1) return HCL_NULL;

	stksize = hcl->option.dfl_procstk_size;
	if (stksize > HCL_TYPE_MAX(hcl_oow_t) - HCL_PROCESS_NAMED_INSTVARS)
		stksize = HCL_TYPE_MAX(hcl_oow_t) - HCL_PROCESS_NAMED_INSTVARS;

	hcl_pushtmp (hcl, (hcl_oop_t*)&c);
	proc = (hcl_oop_process_t)hcl_allocoopobj (hcl, HCL_BRAND_PROCESS, HCL_PROCESS_NAMED_INSTVARS + stksize);
	hcl_poptmp (hcl);
	if (!proc) return HCL_NULL;

	/* assign a process id to the process */
	alloc_pid (hcl, proc);

	proc->state = HCL_SMOOI_TO_OOP(PROC_STATE_SUSPENDED);
	proc->initial_context = c;
	proc->current_context = c;
	proc->sp = HCL_SMOOI_TO_OOP(-1);

	HCL_ASSERT (hcl, (hcl_oop_t)c->sender == hcl->_nil);

#if defined(HCL_DEBUG_VM_PROCESSOR)
	HCL_LOG2 (hcl, HCL_LOG_IC | HCL_LOG_DEBUG, "Processor - process[%zd] **CREATED**->%hs\n", HCL_OOP_TO_SMOOI(proc->id), proc_state_to_string(HCL_OOP_TO_SMOOI(proc->state)));
#endif
	return proc;
}

static HCL_INLINE void sleep_active_process (hcl_t* hcl, int state)
{
#if defined(HCL_DEBUG_VM_PROCESSOR)
	HCL_LOG3 (hcl, HCL_LOG_IC | HCL_LOG_DEBUG, "Processor - put process %O context %O ip=%zd to sleep\n", hcl->processor->active, hcl->active_context, hcl->ip);
#endif

	STORE_ACTIVE_SP(hcl);

#if defined(HCL_DEBUG_VM_PROCESSOR)
	HCL_LOG3 (hcl, HCL_LOG_IC | HCL_LOG_DEBUG, "Processor - process[%zd] %hs->%hs in sleep_active_process\n", HCL_OOP_TO_SMOOI(hcl->processor->active->id), proc_state_to_string(HCL_OOP_TO_SMOOI(hcl->processor->active->state)), proc_state_to_string(state));
#endif

	/* store the current active context to the current process.
	 * it is the suspended context of the process to be suspended */
	HCL_ASSERT (hcl, hcl->processor->active != hcl->nil_process);
	hcl->processor->active->current_context = hcl->active_context;
	hcl->processor->active->state = HCL_SMOOI_TO_OOP(state);
}

static HCL_INLINE void wake_new_process (hcl_t* hcl, hcl_oop_process_t proc)
{

#if defined(HCL_DEBUG_VM_PROCESSOR)
	HCL_LOG2 (hcl, HCL_LOG_IC | HCL_LOG_DEBUG, "Processor - process[%zd] %hs->RUNNING in wake_process\n", HCL_OOP_TO_SMOOI(proc->id), proc_state_to_string(HCL_OOP_TO_SMOOI(proc->state)));
#endif

	/* activate the given process */
	proc->state = HCL_SMOOI_TO_OOP(PROC_STATE_RUNNING);
	hcl->processor->active = proc;

	LOAD_ACTIVE_SP(hcl);

	/* activate the suspended context of the new process */
	SWITCH_ACTIVE_CONTEXT (hcl, proc->current_context);

#if defined(HCL_DEBUG_VM_PROCESSOR) && (HCL_DEBUG_VM_PROCESSOR >= 2)
	HCL_LOG3 (hcl, HCL_LOG_IC | HCL_LOG_DEBUG, "Processor - woke up process[%zd] context %O ip=%zd\n", HCL_OOP_TO_SMOOI(hcl->processor->active->id), hcl->active_context, hcl->ip);
#endif
}

static void switch_to_process (hcl_t* hcl, hcl_oop_process_t proc, int new_state_for_old_active)
{
	/* the new process must not be the currently active process */
	HCL_ASSERT (hcl, hcl->processor->active != proc);

	/* the new process must be in the runnable state */
	HCL_ASSERT (hcl, proc->state == HCL_SMOOI_TO_OOP(PROC_STATE_RUNNABLE) ||
	            proc->state == HCL_SMOOI_TO_OOP(PROC_STATE_WAITING));

	sleep_active_process (hcl, new_state_for_old_active);
	wake_new_process (hcl, proc);

	hcl->proc_switched = 1;
}

static HCL_INLINE hcl_oop_process_t find_next_runnable_process (hcl_t* hcl)
{
	hcl_oop_process_t npr;

	HCL_ASSERT (hcl, hcl->processor->active->state == HCL_SMOOI_TO_OOP(PROC_STATE_RUNNING));
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

static HCL_INLINE int chain_into_processor (hcl_t* hcl, hcl_oop_process_t proc, int new_state)
{
	/* the process is not scheduled at all. 
	 * link it to the processor's process list. */
	hcl_ooi_t tally;

	HCL_ASSERT (hcl, (hcl_oop_t)proc->prev == hcl->_nil);
	HCL_ASSERT (hcl, (hcl_oop_t)proc->next == hcl->_nil);

	HCL_ASSERT (hcl, proc->state == HCL_SMOOI_TO_OOP(PROC_STATE_SUSPENDED));

#if defined(HCL_DEBUG_VM_PROCESSOR)
	HCL_LOG3 (hcl, HCL_LOG_IC | HCL_LOG_DEBUG, 
		"Processor - process[%zd] %hs->%hs in chain_into_processor\n",
		HCL_OOP_TO_SMOOI(proc->id),
		proc_state_to_string(HCL_OOP_TO_SMOOI(proc->state)),
		proc_state_to_string(new_state));
#endif

	tally = HCL_OOP_TO_SMOOI(hcl->processor->tally);

	HCL_ASSERT (hcl, tally >= 0);
	if (tally >= HCL_SMOOI_MAX)
	{
#if defined(HCL_DEBUG_VM_PROCESSOR)
		HCL_LOG0 (hcl, HCL_LOG_IC | HCL_LOG_FATAL, "Processor - too many process\n");
#endif
		hcl_seterrnum (hcl, HCL_EPFULL);
		hcl_seterrbfmt (hcl, HCL_EPFULL, "maximum number(%zd) of processes reached", HCL_SMOOI_MAX);
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
	proc->state = HCL_SMOOI_TO_OOP(new_state);

	tally++;
	hcl->processor->tally = HCL_SMOOI_TO_OOP(tally);

	return 0;
}

static HCL_INLINE void unchain_from_processor (hcl_t* hcl, hcl_oop_process_t proc, int new_state)
{
	hcl_ooi_t tally;

	/* the processor's process chain must be composed of running/runnable
	 * processes only */
	HCL_ASSERT (hcl, proc->state == HCL_SMOOI_TO_OOP(PROC_STATE_RUNNING) ||
	                 proc->state == HCL_SMOOI_TO_OOP(PROC_STATE_RUNNABLE));

	tally = HCL_OOP_TO_SMOOI(hcl->processor->tally);
	HCL_ASSERT (hcl, tally > 0);

	if ((hcl_oop_t)proc->prev != hcl->_nil) proc->prev->next = proc->next;
	else hcl->processor->runnable_head = proc->next;
	if ((hcl_oop_t)proc->next != hcl->_nil) proc->next->prev = proc->prev;
	else hcl->processor->runnable_tail = proc->prev;

#if defined(HCL_DEBUG_VM_PROCESSOR)
	HCL_LOG3 (hcl, HCL_LOG_IC | HCL_LOG_DEBUG, "Processor - process[%zd] %hs->%hs in unchain_from_processor\n", HCL_OOP_TO_SMOOI(proc->id), proc_state_to_string(HCL_OOP_TO_SMOOI(proc->state)), proc_state_to_string(HCL_OOP_TO_SMOOI(new_state)));
#endif

	proc->prev = (hcl_oop_process_t)hcl->_nil;
	proc->next = (hcl_oop_process_t)hcl->_nil;
	proc->state = HCL_SMOOI_TO_OOP(new_state);

	tally--;
	if (tally == 0) hcl->processor->active = hcl->nil_process;
	hcl->processor->tally = HCL_SMOOI_TO_OOP(tally);


}

static HCL_INLINE void chain_into_semaphore (hcl_t* hcl, hcl_oop_process_t proc, hcl_oop_semaphore_t sem)
{
	/* append a process to the process list of a semaphore*/

	HCL_ASSERT (hcl, (hcl_oop_t)proc->sem == hcl->_nil);
	HCL_ASSERT (hcl, (hcl_oop_t)proc->prev == hcl->_nil);
	HCL_ASSERT (hcl, (hcl_oop_t)proc->next == hcl->_nil);

	if ((hcl_oop_t)sem->waiting_head == hcl->_nil)
	{
		HCL_ASSERT (hcl, (hcl_oop_t)sem->waiting_tail == hcl->_nil);
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

	HCL_ASSERT (hcl, (hcl_oop_t)proc->sem != hcl->_nil);

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
		HCL_LOG2 (hcl, HCL_LOG_IC | HCL_LOG_DEBUG, "Processor - process[%zd] %hs->TERMINATED in terminate_process\n", HCL_OOP_TO_SMOOI(proc->id), proc_state_to_string(HCL_OOP_TO_SMOOI(proc->state)));
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
			HCL_ASSERT (hcl, (hcl_oop_t)proc->sem == hcl->_nil);

			if (nrp == proc)
			{
				/* no runnable process after termination */
				HCL_ASSERT (hcl, hcl->processor->active == hcl->nil_process);
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

		/* when terminated, clear it from the pid table and set the process id to a negative number */
		free_pid (hcl, proc);
	}
	else if (proc->state == HCL_SMOOI_TO_OOP(PROC_STATE_SUSPENDED))
	{
		/* SUSPENDED ---> TERMINATED */
	#if defined(HCL_DEBUG_VM_PROCESSOR)
		HCL_LOG2 (hcl, HCL_LOG_IC | HCL_LOG_DEBUG, "Processor - process[%zd] %hs->TERMINATED in terminate_process\n", HCL_OOP_TO_SMOOI(proc->id), proc_state_to_string(HCL_OOP_TO_SMOOI(proc->state)));
	#endif

		proc->state = HCL_SMOOI_TO_OOP(PROC_STATE_TERMINATED);
		proc->sp = HCL_SMOOI_TO_OOP(-1); /* invalidate the proce stack */

		if ((hcl_oop_t)proc->sem != hcl->_nil)
		{
			unchain_from_semaphore (hcl, proc);
		}

		/* when terminated, clear it from the pid table and set the process id to a negative number */
		free_pid (hcl, proc);
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
		HCL_ASSERT (hcl, (hcl_oop_t)proc->prev == hcl->_nil);
		HCL_ASSERT (hcl, (hcl_oop_t)proc->next == hcl->_nil);

	#if defined(HCL_DEBUG_VM_PROCESSOR)
		HCL_LOG2 (hcl, HCL_LOG_IC | HCL_LOG_DEBUG, "Processor - process[%zd] %hs->RUNNABLE in resume_process\n", HCL_OOP_TO_SMOOI(proc->id), proc_state_to_string(HCL_OOP_TO_SMOOI(proc->state)));
	#endif

		chain_into_processor (hcl, proc, PROC_STATE_RUNNABLE); /* TODO: error check */
		/*proc->current_context = proc->initial_context;*/
		

		/* don't switch to this process. just set the state to RUNNING */
	}
#if 0
	else if (proc->state == HCL_SMOOI_TO_OOP(PROC_STATE_RUNNABLE))
	{
		/* RUNNABLE ---> RUNNING */
		/* TODO: should i allow this? */
		HCL_ASSERT (hcl, hcl->processor->active != proc);
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
				HCL_ASSERT (hcl, hcl->processor->active == hcl->nil_process);
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
				HCL_ASSERT (hcl, hcl->processor->active != hcl->nil_process);
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

		HCL_ASSERT (hcl, proc == hcl->processor->active);

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
#if 0
	if (hcl->sem_list_count >= SEM_LIST_MAX)
	{
		hcl_seterrnum (hcl, HCL_ESLFULL);
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
#endif
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

		HCL_ASSERT (hcl, sem->waiting_tail == proc);

		HCL_ASSERT (hcl, hcl->processor->active != proc);
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

#if 0
	if (hcl->sem_heap_count >= SEM_HEAP_MAX)
	{
		hcl_seterrnum (hcl, HCL_ESHFULL);
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

	HCL_ASSERT (hcl, hcl->sem_heap_count <= HCL_SMOOI_MAX);

	index = hcl->sem_heap_count;
	hcl->sem_heap[index] = sem;
	sem->heap_index = HCL_SMOOI_TO_OOP(index);
	hcl->sem_heap_count++;

	sift_up_sem_heap (hcl, index);
#endif
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
	HCL_ASSERT (hcl, HCL_IS_CONTEXT (hcl, rcv_blkctx));
	if (rcv_blkctx->receiver_or_source != hcl->_nil)
	{
		/* the 'source' field is not nil.
		 * this block context has already been activated once.
		 * you can't send 'value' again to reactivate it.
		 * For example, [thisContext value] value. */
		HCL_ASSERT (hcl, HCL_OBJ_GET_SIZE(rcv_blkctx) > HCL_CONTEXT_NAMED_INSTVARS);
		HCL_LOG1 (hcl, HCL_LOG_IC | HCL_LOG_ERROR, 
			"Error - re-valuing of a block context - %O\n", rcv_blkctx);
		hcl_seterrbfmt (hcl, HCL_ERECALL, "cannot recall %O", rcv_blkctx);
		return -1;
	}
	HCL_ASSERT (hcl, HCL_OBJ_GET_SIZE(rcv_blkctx) == HCL_CONTEXT_NAMED_INSTVARS);

	if (HCL_OOP_TO_SMOOI(rcv_blkctx->method_or_nargs) != nargs)
	{
		HCL_LOG3 (hcl, HCL_LOG_IC | HCL_LOG_ERROR, 
			"Error - wrong number of arguments to a block context %O - expecting %zd, got %zd\n",
			rcv_blkctx, HCL_OOP_TO_SMOOI(rcv_blkctx->method_or_nargs), nargs);
		hcl_seterrnum (hcl, HCL_ECALLARG);
		return -1;
	}

	/* the number of temporaries stored in the block context
	 * accumulates the number of temporaries starting from the origin.
	 * simple calculation is needed to find the number of local temporaries */
	local_ntmprs = HCL_OOP_TO_SMOOI(rcv_blkctx->ntmprs) -
	               HCL_OOP_TO_SMOOI(((hcl_oop_context_t)rcv_blkctx->home)->ntmprs);
	HCL_ASSERT (hcl, local_ntmprs >= nargs);

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

	HCL_ASSERT (hcl, blkctx->home != hcl->_nil);
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
	HCL_ASSERT (hcl, HCL_IS_CONTEXT (hcl, rcv));

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
	HCL_ASSERT (hcl, HCL_IS_PRIM(hcl, rcv));
	HCL_ASSERT (hcl, HCL_OBJ_GET_SIZE(rcv) == 4);

	if (nargs < rcv->slot[1] && nargs > rcv->slot[2])
	{
/* TODO: include a primitive name... */
		HCL_LOG3 (hcl, HCL_LOG_IC | HCL_LOG_ERROR, 
			"Error - wrong number of arguments to a primitive - expecting %zd-%zd, got %zd\n",
			rcv->slot[1], rcv->slot[2], nargs);
		hcl_seterrnum (hcl, HCL_ECALLARG);
		return -1;
	}

	return ((hcl_pfimpl_t)rcv->slot[0]) (hcl, (hcl_mod_t*)rcv->slot[3], nargs);
}


#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <limits.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

extern char **environ;

#define _PATH_DEFPATH "/usr/bin:/bin"

static int is_regular_executable_file_by_me(const char *path)
{
	struct stat path_stat;
	stat(path, &path_stat);
	return S_ISREG(path_stat.st_mode) && access(path, X_OK) == 0; //? use eaccess instead??
}

static char* find_exec (hcl_t* hcl, const char *name)
{
	size_t lp, ln;
	char buf[PATH_MAX];
	const char *bp, *path, *p;

	bp = buf;

	/* Get the path we're searching. */
	if (!(path = getenv("PATH"))) path = _PATH_DEFPATH;

	ln = strlen(name);
	do 
	{
		/* Find the end of this path element. */
		for (p = path; *path != 0 && *path != ':'; path++) ;

		/*
		 * It's a SHELL path -- double, leading and trailing colons
		 * mean the current directory.
		 */
		if (p == path) 
		{
			p = ".";
			lp = 1;
		}
		else
		{
			lp = path - p;
		}

		/*
		 * If the path is too long complain.  This is a possible
		 * security issue; given a way to make the path too long
		 * the user may execute the wrong program.
		 */
		if (lp + ln + 2 > sizeof(buf)) continue;

		memcpy(buf, p, lp);
		buf[lp] = '/';
		memcpy(buf + lp + 1, name, ln);
		buf[lp + ln + 1] = '\0';

		if (is_regular_executable_file_by_me(bp)) return strdup(bp);

	} 
	while (*path++ == ':'); /* Otherwise, *path was NUL */


done:
	hcl_seterrbfmt (hcl, HCL_ENOENT, "callable %hs not found", name);
	return HCL_NULL;
}


static HCL_INLINE int exec_syscmd (hcl_t* hcl, hcl_ooi_t nargs)
{
	hcl_oop_word_t rcv;
	hcl_bch_t* cmd, * xcmd;

	rcv = (hcl_oop_word_t)HCL_STACK_GETRCV(hcl, nargs);
	/*HCL_ASSERT (hcl, HCL_IS_STRING(hcl, rcv) || HCL_IS_SYMBOL(hcl, rcv));*/
	HCL_ASSERT (hcl, HCL_OBJ_IS_CHAR_POINTER(rcv));

	if (HCL_OBJ_GET_SIZE(rcv) == 0 || hcl_count_oocstr(HCL_OBJ_GET_CHAR_SLOT(rcv)) != HCL_OBJ_GET_SIZE(rcv))
	{
		/* '\0' is contained in the middle */
		hcl_seterrbfmt (hcl, HCL_EINVAL, "invalid callable %O", rcv);
		return -1;
	}

	cmd = hcl_dupootobcstr(hcl, HCL_OBJ_GET_CHAR_SLOT(rcv), HCL_NULL);
	if (!cmd) return -1;

	if (hcl_find_bchar_in_bcstr(cmd, '/'))
	{
		if (!is_regular_executable_file_by_me(cmd)) 
		{
			hcl_seterrbfmt (hcl, HCL_ECALL, "cannot execute %O", rcv);
			return -1;
		}

		xcmd = cmd;
	}
	else
	{
		xcmd = find_exec(hcl, cmd);
		if (!xcmd) return -1;
	}

{ /* TODO: make it a callback ... */
	pid_t pid;
	int status;

	pid = fork();
	if (pid == -1) return -1;

/* TODO: set a new process group / session leader??? */

	if (pid == 0)
	{

			hcl_bch_t** argv;
			hcl_ooi_t i;

			/* TODO: close file descriptors??? */
			argv = (hcl_bch_t**)hcl_allocmem(hcl, (nargs + 2) * HCL_SIZEOF(*argv));
			if (argv)
			{
				argv[0] = cmd;
HCL_DEBUG1 (hcl, "NARG %d\n", (int)nargs);
				for (i = 0; i < nargs;)
				{
					hcl_oop_t ta = HCL_STACK_GETARG(hcl, nargs, i);
/* TODO: check if an argument is a string or a symbol */
					argv[++i] = hcl_dupootobchars(hcl, HCL_OBJ_GET_CHAR_SLOT(ta), HCL_OBJ_GET_SIZE(ta), HCL_NULL);
HCL_DEBUG2 (hcl, "ARG %d -> %hs\n", (int)i - 1, argv[i]);
				}
				argv[nargs + 1] = HCL_NULL;
				execvp (xcmd, argv);
			}
			_exit (255);
	}

	waitpid (pid, &status, 0);

	HCL_STACK_SETRET (hcl, nargs, HCL_SMOOI_TO_OOP(WEXITSTATUS(status)));
}

	hcl_freemem (hcl, cmd);
	if (xcmd != cmd) hcl_freemem (hcl, xcmd);
	return 0;
}

/* ------------------------------------------------------------------------- */
static hcl_oop_process_t start_initial_process (hcl_t* hcl, hcl_oop_context_t ctx)
{
	hcl_oop_process_t proc;

	/* there must be no active process when this function is called */
	HCL_ASSERT (hcl, hcl->processor->tally == HCL_SMOOI_TO_OOP(0));
	HCL_ASSERT (hcl, hcl->processor->active == hcl->nil_process);

	proc = make_process (hcl, ctx);
	if (!proc) return HCL_NULL;

	/* skip RUNNABLE and go to RUNNING */
	if (chain_into_processor (hcl, proc, PROC_STATE_RUNNING) <= -1) return HCL_NULL; 
	hcl->processor->active = proc;

	/* do something that resume_process() would do with less overhead */
	HCL_ASSERT (hcl, (hcl_oop_t)proc->current_context != hcl->_nil);
	HCL_ASSERT (hcl, proc->current_context == proc->initial_context);
	SWITCH_ACTIVE_CONTEXT (hcl, proc->current_context);

	return proc;
}

static int start_initial_process_and_context (hcl_t* hcl, hcl_ooi_t initial_ip)
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
	hcl->ip = initial_ip;
	hcl->sp = -1;

	ctx->ip = HCL_SMOOI_TO_OOP(0); /* point to the beginning */
	ctx->sp = HCL_SMOOI_TO_OOP(-1); /* pointer to -1 below the bottom */
	ctx->origin = ctx; /* point to self */
	/*ctx->method_or_nargs = (hcl_oop_t)mth;*/ /* fake. help SWITCH_ACTIVE_CONTEXT() not fail. */
	ctx->method_or_nargs = HCL_SMOOI_TO_OOP(0);
/* TODO: XXXXX */
	ctx->ntmprs = HCL_SMOOI_TO_OOP(0);
	ctx->home = (hcl_oop_t)ctx; /*  is this correct??? */
/* END XXXXX */

	/* [NOTE]
	 *  the receiver field and the sender field of ctx are nils.
	 *  especially, the fact that the sender field is nil is used by 
	 *  the main execution loop for breaking out of the loop */

	HCL_ASSERT (hcl, hcl->active_context == HCL_NULL);

	/* hcl_gc() uses hcl->processor when hcl->active_context
	 * is not NULL. at this poinst, hcl->processor should point to
	 * an instance of ProcessScheduler. */
	HCL_ASSERT (hcl, (hcl_oop_t)hcl->processor != hcl->_nil);
	HCL_ASSERT (hcl, hcl->processor->tally == HCL_SMOOI_TO_OOP(0));

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

#if defined(HCL_PROFILE_VM)
	hcl_uintmax_t inst_counter = 0;
#endif

#if defined(HCL_DEBUG_VM_EXEC)
	hcl_ooi_t fetched_instruction_pointer;
#endif

	HCL_ASSERT (hcl, hcl->active_context != HCL_NULL);

	hcl->abort_req = 0;
	if (vm_startup(hcl) <= -1) return -1;
	hcl->proc_switched = 0;

	while (1)
	{
	#if defined(ENABLE_MULTI_PROCS) 
		/* i don't think i will ever implement this in HCL.
		 * but let's keep the code here for some while */
		if (hcl->sem_heap_count > 0)
		{
			hcl_ntime_t ft, now;
			hcl->vmprim.vm_gettime (hcl, &now);

			do
			{
				HCL_ASSERT (hcl, HCL_OOP_IS_SMOOI(hcl->sem_heap[0]->heap_ftime_sec));
				HCL_ASSERT (hcl, HCL_OOP_IS_SMOOI(hcl->sem_heap[0]->heap_ftime_nsec));

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
						HCL_ASSERT (hcl, proc->state == HCL_SMOOI_TO_OOP(PROC_STATE_RUNNABLE));
						HCL_ASSERT (hcl, proc == hcl->processor->runnable_head);

						wake_new_process (hcl, proc);
						hcl->proc_switched = 1;
					}
				}
				else if (hcl->processor->active == hcl->nil_process)
				{
					HCL_SUBNTIME (&ft, &ft, (hcl_ntime_t*)&now);
					hcl->vmprim.vm_sleep (hcl, &ft); /* TODO: change this to i/o multiplexer??? */
					hcl->vmprim.vm_gettime (hcl, &now);
				}
				else 
				{
					break;
				}
			} 
			while (hcl->sem_heap_count > 0);
		}
	#endif

		if (hcl->processor->active == hcl->nil_process) 
		{
			/* no more waiting semaphore and no more process */
			HCL_ASSERT (hcl, hcl->processor->tally = HCL_SMOOI_TO_OOP(0));
			HCL_LOG0 (hcl, HCL_LOG_IC | HCL_LOG_DEBUG, "No more runnable process\n");

			#if 0
			if (there is semaphore awaited.... )
			{
			/* DO SOMETHING */
			}
			#endif

			break;
		}

	#if defined(ENABLE_MULTI_PROCS)
		while (hcl->sem_list_count > 0)
		{
			/* handle async signals */
			--hcl->sem_list_count;
			signal_semaphore (hcl, hcl->sem_list[hcl->sem_list_count]);
		}

		/* TODO: implement different process switching scheme - time-slice or clock based??? */
		#if defined(HCL_EXTERNAL_PROCESS_SWITCH)
		if (!hcl->proc_switched && hcl->switch_proc) { switch_to_next_runnable_process (hcl); }
		hcl->switch_proc = 0;
		#else
		if (!hcl->proc_switched) { switch_to_next_runnable_process (hcl); }
		#endif

		hcl->proc_switched = 0;
	#endif

		if (HCL_UNLIKELY(hcl->ip >= hcl->code.bc.len)) 
		{
			HCL_DEBUG1 (hcl, "Stopping executeion as IP reached the end of bytecode(%zu)\n", hcl->code.bc.len);
			return_value = hcl->_nil;
			goto handle_return;
		}

	#if defined(HCL_DEBUG_VM_EXEC)
		fetched_instruction_pointer = hcl->ip;
	#endif
		FETCH_BYTE_CODE_TO (hcl, bcode);
		/*while (bcode == HCL_CODE_NOOP) FETCH_BYTE_CODE_TO (hcl, bcode);*/

		if (hcl->vm_checkbc_cb_count) vm_checkbc (hcl, bcode);
		
		if (HCL_UNLIKELY(hcl->abort_req))
		{
			/* i place this abortion check after vm_checkbc()
			 * to honor hcl_abort() if called in the callback, */
			HCL_DEBUG0 (hcl, "Stopping execution for abortion request\n");
			return_value = hcl->_nil;
			goto handle_return;
		}

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
				HCL_ASSERT (hcl, HCL_OBJ_GET_FLAGS_TYPE(hcl->active_context->origin->receiver_or_source) == HCL_OBJ_TYPE_OOP);
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
				HCL_ASSERT (hcl, HCL_OBJ_GET_FLAGS_TYPE(hcl->active_context->receiver_or_source) == HCL_OBJ_TYPE_OOP);
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
				HCL_ASSERT (hcl, HCL_OBJ_GET_FLAGS_TYPE(hcl->active_context->receiver_or_source) == HCL_OBJ_TYPE_OOP);
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
				HCL_ASSERT (hcl, HCL_IS_CONTEXT(hcl, ctx));
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
				HCL_ASSERT (hcl, HCL_IS_CONS(hcl, ass));

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
				/*if (HCL_STACK_GETTOP(hcl) == hcl->_true) hcl->ip += b1; TODO: _true or not _false?*/
				if (HCL_STACK_GETTOP(hcl) != hcl->_false) hcl->ip += b1;
				break;

			case HCL_CODE_JUMP2_FORWARD_IF_TRUE:
				FETCH_PARAM_CODE_TO (hcl, b1);
				LOG_INST_1 (hcl, "jump2_forward_if_true %zu", b1);
				/*if (HCL_STACK_GETTOP(hcl) == hcl->_true) hcl->ip += MAX_CODE_JUMP + b1;*/
				if (HCL_STACK_GETTOP(hcl) != hcl->_false) hcl->ip += MAX_CODE_JUMP + b1;
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

				rcv = HCL_STACK_GETRCV(hcl, b1);
				if (HCL_OOP_IS_POINTER(rcv))
				{
					switch (HCL_OBJ_GET_FLAGS_BRAND(rcv))
					{
						case HCL_BRAND_CONTEXT:
							if (activate_context(hcl, b1) <= -1) goto oops;
							break;
						case HCL_BRAND_PRIM:
							if (call_primitive(hcl, b1) <= -1) goto oops;
							break;
						case HCL_BRAND_STRING:
							if ((hcl->option.trait & HCL_CLI_MODE) && exec_syscmd(hcl, b1) >= 0) break;
							/* fall thru */
						default:
							goto cannot_call;
					}
				}
				else
				{
				cannot_call:
					/* run time error */
					hcl_seterrbfmt (hcl, HCL_ECALL, "cannot call %O", rcv);
					goto oops;
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
				HCL_ASSERT (hcl, (hcl_oop_t)ctx != hcl->_nil);
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
				HCL_ASSERT (hcl, HCL_OBJ_GET_FLAGS_TYPE(t) == HCL_OBJ_TYPE_OOP);
				HCL_ASSERT (hcl, b1 < HCL_OBJ_GET_SIZE(t));

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

			case HCL_CODE_MAKE_ARRAY:
			{
				hcl_oop_t t;

				FETCH_PARAM_CODE_TO (hcl, b1);
				LOG_INST_1 (hcl, "make_array %zu", b1);

				/* create an empty array */
				t = hcl_makearray (hcl, b1, 0);
				if (!t) goto oops;

				HCL_STACK_PUSH (hcl, t); /* push the array created */
				break;
			}

			case HCL_CODE_POP_INTO_ARRAY:
			{
				hcl_oop_t t1, t2;
				FETCH_PARAM_CODE_TO (hcl, b1);
				LOG_INST_1 (hcl, "pop_into_array %zu", b1);
				t1 = HCL_STACK_GETTOP(hcl); /* value to store */
				HCL_STACK_POP (hcl);
				t2 = HCL_STACK_GETTOP(hcl); /* array */
				((hcl_oop_oop_t)t2)->slot[b1] = t1;
				break;
			}

			case HCL_CODE_MAKE_BYTEARRAY:
			{
				hcl_oop_t t;

				FETCH_PARAM_CODE_TO (hcl, b1);
				LOG_INST_1 (hcl, "make_bytearray %zu", b1);

				/* create an empty array */
				t = hcl_makebytearray (hcl, HCL_NULL, b1);
				if (!t) goto oops;

				HCL_STACK_PUSH (hcl, t); /* push the byte array created */
				break;
			}

			case HCL_CODE_POP_INTO_BYTEARRAY:
			{
				hcl_oop_t t1, t2;
				hcl_ooi_t bv;

				FETCH_PARAM_CODE_TO (hcl, b1);
				LOG_INST_1 (hcl, "pop_into_bytearray %zu", b1);

				t1 = HCL_STACK_GETTOP(hcl); /* value to store */
				if (!HCL_OOP_IS_SMOOI(t1) || (bv = HCL_OOP_TO_SMOOI(t1)) < 0 || bv > 255)
				{
					hcl_seterrbfmt (hcl, HCL_ERANGE, "not a byte or out of byte range - %O", t1);
					goto oops;
				}
				HCL_STACK_POP (hcl);
				t2 = HCL_STACK_GETTOP(hcl); /* array */
				((hcl_oop_byte_t)t2)->slot[b1] = bv;
				break;
			}


			case HCL_CODE_MAKE_DIC:
			{
				hcl_oop_t t;

				FETCH_PARAM_CODE_TO (hcl, b1);
				LOG_INST_1 (hcl, "make_dic %zu", b1);
				t = (hcl_oop_t)hcl_makedic (hcl, b1 + 10);
				if (!t) goto oops;
				HCL_STACK_PUSH (hcl, t);
				break;
			}

			case HCL_CODE_POP_INTO_DIC:
			{
				hcl_oop_t t1, t2, t3;

				LOG_INST_0 (hcl, "pop_into_dic");
				t1 = HCL_STACK_GETTOP(hcl); /* value */
				HCL_STACK_POP (hcl);
				t2 = HCL_STACK_GETTOP(hcl); /* key */
				HCL_STACK_POP (hcl);
				t3 = HCL_STACK_GETTOP(hcl); /* dictionary */
				if (!hcl_putatdic (hcl, (hcl_oop_dic_t)t3, t2, t1)) goto oops;
				break;
			}

			/* -------------------------------------------------------- */

			case BCODE_DUP_STACKTOP:
			{
				hcl_oop_t t;
				LOG_INST_0 (hcl, "dup_stacktop");
				HCL_ASSERT (hcl, !HCL_STACK_ISEMPTY(hcl));
				t = HCL_STACK_GETTOP(hcl);
				HCL_STACK_PUSH (hcl, t);
				break;
			}

			case HCL_CODE_POP_STACKTOP:
				LOG_INST_0 (hcl, "pop_stacktop");
				HCL_ASSERT (hcl, !HCL_STACK_ISEMPTY(hcl));

				/* at the top level, the value is just popped off the stack
				 * after evaluation of an expressio. so it's likely the
				 * return value of the last expression unless explicit
				 * returning is performed */
				hcl->last_retv = HCL_STACK_GETTOP(hcl);
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
/*
//					HCL_ASSERT (hcl, HCL_CLASSOF(hcl, hcl->active_context) == hcl->_block_context);
//					HCL_ASSERT (hcl, HCL_CLASSOF(hcl, hcl->processor->active->initial_context) == hcl->_block_context);
*/
					/* decrement the instruction pointer back to the return instruction.
					 * even if the context is reentered, it will just return.
					 *hcl->ip--;*/
					terminate_process (hcl, hcl->processor->active);
				}
				else 
				{
					/* set the instruction pointer to an invalid value.
					 * this is stored into the current method context
					 * before context switching and marks a dead context */
					if (hcl->active_context->origin == hcl->active_context)
					{
						/* returning from a method */
/*
//						HCL_ASSERT (hcl, HCL_CLASSOF(hcl, hcl->active_context) == hcl->_method_context);
*/
						hcl->ip = -1;
					}
					else
					{
						hcl_oop_context_t ctx;

						/* method return from within a block(including a non-local return) */
/*
//						HCL_ASSERT (hcl, HCL_CLASSOF(hcl, hcl->active_context) == hcl->_block_context);
*/
						ctx = hcl->active_context;
						while ((hcl_oop_t)ctx != hcl->_nil)
						{
							if (ctx == hcl->active_context->origin) goto non_local_return_ok;
							ctx = ctx->sender;
						}

						/* cannot return from a method that has returned already */
/*
//						HCL_ASSERT (hcl, HCL_CLASSOF(hcl, hcl->active_context->origin) == hcl->_method_context);
*/
						HCL_ASSERT (hcl, hcl->active_context->origin->ip == HCL_SMOOI_TO_OOP(-1));

						HCL_LOG0 (hcl, HCL_LOG_IC | HCL_LOG_ERROR, "Error - cannot return from dead context\n");
						hcl_seterrbfmt (hcl, HCL_EINTERN, "unable to return from dead context"); /* TODO: can i make this error catchable at the hcl level? */
						goto oops;

					non_local_return_ok:
/*HCL_DEBUG2 (hcl, "NON_LOCAL RETURN OK TO... %p %p\n", hcl->active_context->origin, hcl->active_context->origin->sender);*/
						hcl->active_context->origin->ip = HCL_SMOOI_TO_OOP(-1);
					}

/*
//					HCL_ASSERT (hcl, HCL_CLASSOF(hcl, hcl->active_context->origin) == hcl->_method_context);
*/
					/* restore the stack pointer */
					hcl->sp = HCL_OOP_TO_SMOOI(hcl->active_context->origin->sp);
					SWITCH_ACTIVE_CONTEXT (hcl, hcl->active_context->origin->sender);

					/* push the return value to the stack of the new active context */
					HCL_STACK_PUSH (hcl, return_value);

					if (hcl->active_context == hcl->initial_context)
					{
						/* the new active context is the fake initial context.
						 * this context can't get executed further. */
						HCL_ASSERT (hcl, (hcl_oop_t)hcl->active_context->sender == hcl->_nil);
/*
//							HCL_ASSERT (hcl, HCL_CLASSOF(hcl, hcl->active_context) == hcl->_method_context);
*/
						HCL_ASSERT (hcl, hcl->active_context->receiver_or_source == hcl->_nil);
						HCL_ASSERT (hcl, hcl->active_context == hcl->processor->active->initial_context);
						HCL_ASSERT (hcl, hcl->active_context->origin == hcl->processor->active->initial_context->origin);
						HCL_ASSERT (hcl, hcl->active_context->origin == hcl->active_context);

						/* NOTE: this condition is true for the processified block context also.
						 *   hcl->active_context->origin == hcl->processor->active->initial_context->origin
						 *   however, the check here is done after context switching and the
						 *   processified block check has been done against the context before switching */

						/* the stack contains the final return value so the stack pointer must be 0. */
						HCL_ASSERT (hcl, hcl->sp == 0); 

						if (hcl->option.trait & HCL_AWAIT_PROCS)
							terminate_process (hcl, hcl->processor->active);
						else
							goto done;

						/* TODO: store the return value to the VM register.
						 * the caller to hcl_execute() can fetch it to return it to the system */
					}
				}
				break;

			case HCL_CODE_RETURN_FROM_BLOCK:
				LOG_INST_0 (hcl, "return_from_block");

				HCL_ASSERT(hcl, HCL_IS_CONTEXT(hcl, hcl->active_context));
				hcl->last_retv = HCL_STACK_GETTOP(hcl);
				if (hcl->active_context == hcl->processor->active->initial_context)
				{
					/* the active context to return from is an initial context of
					 * the active process. this process must have been created 
					 * over a block using the newProcess method. let's terminate
					 * the process. */
					HCL_ASSERT (hcl, (hcl_oop_t)hcl->active_context->sender == hcl->_nil);
					terminate_process (hcl, hcl->processor->active);
				}
				else
				{
					/* it is a normal block return as the active block context 
					 * is not the initial context of a process */

					/* the process stack is shared. the return value 
					 * doesn't need to get moved. */
/*
					//XXX SWITCH_ACTIVE_CONTEXT (hcl, (hcl_oop_context_t)hcl->active_context->sender);
*/
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

				HCL_ASSERT (hcl, b1 >= 0);
				HCL_ASSERT (hcl, b2 >= b1);

				/* the block context object created here is used as a base
				 * object for block context activation. activate_context()
				 * clones a block context and activates the cloned context.
				 * this base block context is created with no temporaries
				 * for this reason */
				blkctx = (hcl_oop_context_t)make_context (hcl, 0);
				if (!blkctx) goto oops;

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
				HCL_ASSERT (hcl, hcl->sp >= 2);

				HCL_ASSERT (hcl, HCL_OOP_IS_SMOOI(HCL_STACK_GETTOP(hcl)));
				ntmprs = HCL_OOP_TO_SMOOI(HCL_STACK_GETTOP(hcl));
				HCL_STACK_POP (hcl);

				HCL_ASSERT (hcl, HCL_OOP_IS_SMOOI(HCL_STACK_GETTOP(hcl)));
				nargs = HCL_OOP_TO_SMOOI(HCL_STACK_GETTOP(hcl));
				HCL_STACK_POP (hcl);

				HCL_ASSERT (hcl, nargs >= 0);
				HCL_ASSERT (hcl, ntmprs >= nargs);

				/* the block context object created here is used
				 * as a base object for block context activation.
				 * prim_block_value() clones a block 
				 * context and activates the cloned context.
				 * this base block context is created with no 
				 * stack for this reason. */
				blkctx = (hcl_oop_context_t)make_context (hcl, 0);
				if (!blkctx) goto oops;

				/* get the receiver to the block copy message after block context instantiation
				 * not to get affected by potential GC */
				rctx = (hcl_oop_context_t)HCL_STACK_GETTOP(hcl);
				HCL_ASSERT (hcl, rctx == hcl->active_context);

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
				hcl_seterrnum (hcl, HCL_EINTERN);
				goto oops;
		}
	}

done:
	vm_cleanup (hcl);
#if defined(HCL_PROFILE_VM)
	HCL_LOG1 (hcl, HCL_LOG_IC | HCL_LOG_INFO, "TOTAL INST COUTNER = %zu\n", inst_counter);
#endif
	return 0;

oops:
	/* TODO: anything to do here? */
	if (hcl->processor->active != hcl->nil_process) 
	{
		HCL_LOG1 (hcl, HCL_LOG_IC | HCL_LOG_INFO, "TERMINATING ACTIVE PROCESS %zd for execution error\n", HCL_OOP_TO_SMOOI(hcl->processor->active->id));
		terminate_process (hcl, hcl->processor->active);
	}
	return -1;
}

hcl_oop_t hcl_executefromip (hcl_t* hcl, hcl_oow_t initial_ip)
{
	int n;
	hcl_bitmask_t log_default_type_mask;

	HCL_ASSERT (hcl, hcl->code.bc.len < HCL_SMOOI_MAX); /* asserted by the compiler */
	if (initial_ip >= hcl->code.bc.len)
	{
		hcl_seterrnum (hcl, HCL_EINVAL);
		return HCL_NULL;
	}

	log_default_type_mask = hcl->log.default_type_mask;
	hcl->log.default_type_mask |= HCL_LOG_VM;

	HCL_ASSERT (hcl, hcl->initial_context == HCL_NULL);
	HCL_ASSERT (hcl, hcl->active_context == HCL_NULL);

	hcl->last_retv = hcl->_nil;

	if (start_initial_process_and_context(hcl, initial_ip) <= -1) return HCL_NULL;
	hcl->initial_context = hcl->processor->active->initial_context;

	n = execute (hcl);

	HCL_INFO1 (hcl, "RETURNED VALUE - %O\n", hcl->last_retv);

/* TODO: reset processor fields. set processor->tally to zero. processor->active to nil_process... */
	hcl->initial_context = HCL_NULL;
	hcl->active_context = HCL_NULL;

	hcl->log.default_type_mask = log_default_type_mask;
	return (n <= -1)? HCL_NULL: hcl->last_retv;
}

hcl_oop_t hcl_execute (hcl_t* hcl)
{
	return hcl_executefromip (hcl, 0);
}

void hcl_abort (hcl_t* hcl)
{
	hcl->abort_req = 1;
}
