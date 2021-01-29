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

static const char* io_type_str[] =
{
	"input",
	"output"
};

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

/* TODO: adjust these max semaphore pointer buffer capacity,
 *       proably depending on the object memory size? */
#define SEM_LIST_INC 256
#define SEM_HEAP_INC 256
#define SEM_IO_TUPLE_INC 256
#define SEM_LIST_MAX (SEM_LIST_INC * 1000)
#define SEM_HEAP_MAX (SEM_HEAP_INC * 1000)
#define SEM_IO_TUPLE_MAX (SEM_IO_TUPLE_INC * 1000)
#define SEM_IO_MAP_ALIGN 1024 /* this must a power of 2 */

#define SEM_HEAP_PARENT(x) (((x) - 1) / 2)
#define SEM_HEAP_LEFT(x)   ((x) * 2 + 1)
#define SEM_HEAP_RIGHT(x)  ((x) * 2 + 2)

#define SEM_HEAP_EARLIER_THAN(stx,x,y) ( \
	(HCL_OOP_TO_SMOOI((x)->u.timed.ftime_sec) < HCL_OOP_TO_SMOOI((y)->u.timed.ftime_sec)) || \
	(HCL_OOP_TO_SMOOI((x)->u.timed.ftime_sec) == HCL_OOP_TO_SMOOI((y)->u.timed.ftime_sec) && HCL_OOP_TO_SMOOI((x)->u.timed.ftime_nsec) < HCL_OOP_TO_SMOOI((y)->u.timed.ftime_nsec)) \
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
		(hcl)->active_function = (hcl)->active_context->origin->receiver_or_base; \
		(hcl)->active_code = HCL_FUNCTION_GET_CODE_BYTE((hcl)->active_function); \
		LOAD_ACTIVE_IP (hcl); \
		(hcl)->processor->active->current_context = (hcl)->active_context; \
	} while (0)

/*#define FETCH_BYTE_CODE(hcl) ((hcl)->code.bc.arr->slot[(hcl)->ip++])*/
#define FETCH_BYTE_CODE(hcl) ((hcl)->active_code[(hcl)->ip++])
#define FETCH_BYTE_CODE_TO(hcl, v_oow) (v_oow = FETCH_BYTE_CODE(hcl))
#if (HCL_CODE_LONG_PARAM_SIZE == 2)
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
#	define LOG_INST_4(hcl,fmt,a1,a2,a3,a4) HCL_LOG5(hcl, LOG_MASK_INST, "%010zd " fmt "\n", fetched_instruction_pointer, a1, a2, a3, a4)

#else
#	define LOG_INST_0(hcl,fmt)
#	define LOG_INST_1(hcl,fmt,a1)
#	define LOG_INST_2(hcl,fmt,a1,a2)
#	define LOG_INST_3(hcl,fmt,a1,a2,a3)
#	define LOG_INST_4(hcl,fmt,a1,a2,a3,a4)
#endif

static int delete_sem_from_sem_io_tuple (hcl_t* hcl, hcl_oop_semaphore_t sem, int force);
static void signal_io_semaphore (hcl_t* hcl, hcl_ooi_t io_handle, hcl_ooi_t mask);

/* ------------------------------------------------------------------------- */

static HCL_INLINE int vm_startup (hcl_t* hcl)
{
	hcl_cb_t* cb;
	hcl_oow_t i;

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

	for (i = 0; i < hcl->sem_io_map_capa; i++)
	{
		hcl->sem_io_map[i] = -1;
	}

#if defined(ENABLE_GCFIN)
	hcl->sem_gcfin = (moo_oop_semaphore_t)hcl->_nil;
	hcl->sem_gcfin_sigreq = 0;
#endif

	hcl->vmprim.vm_gettime (hcl, &hcl->exec_start_time); /* raw time. no adjustment */

	return 0;
}

static void vm_cleanup (hcl_t* hcl)
{
	hcl_cb_t* cb;
	hcl_oow_t i;

	for (i = 0; i < hcl->sem_io_map_capa;)
	{
		hcl_ooi_t sem_io_index;
		if ((sem_io_index = hcl->sem_io_map[i]) >= 0)
		{
			HCL_ASSERT (hcl, sem_io_index < hcl->sem_io_tuple_count);
			HCL_ASSERT (hcl, hcl->sem_io_tuple[sem_io_index].sem[HCL_SEMAPHORE_IO_TYPE_INPUT] ||
			                 hcl->sem_io_tuple[sem_io_index].sem[HCL_SEMAPHORE_IO_TYPE_OUTPUT]);

			if (hcl->sem_io_tuple[sem_io_index].sem[HCL_SEMAPHORE_IO_TYPE_INPUT])
			{
				delete_sem_from_sem_io_tuple (hcl, hcl->sem_io_tuple[sem_io_index].sem[HCL_SEMAPHORE_IO_TYPE_INPUT], 1);
			}
			if (hcl->sem_io_tuple[sem_io_index].sem[HCL_SEMAPHORE_IO_TYPE_OUTPUT])
			{
				delete_sem_from_sem_io_tuple (hcl, hcl->sem_io_tuple[sem_io_index].sem[HCL_SEMAPHORE_IO_TYPE_OUTPUT], 1);
			}

			HCL_ASSERT (hcl, hcl->sem_io_map[i] <= -1);
		}
		else 
		{
			i++;
		}
	}

	HCL_ASSERT (hcl, hcl->sem_io_tuple_count == 0);
	HCL_ASSERT (hcl, hcl->sem_io_count == 0);

	hcl->vmprim.vm_gettime (hcl, &hcl->exec_end_time); /* raw time. no adjustment */
	for (cb = hcl->cblist; cb; cb = cb->next)
	{
		if (cb->vm_cleanup) cb->vm_cleanup(hcl);
	}

#if defined(ENABLE_GCFIN)
	hcl->sem_gcfin = (moo_oop_semaphore_t)hcl->_nil;
	hcl->sem_gcfin_sigreq = 0;

	/* deregister all pending finalizable objects pending just in case these
	 * have not been removed for various reasons. (e.g. sudden VM abortion)
	 */
	hcl_deregallfinalizables (hcl);
#endif

	HCL_DEBUG0 (hcl, "VM cleaned up\n");
}

static HCL_INLINE void vm_gettime (hcl_t* hcl, hcl_ntime_t* now)
{
	hcl->vmprim.vm_gettime (hcl, now);
	/* in vm_startup(), hcl->exec_start_time has been set to the time of
	 * that moment. time returned here get offset by hcl->exec_start_time and 
	 * thus becomes relative to it. this way, it is kept small such that it
	 * can be represented in a small integer with leaving almost zero chance
	 * of overflow. */
	HCL_SUB_NTIME (now, now, &hcl->exec_start_time);  /* now = now - exec_start_time */
}


static HCL_INLINE int vm_sleep (hcl_t* hcl, const hcl_ntime_t* dur)
{
/* TODO: return 1 if it gets into the halting state */
	hcl->vmprim.vm_sleep(hcl, dur);
	return 0;
}

