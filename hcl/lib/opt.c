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

#include "hcl-opt.h"
#include "hcl-utl.h"

#define BADCH   '?'
#define BADARG  ':'

static hcl_uch_t EMSG_UCH[] = { '\0' };
static hcl_bch_t EMSG_BCH[] = { '\0' };

/* ------------------------------------------------------------ */

#undef XEMSG
#undef xch_t
#undef xci_t
#undef xopt_t
#undef xopt_lng_t
#undef xgetopt
#undef xcompcharscstr
#undef xfindcharincstr
#undef XCI_EOF

#define XEMSG EMSG_UCH
#define xch_t hcl_uch_t
#define xci_t hcl_uci_t
#define xopt_t hcl_uopt_t
#define xopt_lng_t hcl_uopt_lng_t
#define xgetopt hcl_getuopt
#define xcompcharscstr hcl_compucharsucstr
#define xfindcharincstr hcl_finducharinucstr
#define XCI_EOF HCL_BCI_EOF
#include "opt-impl.h"

/* ------------------------------------------------------------ */

#undef XEMSG
#undef xch_t
#undef xci_t
#undef xopt_t
#undef xopt_lng_t
#undef xgetopt
#undef xcompcharscstr
#undef xfindcharincstr
#undef XCI_EOF

#define XEMSG EMSG_BCH
#define xch_t hcl_bch_t
#define xci_t hcl_bci_t
#define xopt_t hcl_bopt_t
#define xopt_lng_t hcl_bopt_lng_t
#define xgetopt hcl_getbopt
#define xcompcharscstr hcl_compbcharsbcstr
#define xfindcharincstr hcl_findbcharinbcstr
#define XCI_EOF HCL_UCI_EOF
#include "opt-impl.h"

/* ------------------------------------------------------------ */
