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

#ifndef _HCL_TMR_H_
#define _HCL_TMR_H_

#include "hcl-cmn.h"

typedef struct hcl_tmr_t hcl_tmr_t;
typedef struct hcl_tmr_event_t hcl_tmr_event_t;
typedef hcl_oow_t hcl_tmr_index_t;

typedef void (*hcl_tmr_handler_t) (
	hcl_tmr_t*         tmr,
	const hcl_ntime_t* now, 
	hcl_tmr_event_t*   evt
);

typedef void (*hcl_tmr_updater_t) (
	hcl_tmr_t*        tmr,
	hcl_tmr_index_t   old_index,
	hcl_tmr_index_t   new_index,
	hcl_tmr_event_t*  evt
);

struct hcl_tmr_t
{
	hcl_t*           hcl;
	hcl_oow_t        capa;
	hcl_oow_t        size;
	hcl_tmr_event_t* event;
};

struct hcl_tmr_event_t
{
	void*              ctx;    /* primary context pointer */
	void*              ctx2;   /* secondary context pointer */
	void*              ctx3;   /* tertiary context pointer */

	hcl_ntime_t        when;
	hcl_tmr_handler_t  handler;
	hcl_tmr_updater_t  updater;
};

#define HCL_TMR_INVALID_INDEX ((hcl_tmr_index_t)-1)

#define HCL_TMR_SIZE(tmr) ((tmr)->size)
#define HCL_TMR_CAPA(tmr) ((tmr)->capa);

#if defined(__cplusplus)
extern "C" {
#endif

HCL_EXPORT hcl_tmr_t* hcl_tmr_open (
	hcl_t*     mmgr, 
	hcl_oow_t  xtnsize,
	hcl_oow_t  capa
);

HCL_EXPORT void hcl_tmr_close (
	hcl_tmr_t* tmr
);

HCL_EXPORT int hcl_tmr_init (
	hcl_tmr_t*  tmr, 
	hcl_t*      mmgr,
	hcl_oow_t   capa
);

HCL_EXPORT void hcl_tmr_fini (
	hcl_tmr_t* tmr
);

/*
HCL_EXPORT hcl_mmgr_t* hcl_tmr_getmmgr (
	hcl_tmr_t* tmr
);*/

HCL_EXPORT void* hcl_tmr_getxtn (
	hcl_tmr_t* tmr
);

HCL_EXPORT void hcl_tmr_clear (
	hcl_tmr_t* tmr
);

/**
 * The hcl_tmr_insert() function schedules a new event.
 *
 * \return #HCL_TMR_INVALID_INDEX on failure, valid index on success.
 */

HCL_EXPORT hcl_tmr_index_t hcl_tmr_insert (
	hcl_tmr_t*             tmr,
	const hcl_tmr_event_t* event
);

HCL_EXPORT hcl_tmr_index_t hcl_tmr_update (
	hcl_tmr_t*             tmr,
	hcl_tmr_index_t        index,
	const hcl_tmr_event_t* event
);

HCL_EXPORT void hcl_tmr_delete (
	hcl_tmr_t*      tmr,
	hcl_tmr_index_t index
);

HCL_EXPORT int hcl_tmr_fire (
	hcl_tmr_t*         tmr,
	const hcl_ntime_t* tm,
	hcl_oow_t*         firecnt
);

HCL_EXPORT int hcl_tmr_gettmout (
	hcl_tmr_t*         tmr,
	const hcl_ntime_t* tm,
	hcl_ntime_t*       tmout
);

/**
 * The hcl_tmr_getevent() function returns the
 * pointer to the registered event at the given index.
 */
HCL_EXPORT hcl_tmr_event_t* hcl_tmr_getevent (
	hcl_tmr_t*        tmr,
	hcl_tmr_index_t   index
);

#if defined(__cplusplus)
}
#endif

#endif