static HCL_INLINE void vm_muxwait (hcl_t* hcl, const hcl_ntime_t* dur)
{
	hcl->vmprim.vm_muxwait (hcl, dur, signal_io_semaphore);
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

static HCL_INLINE hcl_oop_context_t make_context (hcl_t* hcl, hcl_ooi_t ntmprs)
{
	HCL_ASSERT (hcl, ntmprs >= 0);
	return (hcl_oop_context_t)hcl_allocoopobj(hcl, HCL_BRAND_CONTEXT, HCL_CONTEXT_NAMED_INSTVARS + (hcl_oow_t)ntmprs);
}

static HCL_INLINE hcl_oop_function_t make_function (hcl_t* hcl, hcl_oow_t lfsize, const hcl_oob_t* bptr, hcl_oow_t blen, hcl_dbgl_t* locptr)
{
	hcl_oop_function_t func;

	/* the literal frame is placed in the variable part.
	 * the byte code is placed in the trailer space */
	func = (hcl_oop_function_t)hcl_allocoopobjwithtrailer(hcl, HCL_BRAND_FUNCTION, HCL_FUNCTION_NAMED_INSTVARS + lfsize, bptr, blen);
	if (HCL_UNLIKELY(!func)) return HCL_NULL;
	
	if (locptr)
	{
		hcl_oop_t tmp;
		hcl_pushvolat (hcl, (hcl_oop_t*)&func);
		tmp = hcl_makebytearray(hcl, (hcl_oob_t*)locptr, HCL_SIZEOF(*locptr) * blen);
		hcl_popvolat (hcl);
		if (tmp) func->dbgi = tmp;
	}

	return func;
}

static HCL_INLINE void fill_function_data (hcl_t* hcl, hcl_oop_function_t func, hcl_ooi_t nargs, hcl_ooi_t ntmprs, hcl_oop_context_t homectx, const hcl_oop_t* lfptr, hcl_oow_t lfsize)
{
	/* Although this function could be integrated into make_function(),
	 * this function has been separated from make_function() to make GC handling simpler */
	hcl_oow_t i;

	HCL_ASSERT (hcl, nargs >= 0 && nargs <= HCL_SMOOI_MAX);
	HCL_ASSERT (hcl, ntmprs >= 0 && ntmprs <= HCL_SMOOI_MAX);
	HCL_ASSERT (hcl, nargs <= ntmprs);

	/* copy literal frames */
	HCL_ASSERT (hcl, lfsize <= HCL_OBJ_GET_SIZE(func) - HCL_FUNCTION_NAMED_INSTVARS);
	for (i = 0; i < lfsize; i++) 
	{
		func->literal_frame[i] = lfptr[i];
	#if 0
		HCL_DEBUG2 (hcl, "literal frame %d => %O\n", (int)i, lfptr[i]);
	#endif
	}

	/* initialize other fields */
	func->home = homectx;
	func->nargs = HCL_SMOOI_TO_OOP(nargs);
	func->ntmprs = HCL_SMOOI_TO_OOP(ntmprs);
}

static HCL_INLINE hcl_oop_block_t make_block (hcl_t* hcl)
{
	/* create a base block used for creation of a block context */
	return (hcl_oop_block_t)hcl_allocoopobj(hcl, HCL_BRAND_BLOCK, HCL_BLOCK_NAMED_INSTVARS);
}

static HCL_INLINE void fill_block_data (hcl_t* hcl, hcl_oop_block_t blk, hcl_ooi_t nargs, hcl_ooi_t ntmprs, hcl_ooi_t ip, hcl_oop_context_t homectx)
{
	HCL_ASSERT (hcl, nargs >= 0 && nargs <= HCL_SMOOI_MAX);
	HCL_ASSERT (hcl, ntmprs >= 0 && ntmprs <= HCL_SMOOI_MAX);
	HCL_ASSERT (hcl, nargs <= ntmprs);
	HCL_ASSERT (hcl, ip >= 0 && nargs <= HCL_SMOOI_MAX);

	blk->home = homectx;
	blk->nargs = HCL_SMOOI_TO_OOP(nargs);
	blk->ntmprs = HCL_SMOOI_TO_OOP(ntmprs);
	blk->ip = HCL_SMOOI_TO_OOP(ip);
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
	hcl->proc_map_used++;
}

static HCL_INLINE void free_pid (hcl_t* hcl, hcl_oop_process_t proc)
{
	hcl_ooi_t pid;

	pid = HCL_OOP_TO_SMOOI(proc->id);
	HCL_ASSERT (hcl, pid < hcl->proc_map_capa);
	HCL_ASSERT (hcl, hcl->proc_map_used > 0);

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
	hcl->proc_map_used--;
}


static hcl_oop_process_t make_process (hcl_t* hcl, hcl_oop_context_t c)
{
	hcl_oop_process_t proc;
	hcl_oow_t stksize;
	hcl_ooi_t total_count;
	hcl_ooi_t suspended_count;

	total_count = HCL_OOP_TO_SMOOI(hcl->processor->total_count);
	if (total_count >= HCL_SMOOI_MAX)
	{
	#if defined(HCL_DEBUG_VM_PROCESSOR)
		HCL_LOG0 (hcl, HCL_LOG_IC | HCL_LOG_FATAL, "Processor - too many processes\n");
	#endif
		hcl_seterrbfmt (hcl, HCL_EPFULL, "maximum number(%zd) of processes reached", HCL_SMOOI_MAX);
		return HCL_NULL;
	}

	if (hcl->proc_map_free_first <= -1 && prepare_to_alloc_pid(hcl) <= -1) return HCL_NULL;

	stksize = hcl->option.dfl_procstk_size;
	if (stksize > HCL_TYPE_MAX(hcl_oow_t) - HCL_PROCESS_NAMED_INSTVARS)
		stksize = HCL_TYPE_MAX(hcl_oow_t) - HCL_PROCESS_NAMED_INSTVARS;

	hcl_pushvolat (hcl, (hcl_oop_t*)&c);
	proc = (hcl_oop_process_t)hcl_allocoopobj(hcl, HCL_BRAND_PROCESS, HCL_PROCESS_NAMED_INSTVARS + stksize);
	hcl_popvolat (hcl);
	if (HCL_UNLIKELY(!proc)) return HCL_NULL;

#if 0
////////////////////
////	HCL_OBJ_SET_FLAGS_PROC (proc, proc_flags); /* a special flag to indicate an object is a process instance */
////////////////////
#endif
	proc->state = HCL_SMOOI_TO_OOP(PROC_STATE_SUSPENDED);

	/* assign a process id to the process */
	alloc_pid (hcl, proc);

	proc->initial_context = c;
	proc->current_context = c;
	proc->sp = HCL_SMOOI_TO_OOP(-1);

	HCL_ASSERT (hcl, (hcl_oop_t)c->sender == hcl->_nil);

#if defined(HCL_DEBUG_VM_PROCESSOR)
	HCL_LOG2 (hcl, HCL_LOG_IC | HCL_LOG_DEBUG, "Processor - process[%zd] **CREATED**->%hs\n", HCL_OOP_TO_SMOOI(proc->id), proc_state_to_string(HCL_OOP_TO_SMOOI(proc->state)));
#endif

	/* a process is created in the SUSPENDED state. chain it to the suspended process list */
	suspended_count = HCL_OOP_TO_SMOOI(hcl->processor->suspended.count);
	HCL_APPEND_TO_OOP_LIST (hcl, &hcl->processor->suspended, hcl_oop_process_t, proc, ps);
	suspended_count++;
	hcl->processor->suspended.count = HCL_SMOOI_TO_OOP(suspended_count);

	total_count++;
	hcl->processor->total_count = HCL_SMOOI_TO_OOP(total_count);

	return proc;
}

static HCL_INLINE void sleep_active_process (hcl_t* hcl, int state)
{
	STORE_ACTIVE_SP(hcl);

	/* store the current active context to the current process.
	 * it is the suspended context of the process to be suspended */
	HCL_ASSERT (hcl, hcl->processor->active != hcl->nil_process);

#if defined(HCL_DEBUG_VM_PROCESSOR)
	HCL_LOG3 (hcl, HCL_LOG_IC | HCL_LOG_DEBUG, "Processor - process[%zd] %hs->%hs in sleep_active_process\n", HCL_OOP_TO_SMOOI(hcl->processor->active->id), proc_state_to_string(HCL_OOP_TO_SMOOI(hcl->processor->active->state)), proc_state_to_string(state));
#endif

	hcl->processor->active->current_context = hcl->active_context;
	hcl->processor->active->state = HCL_SMOOI_TO_OOP(state);
}

static HCL_INLINE void wake_process (hcl_t* hcl, hcl_oop_process_t proc)
{

#if defined(HCL_DEBUG_VM_PROCESSOR)
	HCL_LOG2 (hcl, HCL_LOG_IC | HCL_LOG_DEBUG, "Processor - process[%zd] %hs->RUNNING in wake_process\n", HCL_OOP_TO_SMOOI(proc->id), proc_state_to_string(HCL_OOP_TO_SMOOI(proc->state)));
#endif

	/* activate the given process */
	HCL_ASSERT (hcl, proc->state == HCL_SMOOI_TO_OOP(PROC_STATE_RUNNABLE));
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
	wake_process (hcl, proc);

	hcl->proc_switched = 1;
}

static HCL_INLINE void switch_to_process_from_nil (hcl_t* hcl, hcl_oop_process_t proc)
{
	HCL_ASSERT (hcl, hcl->processor->active == hcl->nil_process);
	wake_process (hcl, proc);
	hcl->proc_switched = 1;
}

static HCL_INLINE hcl_oop_process_t find_next_runnable_process (hcl_t* hcl)
{
	hcl_oop_process_t nrp;

	HCL_ASSERT (hcl, hcl->processor->active->state == HCL_SMOOI_TO_OOP(PROC_STATE_RUNNING));
	nrp = hcl->processor->active->ps.next;
	if ((hcl_oop_t)nrp == hcl->_nil) nrp = hcl->processor->runnable.first;
	return nrp;
}

static HCL_INLINE void switch_to_next_runnable_process (hcl_t* hcl)
{
	hcl_oop_process_t nrp;

	nrp = find_next_runnable_process (hcl);
	if (nrp != hcl->processor->active) switch_to_process (hcl, nrp, PROC_STATE_RUNNABLE);
}

static HCL_INLINE void chain_into_processor (hcl_t* hcl, hcl_oop_process_t proc, int new_state)
{
	/* the process is not scheduled at all. 
	 * link it to the processor's process list. */
	hcl_ooi_t runnable_count;
	hcl_ooi_t suspended_count;

	/*HCL_ASSERT (hcl, (hcl_oop_t)proc->ps.prev == hcl->_nil);
	HCL_ASSERT (hcl, (hcl_oop_t)proc->ps.next == hcl->_nil);*/

	HCL_ASSERT (hcl, proc->state == HCL_SMOOI_TO_OOP(PROC_STATE_SUSPENDED));
	HCL_ASSERT (hcl, new_state == PROC_STATE_RUNNABLE || new_state == PROC_STATE_RUNNING);

#if defined(HCL_DEBUG_VM_PROCESSOR)
	HCL_LOG3 (hcl, HCL_LOG_IC | HCL_LOG_DEBUG, 
		"Processor - process[%zd] %hs->%hs in chain_into_processor\n",
		HCL_OOP_TO_SMOOI(proc->id),
		proc_state_to_string(HCL_OOP_TO_SMOOI(proc->state)),
		proc_state_to_string(new_state));
#endif

	runnable_count = HCL_OOP_TO_SMOOI(hcl->processor->runnable.count);

	HCL_ASSERT (hcl, runnable_count >= 0);

	suspended_count = HCL_OOP_TO_SMOOI(hcl->processor->suspended.count);
	HCL_DELETE_FROM_OOP_LIST (hcl, &hcl->processor->suspended, proc, ps);
	suspended_count--;
	hcl->processor->suspended.count = HCL_SMOOI_TO_OOP(suspended_count);

	/* append to the runnable list */
	HCL_APPEND_TO_OOP_LIST (hcl, &hcl->processor->runnable, hcl_oop_process_t, proc, ps);
	proc->state = HCL_SMOOI_TO_OOP(new_state);

	runnable_count++;
	hcl->processor->runnable.count = HCL_SMOOI_TO_OOP(runnable_count);
}

static HCL_INLINE void unchain_from_processor (hcl_t* hcl, hcl_oop_process_t proc, int new_state)
{
	hcl_ooi_t runnable_count;
	hcl_ooi_t suspended_count;
	hcl_ooi_t total_count;

	HCL_ASSERT (hcl, proc->state == HCL_SMOOI_TO_OOP(PROC_STATE_RUNNING) ||
	                 proc->state == HCL_SMOOI_TO_OOP(PROC_STATE_RUNNABLE) ||
	                 proc->state == HCL_SMOOI_TO_OOP(PROC_STATE_SUSPENDED));

	HCL_ASSERT (hcl, proc->state != HCL_SMOOI_TO_OOP(new_state));

#if defined(HCL_DEBUG_VM_PROCESSOR)
	HCL_LOG3 (hcl, HCL_LOG_IC | HCL_LOG_DEBUG, "Processor - process[%zd] %hs->%hs in unchain_from_processor\n", HCL_OOP_TO_SMOOI(proc->id), proc_state_to_string(HCL_OOP_TO_SMOOI(proc->state)), proc_state_to_string(HCL_OOP_TO_SMOOI(new_state)));
#endif

	if (proc->state == HCL_SMOOI_TO_OOP(PROC_STATE_SUSPENDED))
	{
		suspended_count = HCL_OOP_TO_SMOOI(hcl->processor->suspended.count);
		HCL_ASSERT (hcl, suspended_count > 0);
		HCL_DELETE_FROM_OOP_LIST (hcl, &hcl->processor->suspended, proc, ps);
		suspended_count--;
		hcl->processor->suspended.count = HCL_SMOOI_TO_OOP(suspended_count);
	}
	else
	{
		runnable_count = HCL_OOP_TO_SMOOI(hcl->processor->runnable.count);
		HCL_ASSERT (hcl, runnable_count > 0);
		HCL_DELETE_FROM_OOP_LIST (hcl, &hcl->processor->runnable, proc, ps);
		runnable_count--;
		hcl->processor->runnable.count = HCL_SMOOI_TO_OOP(runnable_count);
		if (runnable_count == 0) hcl->processor->active = hcl->nil_process;
	}

	if (new_state == PROC_STATE_TERMINATED)
	{
		/* do not chain it to the suspended process list as it's being terminated */
		proc->ps.prev = (hcl_oop_process_t)hcl->_nil;
		proc->ps.next = (hcl_oop_process_t)hcl->_nil;

		total_count = HCL_OOP_TO_SMOOI(hcl->processor->total_count);
		total_count--;
		hcl->processor->total_count = HCL_SMOOI_TO_OOP(total_count);
	}
	else
	{
		/* append to the suspended process list */
		HCL_ASSERT (hcl, new_state == PROC_STATE_SUSPENDED);

		suspended_count = HCL_OOP_TO_SMOOI(hcl->processor->suspended.count);
		HCL_APPEND_TO_OOP_LIST (hcl, &hcl->processor->suspended, hcl_oop_process_t, proc, ps);
		suspended_count++;
		hcl->processor->suspended.count= HCL_SMOOI_TO_OOP(suspended_count);
	}

	proc->state = HCL_SMOOI_TO_OOP(new_state);
}

static HCL_INLINE void chain_into_semaphore (hcl_t* hcl, hcl_oop_process_t proc, hcl_oop_semaphore_t sem)
{
	/* append a process to the process list of a semaphore or a semaphore group */

	/* a process chained to a semaphore cannot get chained to
	 * a semaphore again. a process can get chained to a single semaphore 
	 * or a single semaphore group only */
	if ((hcl_oop_t)proc->sem != hcl->_nil) return; /* ignore it if it happens anyway. TODO: is it desirable???? */

	HCL_ASSERT (hcl, (hcl_oop_t)proc->sem == hcl->_nil);
	HCL_ASSERT (hcl, (hcl_oop_t)proc->sem_wait.prev == hcl->_nil);
	HCL_ASSERT (hcl, (hcl_oop_t)proc->sem_wait.next == hcl->_nil);

	/* a semaphore or a semaphore group must be given for process chaining */
	HCL_ASSERT (hcl, HCL_IS_SEMAPHORE(hcl, sem) || HCL_IS_SEMAPHORE_GROUP(hcl, sem));

	/* i assume the head part of the semaphore has the same layout as
	 * the semaphore group */
	HCL_ASSERT (hcl, HCL_OFFSETOF(hcl_semaphore_t,waiting) ==
	                 HCL_OFFSETOF(hcl_semaphore_group_t,waiting));

	HCL_APPEND_TO_OOP_LIST (hcl, &sem->waiting, hcl_oop_process_t, proc, sem_wait);

	proc->sem = (hcl_oop_t)sem;
}

static HCL_INLINE void unchain_from_semaphore (hcl_t* hcl, hcl_oop_process_t proc)
{
	hcl_oop_semaphore_t sem;

	HCL_ASSERT (hcl, (hcl_oop_t)proc->sem != hcl->_nil);
	HCL_ASSERT (hcl, HCL_IS_SEMAPHORE(hcl, proc->sem) || HCL_IS_SEMAPHORE_GROUP(hcl, proc->sem));
	HCL_ASSERT (hcl, HCL_OFFSETOF(hcl_semaphore_t,waiting) == HCL_OFFSETOF(hcl_semaphore_group_t,waiting));

	/* proc->sem may be one of a semaphore or a semaphore group.
	 * i assume that 'waiting' is defined to the same position 
	 * in both Semaphore and SemaphoreGroup. there is no need to 
	 * write different code for each class. */
	sem = (hcl_oop_semaphore_t)proc->sem;  /* semgrp = (hcl_oop_semaphore_group_t)proc->sem */
	HCL_DELETE_FROM_OOP_LIST (hcl, &sem->waiting, proc, sem_wait); 

	proc->sem_wait.prev = (hcl_oop_process_t)hcl->_nil;
	proc->sem_wait.next = (hcl_oop_process_t)hcl->_nil;
	proc->sem = hcl->_nil;
}

static void dump_process_info (hcl_t* hcl, hcl_bitmask_t log_mask)
{
	if (HCL_OOP_TO_SMOOI(hcl->processor->runnable.count) > 0)
	{
		hcl_oop_process_t p;
		HCL_LOG0 (hcl, log_mask, "> Runnable:");
		p = hcl->processor->runnable.first;
		while (p)
		{
			HCL_LOG1 (hcl, log_mask, " %O", p->id);
			if (p == hcl->processor->runnable.last) break;
			p = p->ps.next;
		}
		HCL_LOG0 (hcl, log_mask, "\n");
	}
	if (HCL_OOP_TO_SMOOI(hcl->processor->suspended.count) > 0)
	{
		hcl_oop_process_t p;
		HCL_LOG0 (hcl, log_mask, "> Suspended:");
		p = hcl->processor->suspended.first;
		while (p)
		{
			HCL_LOG1 (hcl, log_mask, " %O", p->id);
			if (p == hcl->processor->suspended.last) break;
			p = p->ps.next;
		}
		HCL_LOG0 (hcl, log_mask, "\n");
	}
	if (hcl->sem_io_wait_count > 0)
	{
		hcl_ooi_t io_handle;

		HCL_LOG0 (hcl, log_mask, "> IO semaphores:");
		for (io_handle = 0; io_handle < hcl->sem_io_map_capa; io_handle++)
		{
			hcl_ooi_t index;

			index = hcl->sem_io_map[io_handle];
			if (index >= 0)
			{
				hcl_oop_semaphore_t sem;

				HCL_LOG1 (hcl, log_mask, " h=%zd", io_handle);

				/* dump process IDs waiting for input signaling */
				HCL_LOG0 (hcl, log_mask, "(wpi");
				sem = hcl->sem_io_tuple[index].sem[HCL_SEMAPHORE_IO_TYPE_INPUT];
				if (sem) 
				{
					hcl_oop_process_t wp; /* waiting process */
					for (wp = sem->waiting.first; (hcl_oop_t)wp != hcl->_nil; wp = wp->sem_wait.next)
					{
						HCL_LOG1 (hcl, log_mask, ":%zd", HCL_OOP_TO_SMOOI(wp->id));
					}
				}
				else
				{
					HCL_LOG0 (hcl, log_mask, ":none");
				}

				/* dump process IDs waitingt for output signaling */
				HCL_LOG0 (hcl, log_mask, ",wpo");
				sem = hcl->sem_io_tuple[index].sem[HCL_SEMAPHORE_IO_TYPE_OUTPUT];
				if (sem) 
				{
					hcl_oop_process_t wp; /* waiting process */
					for (wp = sem->waiting.first; (hcl_oop_t)wp != hcl->_nil; wp = wp->sem_wait.next)
					{
						HCL_LOG1 (hcl, log_mask, ":%zd", HCL_OOP_TO_SMOOI(wp->id));
					}
				}
				else
				{
					HCL_LOG0 (hcl, log_mask, ":none");
				}

				HCL_LOG0 (hcl, log_mask, ")");
			}
		}
		HCL_LOG0 (hcl, log_mask, "\n");
	}
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

			nrp = find_next_runnable_process(hcl);

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
				if (HCL_LOG_ENABLED(hcl, HCL_LOG_IC | HCL_LOG_DEBUG))
				{
					HCL_LOG5 (hcl, HCL_LOG_IC | HCL_LOG_DEBUG, 
						"No runnable process after termination of process %zd - total %zd runnable/running %zd suspended %zd - sem_io_wait_count %zu\n", 
						HCL_OOP_TO_SMOOI(proc->id),
						HCL_OOP_TO_SMOOI(hcl->processor->total_count),
						HCL_OOP_TO_SMOOI(hcl->processor->runnable.count),
						HCL_OOP_TO_SMOOI(hcl->processor->suspended.count),
						hcl->sem_io_wait_count
					);

					dump_process_info (hcl, HCL_LOG_IC | HCL_LOG_DEBUG);
				}
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

		/*proc->state = HCL_SMOOI_TO_OOP(PROC_STATE_TERMINATED);*/
		unchain_from_processor (hcl, proc, PROC_STATE_TERMINATED);
		proc->sp = HCL_SMOOI_TO_OOP(-1); /* invalidate the proce stack */

		if ((hcl_oop_t)proc->sem != hcl->_nil)
		{
			if (HCL_IS_SEMAPHORE_GROUP(hcl, proc->sem))
			{
				if (HCL_OOP_TO_SMOOI(((hcl_oop_semaphore_group_t)proc->sem)->sem_io_count) > 0)
				{
					HCL_ASSERT (hcl, hcl->sem_io_wait_count > 0);
					hcl->sem_io_wait_count--;
					HCL_DEBUG1 (hcl, "terminate_process(sg) - lowered sem_io_wait_count to %zu\n", hcl->sem_io_wait_count);
				}
			}
			else 
			{
				HCL_ASSERT (hcl, HCL_IS_SEMAPHORE(hcl, proc->sem));
				if (((hcl_oop_semaphore_t)proc->sem)->subtype == HCL_SMOOI_TO_OOP(HCL_SEMAPHORE_SUBTYPE_IO))
				{
					HCL_ASSERT (hcl, hcl->sem_io_wait_count > 0);
					hcl->sem_io_wait_count--;
					HCL_DEBUG3 (hcl, "terminate_process(s) - lowered sem_io_wait_count to %zu for IO semaphore at index %zd handle %zd\n",
						hcl->sem_io_wait_count, 
						HCL_OOP_TO_SMOOI(((hcl_oop_semaphore_t)proc->sem)->u.io.index),
						HCL_OOP_TO_SMOOI(((hcl_oop_semaphore_t)proc->sem)->u.io.handle)
					);
				}
			}

			unchain_from_semaphore (hcl, proc);
		}

		/* when terminated, clear it from the pid table and set the process id to a negative number */
		free_pid (hcl, proc);
	}
#if 0
	else if (proc->state == HCL_SMOOI_TO_OOP(PROC_STATE_WAITING))
	{
		/* WAITING ---> TERMINATED */
		/* TODO: */
	}
#endif
}

