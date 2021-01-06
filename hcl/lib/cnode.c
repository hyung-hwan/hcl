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

static hcl_cnode_t* make_cnode (hcl_t* hcl, hcl_cnode_type_t type, const hcl_ioloc_t* loc, hcl_oow_t extra_space)
{
	hcl_cnode_t* cnode;

	cnode = hcl_allocmem(hcl, HCL_SIZEOF(*cnode) + extra_space);
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

hcl_cnode_t* hcl_makecnodestrlit (hcl_t* hcl, const hcl_ioloc_t* loc, const hcl_ooch_t* ptr, hcl_oow_t len)
{
	hcl_cnode_t* c =  make_cnode(hcl, HCL_CNODE_STRLIT, loc, HCL_SIZEOF(*ptr) * (len + 1));
	if (HCL_UNLIKELY(!c)) return HCL_NULL;

	c->u.strlit.ptr = (hawk_ooch_t*)(c + 1);
	c->u.strlit.len = len;
	hawk_copy_bchars (c->u.strlit.ptr, c->u.strlit.len, ptr, len);	
	return c;
}

hcl_cnode_t* hcl_makecnodeerrlit (hcl_t* hcl, const hcl_ioloc_t* loc, hcl_ooi_t v)
{
	hcl_cnode_t* c =  make_cnode(hcl, HCL_CNODE_ERRLIT, loc, HCL_SIZEOF(*ptr) * (len + 1));
	if (HCL_UNLIKELY(!c)) return HCL_NULL;

	c->u.errlit.v = v;
	return c;
}

