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

#include "hcl-prv.h"

hcl_cnode_t* hcl_makecnode (hcl_t* hcl, hcl_cnode_type_t type, int flags, const hcl_loc_t* loc, const hcl_oocs_t* tok)
{
	hcl_cnode_t* cnode;
	hcl_oocs_t empty;
	hcl_ooch_t dummy;

	if (!tok)
	{
		empty.ptr = &dummy;
		empty.len = 0;
		tok = &empty;
	}
	cnode = (hcl_cnode_t*)hcl_callocmem(hcl, HCL_SIZEOF(*cnode) + HCL_SIZEOF(*tok->ptr) * (tok->len + 1));
	if (HCL_UNLIKELY(!cnode)) return HCL_NULL;

	cnode->cn_type = type;
	cnode->cn_flags = flags;
	cnode->cn_loc = *loc;

	cnode->cn_tok.ptr = (hcl_ooch_t*)(cnode + 1);
	cnode->cn_tok.len = tok->len;
	hcl_copy_oochars (cnode->cn_tok.ptr, tok->ptr, tok->len);
	cnode->cn_tok.ptr[tok->len] = '\0';

	return cnode;
}

hcl_cnode_t* hcl_makecnodenil (hcl_t* hcl, int flags, const hcl_loc_t* loc, const  hcl_oocs_t* tok)
{
	return hcl_makecnode(hcl, HCL_CNODE_NIL, flags, loc, tok);
}

hcl_cnode_t* hcl_makecnodetrue (hcl_t* hcl, int flags, const hcl_loc_t* loc, const  hcl_oocs_t* tok)
{
	return hcl_makecnode(hcl, HCL_CNODE_TRUE, flags, loc, tok);
}

hcl_cnode_t* hcl_makecnodefalse (hcl_t* hcl, int flags, const hcl_loc_t* loc, const  hcl_oocs_t* tok)
{
	return hcl_makecnode(hcl, HCL_CNODE_FALSE, flags, loc, tok);
}

hcl_cnode_t* hcl_makecnodeself (hcl_t* hcl, int flags, const hcl_loc_t* loc, const  hcl_oocs_t* tok)
{
	return hcl_makecnode(hcl, HCL_CNODE_SELF, flags, loc, tok);
}

hcl_cnode_t* hcl_makecnodesuper (hcl_t* hcl, int flags, const hcl_loc_t* loc, const  hcl_oocs_t* tok)
{
	return hcl_makecnode(hcl, HCL_CNODE_SUPER, flags, loc, tok);
}

hcl_cnode_t* hcl_makecnodeellipsis (hcl_t* hcl, int flags, const hcl_loc_t* loc, const  hcl_oocs_t* tok)
{
	return hcl_makecnode(hcl, HCL_CNODE_ELLIPSIS, flags, loc, tok);
}

hcl_cnode_t* hcl_makecnodetrpcolons (hcl_t* hcl, int flags, const hcl_loc_t* loc, const  hcl_oocs_t* tok)
{
	return hcl_makecnode(hcl, HCL_CNODE_TRPCOLONS, flags, loc, tok);
}

hcl_cnode_t* hcl_makecnodedblcolons (hcl_t* hcl, int flags, const hcl_loc_t* loc, const  hcl_oocs_t* tok)
{
	return hcl_makecnode(hcl, HCL_CNODE_DBLCOLONS, flags, loc, tok);
}

hcl_cnode_t* hcl_makecnodecolon (hcl_t* hcl, int flags, const hcl_loc_t* loc, const  hcl_oocs_t* tok)
{
	return hcl_makecnode(hcl, HCL_CNODE_COLON, flags, loc, tok);
}

hcl_cnode_t* hcl_makecnodecolongt (hcl_t* hcl, int flags, const hcl_loc_t* loc, const  hcl_oocs_t* tok)
{
	return hcl_makecnode(hcl, HCL_CNODE_COLONGT, flags, loc, tok);
}

hcl_cnode_t* hcl_makecnodecolonlt (hcl_t* hcl, int flags, const hcl_loc_t* loc, const  hcl_oocs_t* tok)
{
	return hcl_makecnode(hcl, HCL_CNODE_COLONLT, flags, loc, tok);
}

hcl_cnode_t* hcl_makecnodecharlit (hcl_t* hcl, int flags, const hcl_loc_t* loc, const  hcl_oocs_t* tok, hcl_ooch_t v)
{
	hcl_cnode_t* c = hcl_makecnode(hcl, HCL_CNODE_CHARLIT, flags, loc, tok);
	if (HCL_UNLIKELY(!c)) return HCL_NULL;
	c->u.charlit.v = v;
	return c;
}

hcl_cnode_t* hcl_makecnodebchrlit (hcl_t* hcl, int flags, const hcl_loc_t* loc, const  hcl_oocs_t* tok, hcl_oob_t v)
{
	hcl_cnode_t* c = hcl_makecnode(hcl, HCL_CNODE_BCHRLIT, flags, loc, tok);
	if (HCL_UNLIKELY(!c)) return HCL_NULL;
	c->u.bchrlit.v = v;
	return c;
}

hcl_cnode_t* hcl_makecnodesymbol (hcl_t* hcl, int flags, const hcl_loc_t* loc, const  hcl_oocs_t* tok)
{
	return hcl_makecnode(hcl, HCL_CNODE_SYMBOL, flags, loc, tok);
}

hcl_cnode_t* hcl_makecnodedsymbol (hcl_t* hcl, int flags, const hcl_loc_t* loc, const hcl_oocs_t* tok, int is_cla)
{
	hcl_cnode_t* c = hcl_makecnode(hcl, HCL_CNODE_DSYMBOL, flags, loc, tok);
	if (HCL_UNLIKELY(!c)) return HCL_NULL;
	c->u.dsymbol.is_cla = is_cla;
	return c;
}