static void resume_process (hcl_t* hcl, hcl_oop_process_t proc)
{
	if (proc->state == HCL_SMOOI_TO_OOP(PROC_STATE_SUSPENDED))
	{
		/* SUSPENDED ---> RUNNABLE */
		/*HCL_ASSERT (hcl, (hcl_oop_t)proc->ps.prev == hcl->_nil);
		HCL_ASSERT (hcl, (hcl_oop_t)proc->ps.next == hcl->_nil);*/

	#if defined(HCL_DEBUG_VM_PROCESSOR)
		HCL_LOG2 (hcl, HCL_LOG_IC | HCL_LOG_DEBUG, "Processor - process[%zd] %hs->RUNNABLE in resume_process\n", HCL_OOP_TO_SMOOI(proc->id), proc_state_to_string(HCL_OOP_TO_SMOOI(proc->state)));
	#endif

		/* don't switch to this process. just change the state to RUNNABLE.
		 * process switching should be triggerd by the process scheduler. */
		chain_into_processor (hcl, proc, PROC_STATE_RUNNABLE); 
		/*HCL_STORE_OOP (hcl, &proc->current_context = proc->initial_context);*/
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
		HCL_LOG2 (hcl, HCL_LOG_IC | HCL_LOG_DEBUG, "Processor - process[%zd] %hs->SUSPENDED in suspend_process\n", HCL_OOP_TO_SMOOI(proc->id), proc_state_to_string(HCL_OOP_TO_SMOOI(proc->state)));
	#endif

		if (proc == hcl->processor->active)
		{
			/* suspend the active process */
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
				/* unchain_from_processor moves the process to the suspended
				 * process and sets its state to the given state(SUSPENDED here).
				 * it doesn't change the active process. we switch the active
				 * process with switch_to_process(). setting the state of the
				 * old active process to SUSPENDED is redundant because it's
				 * done in unchain_from_processor(). the state of the active
				 * process is somewhat wrong for a short period of time until
				 * switch_to_process() has changed the active process. */
				unchain_from_processor (hcl, proc, PROC_STATE_SUSPENDED);
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
			HCL_LOG2 (hcl, HCL_LOG_IC | HCL_LOG_DEBUG, "Processor - process[%zd] %hs->RUNNABLE in yield_process\n", HCL_OOP_TO_SMOOI(proc->id), proc_state_to_string(HCL_OOP_TO_SMOOI(proc->state)));
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
		tmp = (hcl_oop_semaphore_t*)hcl_reallocmem (hcl, hcl->sem_list, HCL_SIZEOF(hcl_oop_semaphore_t) * new_capa);
		if (HCL_UNLIKELY(!tmp)) return -1;

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
	hcl_oop_semaphore_group_t sg;

	sg = sem->group;
	if ((hcl_oop_t)sg != hcl->_nil)
	{
		/* the semaphore belongs to a semaphore group */
		if ((hcl_oop_t)sg->waiting.first != hcl->_nil)
		{
			hcl_ooi_t sp;

			/* there is a process waiting on the process group */
			proc = sg->waiting.first; /* will wake the first process in the waiting list */

			unchain_from_semaphore (hcl, proc);
			resume_process (hcl, proc);

			/* [IMPORTANT] RETURN VALUE of SemaphoreGroup's wait.
			 * ------------------------------------------------------------
			 * the waiting process has been suspended after a waiting
			 * primitive function in Semaphore or SemaphoreGroup.
			 * the top of the stack of the process must hold the temporary 
			 * return value set by await_semaphore() or await_semaphore_group().
			 * change the return value forcibly to the actual signaled 
			 * semaphore */
			HCL_ASSERT (hcl, HCL_OOP_TO_SMOOI(proc->sp) < (hcl_ooi_t)(HCL_OBJ_GET_SIZE(proc) - HCL_PROCESS_NAMED_INSTVARS));
			sp = HCL_OOP_TO_SMOOI(proc->sp);
			proc->slot[sp] = (hcl_oop_t)sem;

			/* i should decrement the counter as long as the group being 
			 * signaled contains an IO semaphore */
			if (HCL_OOP_TO_SMOOI(sg->sem_io_count) > 0) 
			{
				HCL_ASSERT (hcl, hcl->sem_io_wait_count > 0);
				hcl->sem_io_wait_count--;
				HCL_DEBUG2 (hcl, "signal_semaphore(sg) - lowered sem_io_wait_count to %zu for handle %zd\n", hcl->sem_io_wait_count, HCL_OOP_TO_SMOOI(sem->u.io.handle));
			}
			return proc;
		}
	}

	/* if the semaphore belongs to a semaphore group and the control reaches 
	 * here, no process is waiting on the semaphore group. however, a process
	 * may still be waiting on the semaphore. If a process waits on a semaphore
	 * group and another process wait on a semaphore that belongs to the 
	 * semaphore group, the process waiting on the group always wins. 
	 * 
	 *    TODO: implement a fair scheduling policy. or do i simply have to disallow individual wait on a semaphore belonging to a group?
	 *       
	 * if it doesn't belong to a sempahore group, i'm free from the starvation issue.
	 */
	if ((hcl_oop_t)sem->waiting.first == hcl->_nil)
	{
		/* no process is waiting on this semaphore */

		count = HCL_OOP_TO_SMOOI(sem->count);
		count++;
		sem->count = HCL_SMOOI_TO_OOP(count);

		HCL_ASSERT (hcl, count >= 1);
		if (count == 1 && (hcl_oop_t)sg != hcl->_nil)
		{
			/* move the semaphore from the unsignaled list to the signaled list
			 * if the semaphore count has changed from 0 to 1 in a group */
			HCL_DELETE_FROM_OOP_LIST (hcl, &sg->sems[HCL_SEMAPHORE_GROUP_SEMS_UNSIG], sem, grm);
			HCL_APPEND_TO_OOP_LIST (hcl, &sg->sems[HCL_SEMAPHORE_GROUP_SEMS_SIG], hcl_oop_semaphore_t, sem, grm);
		}

		/* no process has been resumed */
		return (hcl_oop_process_t)hcl->_nil;
	}
	else
	{
		proc = sem->waiting.first;

		/* [NOTE] no GC must occur as 'proc' isn't protected with hcl_pushvolat(). */

		/* detach a process from a semaphore's waiting list and 
		 * make it runnable */
		unchain_from_semaphore (hcl, proc);
		resume_process (hcl, proc);

		if (sem->subtype == HCL_SMOOI_TO_OOP(HCL_SEMAPHORE_SUBTYPE_IO)) 
		{
			HCL_ASSERT (hcl, hcl->sem_io_wait_count > 0);
			hcl->sem_io_wait_count--;
			HCL_DEBUG3 (hcl, "signal_semaphore(s) - lowered sem_io_wait_count to %zu for IO semaphore at index %zd handle %zd\n",
				hcl->sem_io_wait_count, HCL_OOP_TO_SMOOI(sem->u.io.index), HCL_OOP_TO_SMOOI(sem->u.io.handle));
		}

		/* return the resumed(runnable) process */
		return proc;
	}
}

static HCL_INLINE int can_await_semaphore (hcl_t* hcl, hcl_oop_semaphore_t sem)
{
	/* a sempahore that doesn't belong to a gruop can be waited on */
	return (hcl_oop_t)sem->group == hcl->_nil;
}

static HCL_INLINE void await_semaphore (hcl_t* hcl, hcl_oop_semaphore_t sem)
{
	hcl_oop_process_t proc;
	hcl_ooi_t count;
	hcl_oop_semaphore_group_t semgrp;

	semgrp = sem->group;

	/* the caller of this function must ensure that the semaphore doesn't belong to a group */
	HCL_ASSERT (hcl, (hcl_oop_t)semgrp == hcl->_nil);

	count = HCL_OOP_TO_SMOOI(sem->count);
	if (count > 0)
	{
		/* it's already signaled */
		count--;
		sem->count = HCL_SMOOI_TO_OOP(count);

		if ((hcl_oop_t)semgrp != hcl->_nil && count == 0)
		{

			int sems_idx;
			/* TODO: if i disallow individual wait on a semaphore in a group,
			 *       this membership manipulation is redundant */
			HCL_DELETE_FROM_OOP_LIST (hcl, &semgrp->sems[HCL_SEMAPHORE_GROUP_SEMS_SIG], sem, grm);
			sems_idx = count > 0? HCL_SEMAPHORE_GROUP_SEMS_SIG: HCL_SEMAPHORE_GROUP_SEMS_UNSIG;
			HCL_APPEND_TO_OOP_LIST (hcl, &semgrp->sems[sems_idx], hcl_oop_semaphore_t, sem, grm);
		}
	}
	else
	{
		/* not signaled. need to wait */
		proc = hcl->processor->active;

		/* suspend the active process */
		suspend_process (hcl, proc); 

		/* link the suspended process to the semaphore's process list */
		chain_into_semaphore (hcl, proc, sem); 

		HCL_ASSERT (hcl, sem->waiting.last == proc);

		if (sem->subtype == HCL_SMOOI_TO_OOP(HCL_SEMAPHORE_SUBTYPE_IO)) 
		{
			hcl->sem_io_wait_count++;
			HCL_DEBUG3 (hcl, "await_semaphore - raised sem_io_wait_count to %zu for IO semaphore at index %zd handle %zd\n",
				hcl->sem_io_wait_count, HCL_OOP_TO_SMOOI(sem->u.io.index), HCL_OOP_TO_SMOOI(sem->u.io.handle));
		}

		HCL_ASSERT (hcl, hcl->processor->active != proc);
	}
}

static HCL_INLINE hcl_oop_t await_semaphore_group (hcl_t* hcl, hcl_oop_semaphore_group_t semgrp)
{
	/* wait for one of semaphores in the group to be signaled */

	hcl_oop_process_t proc;
	hcl_oop_semaphore_t sem;

	HCL_ASSERT (hcl, HCL_IS_SEMAPHORE_GROUP(hcl, semgrp));

	if (HCL_OOP_TO_SMOOI(semgrp->sem_count) <= 0)
	{
		/* cannot wait on a semaphore group that has no member semaphores.
		 * return failure if waiting on such a semapohre group is attempted */
		HCL_ASSERT (hcl, (hcl_oop_t)semgrp->sems[HCL_SEMAPHORE_GROUP_SEMS_SIG].first == hcl->_nil);
		HCL_ASSERT (hcl, (hcl_oop_t)semgrp->sems[HCL_SEMAPHORE_GROUP_SEMS_SIG].last == hcl->_nil);
		return HCL_ERROR_TO_OOP(HCL_EINVAL); /* TODO: better error code? */
	}

	sem = semgrp->sems[HCL_SEMAPHORE_GROUP_SEMS_SIG].first;
	if ((hcl_oop_t)sem != hcl->_nil)
	{
		hcl_ooi_t count;
		int sems_idx;

		/* there is a semaphore signaled in the group */
		count = HCL_OOP_TO_SMOOI(sem->count);
		HCL_ASSERT (hcl, count > 0);
		count--;
		sem->count = HCL_SMOOI_TO_OOP(count);

		HCL_DELETE_FROM_OOP_LIST (hcl, &semgrp->sems[HCL_SEMAPHORE_GROUP_SEMS_SIG], sem, grm);
		sems_idx = count > 0? HCL_SEMAPHORE_GROUP_SEMS_SIG: HCL_SEMAPHORE_GROUP_SEMS_UNSIG;
		HCL_APPEND_TO_OOP_LIST (hcl, &semgrp->sems[sems_idx], hcl_oop_semaphore_t, sem, grm);

		return (hcl_oop_t)sem;
	}

	/* no semaphores have been signaled. suspend the current process
	 * until at least one of them is signaled */
	proc = hcl->processor->active;

	/* suspend the active process */
	suspend_process (hcl, proc); 

	/* link the suspended process to the semaphore group's process list */
	chain_into_semaphore (hcl, proc, (hcl_oop_semaphore_t)semgrp); 

	HCL_ASSERT (hcl, semgrp->waiting.last == proc);

	if (HCL_OOP_TO_SMOOI(semgrp->sem_io_count) > 0) 
	{
		/* there might be more than 1 IO semaphores in the group
		 * but i increment hcl->sem_io_wait_count by 1 only */
		hcl->sem_io_wait_count++; 
		HCL_DEBUG1 (hcl, "await_semaphore_group - raised sem_io_wait_count to %zu\n", hcl->sem_io_wait_count);
	}

	/* the current process will get suspended after the caller (mostly a 
	 * a primitive function handler) is over as it's added to a suspened
	 * process list above */
	HCL_ASSERT (hcl, hcl->processor->active != proc);
	return hcl->_nil; 
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
				parsem->u.timed.index = HCL_SMOOI_TO_OOP(index);
				hcl->sem_heap[index] = parsem;

				/* traverse up */
				index = parent;
				if (index <= 0) break;

				parent = SEM_HEAP_PARENT(parent);
				parsem = hcl->sem_heap[parent];
			}
			while (SEM_HEAP_EARLIER_THAN(hcl, sem, parsem));

			sem->u.timed.index = HCL_SMOOI_TO_OOP(index);
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

			if (right < hcl->sem_heap_count && SEM_HEAP_EARLIER_THAN(hcl, hcl->sem_heap[right], hcl->sem_heap[left]))
			{
				child = right;
			}
			else
			{
				child = left;
			}

			chisem = hcl->sem_heap[child];
			if (SEM_HEAP_EARLIER_THAN(hcl, sem, chisem)) break;

			chisem->u.timed.index = HCL_SMOOI_TO_OOP(index);
			hcl->sem_heap[index] = chisem;

			index = child;
		}
		while (index < base);

		sem->u.timed.index = HCL_SMOOI_TO_OOP(index);
		hcl->sem_heap[index] = sem;
	}
}

static int add_to_sem_heap (hcl_t* hcl, hcl_oop_semaphore_t sem)
{
	hcl_ooi_t index;

	HCL_ASSERT (hcl, sem->subtype == hcl->_nil);

	if (hcl->sem_heap_count >= SEM_HEAP_MAX)
	{
		hcl_seterrbfmt(hcl, HCL_ESEMFLOOD, "too many semaphores in the semaphore heap");
		return -1;
	}

	if (hcl->sem_heap_count >= hcl->sem_heap_capa)
	{
		hcl_oow_t new_capa;
		hcl_oop_semaphore_t* tmp;

		/* no overflow check when calculating the new capacity
		 * owing to SEM_HEAP_MAX check above */
		new_capa = hcl->sem_heap_capa + SEM_HEAP_INC;
		tmp = (hcl_oop_semaphore_t*)hcl_reallocmem(hcl, hcl->sem_heap, HCL_SIZEOF(hcl_oop_semaphore_t) * new_capa);
		if (HCL_UNLIKELY(!tmp)) return -1;

		hcl->sem_heap = tmp;
		hcl->sem_heap_capa = new_capa;
	}

	HCL_ASSERT (hcl, hcl->sem_heap_count <= HCL_SMOOI_MAX);

	index = hcl->sem_heap_count;
	hcl->sem_heap[index] = sem;
	sem->u.timed.index = HCL_SMOOI_TO_OOP(index);
	sem->subtype = HCL_SMOOI_TO_OOP(HCL_SEMAPHORE_SUBTYPE_TIMED);
	hcl->sem_heap_count++;

	sift_up_sem_heap (hcl, index);
	return 0;
}

