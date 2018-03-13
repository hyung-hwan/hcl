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

#include "hcl-s.h"
#include "hcl-opt.h"
#include "hcl-utl.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <locale.h>
#include <assert.h>


#if defined(HAVE_TIME_H)
#	include <time.h>
#endif
#if defined(HAVE_SYS_TIME_H)
#	include <sys/time.h>
#endif
#if defined(HAVE_SIGNAL_H)
#	include <signal.h>
#endif
#if defined(HAVE_SYS_MMAN_H)
#	include <sys/mman.h>
#endif

#include <errno.h>
#include <unistd.h>
#include <fcntl.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

/* ========================================================================= */

typedef struct server_xtn_t server_xtn_t;
struct xtn_t
{
	int logfd;
	unsigned int logmask;
	int logfd_istty;
};

/* ========================================================================= */

static hcl_server_t* g_server = HCL_NULL;

/* ========================================================================= */

typedef void (*signal_handler_t) (int, siginfo_t*, void*);

static void handle_sigint (int sig, siginfo_t* siginfo, void* ctx)
{
	if (g_server) hcl_server_stop (g_server);
}

static void set_signal (int sig, signal_handler_t handler)
{
	struct sigaction sa;

	memset (&sa, 0, sizeof(sa));
	/*sa.sa_handler = handler;*/
	sa.sa_flags = SA_SIGINFO;
	sa.sa_sigaction = handler;
	sigemptyset (&sa.sa_mask);

	sigaction (sig, &sa, NULL);
}

static void set_signal_to_ignore (int sig)
{
	struct sigaction sa; 

	memset (&sa, 0, sizeof(sa));
	sa.sa_handler = SIG_IGN;
	sa.sa_flags = 0;
	sigemptyset (&sa.sa_mask);

	sigaction (sig, &sa, NULL);
}

static void set_signal_to_default (int sig)
{
	struct sigaction sa; 

	memset (&sa, 0, sizeof(sa));
	sa.sa_handler = SIG_DFL;
	sa.sa_flags = 0;
	sigemptyset (&sa.sa_mask);

	sigaction (sig, &sa, NULL);
}

/* ========================================================================= */

static int handle_logopt (hcl_server_t* server, const hcl_bch_t* str)
{
	hcl_bch_t* xstr = (hcl_bch_t*)str;
	hcl_bch_t* cm, * flt;
	unsigned int logmask;

	hcl_server_getoption (server, HCL_SERVER_LOG_MASK, &logmask);

	cm = strchr(xstr, ',');
	if (cm) 
	{
		/* i duplicate this string for open() below as open() doesn't 
		 * accept a length-bounded string */
		xstr = strdup (str);
		if (!xstr) 
		{
			fprintf (stderr, "ERROR: out of memory in duplicating %s\n", str);
			return -1;
		}

		cm = strchr(xstr, ',');
		*cm = '\0';

		do
		{
			flt = cm + 1;

			cm = strchr(flt, ',');
			if (cm) *cm = '\0';

			if (hcl_compbcstr(flt, "app") == 0) logmask |= HCL_LOG_APP;
			else if (hcl_compbcstr(flt, "compiler") == 0) logmask |= HCL_LOG_COMPILER;
			else if (hcl_compbcstr(flt, "vm") == 0) logmask |= HCL_LOG_VM;
			else if (hcl_compbcstr(flt, "mnemonic") == 0) logmask |= HCL_LOG_MNEMONIC;
			else if (hcl_compbcstr(flt, "gc") == 0) logmask |= HCL_LOG_GC;
			else if (hcl_compbcstr(flt, "ic") == 0) logmask |= HCL_LOG_IC;
			else if (hcl_compbcstr(flt, "primitive") == 0) logmask |= HCL_LOG_PRIMITIVE;

			else if (hcl_compbcstr(flt, "fatal") == 0) logmask |= HCL_LOG_FATAL;
			else if (hcl_compbcstr(flt, "error") == 0) logmask |= HCL_LOG_ERROR;
			else if (hcl_compbcstr(flt, "warn") == 0) logmask |= HCL_LOG_WARN;
			else if (hcl_compbcstr(flt, "info") == 0) logmask |= HCL_LOG_INFO;
			else if (hcl_compbcstr(flt, "debug") == 0) logmask |= HCL_LOG_DEBUG;

			else if (hcl_compbcstr(flt, "fatal+") == 0) logmask |= HCL_LOG_FATAL;
			else if (hcl_compbcstr(flt, "error+") == 0) logmask |= HCL_LOG_FATAL | HCL_LOG_ERROR;
			else if (hcl_compbcstr(flt, "warn+") == 0) logmask |= HCL_LOG_FATAL | HCL_LOG_ERROR | HCL_LOG_WARN;
			else if (hcl_compbcstr(flt, "info+") == 0) logmask |= HCL_LOG_FATAL | HCL_LOG_ERROR | HCL_LOG_WARN | HCL_LOG_INFO;
			else if (hcl_compbcstr(flt, "debug+") == 0) logmask |= HCL_LOG_FATAL | HCL_LOG_ERROR | HCL_LOG_WARN | HCL_LOG_INFO | HCL_LOG_DEBUG;

			else
			{
				fprintf (stderr, "ERROR: unknown log option value - %s\n", flt);
				if (str != xstr) free (xstr);
				return -1;
			}
		}
		while (cm);

		if (!(logmask & HCL_LOG_ALL_TYPES)) logmask |= HCL_LOG_ALL_TYPES;  /* no types specified. force to all types */
		if (!(logmask & HCL_LOG_ALL_LEVELS)) logmask |= HCL_LOG_ALL_LEVELS;  /* no levels specified. force to all levels */
	}
	else
	{
		logmask = HCL_LOG_ALL_LEVELS | HCL_LOG_ALL_TYPES;
	}

	hcl_server_setoption (server, HCL_SERVER_LOG_MASK, &logmask);

	server->logfd = open(xstr, O_CREAT | O_WRONLY | O_APPEND , 0644);
	if (server->logfd == -1)
	{
		fprintf (stderr, "ERROR: cannot open a log file %s\n", xstr);
		if (str != xstr) free (xstr);
		return -1;
	}

#if defined(HAVE_ISATTY)
	server->logfd_istty = isatty(server->logfd);
#endif

	if (str != xstr) free (xstr);
	return 0;
}