hcl_cnode_t* hcl_makecnodestrlit (hcl_t* hcl, int flags, const hcl_loc_t* loc, const hcl_oocs_t* tok)
{
	return hcl_makecnode(hcl, HCL_CNODE_STRLIT, flags, loc, tok);
}

hcl_cnode_t* hcl_makecnodebstrlit (hcl_t* hcl, int flags, const hcl_loc_t* loc, const hcl_oocs_t* tok)
{
	return hcl_makecnode(hcl, HCL_CNODE_BSTRLIT, flags, loc, tok);
}

hcl_cnode_t* hcl_makecnodesymlit (hcl_t* hcl, int flags, const hcl_loc_t* loc, const hcl_oocs_t* tok)
{
	return hcl_makecnode(hcl, HCL_CNODE_SYMLIT, flags, loc, tok);
}

hcl_cnode_t* hcl_makecnodenumlit (hcl_t* hcl, int flags, const hcl_loc_t* loc, const hcl_oocs_t* tok)
{
	return hcl_makecnode(hcl, HCL_CNODE_NUMLIT, flags, loc, tok);
}

hcl_cnode_t* hcl_makecnoderadnumlit (hcl_t* hcl, int flags, const hcl_loc_t* loc, const hcl_oocs_t* tok)
{
	return hcl_makecnode(hcl, HCL_CNODE_RADNUMLIT, flags, loc, tok);
}

hcl_cnode_t* hcl_makecnodefpdeclit (hcl_t* hcl, int flags, const hcl_loc_t* loc, const hcl_oocs_t* tok)
{
	return hcl_makecnode(hcl, HCL_CNODE_FPDECLIT, flags, loc, tok);
}

hcl_cnode_t* hcl_makecnodesmptrlit (hcl_t* hcl, int flags, const hcl_loc_t* loc, const hcl_oocs_t* tok, hcl_oow_t v)
{
	hcl_cnode_t* c = hcl_makecnode(hcl, HCL_CNODE_SMPTRLIT, flags,  loc, tok);
	if (HCL_UNLIKELY(!c)) return HCL_NULL;
	c->u.smptrlit.v = v;
	return c;
}

hcl_cnode_t* hcl_makecnodeerrlit (hcl_t* hcl, int flags, const hcl_loc_t* loc, const hcl_oocs_t* tok, hcl_ooi_t v)
{
	hcl_cnode_t* c = hcl_makecnode(hcl, HCL_CNODE_ERRLIT, flags, loc, tok);
	if (HCL_UNLIKELY(!c)) return HCL_NULL;
	c->u.errlit.v = v;
	return c;
}

hcl_cnode_t* hcl_makecnodecons (hcl_t* hcl, int flags, const hcl_loc_t* loc, const hcl_oocs_t* tok, hcl_cnode_t* car, hcl_cnode_t* cdr)
{
	hcl_cnode_t* c = hcl_makecnode(hcl, HCL_CNODE_CONS, flags, loc, tok);
	if (HCL_UNLIKELY(!c)) return HCL_NULL;
	c->u.cons.car = car;
	c->u.cons.cdr = cdr;
	return c;
}

hcl_cnode_t* hcl_makecnodeelist (hcl_t* hcl, int flags, const hcl_loc_t* loc, hcl_concode_t type)
{
	hcl_cnode_t* c = hcl_makecnode(hcl, HCL_CNODE_ELIST, flags, loc, HCL_NULL);
	if (HCL_UNLIKELY(!c)) return HCL_NULL;
	c->u.elist.concode = type;
	return c;
}

hcl_cnode_t* hcl_makecnodeshell (hcl_t* hcl, int flags, const hcl_loc_t* loc, hcl_cnode_t* obj)
{
	hcl_cnode_t* c = hcl_makecnode(hcl, HCL_CNODE_SHELL, flags, loc, HCL_NULL);
	if (HCL_UNLIKELY(!c)) return HCL_NULL;
	c->u.shell.obj = obj;
	return c;
}

void hcl_freesinglecnode (hcl_t* hcl, hcl_cnode_t* c)
{
	hcl_freemem (hcl, c);
}

void hcl_freecnode (hcl_t* hcl, hcl_cnode_t* c)
{
redo:
	switch (c->cn_type)
	{
		case HCL_CNODE_CONS:
		{
			hcl_cnode_t* tmp1, * tmp2;

			tmp1 = c->u.cons.car;
			tmp2 = c->u.cons.cdr;

			HCL_ASSERT (hcl, tmp1 != HCL_NULL);
			hcl_freemem (hcl, c);
			hcl_freecnode (hcl, tmp1); /* TODO: remove recursion? */
			if (tmp2)
			{
				c = tmp2;
				goto redo;
			}

			break;
		}

		case HCL_CNODE_SHELL:
		{
			hcl_cnode_t* tmp;

			tmp = c->u.shell.obj;
			hcl_freemem (hcl, c);
			if (tmp)
			{
				c = tmp;
				goto redo;
			}

			break;
		}

		default:
			hcl_freemem (hcl, c);
			break;
	}
}

hcl_oow_t hcl_countcnodecons (hcl_t* hcl, hcl_cnode_t* cons)
{
	/* this function ignores the last cdr */
	hcl_oow_t count = 1;

	HCL_ASSERT (hcl, HCL_CNODE_IS_CONS(cons));
	do
	{
		cons = HCL_CNODE_CONS_CDR(cons);
		if (!cons) break;
		count++;
	}
	while (1);

	return count;
}