static void delete_from_sem_heap (hcl_t* hcl, hcl_ooi_t index)
{
	hcl_oop_semaphore_t sem, lastsem;

	HCL_ASSERT (hcl, index >= 0 && index < hcl->sem_heap_count);

	sem = hcl->sem_heap[index];

	sem->subtype = hcl->_nil;
	sem->u.timed.index = hcl->_nil;
	sem->u.timed.ftime_sec = hcl->_nil;
	sem->u.timed.ftime_nsec = hcl->_nil;

	hcl->sem_heap_count--;
	if (/*hcl->sem_heap_count > 0 &&*/ index != hcl->sem_heap_count)
	{
		/* move the last item to the deletion position */
		lastsem = hcl->sem_heap[hcl->sem_heap_count];
		lastsem->u.timed.index = HCL_SMOOI_TO_OOP(index);
		hcl->sem_heap[index] = lastsem;

		if (SEM_HEAP_EARLIER_THAN(hcl, lastsem, sem)) 
			sift_up_sem_heap (hcl, index);
		else
			sift_down_sem_heap (hcl, index);
	}
}

#if 0
/* unused */
static void update_sem_heap (hcl_t* hcl, hcl_ooi_t index, hcl_oop_semaphore_t newsem)
{
	hcl_oop_semaphore_t sem;

	sem = hcl->sem_heap[index];
	sem->timed.index = hcl->_nil;

	newsem->timed.index = HCL_SMOOI_TO_OOP(index);
	hcl->sem_heap[index] = newsem;

	if (SEM_HEAP_EARLIER_THAN(hcl, newsem, sem))
		sift_up_sem_heap (hcl, index);
	else
		sift_down_sem_heap (hcl, index);
}
#endif

static int add_sem_to_sem_io_tuple (hcl_t* hcl, hcl_oop_semaphore_t sem, hcl_ooi_t io_handle, hcl_semaphore_io_type_t io_type)
{
	hcl_ooi_t index;
	hcl_ooi_t new_mask;
	int n, tuple_added = 0;

	HCL_ASSERT (hcl, sem->subtype == (hcl_oop_t)hcl->_nil);
	HCL_ASSERT (hcl, sem->u.io.index == (hcl_oop_t)hcl->_nil);
	/*HCL_ASSERT (hcl, sem->io.handle == (hcl_oop_t)hcl->_nil);
	HCL_ASSERT (hcl, sem->io.type == (hcl_oop_t)hcl->_nil);*/

	if (io_handle < 0)
	{
		hcl_seterrbfmt (hcl, HCL_EINVAL, "handle %zd out of supported range", io_handle);
		return -1;
	}
	
	if (io_handle >= hcl->sem_io_map_capa)
	{
		hcl_oow_t new_capa, i;
		hcl_ooi_t* tmp;

/* TODO: specify the maximum io_handle supported and check it here instead of just relying on memory allocation success/failure? */
		new_capa = HCL_ALIGN_POW2(io_handle + 1, SEM_IO_MAP_ALIGN);

		tmp = hcl_reallocmem (hcl, hcl->sem_io_map, HCL_SIZEOF(*tmp) * new_capa);
		if (!tmp) 
		{
			const hcl_ooch_t* oldmsg = hcl_backuperrmsg(hcl);
			hcl_seterrbfmt (hcl, hcl->errnum, "handle %zd out of supported range - %js", oldmsg);
			return -1;
		}

		for (i = hcl->sem_io_map_capa; i < new_capa; i++) tmp[i] = -1;

		hcl->sem_io_map = tmp;
		hcl->sem_io_map_capa = new_capa;
	}

	index = hcl->sem_io_map[io_handle];
	if (index <= -1)
	{
		/* this handle is not in any tuples. add it to a new tuple */
		if (hcl->sem_io_tuple_count >= SEM_IO_TUPLE_MAX)
		{
			hcl_seterrbfmt (hcl, HCL_ESEMFLOOD, "too many IO semaphore tuples"); 
			return -1;
		}

		if (hcl->sem_io_tuple_count >= hcl->sem_io_tuple_capa)
		{
			hcl_oow_t new_capa;
			hcl_sem_tuple_t* tmp;

			/* no overflow check when calculating the new capacity
			 * owing to SEM_IO_TUPLE_MAX check above */
			new_capa = hcl->sem_io_tuple_capa + SEM_IO_TUPLE_INC;
			tmp = hcl_reallocmem (hcl, hcl->sem_io_tuple, HCL_SIZEOF(hcl_sem_tuple_t) * new_capa);
			if (!tmp) return -1;

			hcl->sem_io_tuple = tmp;
			hcl->sem_io_tuple_capa = new_capa;
		}

		/* this condition must be true assuming SEM_IO_TUPLE_MAX <= HCL_SMOOI_MAX */
		HCL_ASSERT (hcl, hcl->sem_io_tuple_count <= HCL_SMOOI_MAX); 
		index = hcl->sem_io_tuple_count;

		tuple_added = 1;

		/* safe to initialize before vm_muxadd() because
		 * hcl->sem_io_tuple_count has not been incremented. 
		 * still no impact even if it fails. */
		hcl->sem_io_tuple[index].sem[HCL_SEMAPHORE_IO_TYPE_INPUT] = HCL_NULL;
		hcl->sem_io_tuple[index].sem[HCL_SEMAPHORE_IO_TYPE_OUTPUT] = HCL_NULL;
		hcl->sem_io_tuple[index].handle = io_handle;
		hcl->sem_io_tuple[index].mask = 0;

		new_mask = ((hcl_ooi_t)1 << io_type);

		hcl_pushvolat (hcl, (hcl_oop_t*)&sem);
		n = hcl->vmprim.vm_muxadd(hcl, io_handle, new_mask);
		hcl_popvolat (hcl);
	}
	else
	{
		if (hcl->sem_io_tuple[index].sem[io_type])
		{
			hcl_seterrbfmt (hcl, HCL_EINVAL, "handle %zd already linked with an IO semaphore for %hs", io_handle, io_type_str[io_type]);
			return -1;
		}

		new_mask = hcl->sem_io_tuple[index].mask; /* existing mask */
		new_mask |= ((hcl_ooi_t)1 << io_type);

		hcl_pushvolat (hcl, (hcl_oop_t*)&sem);
		n = hcl->vmprim.vm_muxmod(hcl, io_handle, new_mask);
		hcl_popvolat (hcl);
	}

	if (n <= -1) 
	{
		HCL_LOG3 (hcl, HCL_LOG_WARN, "Failed to add IO semaphore at index %zd for %hs on handle %zd\n", index, io_type_str[io_type], io_handle);
		return -1;
	}

	HCL_LOG3 (hcl, HCL_LOG_DEBUG, "Added IO semaphore at index %zd for %hs on handle %zd\n", index, io_type_str[io_type], io_handle);

	sem->subtype = HCL_SMOOI_TO_OOP(HCL_SEMAPHORE_SUBTYPE_IO);
	sem->u.io.index = HCL_SMOOI_TO_OOP(index);
	sem->u.io.handle = HCL_SMOOI_TO_OOP(io_handle);
	sem->u.io.type = HCL_SMOOI_TO_OOP((hcl_ooi_t)io_type);

	hcl->sem_io_tuple[index].handle = io_handle;
	hcl->sem_io_tuple[index].mask = new_mask;
	hcl->sem_io_tuple[index].sem[io_type] = sem;

	hcl->sem_io_count++;
	if (tuple_added) 
	{
		hcl->sem_io_tuple_count++;
		hcl->sem_io_map[io_handle] = index;
	}

	/* update the number of IO semaphores in a group if necessary */
	if ((hcl_oop_t)sem->group != hcl->_nil)
	{
		hcl_ooi_t count;
		count = HCL_OOP_TO_SMOOI(sem->group->sem_io_count);
		count++;
		sem->group->sem_io_count = HCL_SMOOI_TO_OOP(count);
	}

	return 0;
}

static int delete_sem_from_sem_io_tuple (hcl_t* hcl, hcl_oop_semaphore_t sem, int force)
{
	hcl_ooi_t index;
	hcl_ooi_t new_mask, io_handle, io_type;
	int x;

	HCL_ASSERT (hcl, sem->subtype == HCL_SMOOI_TO_OOP(HCL_SEMAPHORE_SUBTYPE_IO));
	HCL_ASSERT (hcl, HCL_OOP_IS_SMOOI(sem->u.io.type));
	HCL_ASSERT (hcl, HCL_OOP_IS_SMOOI(sem->u.io.index));
	HCL_ASSERT (hcl, HCL_OOP_IS_SMOOI(sem->u.io.handle));

	index = HCL_OOP_TO_SMOOI(sem->u.io.index);
	HCL_ASSERT (hcl, index >= 0 && index < hcl->sem_io_tuple_count);

	io_handle = HCL_OOP_TO_SMOOI(sem->u.io.handle);
	if (io_handle < 0 || io_handle >= hcl->sem_io_map_capa)
	{
		hcl_seterrbfmt (hcl, HCL_EINVAL, "handle %zd out of supported range", io_handle);
		return -1;
	}
	HCL_ASSERT (hcl, hcl->sem_io_map[io_handle] == HCL_OOP_TO_SMOOI(sem->u.io.index));

	io_type = HCL_OOP_TO_SMOOI(sem->u.io.type);

	new_mask = hcl->sem_io_tuple[index].mask;
	new_mask &= ~((hcl_ooi_t)1 << io_type); /* this is the new mask after deletion */

	hcl_pushvolat (hcl, (hcl_oop_t*)&sem);
	x = new_mask? hcl->vmprim.vm_muxmod(hcl, io_handle, new_mask):
	              hcl->vmprim.vm_muxdel(hcl, io_handle); 
	hcl_popvolat (hcl);
	if (x <= -1) 
	{
		HCL_LOG3 (hcl, HCL_LOG_WARN, "Failed to delete IO semaphore at index %zd handle %zd for %hs\n", index, io_handle, io_type_str[io_type]);
		if (!force) return -1;

		/* [NOTE] 
		 *   this means there could be some issue handling the file handles.
		 *   the file handle might have been closed before reaching here.
		 *   assuming the callback works correctly, it's not likely that the
		 *   underlying operating system returns failure for no reason.
		 *   i should inspect the overall vm implementation */
		HCL_LOG1 (hcl, HCL_LOG_ERROR, "Forcibly unmapping the IO semaphored handle %zd as if it's deleted\n", io_handle);
	}
	else
	{
		HCL_LOG3 (hcl, HCL_LOG_DEBUG, "Deleted IO semaphore at index %zd handle %zd for %hs\n", index, io_handle, io_type_str[io_type]);
	}

	sem->subtype = hcl->_nil;
	sem->u.io.index = hcl->_nil;
	sem->u.io.handle = hcl->_nil;
	sem->u.io.type = hcl->_nil;
	hcl->sem_io_count--;

	if ((hcl_oop_t)sem->group != hcl->_nil)
	{
		hcl_ooi_t count;
		count = HCL_OOP_TO_SMOOI(sem->group->sem_io_count);
		HCL_ASSERT (hcl, count > 0);
		count--;
		sem->group->sem_io_count = HCL_SMOOI_TO_OOP(count);
	}

	if (new_mask)
	{
		hcl->sem_io_tuple[index].mask = new_mask;
		hcl->sem_io_tuple[index].sem[io_type] = HCL_NULL;
	}
	else
	{
		hcl->sem_io_tuple_count--;

		if (/*hcl->sem_io_tuple_count > 0 &&*/ index != hcl->sem_io_tuple_count)
		{
			/* migrate the last item to the deleted slot to compact the gap */
			hcl->sem_io_tuple[index] = hcl->sem_io_tuple[hcl->sem_io_tuple_count];

			if (hcl->sem_io_tuple[index].sem[HCL_SEMAPHORE_IO_TYPE_INPUT]) 
				hcl->sem_io_tuple[index].sem[HCL_SEMAPHORE_IO_TYPE_INPUT]->u.io.index = HCL_SMOOI_TO_OOP(index);
			if (hcl->sem_io_tuple[index].sem[HCL_SEMAPHORE_IO_TYPE_OUTPUT])
				hcl->sem_io_tuple[index].sem[HCL_SEMAPHORE_IO_TYPE_OUTPUT]->u.io.index = HCL_SMOOI_TO_OOP(index);

			hcl->sem_io_map[hcl->sem_io_tuple[index].handle] = index;

			HCL_LOG2 (hcl, HCL_LOG_DEBUG, "Migrated IO semaphore tuple from index %zd to %zd\n", hcl->sem_io_tuple_count, index);
		}

		hcl->sem_io_map[io_handle] = -1;
	}

	return 0;
}

static void _signal_io_semaphore (hcl_t* hcl, hcl_oop_semaphore_t sem)
{
	hcl_oop_process_t proc;

	proc = signal_semaphore (hcl, sem);

	if (hcl->processor->active == hcl->nil_process && (hcl_oop_t)proc != hcl->_nil)
	{
		/* this is the only runnable process. 
		 * switch the process to the running state.
		 * it uses wake_process() instead of
		 * switch_to_process() as there is no running 
		 * process at this moment */
		HCL_ASSERT (hcl, proc->state == HCL_SMOOI_TO_OOP(PROC_STATE_RUNNABLE));
		HCL_ASSERT (hcl, proc == hcl->processor->runnable.first);

	#if 0
		wake_process (hcl, proc); /* switch to running */
		hcl->proc_switched = 1;
	#else
		switch_to_process_from_nil (hcl, proc);
	#endif
	}
}

static void signal_io_semaphore (hcl_t* hcl, hcl_ooi_t io_handle, hcl_ooi_t mask)
{
	if (io_handle >= 0 && io_handle < hcl->sem_io_map_capa && hcl->sem_io_map[io_handle] >= 0)
	{
		hcl_oop_semaphore_t insem, outsem;
		hcl_ooi_t sem_io_index;

		sem_io_index = hcl->sem_io_map[io_handle];
		insem = hcl->sem_io_tuple[sem_io_index].sem[HCL_SEMAPHORE_IO_TYPE_INPUT];
		outsem = hcl->sem_io_tuple[sem_io_index].sem[HCL_SEMAPHORE_IO_TYPE_OUTPUT];

		if (outsem)
		{
			if ((mask & (HCL_SEMAPHORE_IO_MASK_OUTPUT | HCL_SEMAPHORE_IO_MASK_ERROR)) ||
			    (!insem && (mask & HCL_SEMAPHORE_IO_MASK_HANGUP)))
			{
				_signal_io_semaphore (hcl, outsem);
			}
		}

		if (insem)
		{
			if (mask & (HCL_SEMAPHORE_IO_MASK_INPUT | HCL_SEMAPHORE_IO_MASK_HANGUP | HCL_SEMAPHORE_IO_MASK_ERROR))
			{
				_signal_io_semaphore (hcl, insem);
			}
		}
	}
	else
	{
		/* you may come across this warning message if the multiplexer returned
		 * an IO event */
		HCL_LOG2 (hcl, HCL_LOG_WARN, "Warning - semaphore signaling requested on an unmapped handle %zd with mask %#zx\n", io_handle, mask);
	}
}