#if defined(HCL_BUILD_DEBUG)
static int parse_dbgopt (const char* str, unsigned int* dbgoptp)
{
	const hcl_bch_t* cm, * flt;
	hcl_oow_t len;
	unsigned int dbgopt = 0;

	cm = str - 1;
	do
	{
		flt = cm + 1;

		cm = strchr(flt, ',');
		len = cm? (cm - flt): strlen(flt);
		if (strncasecmp(flt, "gc", len) == 0)  dbgopt |= HCL_DEBUG_GC;
		else if (strncasecmp (flt, "bigint", len) == 0)  dbgopt |= HCL_DEBUG_BIGINT;
		else
		{
			fprintf (stderr, "ERROR: unknown debug option value - %.*s\n", (int)len, flt);
			return -1;
		}
	}
	while (cm);

	*dbgoptp = dbgopt;
	return 0;
}
#endif

/* ========================================================================= */

#define MIN_MEMSIZE 512000ul
 
int main (int argc, char* argv[])
{
	hcl_bci_t c;
	static hcl_bopt_lng_t lopt[] =
	{
		{ ":log",         'l' },
		{ ":memsize",     'm' },
		{ "large-pages",  '\0' },
	#if defined(HCL_BUILD_DEBUG)
		{ ":debug",       '\0' }, /* NOTE: there is no short option for --debug */
	#endif
		{ HCL_NULL,       '\0' }
	};
	static hcl_bopt_t opt =
	{
		"l:m:",
		lopt
	};

	hcl_server_t* server;
	server_xtn_t* xtn;
	int n;

	const char* logopt = HCL_NULL;
	hcl_oow_t memsize = MIN_MEMSIZE;
	int large_pages = 0;
	unsigned int dbgopt = 0;
	unsigned int trait;

	setlocale (LC_ALL, "");

	if (argc < 2)
	{
	print_usage:
		fprintf (stderr, "Usage: %s bind-address:port\n", argv[0]);
		return -1;
	}

	while ((c = hcl_getbopt (argc, argv, &opt)) != HCL_BCI_EOF)
	{
		switch (c)
		{
			case 'l':
				logopt = opt.arg;
				break;

			case 'm':
				memsize = strtoul(opt.arg, HCL_NULL, 0);
				if (memsize <= MIN_MEMSIZE) memsize = MIN_MEMSIZE;
				break;

			case '\0':
				if (hcl_compbcstr(opt.lngopt, "large-pages") == 0)
				{
					large_pages = 1;
					break;
				}
			#if defined(HCL_BUILD_DEBUG)
				else if (hcl_compbcstr(opt.lngopt, "debug") == 0)
				{
					if (parse_dbgopt (opt.arg, &dbgopt) <= -1) return -1;
					break;
				}
			#endif

				goto print_usage;

			case ':':
				if (opt.lngopt)
					fprintf (stderr, "bad argument for '%s'\n", opt.lngopt);
				else
					fprintf (stderr, "bad argument for '%c'\n", opt.opt);

				return -1;

			default:
				goto print_usage;
		}
	}

	if (opt.ind >= argc) goto print_usage;

	server = hcl_server_open(HCL_SIZEOF(xtn_t), dbgopt);
	if (!server)
	{
		fprintf (stderr, "cannot open server\n");
		return -1;
	}

	xtn = (server_xtn_t*)hcl_server_getxtn(server);
	xtn->logfd = -1;
	xtn->logfd_istty = 0;

	if (handle_logopt(server, logopt) <= -1) goto oops;

	hcl_server_getoption (server, HCL_SERVER_TRAIT, &trait);
	if (large_pages) trait |= HCL_SERVER_TRAIT_USE_LARGE_PAGES;
	else trait &= HCL_SERVER_TRAIT_USE_LARGE_PAGES;
	hcl_server_setoption (server, HCL_SERVER_TRAIT, &trait);

	/*hcl_server_setoption (server, HCL_SERVER_WORKER_STACK_SIZE, ???);*/
	hcl_server_setoption (server, HCL_SERVER_ACTOR_HEAP_SIZE, &memsize);
	/*hcl_server_setoption (server, HCL_SERVER_ACTOR_DEBUG, &memsize);*/

	g_server = server;
	set_signal (SIGINT, handle_sigint);
	set_signal_to_ignore (SIGPIPE);

	n = hcl_server_start(server, argv[opt.ind]);
	
	set_signal_to_default (SIGINT);
	set_signal_to_default (SIGPIPE);
	g_server = NULL;

	hcl_server_close (server);
	return n;

oops:
	if (server) hcl_server_close (server);
	return -1;
}
