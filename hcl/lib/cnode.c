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

static hcl_cnode_t* make_cnode (hcl_t* hcl, hcl_cnode_type_t type, const hcl_ioloc_t* loc, hcl_oow_t extra_space)
{
	hcl_cnode_t* cnode;

	cnode = hcl_callocmem(hcl, HCL_SIZEOF(*cnode) + extra_space);
	if (HCL_UNLIKELY(!cnode)) return HCL_NULL;

	cnode->type = type;
	cnode->loc = *loc;
	return cnode;
}

hcl_cnode_t* hcl_makecnodenil (hcl_t* hcl, const hcl_ioloc_t* loc)
{
	return make_cnode(hcl, HCL_CNODE_NIL, loc, 0);
}

hcl_cnode_t* hcl_makecnodetrue (hcl_t* hcl, const hcl_ioloc_t* loc)
{
	return make_cnode(hcl, HCL_CNODE_TRUE, loc, 0);
}

hcl_cnode_t* hcl_makecnodefalse (hcl_t* hcl, const hcl_ioloc_t* loc)
{
	return make_cnode(hcl, HCL_CNODE_FALSE, loc, 0);
}

hcl_cnode_t* hcl_makecnodecharlit (hcl_t* hcl, const hcl_ioloc_t* loc, const hcl_ooch_t ch)
{
	hcl_cnode_t* c =  make_cnode(hcl, HCL_CNODE_CHARLIT, loc, 0);
	if (HCL_UNLIKELY(!c)) return HCL_NULL;
	c->u.charlit.v = ch;
	return c;
}

hcl_cnode_t* hcl_makecnodesymbol (hcl_t* hcl, const hcl_ioloc_t* loc, int dotted, const hcl_ooch_t* ptr, hcl_oow_t len)
{
	hcl_cnode_t* c =  make_cnode(hcl, HCL_CNODE_SYMBOL, loc, HCL_SIZEOF(*ptr) * (len + 1));
	if (HCL_UNLIKELY(!c)) return HCL_NULL;
	c->u.symbol.dotted = dotted;
	c->u.symbol.ptr = (hcl_ooch_t*)(c + 1);
	c->u.symbol.len = len;
	hcl_copy_oochars (c->u.symbol.ptr, ptr, len);
	c->u.symbol.ptr[len] = '\0';
	return c;
}

hcl_cnode_t* hcl_makecnodestrlit (hcl_t* hcl, const hcl_ioloc_t* loc, const hcl_ooch_t* ptr, hcl_oow_t len)
{
	hcl_cnode_t* c =  make_cnode(hcl, HCL_CNODE_STRLIT, loc, HCL_SIZEOF(*ptr) * (len + 1));
	if (HCL_UNLIKELY(!c)) return HCL_NULL;
	c->u.strlit.ptr = (hcl_ooch_t*)(c + 1);
	c->u.strlit.len = len;
	hcl_copy_oochars (c->u.strlit.ptr, ptr, len);
	c->u.strlit.ptr[len] = '\0';
	return c;
}

hcl_cnode_t* hcl_makecnodenumlit (hcl_t* hcl, const hcl_ioloc_t* loc, const hcl_ooch_t* ptr, hcl_oow_t len)
{
	hcl_cnode_t* c =  make_cnode(hcl, HCL_CNODE_NUMLIT, loc, HCL_SIZEOF(*ptr) * (len + 1));
	if (HCL_UNLIKELY(!c)) return HCL_NULL;
	c->u.numlit.ptr = (hcl_ooch_t*)(c + 1);
	c->u.numlit.len = len;
	hcl_copy_oochars (c->u.numlit.ptr, ptr, len);
	c->u.numlit.ptr[len] = '\0';
	return c;
}