void hcl_releaseiohandle (hcl_t* hcl, hcl_ooi_t io_handle)
{
	/* TODO: optimize io semapore unmapping. since i'm to close the handle,
	 *       i don't need to call delete_sem_from_sem_io_tuple() seperately for input
	 *       and output. */
	if (io_handle < hcl->sem_io_map_capa)
	{
		hcl_ooi_t index;
		hcl_oop_semaphore_t sem;

		index = hcl->sem_io_map[io_handle];
		if (index >= 0)
		{
			HCL_ASSERT(hcl, hcl->sem_io_tuple[index].handle == io_handle);
			sem = hcl->sem_io_tuple[index].sem[HCL_SEMAPHORE_IO_TYPE_INPUT];
			if (sem) 
			{
				HCL_ASSERT(hcl, sem->subtype == HCL_SMOOI_TO_OOP(HCL_SEMAPHORE_SUBTYPE_IO));
				delete_sem_from_sem_io_tuple (hcl, sem, 0);
			}
		}
	}

	if (io_handle < hcl->sem_io_map_capa)
	{
		hcl_ooi_t index;
		hcl_oop_semaphore_t sem;

		index = hcl->sem_io_map[io_handle];
		if (index >= 0)
		{
			HCL_ASSERT(hcl, hcl->sem_io_tuple[index].handle == io_handle);
			sem = hcl->sem_io_tuple[index].sem[HCL_SEMAPHORE_IO_TYPE_OUTPUT];
			if (sem) 
			{
				HCL_ASSERT(hcl, sem->subtype == HCL_SMOOI_TO_OOP(HCL_SEMAPHORE_SUBTYPE_IO));
				delete_sem_from_sem_io_tuple (hcl, sem, 0);
			}
		}
	}
}

/* ------------------------------------------------------------------------- */

static int __activate_block (hcl_t* hcl, hcl_oop_block_t rcv_blk, hcl_ooi_t nargs, hcl_oop_context_t* pnewctx)
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
	HCL_ASSERT (hcl, HCL_IS_BLOCK(hcl, rcv_blk));

	if (HCL_OOP_TO_SMOOI(rcv_blk->nargs) != nargs)
	{
		HCL_LOG3 (hcl, HCL_LOG_IC | HCL_LOG_ERROR, 
			"Error - wrong number of arguments to a block %O - expecting %zd, got %zd\n",
			rcv_blk, HCL_OOP_TO_SMOOI(rcv_blk->nargs), nargs);
		hcl_seterrnum (hcl, HCL_ECALLARG);
		return -1;
	}

	local_ntmprs = HCL_OOP_TO_SMOOI(rcv_blk->ntmprs);
	HCL_ASSERT (hcl, local_ntmprs >= nargs);

	/* create a new block context to clone rcv_blk */
	hcl_pushvolat (hcl, (hcl_oop_t*)&rcv_blk);
	blkctx = make_context(hcl, local_ntmprs); 
	hcl_popvolat (hcl);
	if (HCL_UNLIKELY(!blkctx)) return -1;

#if 0
	/* shallow-copy the named part including home, origin, etc. */
	for (i = 0; i < HCL_CONTEXT_NAMED_INSTVARS; i++)
	{
		((hcl_oop_oop_t)blkctx)->slot[i] = ((hcl_oop_oop_t)rcv_blk)->slot[i];
	}
#else
	blkctx->ip = rcv_blk->ip;
	blkctx->ntmprs = rcv_blk->ntmprs;
	blkctx->nargs = rcv_blk->nargs;
	blkctx->receiver_or_base = (hcl_oop_t)rcv_blk;
	blkctx->home = rcv_blk->home;
	/* blkctx->origin = rcv_blk->origin; */
	blkctx->origin = rcv_blk->home->origin;
#endif

/* TODO: check the stack size of a block context to see if it's large enough to hold arguments */
	/* copy the arguments to the stack */
	for (i = 0; i < nargs; i++)
	{
		blkctx->slot[i] = HCL_STACK_GETARG(hcl, nargs, i);
	}

	HCL_STACK_POPS (hcl, nargs + 1); /* pop arguments and receiver */

	blkctx->sender = hcl->active_context;
	HCL_ASSERT (hcl, (hcl_oop_t)blkctx->home != hcl->_nil); /* if not intial context, the home must not be null */

	*pnewctx = blkctx;
	return 0;
}

static HCL_INLINE int activate_block (hcl_t* hcl, hcl_ooi_t nargs)
{
	int x;
	hcl_oop_block_t rcv;
	hcl_oop_context_t newctx;

	rcv = (hcl_oop_block_t)HCL_STACK_GETRCV(hcl, nargs);
	HCL_ASSERT (hcl, HCL_IS_BLOCK(hcl, rcv));

	x = __activate_block(hcl, rcv, nargs, &newctx);
	if (HCL_UNLIKELY(x <= -1)) return -1;

	SWITCH_ACTIVE_CONTEXT (hcl, newctx);
	return 0;
}

/* ------------------------------------------------------------------------- */

static int __activate_function (hcl_t* hcl, hcl_oop_function_t rcv_func, hcl_ooi_t nargs, hcl_oop_context_t* pnewctx)
{
	/* prepare a new block context for activation.
	 * the receiver must be a block context which becomes the base
	 * for a new block context. */

	hcl_oop_context_t functx;
	hcl_ooi_t local_ntmprs, i;

	/*
	  (defun sum (x)
	      (if (< x 2) 1
	       else (+ x (sum (- x 1)))))
	  (printf ">>>> %d\n" (sum 10))
	 */

	/* the receiver must be a function */
	HCL_ASSERT (hcl, HCL_IS_FUNCTION(hcl, rcv_func));

	if (HCL_OOP_TO_SMOOI(rcv_func->nargs) != nargs)
	{
		HCL_LOG3 (hcl, HCL_LOG_IC | HCL_LOG_ERROR, 
			"Error - wrong number of arguments to a function %O - expecting %zd, got %zd\n",
			rcv_func, HCL_OOP_TO_SMOOI(rcv_func->nargs), nargs);
		hcl_seterrnum (hcl, HCL_ECALLARG);
		return -1;
	}

	local_ntmprs = HCL_OOP_TO_SMOOI(rcv_func->ntmprs);
	HCL_ASSERT (hcl, local_ntmprs >= nargs);

	/* create a new block context to clone rcv_func */
	hcl_pushvolat (hcl, (hcl_oop_t*)&rcv_func);
	functx = make_context(hcl, local_ntmprs); 
	hcl_popvolat (hcl);
	if (HCL_UNLIKELY(!functx)) return -1;

	functx->ip = HCL_SMOOI_TO_OOP(0);
	functx->ntmprs = rcv_func->ntmprs;
	functx->nargs = rcv_func->nargs;
	functx->receiver_or_base = (hcl_oop_t)rcv_func;
	functx->home = rcv_func->home;
	functx->origin = functx; /* the origin of the context over a function should be itself */

/* TODO: check the stack size of a block context to see if it's large enough to hold arguments */
	/* copy the arguments to the stack */
	for (i = 0; i < nargs; i++)
	{
		functx->slot[i] = HCL_STACK_GETARG(hcl, nargs, i);
	}

	HCL_STACK_POPS (hcl, nargs + 1); /* pop arguments and receiver */

	HCL_ASSERT (hcl, (hcl_oop_t)functx->home != hcl->_nil);
	functx->sender = hcl->active_context;

	*pnewctx = functx;
	return 0;
}

static HCL_INLINE int activate_function (hcl_t* hcl, hcl_ooi_t nargs)
{
	int x;
	hcl_oop_function_t rcv;
	hcl_oop_context_t newctx;

	rcv = (hcl_oop_function_t)HCL_STACK_GETRCV(hcl, nargs);
	HCL_ASSERT (hcl, HCL_IS_FUNCTION(hcl, rcv));

	x = __activate_function(hcl, rcv, nargs, &newctx);
	if (HCL_UNLIKELY(x <= -1)) return -1;

	SWITCH_ACTIVE_CONTEXT (hcl, newctx);
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



#if 0
/* EXPERIMENTAL CODE INTEGRATING EXTERNAL COMMANDS */

#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <limits.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>

extern char **environ;

#define _PATH_DEFPATH "/usr/bin:/bin"

static int is_regular_executable_file_by_me(const char *path)
{
	struct stat st;
	if (stat(path, &st) == -1) return 0;
	return S_ISREG(st.st_mode) && access(path, X_OK) == 0; //? use eaccess instead??
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
	hcl_bch_t* cmd = HCL_NULL;
	hcl_bch_t* xcmd = HCL_NULL;

	rcv = (hcl_oop_word_t)HCL_STACK_GETRCV(hcl, nargs);
	/*HCL_ASSERT (hcl, HCL_IS_STRING(hcl, rcv) || HCL_IS_SYMBOL(hcl, rcv));*/
	HCL_ASSERT (hcl, HCL_OBJ_IS_CHAR_POINTER(rcv));

	if (HCL_OBJ_GET_SIZE(rcv) == 0 || hcl_count_oocstr(HCL_OBJ_GET_CHAR_SLOT(rcv)) != HCL_OBJ_GET_SIZE(rcv))
	{
		/* '\0' is contained in the middle */
		hcl_seterrbfmt (hcl, HCL_EINVAL, "invalid callable %O", rcv);
		goto oops;
	}

	cmd = hcl_dupootobcstr(hcl, HCL_OBJ_GET_CHAR_SLOT(rcv), HCL_NULL);
	if (!cmd) goto oops;

	if (hcl_find_bchar_in_bcstr(cmd, '/'))
	{
		if (!is_regular_executable_file_by_me(cmd)) 
		{
			hcl_seterrbfmt (hcl, HCL_ECALL, "cannot execute %O", rcv);
			goto oops;
		}

		xcmd = cmd;
	}
	else
	{
		xcmd = find_exec(hcl, cmd);
		if (!xcmd) goto oops;
	}

{ /* TODO: make it a callback ... */
	pid_t pid;
	int status;

	pid = fork();
	if (pid == -1) goto oops;

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
				if (HCL_OOP_IS_SMOOI(ta))
				{
/* TODO: rewrite this part */
					hcl_bch_t tmp[64];
					snprintf (tmp, sizeof(tmp), "%ld", (long int)HCL_OOP_TO_SMOOI(ta));
					argv[++i] = hcl_dupbchars(hcl, tmp, strlen(tmp));
				}
				else
				{
					argv[++i] = hcl_dupootobchars(hcl, HCL_OBJ_GET_CHAR_SLOT(ta), HCL_OBJ_GET_SIZE(ta), HCL_NULL);
				}
				/*HCL_DEBUG2 (hcl, "ARG %d -> %hs\n", (int)i - 1, argv[i]);*/
			}
			argv[nargs + 1] = HCL_NULL;
			execvp (xcmd, argv);
		}

		if (cmd) hcl_freemem (hcl, cmd);
		if (xcmd && xcmd != cmd) hcl_freemem (hcl, xcmd);
		_exit (255);
	}

	waitpid (pid, &status, 0); /* TOOD: enhance this waiting */

	HCL_STACK_SETRET (hcl, nargs, HCL_SMOOI_TO_OOP(WEXITSTATUS(status)));
}

	hcl_freemem (hcl, cmd);
	if (xcmd != cmd) hcl_freemem (hcl, xcmd);
	return 0;

oops:
	if (cmd) hcl_freemem (hcl, cmd);
	if (xcmd && xcmd != cmd) hcl_freemem (hcl, xcmd);
	return -1;
}

#endif

/* ------------------------------------------------------------------------- */
static hcl_oop_process_t start_initial_process (hcl_t* hcl, hcl_oop_context_t ctx)
{
	hcl_oop_process_t proc;

	/* there must be no active process when this function is called */
	HCL_ASSERT (hcl, hcl->processor->runnable.count == HCL_SMOOI_TO_OOP(0));
	HCL_ASSERT (hcl, hcl->processor->active == hcl->nil_process);

	proc = make_process(hcl, ctx);
	if (HCL_UNLIKELY(!proc)) return HCL_NULL;

	/* skip RUNNABLE and go to RUNNING */
	chain_into_processor(hcl, proc, PROC_STATE_RUNNING);
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

	/* create the initial context over the initial function */
	ctx = make_context(hcl, 0); /* no temporary variables */
	if (HCL_UNLIKELY(!ctx)) return -1;

	hcl->ip = initial_ip;
	hcl->sp = -1;

	ctx->ip = HCL_SMOOI_TO_OOP(initial_ip);
	ctx->nargs = HCL_SMOOI_TO_OOP(0);
	ctx->ntmprs = HCL_SMOOI_TO_OOP(0);
	ctx->origin = ctx; /* the origin of the initial context is itself as this is created over the initial function */
	ctx->home = hcl->initial_function->home; /* this should be nil */
	ctx->sender = (hcl_oop_context_t)hcl->_nil;
	ctx->receiver_or_base = hcl->initial_function;
	HCL_ASSERT (hcl, (hcl_oop_t)ctx->home == hcl->_nil);

	/* [NOTE]
	 *  the sender field of the initial context is nil.
	 *  especially, the fact that the sender field is nil is used by 
	 *  the main execution loop for breaking out of the loop */

	HCL_ASSERT (hcl, hcl->active_context == HCL_NULL);

	/* hcl_gc() uses hcl->processor when hcl->active_context
	 * is not NULL. at this poinst, hcl->processor should point to
	 * an instance of ProcessScheduler. */
	HCL_ASSERT (hcl, (hcl_oop_t)hcl->processor != hcl->_nil);
	HCL_ASSERT (hcl, hcl->processor->runnable.count == HCL_SMOOI_TO_OOP(0));

	/* start_initial_process() calls the SWITCH_ACTIVE_CONTEXT() macro.
	 * the macro assumes a non-null value in hcl->active_context.
	 * let's force set active_context to ctx directly. */
	hcl->active_context = ctx;

	hcl_pushvolat (hcl, (hcl_oop_t*)&ctx);
	proc = start_initial_process(hcl, ctx); 
	hcl_popvolat (hcl);
	if (HCL_UNLIKELY(!proc)) return -1;

	/* the stack must contain nothing as it should emulate the expresssion - (the-initial-function). 
	 * for a normal function call, the function object and arguments are pushed by the caller.
	 * __activate_function() creates a new context and pops the function object and arguments off the stack.
	 * at this point, it should be as if the pop-off has been completed.
	 * because this is the very beginning, nothing should exist in the stack */
	HCL_ASSERT (hcl, hcl->sp == -1);
	HCL_ASSERT (hcl, hcl->sp == HCL_OOP_TO_SMOOI(proc->sp));

	HCL_ASSERT (hcl, proc == hcl->processor->active);
	hcl->initial_context = proc->initial_context;
	HCL_ASSERT (hcl, hcl->initial_context == hcl->active_context);

	return 0;
}

/* ------------------------------------------------------------------------- */

