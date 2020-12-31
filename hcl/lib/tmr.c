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
    IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WAfRRANTIES
    OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
    IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
    INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
    NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
    DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
    THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
    THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <hcl-tmr.h>
#include "hcl-prv.h"

#define HEAP_PARENT(x) (((x) - 1) / 2)
#define HEAP_LEFT(x)   ((x) * 2 + 1)
#define HEAP_RIGHT(x)  ((x) * 2 + 2)

#define YOUNGER_THAN(x,y) (HCL_CMP_NTIME(&(x)->when, &(y)->when) < 0)

hcl_tmr_t* hcl_tmr_open (hcl_t* hcl, hcl_oow_t xtnsize, hcl_oow_t capa)
{
	hcl_tmr_t* tmr;

	tmr = (hcl_tmr_t*)hcl_allocmem(hcl, HCL_SIZEOF(*tmr) + xtnsize);
	if (tmr)
	{
		if (hcl_tmr_init(tmr, hcl, capa) <= -1)
		{
			hcl_freemem (tmr->hcl, tmr);
			return HCL_NULL;
		}
		else HCL_MEMSET (tmr + 1, 0, xtnsize);
	}

	return tmr;
}

void hcl_tmr_close (hcl_tmr_t* tmr)
{
	hcl_tmr_fini (tmr);
	hcl_freemem (tmr->hcl, tmr);
}

int hcl_tmr_init (hcl_tmr_t* tmr, hcl_t* hcl, hcl_oow_t capa)
{
	hcl_tmr_event_t* tmp;

	HCL_MEMSET (tmr, 0, HCL_SIZEOF(*tmr));

	if (capa <= 0) capa = 1;

	tmp = (hcl_tmr_event_t*)hcl_allocmem(hcl, capa * HCL_SIZEOF(*tmp));
	if (!tmp) return -1;

	tmr->hcl = hcl;
	tmr->capa = capa;
	tmr->event = tmp;

	return 0;
}

void hcl_tmr_fini (hcl_tmr_t* tmr)
{
	hcl_tmr_clear (tmr);
	if (tmr->event) 
	{
		hcl_freemem (tmr->hcl, tmr->event);
		tmr->event = HCL_NULL;
	}
}

/*
hcl_mmgr_t* hcl_tmr_getmmgr (hcl_tmr_t* tmr)
{
	return tmr->hcl->mmgr;
}*/

void* hcl_tmr_getxtn (hcl_tmr_t* tmr)
{
	return (void*)(tmr + 1);
}

void hcl_tmr_clear (hcl_tmr_t* tmr)
{
	while (tmr->size > 0) hcl_tmr_delete (tmr, 0);
}

static hcl_tmr_index_t sift_up (hcl_tmr_t* tmr, hcl_tmr_index_t index, int notify)
{
	hcl_oow_t parent;

	parent = HEAP_PARENT(index);
	if (index > 0 && YOUNGER_THAN(&tmr->event[index], &tmr->event[parent]))
	{
		hcl_tmr_event_t item;
		hcl_oow_t old_index;

		item = tmr->event[index]; 
		old_index = index;

		do
		{
			/* move down the parent to my current position */
			tmr->event[index] = tmr->event[parent];
			tmr->event[index].updater (tmr, parent, index, &tmr->event[index]);

			/* traverse up */
			index = parent;
			parent = HEAP_PARENT(parent);
		}
		while (index > 0 && YOUNGER_THAN(&item, &tmr->event[parent]));

		/* we send no notification if the item is added with hcl_tmr_insert()
		 * or updated with hcl_tmr_update(). the caller of these functions
		 * must rely on the return value. */
		tmr->event[index] = item;
		if (notify && index != old_index)
			tmr->event[index].updater (tmr, old_index, index, &tmr->event[index]);
	}

	return index;
}

static hcl_tmr_index_t sift_down (hcl_tmr_t* tmr, hcl_tmr_index_t index, int notify)
{
	hcl_oow_t base = tmr->size / 2;

	if (index < base) /* at least 1 child is under the 'index' position */
	{
		hcl_tmr_event_t item;
		hcl_oow_t old_index;

		item = tmr->event[index];
		old_index = index;

		do
		{
			hcl_oow_t left, right, younger;

			left = HEAP_LEFT(index);
			right = HEAP_RIGHT(index);

			if (right < tmr->size && YOUNGER_THAN(&tmr->event[right], &tmr->event[left]))
			{
				younger = right;
			}
			else
			{
				younger = left;
			}

			if (YOUNGER_THAN(&item, &tmr->event[younger])) break;

			tmr->event[index] = tmr->event[younger];
			tmr->event[index].updater (tmr, younger, index, &tmr->event[index]);

			index = younger;
		}
		while (index < base);
		
		tmr->event[index] = item;
		if (notify && index != old_index)
			tmr->event[index].updater (tmr, old_index, index, &tmr->event[index]);
	}

	return index;
}