hcl_cnode_t* hcl_makecnoderadnumlit (hcl_t* hcl, const hcl_ioloc_t* loc, const hcl_ooch_t* ptr, hcl_oow_t len)
{
	hcl_cnode_t* c =  make_cnode(hcl, HCL_CNODE_RADNUMLIT, loc, HCL_SIZEOF(*ptr) * (len + 1));
	if (HCL_UNLIKELY(!c)) return HCL_NULL;

	c->u.radnumlit.ptr = (hcl_ooch_t*)(c + 1);
	c->u.radnumlit.len = len;
	hcl_copy_oochars (c->u.radnumlit.ptr, ptr, len);
	c->u.radnumlit.ptr[len] = '\0';
	return c;
}

hcl_cnode_t* hcl_makecnodefpdeclit (hcl_t* hcl, const hcl_ioloc_t* loc, const hcl_ooch_t* ptr, hcl_oow_t len)
{
	hcl_cnode_t* c =  make_cnode(hcl, HCL_CNODE_FPDECLIT, loc, HCL_SIZEOF(*ptr) * (len + 1));
	if (HCL_UNLIKELY(!c)) return HCL_NULL;
	c->u.fpdeclit.ptr = (hcl_ooch_t*)(c + 1);
	c->u.fpdeclit.len = len;
	hcl_copy_oochars (c->u.fpdeclit.ptr, ptr, len);
	c->u.fpdeclit.ptr[len] = '\0';
	return c;
}

hcl_cnode_t* hcl_makecnodesmptrlit (hcl_t* hcl, const hcl_ioloc_t*  loc, hcl_oow_t v)
{
	hcl_cnode_t* c =  make_cnode(hcl, HCL_CNODE_SMPTRLIT, loc, 0);
	if (HCL_UNLIKELY(!c)) return HCL_NULL;
	c->u.smptrlit.v = v;
	return c;
}

hcl_cnode_t* hcl_makecnodeerrlit (hcl_t* hcl, const hcl_ioloc_t* loc, hcl_ooi_t v)
{
	hcl_cnode_t* c =  make_cnode(hcl, HCL_CNODE_ERRLIT, loc, 0);
	if (HCL_UNLIKELY(!c)) return HCL_NULL;
	c->u.errlit.v = v;
	return c;
}

hcl_cnode_t* hcl_makecnodecons (hcl_t* hcl, const hcl_ioloc_t* loc, hcl_cnode_t* car, hcl_cnode_t* cdr)
{
	hcl_cnode_t* c =  make_cnode(hcl, HCL_CNODE_CONS, loc, 0);
	if (HCL_UNLIKELY(!c)) return HCL_NULL;
	c->u.cons.car = car;
	c->u.cons.cdr = cdr;
	return c;
}

hcl_cnode_t* hcl_makecnodelist (hcl_t* hcl, const hcl_ioloc_t* loc, hcl_concode_t type, hcl_cnode_t* head)
{
	hcl_cnode_t* c =  make_cnode(hcl, HCL_CNODE_LIST, loc, 0);
	if (HCL_UNLIKELY(!c)) return HCL_NULL;
	c->u.list.type = type;
	c->u.list.head = head;
	return c;
}

void hcl_freesinglecnode (hcl_t* hcl, hcl_cnode_t* c)
{
	hcl_freemem (hcl, c);
}

void hcl_freecnode (hcl_t* hcl, hcl_cnode_t* c)
{
redo:
	switch (c->type)
	{
		case HCL_CNODE_LIST:
		{
			hcl_cnode_t* tmp;
			tmp = c->u.list.head;
			hcl_freemem (hcl, c);
			if (tmp)
			{
				c = tmp;
				goto redo;
			}
			break;
		}

		case HCL_CNODE_CONS:
		{
			hcl_cnode_t* tmp1, * tmp2;

			tmp1 = c->u.cons.car;
			tmp2 = c->u.cons.cdr;

			hcl_freemem (hcl, c);
			hcl_freecnode (hcl, tmp1); /* TODO: remove recursion? */

			if (tmp2)
			{
				c = tmp2;
				goto redo;
			}
		}

		default:
			hcl_freemem (hcl, c);
	}
}