static HCL_INLINE int switch_process_if_needed (hcl_t* hcl)
{
	if (hcl->sem_heap_count > 0)
	{
		/* handle timed semaphores */
		hcl_ntime_t ft, now;

		vm_gettime (hcl, &now);

		do
		{
			HCL_ASSERT (hcl, hcl->sem_heap[0]->subtype == HCL_SMOOI_TO_OOP(HCL_SEMAPHORE_SUBTYPE_TIMED));
			HCL_ASSERT (hcl, HCL_OOP_IS_SMOOI(hcl->sem_heap[0]->u.timed.ftime_sec));
			HCL_ASSERT (hcl, HCL_OOP_IS_SMOOI(hcl->sem_heap[0]->u.timed.ftime_nsec));

			HCL_INIT_NTIME (&ft,
				HCL_OOP_TO_SMOOI(hcl->sem_heap[0]->u.timed.ftime_sec),
				HCL_OOP_TO_SMOOI(hcl->sem_heap[0]->u.timed.ftime_nsec)
			);

			if (HCL_CMP_NTIME(&ft, (hcl_ntime_t*)&now) <= 0)
			{
				hcl_oop_process_t proc;

			signal_timed:
				/* waited long enough. signal the semaphore */

				proc = signal_semaphore(hcl, hcl->sem_heap[0]);
				/* [NOTE] no hcl_pushvolat() on proc. no GC must occur
				 *        in the following line until it's used for
				 *        wake_process() below. */
				delete_from_sem_heap (hcl, 0); /* hcl->sem_heap_count is decremented in delete_from_sem_heap() */

				/* if no process is waiting on the semaphore, 
				 * signal_semaphore() returns hcl->_nil. */

				if (hcl->processor->active == hcl->nil_process && (hcl_oop_t)proc != hcl->_nil)
				{
					/* this is the only runnable process. 
					 * switch the process to the running state.
					 * it uses wake_process() instead of
					 * switch_to_process() as there is no running 
					 * process at this moment */

				#if defined(HCL_DEBUG_VM_PROCESSOR) && (HCL_DEBUG_VM_PROCESSOR >= 2)
					HCL_LOG2 (hcl, HCL_LOG_IC | HCL_LOG_DEBUG, "Processor - switching to a process[%zd] while no process is active - total runnables %zd\n", HCL_OOP_TO_SMOOI(proc->id), HCL_OOP_TO_SMOOI(hcl->processor->runnable.count));
				#endif

					HCL_ASSERT (hcl, proc->state == HCL_SMOOI_TO_OOP(PROC_STATE_RUNNABLE));
					HCL_ASSERT (hcl, proc == hcl->processor->runnable.last); /* resume_process() appends to the runnable list */
				#if 0
					wake_process (hcl, proc); /* switch to running */
					hcl->proc_switched = 1;
				#else
					switch_to_process_from_nil (hcl, proc);
				#endif
				}
			}
			else if (hcl->processor->active == hcl->nil_process)
			{
				/* no running process. before firing time. */
				HCL_SUB_NTIME (&ft, &ft, (hcl_ntime_t*)&now);

				if (hcl->sem_io_wait_count > 0)
				{
					/* no running process but io semaphore being waited on */
					vm_muxwait (hcl, &ft);

					/* exit early if a process has been woken up. 
					 * the break in the else part further down will get hit
					 * eventually even if the following line doesn't exist.
					 * having the following line causes to skip firing the
					 * timed semaphore that would expire between now and the 
					 * moment the next inspection occurs. */
					if (hcl->processor->active != hcl->nil_process) goto switch_to_next;
				}
				else
				{
					int halting;

#if defined(ENABLE_GCFIN)
					/* no running process, no io semaphore */
					if ((hcl_oop_t)hcl->sem_gcfin != hcl->_nil && hcl->sem_gcfin_sigreq) goto signal_sem_gcfin;
#endif
					halting = vm_sleep(hcl, &ft);

					if (halting)
					{
						vm_gettime (hcl, &now);
						goto signal_timed;
					}
				}
				vm_gettime (hcl, &now);
			}
			else 
			{
				/* there is a running process. go on */
				break;
			}
		}
		while (hcl->sem_heap_count > 0 && !hcl->abort_req);
	}

	if (hcl->sem_io_wait_count > 0) 
	{
		if (hcl->processor->active == hcl->nil_process)
		{
			hcl_ntime_t ft;

			HCL_ASSERT (hcl, hcl->processor->runnable.count == HCL_SMOOI_TO_OOP(0));

#if defined(ENABLE_GCFIN)
			/* no runnable process while there is an io semaphore being waited for */
			if ((hcl_oop_t)hcl->sem_gcfin != hcl->_nil && hcl->sem_gcfin_sigreq) goto signal_sem_gcfin;
#endif

			if (hcl->processor->suspended.count == HCL_SMOOI_TO_OOP(0))
			{
				/* no suspended process. the program is buggy or is probably being
				 * terminated forcibly. 
				 * the default signal handler may lead to this situation. */
				hcl->abort_req = 1;
			}
			else
			{
				do
				{
					HCL_INIT_NTIME (&ft, 3, 0); /* TODO: use a configured time */
					vm_muxwait (hcl, &ft);
				}
				while (hcl->processor->active == hcl->nil_process && !hcl->abort_req);
			}
		}
		else
		{
			/* well, there is a process waiting on one or more semaphores while
			 * there are other normal processes to run. check IO activities
			 * before proceeding to handle normal process scheduling */

			/* [NOTE] the check with the multiplexer may happen too frequently
			 *       because this is called everytime process switching is requested.
			 *       the actual callback implementation should try to avoid invoking 
			 *       actual system calls too frequently for less overhead. */
			vm_muxwait (hcl, HCL_NULL);
		}
	}

#if defined(ENABLE_GCFIN)
	if ((hcl_oop_t)hcl->sem_gcfin != hcl->_nil) 
	{
		hcl_oop_process_t proc;

		if (hcl->sem_gcfin_sigreq)
		{
		signal_sem_gcfin:
			HCL_LOG0 (hcl, HCL_LOG_IC | HCL_LOG_DEBUG, "Signaled GCFIN semaphore\n");
			proc = signal_semaphore(hcl, hcl->sem_gcfin);

			if (hcl->processor->active == hcl->nil_process && (hcl_oop_t)proc != hcl->_nil)
			{
				HCL_ASSERT (hcl, proc->state == HCL_SMOOI_TO_OOP(PROC_STATE_RUNNABLE));
				HCL_ASSERT (hcl, proc == hcl->processor->runnable.first);
				switch_to_process_from_nil (hcl, proc);
			}

			hcl->sem_gcfin_sigreq = 0;
		}
		else
		{
			/* the gcfin semaphore signalling is not requested and there are 
			 * no runnable processes nor no waiting semaphores. if there is 
			 * process waiting on the gcfin semaphore, i will just schedule
			 * it to run by calling signal_semaphore() on hcl->sem_gcfin.
			 */
			/* TODO: check if this is the best implementation practice */
			if (hcl->processor->active == hcl->nil_process)
			{
				/* there is no active process. in most cases, the only process left
				 * should be the gc finalizer process started in the System>>startup.
				 * if there are other suspended processes at this point, the processes
				 * are not likely to run again. 
				 * 
				 * imagine the following single line program that creates a process 
				 * but never start it.
				 *
				 *    method(#class) main { | p |  p := [] newProcess. }
				 *
				 * the gc finalizer process and the process assigned to p exist. 
				 * when the code reaches here, the 'p' process still is alive
				 * despite no active process nor no process waiting on timers
				 * and semaphores. so when the entire program terminates, there
				 * might still be some suspended processes that are not possible
				 * to schedule.
				 */

				HCL_LOG4 (hcl, HCL_LOG_IC | HCL_LOG_DEBUG, 
					"Signaled GCFIN semaphore without gcfin signal request - total %zd runnable/running %zd suspended %zd - sem_io_wait_count %zu\n",
					HCL_OOP_TO_SMOOI(hcl->processor->total_count),
					HCL_OOP_TO_SMOOI(hcl->processor->runnable.count),
					HCL_OOP_TO_SMOOI(hcl->processor->suspended.count),
					hcl->sem_io_wait_count);
				proc = signal_semaphore(hcl, hcl->sem_gcfin);
				if ((hcl_oop_t)proc != hcl->_nil) 
				{
					HCL_ASSERT (hcl, proc->state == HCL_SMOOI_TO_OOP(PROC_STATE_RUNNABLE));
					HCL_ASSERT (hcl, proc == hcl->processor->runnable.first);
					hcl->_system->cvar[2] = hcl->_true; /* set gcfin_should_exit in System to true. if the postion of the class variable changes, the index must get changed, too. */
					switch_to_process_from_nil (hcl, proc); /* sechedule the gc finalizer process */
				}
			}
		}
	}
#endif


#if 0
	while (hcl->sem_list_count > 0)
	{
		/* handle async signals */
		--hcl->sem_list_count;
		signal_semaphore (hcl, hcl->sem_list[hcl->sem_list_count]);
		if (hcl->processor->active == hcl->nil_process)
		{suspended process
		}
	}
	/*
	if (semaphore heap has pending request)
	{
		signal them...
	}*/
#endif

	if (hcl->processor->active == hcl->nil_process) 
	{
		/* no more waiting semaphore and no more process */
		HCL_ASSERT (hcl, hcl->processor->runnable.count = HCL_SMOOI_TO_OOP(0));
		HCL_LOG0 (hcl, HCL_LOG_IC | HCL_LOG_DEBUG, "No more runnable process\n");

		if (HCL_OOP_TO_SMOOI(hcl->processor->suspended.count) > 0)
		{
			/* there exist suspended processes while no processes are runnable.
			 * most likely, the running program contains process/semaphore related bugs */
			HCL_LOG1 (hcl, HCL_LOG_IC | HCL_LOG_WARN, 
				"%zd suspended process(es) found - check your program\n",
				HCL_OOP_TO_SMOOI(hcl->processor->suspended.count));
		}
		return 0;
	}

switch_to_next:
	/* TODO: implement different process switching scheme - time-slice or clock based??? */
#if defined(HCL_EXTERNAL_PROCESS_SWITCH)
	if (hcl->switch_proc)
	{
#endif
		if (!hcl->proc_switched) 
		{
			switch_to_next_runnable_process (hcl);
			hcl->proc_switched = 0;
		}
#if defined(HCL_EXTERNAL_PROCESS_SWITCH)
		hcl->switch_proc = 0;
	}
	else hcl->proc_switched = 0;
#endif

	return 1;
}
/* ------------------------------------------------------------------------- */

static HCL_INLINE int do_return (hcl_t* hcl, hcl_oop_t return_value)
{
	/* if (hcl->active_context == hcl->processor->active->initial_context) // read the interactive mode note below... */
	if ((hcl_oop_t)hcl->active_context->home == hcl->_nil)
	{
		/* returning from the intial context.
		 *  (return-from-home 999) */
		HCL_ASSERT (hcl, (hcl_oop_t)hcl->active_context->sender == hcl->_nil);
		hcl->active_context->ip = HCL_SMOOI_TO_OOP(-1); /* mark the active context dead */

		if (hcl->sp >= 0)
		{
			/* return-from-home has been called from where it shouldn't be. for instance,
			 *  (printf "xxx %d\n" (return-from-home 999))
			 *  -----------------------------------------------
			 *  (if (>  19 (return-from-home 20)) 30) */
			HCL_LOG0 (hcl, HCL_LOG_IC | HCL_LOG_WARN, "Warning - stack not empty on return-from-home\n"); /* TODO: include line number and file name */
		}

		terminate_process (hcl, hcl->processor->active);
	}
	/*else if (hcl->active_context->home == hcl->processor->active->initial_context) // read the interactive mode note below...*/
	else if ((hcl_oop_t)hcl->active_context->home->home == hcl->_nil)
	{
		/* non-local return out of the initial context 
		 *  (defun y(x) (return-from-home (* x x)))
		 *  (y 999) */

		/* [NOTE]
		 * in the interactive mode, a new initial context/function/process is created
		 * for each expression (as implemented bin/main.c)
		 * hcl->active_context may be the intial context of the previous expression.
		 *   (defun y(x) (return-from-home (* x x))) <-- initial context
		 *   (y 999) <- another initial context
		 * when y is called from the second initial context, the home context to return
		 * from the the first initial context. comparing hcl->active_context->home againt
		 * hcl->initial_context doesn't return true in this case.
		 */

		HCL_ASSERT (hcl, (hcl_oop_t)hcl->active_context->home->sender == hcl->_nil);
		hcl->active_context->home->ip = HCL_SMOOI_TO_OOP(-1); /* mark that this context has returned */

		if (hcl->sp >= 0)
		{
			/* return-from-home has been called from where it shouldn't be
			 *  (defun y(x) (return-from-home (* x x)))
			 *  (printf "xxx %d\n" (y 999)) */
			HCL_LOG0 (hcl, HCL_LOG_IC | HCL_LOG_WARN, "Warning - stack not empty on non-local return-from-home\n"); /* TODO: include line number and file name */
		}

		terminate_process (hcl, hcl->processor->active);
	}
	else
	{
		/*
		(defun f(x)
			(defun y(x) (return-from-home (* x  x)))
			(y x)
			(printf "this line must not be printed\n");
		)
		(printf "%d\n" (f 90)) ; this should print 8100.
		(y 10); this ends up with the "unable to return from dead context" error.
		*/
		HCL_ASSERT (hcl, hcl->active_context != hcl->processor->active->initial_context);
		HCL_ASSERT (hcl, (hcl_oop_t)hcl->active_context->home->sender != hcl->_nil);

		if (hcl->active_context->home->ip == HCL_SMOOI_TO_OOP(-1))
		{
			HCL_LOG0 (hcl, HCL_LOG_IC | HCL_LOG_ERROR, "Error - cannot return from dead context\n");
			hcl_seterrbfmt (hcl, HCL_EINTERN, "unable to return from dead context"); /* TODO: can i make this error catchable at the hcl level? */
			return -1;
		}

		hcl->active_context->home->ip = HCL_SMOOI_TO_OOP(-1); /* mark that this context has returned */
		hcl->ip = -1; /* mark that the active context has returned. saved into hcl->active_context->ip in SWITCH_ACTIVE_CONTEXT() */
		SWITCH_ACTIVE_CONTEXT (hcl, hcl->active_context->home->sender);

		/* push the return value to the stack of the new active context */
		HCL_STACK_PUSH (hcl, return_value);


#if 0
		/* stack dump */
		HCL_DEBUG1 (hcl, "****** non local returning %O\n", return_value);
		{
			int i;
			for (i = hcl->sp; i >= 0; i--)
			{
				HCL_DEBUG2 (hcl, "STACK[%d] => %O\n", i, HCL_STACK_GET(hcl, i));
			}
		}
#endif
	}

	return 0;
}

static HCL_INLINE void do_return_from_block (hcl_t* hcl)
{
	/*if (hcl->active_context == hcl->processor->active->initial_context)*/
	if (hcl->active_context->home == hcl->_nil)
	{
		/* the active context to return from is an initial context of
		 * the active process. let's terminate the process. 
		 * the initial context has been forged over the initial function
		 * in start_initial_process_and_context() */
		HCL_ASSERT (hcl, (hcl_oop_t)hcl->active_context->sender == hcl->_nil);
		hcl->active_context->ip = HCL_SMOOI_TO_OOP(-1); /* mark context dead */
		terminate_process (hcl, hcl->processor->active);
	}
	else
	{
		/* it is a normal block return as the active block context 
		 * is not the initial context of a process */
		hcl->ip = -1; /* mark context dead. saved into hcl->active_context->ip in SWITCH_ACTIVE_CONTEXT */
		SWITCH_ACTIVE_CONTEXT (hcl, (hcl_oop_context_t)hcl->active_context->sender);
	}
}
/* ------------------------------------------------------------------------- */