void hcl_tmr_delete (hcl_tmr_t* tmr, hcl_tmr_index_t index)
{
	hcl_tmr_event_t item;

	HCL_ASSERT (tmr->hcl, index < tmr->size);

	item = tmr->event[index];
	tmr->event[index].updater (tmr, index, HCL_TMR_INVALID_INDEX, &tmr->event[index]);

	tmr->size = tmr->size - 1;
	if (tmr->size > 0 && index != tmr->size)
	{
		tmr->event[index] = tmr->event[tmr->size];
		tmr->event[index].updater (tmr, tmr->size, index, &tmr->event[index]);
		YOUNGER_THAN(&tmr->event[index], &item)? sift_up(tmr, index, 1): sift_down(tmr, index, 1);
	}
}

hcl_tmr_index_t hcl_tmr_insert (hcl_tmr_t* tmr, const hcl_tmr_event_t* event)
{
	hcl_tmr_index_t index = tmr->size;

	if (index >= tmr->capa)
	{
		hcl_tmr_event_t* tmp;
		hcl_oow_t new_capa;

		HCL_ASSERT (tmr->hcl, tmr->capa >= 1);
		new_capa = tmr->capa * 2;
		tmp = (hcl_tmr_event_t*)hcl_reallocmem(tmr->hcl, tmr->event, new_capa * HCL_SIZEOF(*tmp));
		if (!tmp) return HCL_TMR_INVALID_INDEX;

		tmr->event = tmp;
		tmr->capa = new_capa;
	}

	HCL_ASSERT (tmr->hcl, event->handler != HCL_NULL);
	HCL_ASSERT (tmr->hcl, event->updater != HCL_NULL);

	tmr->size = tmr->size + 1;
	tmr->event[index] = *event;
	return sift_up(tmr, index, 0);
}

hcl_tmr_index_t hcl_tmr_update (hcl_tmr_t* tmr, hcl_oow_t index, const hcl_tmr_event_t* event)
{
	hcl_tmr_event_t item;

	HCL_ASSERT (tmr->hcl, event->handler != HCL_NULL);
	HCL_ASSERT (tmr->hcl, event->updater != HCL_NULL);

	item = tmr->event[index];
	tmr->event[index] = *event;
	return YOUNGER_THAN(event, &item)? sift_up(tmr, index, 0): sift_down(tmr, index, 0);
}

int hcl_tmr_fire (hcl_tmr_t* tmr, const hcl_ntime_t* tm, hcl_oow_t* firecnt)
{
	hcl_ntime_t now;
	hcl_tmr_event_t event;
	hcl_oow_t fire_count = 0;

	/* if the current time is not specified, get it from the system */
	if (tm) now = *tm;
	/*else if (hcl_gettime(&now) <= -1) return -1;*/
	tmr->hcl->vmprim.vm_gettime (tmr->hcl, &now);

	while (tmr->size > 0)
	{
		if (HCL_CMP_NTIME(&tmr->event[0].when, &now) > 0) break;

		event = tmr->event[0];
		hcl_tmr_delete (tmr, 0); /* remove the registered event structure */

		fire_count++;
		event.handler (tmr, &now, &event); /* then fire the event */
	}

	if (firecnt) *firecnt = fire_count;
	return 0;
}

int hcl_tmr_gettmout (hcl_tmr_t* tmr, const hcl_ntime_t* tm, hcl_ntime_t* tmout)
{
	hcl_ntime_t now;

	/* time-out can't be calculated when there's no event scheduled */
	if (tmr->size <= 0) return -1;

	/* if the current time is not specified, get it from the system */
	if (tm) now = *tm;
	/*else if (hcl_gettime(&now) <= -1) return -1;*/
	tmr->hcl->vmprim.vm_gettime (tmr->hcl, &now);

	HCL_SUB_NTIME (tmout, &tmr->event[0].when, &now);
	if (tmout->sec < 0) HCL_CLEAR_NTIME (tmout);

	return 0;
}

hcl_tmr_event_t* hcl_tmr_getevent (hcl_tmr_t* tmr, hcl_tmr_index_t index)
{
	return (index < 0 || index >= tmr->size)? HCL_NULL: &tmr->event[index];
}