static void xma_dumper (void* ctx, const char* fmt, ...)
{
	va_list ap;
	va_start (ap, fmt);
	hcl_logbfmtv ((hcl_t*)ctx, HCL_LOG_IC | HCL_LOG_INFO, fmt, ap);
	va_end (ap);
}

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

	hcl->gci.lazy_sweep = 1; /* TODO: make it configurable?? */
	HCL_INIT_NTIME (&hcl->gci.stat.alloc, 0, 0);
	HCL_INIT_NTIME (&hcl->gci.stat.mark, 0, 0);
	HCL_INIT_NTIME (&hcl->gci.stat.sweep, 0, 0);

	while (1)
	{
		/* stop requested or no more runnable process */
		if (hcl->abort_req || (!hcl->no_proc_switch && switch_process_if_needed(hcl) == 0)) 
		{
/* TODO: if aborting, ensure to terminate all ongoing processes */
			break;
		}

		if (HCL_UNLIKELY(hcl->ip >= HCL_FUNCTION_GET_CODE_SIZE(hcl->active_function)))
		{
			HCL_DEBUG2 (hcl, "Stopping execution as IP reached the end of bytecode(%zu) - SP %zd\n", hcl->code.bc.len, hcl->sp);
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
			case HCL_CODE_PUSH_INSTVAR_X:
				FETCH_PARAM_CODE_TO (hcl, b1);
				goto push_instvar;
			case HCL_CODE_PUSH_INSTVAR_0:
			case HCL_CODE_PUSH_INSTVAR_1:
			case HCL_CODE_PUSH_INSTVAR_2:
			case HCL_CODE_PUSH_INSTVAR_3:
			case HCL_CODE_PUSH_INSTVAR_4:
			case HCL_CODE_PUSH_INSTVAR_5:
			case HCL_CODE_PUSH_INSTVAR_6:
			case HCL_CODE_PUSH_INSTVAR_7:
				b1 = bcode & 0x7; /* low 3 bits */
			push_instvar:
				LOG_INST_1 (hcl, "push_instvar %zu", b1);
				HCL_ASSERT (hcl, HCL_OBJ_GET_FLAGS_TYPE(hcl->active_context->origin->receiver_or_base) == HCL_OBJ_TYPE_OOP);
				HCL_STACK_PUSH (hcl, ((hcl_oop_oop_t)hcl->active_context->origin->receiver_or_base)->slot[b1]);
				break;

			/* ------------------------------------------------- */

			case HCL_CODE_STORE_INTO_INSTVAR_X:
				FETCH_PARAM_CODE_TO (hcl, b1);
				goto store_instvar;
			case HCL_CODE_STORE_INTO_INSTVAR_0:
			case HCL_CODE_STORE_INTO_INSTVAR_1:
			case HCL_CODE_STORE_INTO_INSTVAR_2:
			case HCL_CODE_STORE_INTO_INSTVAR_3:
			case HCL_CODE_STORE_INTO_INSTVAR_4:
			case HCL_CODE_STORE_INTO_INSTVAR_5:
			case HCL_CODE_STORE_INTO_INSTVAR_6:
			case HCL_CODE_STORE_INTO_INSTVAR_7:
				b1 = bcode & 0x7; /* low 3 bits */
			store_instvar:
				LOG_INST_1 (hcl, "store_into_instvar %zu", b1);
				HCL_ASSERT (hcl, HCL_OBJ_GET_FLAGS_TYPE(hcl->active_context->receiver_or_base) == HCL_OBJ_TYPE_OOP);
				((hcl_oop_oop_t)hcl->active_context->origin->receiver_or_base)->slot[b1] = HCL_STACK_GETTOP(hcl);
				break;

			/* ------------------------------------------------- */
			case HCL_CODE_POP_INTO_INSTVAR_X:
				FETCH_PARAM_CODE_TO (hcl, b1);
				goto pop_into_instvar;
			case HCL_CODE_POP_INTO_INSTVAR_0:
			case HCL_CODE_POP_INTO_INSTVAR_1:
			case HCL_CODE_POP_INTO_INSTVAR_2:
			case HCL_CODE_POP_INTO_INSTVAR_3:
			case HCL_CODE_POP_INTO_INSTVAR_4:
			case HCL_CODE_POP_INTO_INSTVAR_5:
			case HCL_CODE_POP_INTO_INSTVAR_6:
			case HCL_CODE_POP_INTO_INSTVAR_7:
				b1 = bcode & 0x7; /* low 3 bits */
			pop_into_instvar:
				LOG_INST_1 (hcl, "pop_into_instvar %zu", b1);
				HCL_ASSERT (hcl, HCL_OBJ_GET_FLAGS_TYPE(hcl->active_context->receiver_or_base) == HCL_OBJ_TYPE_OOP);
				((hcl_oop_oop_t)hcl->active_context->origin->receiver_or_base)->slot[b1] = HCL_STACK_GETTOP(hcl);
				HCL_STACK_POP (hcl);
				break;
#endif

			/* ------------------------------------------------- */
			case HCL_CODE_PUSH_TEMPVAR_X:
			case HCL_CODE_STORE_INTO_TEMPVAR_X:
			case HCL_CODE_POP_INTO_TEMPVAR_X:
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
			case HCL_CODE_POP_INTO_TEMPVAR_0:
			case HCL_CODE_POP_INTO_TEMPVAR_1:
			case HCL_CODE_POP_INTO_TEMPVAR_2:
			case HCL_CODE_POP_INTO_TEMPVAR_3:
			case HCL_CODE_POP_INTO_TEMPVAR_4:
			case HCL_CODE_POP_INTO_TEMPVAR_5:
			case HCL_CODE_POP_INTO_TEMPVAR_6:
			case HCL_CODE_POP_INTO_TEMPVAR_7:
			{
				hcl_oop_context_t ctx;
				hcl_ooi_t bx;

				b1 = bcode & 0x7; /* low 3 bits */
			handle_tempvar:

				/* when CTXTEMPVAR inststructions are used, the above 
				 * instructions are used only for temporary access 
				 * outside a block. i can assume that the temporary
				 * variable index is pointing to one of temporaries
				 * in the relevant method context */
				ctx = hcl->active_context->origin;
				bx = b1;
				HCL_ASSERT (hcl, HCL_IS_CONTEXT(hcl, ctx));

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
		#if (HCL_CODE_LONG_PARAM_SIZE == 2)
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
				/*HCL_STACK_PUSH (hcl, hcl->code.lit.arr->slot[b1]);*/
				HCL_STACK_PUSH (hcl, hcl->active_function->literal_frame[b1]);
				break;

			/* ------------------------------------------------- */
			case HCL_CODE_PUSH_OBJECT_X:
			case HCL_CODE_STORE_INTO_OBJECT_X:
			case HCL_CODE_POP_INTO_OBJECT_X:
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
			case HCL_CODE_POP_INTO_OBJECT_0:
			case HCL_CODE_POP_INTO_OBJECT_1:
			case HCL_CODE_POP_INTO_OBJECT_2:
			case HCL_CODE_POP_INTO_OBJECT_3:
			{
				hcl_oop_cons_t ass;

				b1 = bcode & 0x3; /* low 2 bits */
			handle_object:
				/*ass = hcl->code.lit.arr->slot[b1];*/
				ass = (hcl_oop_cons_t)hcl->active_function->literal_frame[b1];
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

			case HCL_CODE_JUMP_BACKWARD_IF_TRUE:
				FETCH_PARAM_CODE_TO (hcl, b1);
				LOG_INST_1 (hcl, "jump_backward_if_true %zu", b1);
				if (HCL_STACK_GETTOP(hcl) != hcl->_false) hcl->ip -= b1;
				break;

			case HCL_CODE_JUMP2_BACKWARD_IF_TRUE:
				FETCH_PARAM_CODE_TO (hcl, b1);
				LOG_INST_1 (hcl, "jump2_backward_if_true %zu", b1);
				if (HCL_STACK_GETTOP(hcl) != hcl->_false) hcl->ip -= MAX_CODE_JUMP + b1;
				break;

			case HCL_CODE_JUMP_BACKWARD_IF_FALSE:
				FETCH_PARAM_CODE_TO (hcl, b1);
				LOG_INST_1 (hcl, "jump_backward_if_false %zu", b1);
				if (HCL_STACK_GETTOP(hcl) == hcl->_false) hcl->ip -= b1;
				break;

			case HCL_CODE_JUMP2_BACKWARD_IF_FALSE:
				FETCH_PARAM_CODE_TO (hcl, b1);
				LOG_INST_1 (hcl, "jump2_backward_if_false %zu", b1);
				if (HCL_STACK_GETTOP(hcl) == hcl->_false) hcl->ip -= MAX_CODE_JUMP + b1;
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
						case HCL_BRAND_FUNCTION:
							if (activate_function(hcl, b1) <= -1) goto oops;
							break;

						case HCL_BRAND_BLOCK:
							if (activate_block(hcl, b1) <= -1) goto oops;
							break;

						case HCL_BRAND_PRIM:
							if (call_primitive(hcl, b1) <= -1) goto oops;
							break;

						default:
							goto cannot_call;
					}
				}
				else
				{
				cannot_call:
					/* run time error */
if (hcl->active_function->dbgi != hcl->_nil)
{
	hcl_dbgl_t* dbgl;
	hcl_ooi_t ip;
	static hcl_ooch_t dash[] = { '-', '\0' };

	HCL_ASSERT (hcl, HCL_IS_BYTEARRAY(hcl, hcl->active_function->dbgi));
	dbgl = (hcl_dbgl_t*)HCL_OBJ_GET_BYTE_SLOT(hcl->active_function->dbgi);	
	ip = hcl->ip - 1;
	if (bcode == HCL_CODE_CALL_X) ip -= HCL_CODE_LONG_PARAM_SIZE;

hcl_seterrbfmt (hcl, HCL_ECALL, "cannot call %O (%js %zu)", 
	rcv, (dbgl[ip].fname? dbgl[ip].fname: dash), dbgl[ip].sline);
}
else
{
					hcl_seterrbfmt (hcl, HCL_ECALL, "cannot call %O", rcv);
}
					goto oops;
				}
				break;
			}
			
			/* -------------------------------------------------------- */

			case HCL_CODE_PUSH_CTXTEMPVAR_X:
			case HCL_CODE_STORE_INTO_CTXTEMPVAR_X:
			case HCL_CODE_POP_INTO_CTXTEMPVAR_X:
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
			case HCL_CODE_POP_INTO_CTXTEMPVAR_0:
			case HCL_CODE_POP_INTO_CTXTEMPVAR_1:
			case HCL_CODE_POP_INTO_CTXTEMPVAR_2:
			case HCL_CODE_POP_INTO_CTXTEMPVAR_3:
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
					/* the initial context has nil in the home field. 
					 * the loop must not reach beyond the initial context */
					HCL_ASSERT (hcl, (hcl_oop_t)ctx != hcl->_nil);
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

			case HCL_CODE_PUSH_OBJVAR_X:
			case HCL_CODE_STORE_INTO_OBJVAR_X:
			case HCL_CODE_POP_INTO_OBJVAR_X:
				FETCH_PARAM_CODE_TO (hcl, b1);
				FETCH_PARAM_CODE_TO (hcl, b2);
				goto handle_objvar;

			case HCL_CODE_PUSH_OBJVAR_0:
			case HCL_CODE_PUSH_OBJVAR_1:
			case HCL_CODE_PUSH_OBJVAR_2:
			case HCL_CODE_PUSH_OBJVAR_3:
			case HCL_CODE_STORE_INTO_OBJVAR_0:
			case HCL_CODE_STORE_INTO_OBJVAR_1:
			case HCL_CODE_STORE_INTO_OBJVAR_2:
			case HCL_CODE_STORE_INTO_OBJVAR_3:
			case HCL_CODE_POP_INTO_OBJVAR_0:
			case HCL_CODE_POP_INTO_OBJVAR_1:
			case HCL_CODE_POP_INTO_OBJVAR_2:
			case HCL_CODE_POP_INTO_OBJVAR_3:
			{
				hcl_oop_oop_t t;

				/* b1 -> variable index to the object indicated by b2.
				 * b2 -> object index stored in the literal frame. */
				b1 = bcode & 0x3; /* low 2 bits */
				FETCH_BYTE_CODE_TO (hcl, b2);

			handle_objvar:
				/*t = hcl->code.lit.arr->slot[b2];*/
				t = (hcl_oop_oop_t)hcl->active_function->literal_frame[b2];
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
			case HCL_CODE_SEND_MESSAGE_X:
			case HCL_CODE_SEND_MESSAGE_TO_SUPER_X:
				/* b1 -> number of arguments 
				 * b2 -> selector index stored in the literal frame */
				FETCH_PARAM_CODE_TO (hcl, b1);
				FETCH_PARAM_CODE_TO (hcl, b2);
				goto handle_send_message;

			case HCL_CODE_SEND_MESSAGE_0:
			case HCL_CODE_SEND_MESSAGE_1:
			case HCL_CODE_SEND_MESSAGE_2:
			case HCL_CODE_SEND_MESSAGE_3:
			case HCL_CODE_SEND_MESSAGE_TO_SUPER_0:
			case HCL_CODE_SEND_MESSAGE_TO_SUPER_1:
			case HCL_CODE_SEND_MESSAGE_TO_SUPER_2:
			case HCL_CODE_SEND_MESSAGE_TO_SUPER_3:
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

			case HCL_CODE_PUSH_RECEIVER: /* push self or super */
				LOG_INST_0 (hcl, "push_receiver");
				HCL_STACK_PUSH (hcl, hcl->active_context->origin->receiver_or_base);
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

			case HCL_CODE_PUSH_CONTEXT:
				LOG_INST_0 (hcl, "push_context");
				HCL_STACK_PUSH (hcl, (hcl_oop_t)hcl->active_context);
				break;

			case HCL_CODE_PUSH_PROCESS:
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
				t = hcl_makearray(hcl, b1, 0);
				if (HCL_UNLIKELY(!t)) goto oops;

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
				if (HCL_UNLIKELY(b1 >= HCL_OBJ_GET_SIZE(t2)))
				{
					hcl_seterrbfmt (hcl, HCL_ECALL, "index %zu out of upper bound %zd ", b1, (hcl_oow_t)HCL_OBJ_GET_SIZE(t2));
					goto oops;
				}

				((hcl_oop_oop_t)t2)->slot[b1] = t1;
				break;
			}

			case HCL_CODE_MAKE_BYTEARRAY:
			{
				hcl_oop_t t;

				FETCH_PARAM_CODE_TO (hcl, b1);
				LOG_INST_1 (hcl, "make_bytearray %zu", b1);

				/* create an empty array */
				t = hcl_makebytearray(hcl, HCL_NULL, b1);
				if (HCL_UNLIKELY(!t)) goto oops;

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
				t = (hcl_oop_t)hcl_makedic(hcl, b1 + 10);
				if (HCL_UNLIKELY(!t)) goto oops;
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
				if (!hcl_putatdic(hcl, (hcl_oop_dic_t)t3, t2, t1)) goto oops;
				break;
			}

			case HCL_CODE_MAKE_CONS:
			{
				hcl_oop_t t;

				LOG_INST_0 (hcl, "make_cons");

				t = hcl_makecons(hcl, hcl->_nil, hcl->_nil);
				if (HCL_UNLIKELY(!t)) goto oops;

				HCL_STACK_PUSH (hcl, t); /* push the head cons cell */
				HCL_STACK_PUSH (hcl, hcl->_nil); /* sentinnel */
				break;
			}

			case HCL_CODE_POP_INTO_CONS:
			{
				hcl_oop_t t1, t2, t3;
				LOG_INST_0 (hcl, "pop_into_cons");

				t1 = HCL_STACK_GETTOP(hcl); /* value to store */
				HCL_STACK_POP (hcl);

				t3 = HCL_STACK_GETTOP(hcl); /* sentinnel */
				HCL_STACK_POP (hcl);

				t2 = HCL_STACK_GETTOP(hcl); /* head cons */
				if (HCL_UNLIKELY(!HCL_IS_CONS(hcl, t2)))
				{
					hcl_seterrbfmt (hcl, HCL_EINTERN, "internal error - invalid vm state detected in pop_into_cons");
					goto oops;
				}

				if (t3 == hcl->_nil)
				{
					((hcl_oop_oop_t)t2)->slot[0] = t1;
					HCL_STACK_PUSH (hcl, t2); /* push self again */
				}
				else
				{
					hcl_oop_t t;

					hcl_pushvolat (hcl, &t3);
					t = hcl_makecons(hcl, t1, hcl->_nil);
					hcl_popvolat (hcl);
					if (HCL_UNLIKELY(!t)) goto oops;

					((hcl_oop_oop_t)t3)->slot[1] = t;
					HCL_STACK_PUSH (hcl, t);
				}

#if 0
				if (b1 == 1 || b1 == 3)
				{
					if (t3 == hcl->_nil)
					{
						((hcl_oop_oop_t)t2)->slot[0] = t1;
						if (b1 == 1) HCL_STACK_PUSH (hcl, t2); /* push self again */
					}
					else
					{
						hcl_oop_t t;

						t = hcl_makecons(hcl, t1, hcl->_nil);
						if (HCL_UNLIKELY(!t)) goto oops;

						((hcl_oop_oop_t)t3)->slot[1] = t;
						if (b1 == 1) HCL_STACK_PUSH (hcl, t);
					}
				}
				else if (b1 == 2)
				{
					if (t3 == hcl->_nil)
					{
						((hcl_oop_oop_t)t2)->slot[1] = t1;
					} 
					else
					{
						((hcl_oop_oop_t)t3)->slot[1] = t1;
					}
				}
#endif
				break;
			}

			case HCL_CODE_POP_INTO_CONS_END:
			{
				hcl_oop_t t1, t2, t3;
				LOG_INST_0 (hcl, "pop_into_cons_end");

				t1 = HCL_STACK_GETTOP(hcl); /* value to store */
				HCL_STACK_POP (hcl);

				t3 = HCL_STACK_GETTOP(hcl); /* sentinnel */
				HCL_STACK_POP (hcl);

				t2 = HCL_STACK_GETTOP(hcl); /* head cons */
				if (HCL_UNLIKELY(!HCL_IS_CONS(hcl, t2)))
				{
					hcl_seterrbfmt (hcl, HCL_EINTERN, "internal error - invalid vm state detected in pop_into_cons");
					goto oops;
				}

				if (t3 == hcl->_nil)
				{
					((hcl_oop_oop_t)t2)->slot[0] = t1;
				}
				else
				{
					hcl_oop_t t;

					hcl_pushvolat (hcl, &t3);
					t = hcl_makecons(hcl, t1, hcl->_nil);
					hcl_popvolat (hcl);
					if (HCL_UNLIKELY(!t)) goto oops;

					((hcl_oop_oop_t)t3)->slot[1] = t;
				}

				break;
			}

			case HCL_CODE_POP_INTO_CONS_CDR:
			{
				hcl_oop_t t1, t2, t3;
				LOG_INST_0 (hcl, "pop_into_cons_end");

				t1 = HCL_STACK_GETTOP(hcl); /* value to store */
				HCL_STACK_POP (hcl);

				t3 = HCL_STACK_GETTOP(hcl); /* sentinnel */
				HCL_STACK_POP (hcl);

				t2 = HCL_STACK_GETTOP(hcl); /* head cons */
				if (HCL_UNLIKELY(!HCL_IS_CONS(hcl, t2)))
				{
					hcl_seterrbfmt (hcl, HCL_EINTERN, "internal error - invalid vm state detected in pop_into_cons");
					goto oops;
				}

				if (t3 == hcl->_nil)
				{
					((hcl_oop_oop_t)t2)->slot[1] = t1;
				} 
				else
				{
					((hcl_oop_oop_t)t3)->slot[1] = t1;
				}

				/* no push back of the sentinnel */
				break;
			}

			/* -------------------------------------------------------- */

			case HCL_CODE_DUP_STACKTOP:
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
				 * after evaluation of an expression. so it's likely the
				 * return value of the last expression unless explicit
				 * returning is performed */
				hcl->last_retv = HCL_STACK_GETTOP(hcl);
				HCL_STACK_POP (hcl);
				break;

			case HCL_CODE_RETURN_STACKTOP:
				LOG_INST_0 (hcl, "return_stacktop");
				return_value = HCL_STACK_GETTOP(hcl);
				HCL_STACK_POP (hcl);
				goto handle_return;

			case HCL_CODE_RETURN_RECEIVER:
				LOG_INST_0 (hcl, "return_receiver");
				return_value = hcl->active_context->origin->receiver_or_base;

			handle_return:
				hcl->last_retv = return_value;
				if (do_return(hcl, return_value) <= -1) goto oops;
				break;

			case HCL_CODE_RETURN_FROM_BLOCK:
				LOG_INST_0 (hcl, "return_from_block");

				HCL_ASSERT(hcl, HCL_IS_CONTEXT(hcl, hcl->active_context));
				hcl->last_retv = HCL_STACK_GETTOP(hcl);

				do_return_from_block (hcl);
				break;

			case HCL_CODE_MAKE_FUNCTION:
			{
				hcl_oop_function_t func;
				hcl_oow_t b3, b4, i, j;
				hcl_oow_t joff;

				/* b1 - number of block arguments
				 * b2 - number of block temporaries
				 * b3 - literal frame base
				 * b4 - literal frame size */
				FETCH_PARAM_CODE_TO (hcl, b1);
				FETCH_PARAM_CODE_TO (hcl, b2);
				FETCH_PARAM_CODE_TO (hcl, b3);
				FETCH_PARAM_CODE_TO (hcl, b4);

				LOG_INST_4 (hcl, "make_function %zu %zu %zu %zu", b1, b2, b3, b4);

				HCL_ASSERT (hcl, b1 >= 0);
				HCL_ASSERT (hcl, b2 >= b1);

				/* the MAKE_FUNCTION instruction is followed by the long JUMP_FORWARD_X instruction.
				* i can decode the instruction and get the size of instructions
				* of the block context */
				HCL_ASSERT (hcl, hcl->active_code[hcl->ip] == HCL_CODE_JUMP_FORWARD_X);
				joff = hcl->active_code[hcl->ip + 1];
			#if (HCL_CODE_LONG_PARAM_SIZE == 2)
				joff = (joff << 8) | hcl->active_code[hcl->ip + 2];
			#endif

				/* copy the byte codes from the active context to the new context */
			#if (HCL_CODE_LONG_PARAM_SIZE == 2)
				func = make_function(hcl, b4, &hcl->active_code[hcl->ip + 3], joff, HCL_NULL);
			#else
				func = make_function(hcl, b4, &hcl->active_code[hcl->ip + 2], joff, HCL_NULL);
			#endif
				if (HCL_UNLIKELY(!func)) goto oops;

				fill_function_data (hcl, func, b1, b2, hcl->active_context, &hcl->active_function->literal_frame[b3], b4);

				/* push the new function to the stack of the active context */
				HCL_STACK_PUSH (hcl, (hcl_oop_t)func);
				break;
			}

			case HCL_CODE_MAKE_BLOCK:
			{
				hcl_oop_block_t blkobj;

				/* b1 - number of block arguments
				 * b2 - number of block temporaries */
				FETCH_PARAM_CODE_TO (hcl, b1);
				FETCH_PARAM_CODE_TO (hcl, b2);

				LOG_INST_2 (hcl, "make_block %zu %zu", b1, b2);

				HCL_ASSERT (hcl, b1 >= 0);
				HCL_ASSERT (hcl, b2 >= b1);

				blkobj = make_block(hcl);
				if (HCL_UNLIKELY(!blkobj)) goto oops;

				/* the long forward jump instruction has the format of 
				 *   11000100 KKKKKKKK or 11000100 KKKKKKKK KKKKKKKK 
				 * depending on HCL_CODE_LONG_PARAM_SIZE. change 'ip' to point to
				 * the instruction after the jump. */
				fill_block_data (hcl, blkobj, b1, b2, hcl->ip + HCL_CODE_LONG_PARAM_SIZE + 1, hcl->active_context);

				/* push the new block context to the stack of the active context */
				HCL_STACK_PUSH (hcl, (hcl_oop_t)blkobj);
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
	hcl->gci.lazy_sweep = 1;

	vm_cleanup (hcl);
#if defined(HCL_PROFILE_VM)
	HCL_LOG1 (hcl, HCL_LOG_IC | HCL_LOG_INFO, "TOTAL INST COUTNER = %zu\n", inst_counter);
#endif
	return 0;

oops:
	hcl->gci.lazy_sweep = 1;

	/* TODO: anything to do here? */
	if (hcl->processor->active != hcl->nil_process) 
	{
		HCL_LOG1 (hcl, HCL_LOG_IC | HCL_LOG_INFO, "TERMINATING ACTIVE PROCESS %zd for execution error\n", HCL_OOP_TO_SMOOI(hcl->processor->active->id));
		terminate_process (hcl, hcl->processor->active);
	}
	return -1;
}

hcl_oop_t hcl_execute (hcl_t* hcl)
{
	hcl_oop_function_t func;
	int n;
	hcl_bitmask_t log_default_type_mask;

	HCL_ASSERT (hcl, hcl->code.bc.len < HCL_SMOOI_MAX); /* asserted by the compiler */

	log_default_type_mask = hcl->log.default_type_mask;
	hcl->log.default_type_mask |= HCL_LOG_VM;

	HCL_ASSERT (hcl, hcl->initial_context == HCL_NULL);
	HCL_ASSERT (hcl, hcl->active_context == HCL_NULL);

	/* the code generated doesn't cater for its use as an initial funtion.
	 * mutate the generated code so that the intiail function can break
	 * out of the execution loop in execute() smoothly */

	if (hcl->code.bc.len > 0)
	{
		HCL_ASSERT (hcl, hcl->code.bc.ptr[hcl->code.bc.len - 1] == HCL_CODE_POP_STACKTOP);
	#if 1
		/* append RETURN_FROM_BLOCK 
		if (hcl_emitbyteinstruction(hcl, HCL_CODE_RETURN_FROM_BLOCK) <= -1) return -1;*/
		/* substitute RETURN_FROM_BLOCK for POP_STACKTOP) */
		hcl->code.bc.ptr[hcl->code.bc.len - 1] = HCL_CODE_RETURN_FROM_BLOCK;
	#else
		/* substitute RETURN_STACKTOP for POP_STACKTOP) */
		hcl->code.bc.ptr[hcl->code.bc.len - 1] = HCL_CODE_RETURN_STACKTOP;
	#endif
	}

	/* create a virtual function object that hold the bytes codes generated */
	func = make_function(hcl, hcl->code.lit.len, hcl->code.bc.ptr, hcl->code.bc.len, hcl->code.locptr);
	if (HCL_UNLIKELY(!func)) return HCL_NULL;

	/* pass nil for the home context of the initial function */
	fill_function_data (hcl, func, 0, 0, (hcl_oop_context_t)hcl->_nil, hcl->code.lit.arr->slot, hcl->code.lit.len);

	hcl->initial_function = func; /* the initial function is ready */

#if 0 
	/* unless the system is buggy, hcl->proc_map_used should be 0.
	 * the standard library terminates all processes before halting.
	 *
	 * [EXPERIMENTAL]
	 * if you like the process allocation to start from 0, uncomment
	 * the following 'if' block */
	if (hcl->proc_map_capa > 0 && hcl->proc_map_used == 0)
	{
		/* rechain the process map. it must be compatible with prepare_to_alloc_pid().
		 * by placing the low indiced slot at the beginning of the free list, 
		 * the special processes (main_proc, gcfin_proc, ossig_proc) are allocated
		 * with low process IDs. */
		hcl_ooi_t i, j;

		hcl->proc_map_free_first = 0;
		for (i = 0, j = 1; j < hcl->proc_map_capa; i++, j++)
		{
			hcl->proc_map[i] = HCL_SMOOI_TO_OOP(j);
		}
		hcl->proc_map[i] = HCL_SMOOI_TO_OOP(-1);
		hcl->proc_map_free_last = i;
	}
#endif

	n = start_initial_process_and_context(hcl, 0); /*  set up the initial context over the initial function */
	if (n >= 0) 
	{
		hcl->last_retv = hcl->_nil;

		n = execute(hcl);
		HCL_INFO1 (hcl, "RETURNED VALUE - %O\n", hcl->last_retv);
	}

	hcl->initial_context = HCL_NULL;
	hcl->active_context = HCL_NULL;
	HCL_ASSERT (hcl, hcl->processor->total_count == HCL_SMOOI_TO_OOP(0));
	HCL_ASSERT (hcl, hcl->processor->active == hcl->nil_process);
	LOAD_ACTIVE_SP (hcl); /* sync hcl->nil_process->sp with hcl->sp */
	HCL_ASSERT (hcl, hcl->sp == -1);

#if defined(HCL_PROFILE_VM)
	HCL_LOG2 (hcl, HCL_LOG_IC | HCL_LOG_INFO, "GC - gci.bsz: %zu, gci.stack.max: %zu\n", hcl->gci.bsz, hcl->gci.stack.max);
	if (hcl->heap->xma) hcl_xma_dump (hcl->heap->xma, xma_dumper, hcl);
	HCL_LOG2 (hcl, HCL_LOG_IC | HCL_LOG_INFO, "GC - gci.stat.alloc: %ld.%09u\n", (unsigned long int)hcl->gci.stat.alloc.sec, (unsigned int)hcl->gci.stat.alloc.nsec);
	HCL_LOG2 (hcl, HCL_LOG_IC | HCL_LOG_INFO, "GC - gci.stat.mark: %ld.%09u\n", (unsigned long int)hcl->gci.stat.mark.sec, (unsigned int)hcl->gci.stat.mark.nsec);
	HCL_LOG2 (hcl, HCL_LOG_IC | HCL_LOG_INFO, "GC - gci.stat.sweep: %ld.%09u\n", (unsigned long int)hcl->gci.stat.sweep.sec, (unsigned int)hcl->gci.stat.sweep.nsec);
#endif

	hcl->log.default_type_mask = log_default_type_mask;
	return (n <= -1)? HCL_NULL: hcl->last_retv;
}

void hcl_abort (hcl_t* hcl)
{
	hcl->abort_req = 1;
}
