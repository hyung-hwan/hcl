#include <hcl-json.h>
#include <hcl-utl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>

/* ========================================================================= */

typedef struct json_xtn_t json_xtn_t;
struct json_xtn_t
{
	int logfd;
	hcl_bitmask_t logmask;
	int logfd_istty;
	
	struct
	{
		hcl_bch_t buf[4096];
		hcl_oow_t len;
	} logbuf;

	int depth;
};
/* ========================================================================= */

static void* sys_alloc (hcl_mmgr_t* mmgr, hcl_oow_t size)
{
	return malloc(size);
}

static void* sys_realloc (hcl_mmgr_t* mmgr, void* ptr, hcl_oow_t size)
{
	return realloc(ptr, size);
}

static void sys_free (hcl_mmgr_t* mmgr, void* ptr)
{
	free (ptr);
}

static hcl_mmgr_t sys_mmgr =
{
	sys_alloc,
	sys_realloc,
	sys_free,
	HCL_NULL
};

/* ========================================================================= */

static int write_all (int fd, const hcl_bch_t* ptr, hcl_oow_t len)
{
	while (len > 0)
	{
		hcl_ooi_t wr;

		wr = write(fd, ptr, len);

		if (wr <= -1)
		{
		#if defined(EAGAIN) && defined(EWOULDBLOCK) && (EAGAIN == EWOULDBLOCK)
			if (errno == EAGAIN) continue;
		#else
			#if defined(EAGAIN)
			if (errno == EAGAIN) continue;
			#elif defined(EWOULDBLOCK)
			if (errno == EWOULDBLOCK) continue;
			#endif
		#endif

		#if defined(EINTR)
			/* TODO: would this interfere with non-blocking nature of this VM? */
			if (errno == EINTR) continue;
		#endif
			return -1;
		}

		ptr += wr;
		len -= wr;
	}

	return 0;
}


static int write_log (hcl_json_t* json, int fd, const hcl_bch_t* ptr, hcl_oow_t len)
{
	json_xtn_t* xtn;

	xtn = hcl_json_getxtn(json);

	while (len > 0)
	{
		if (xtn->logbuf.len > 0)
		{
			hcl_oow_t rcapa, cplen;

			rcapa = HCL_COUNTOF(xtn->logbuf.buf) - xtn->logbuf.len;
			cplen = (len >= rcapa)? rcapa: len;

			memcpy (&xtn->logbuf.buf[xtn->logbuf.len], ptr, cplen);
			xtn->logbuf.len += cplen;
			ptr += cplen;
			len -= cplen;

			if (xtn->logbuf.len >= HCL_COUNTOF(xtn->logbuf.buf))
			{
				write_all(fd, xtn->logbuf.buf, xtn->logbuf.len);
				xtn->logbuf.len = 0;
			}
		}
		else
		{
			hcl_oow_t rcapa;

			rcapa = HCL_COUNTOF(xtn->logbuf.buf);
			if (len >= rcapa)
			{
				write_all (fd, ptr, rcapa);
				ptr += rcapa;
				len -= rcapa;
			}
			else
			{
				memcpy (xtn->logbuf.buf, ptr, len);
				xtn->logbuf.len += len;
				ptr += len;
				len -= len;
				
			}
		}
	}

	return 0;
}

static void flush_log (hcl_json_t* json, int fd)
{
	json_xtn_t* xtn;
	xtn = hcl_json_getxtn(json);
	if (xtn->logbuf.len > 0)
	{
		write_all (fd, xtn->logbuf.buf, xtn->logbuf.len);
		xtn->logbuf.len = 0;
	}
}

static void log_write (hcl_json_t* json, hcl_bitmask_t mask, const hcl_ooch_t* msg, hcl_oow_t len)
{
	hcl_bch_t buf[256];
	hcl_oow_t ucslen, bcslen;
	json_xtn_t* xtn;
	hcl_oow_t msgidx;
	int n, logfd;

	xtn = hcl_json_getxtn(json);

	if (mask & HCL_LOG_STDERR)
	{
		/* the messages that go to STDERR don't get masked out */
		logfd = 2;
	}
	else
	{
		if (!(xtn->logmask & mask & ~HCL_LOG_ALL_LEVELS)) return;  /* check log types */
		if (!(xtn->logmask & mask & ~HCL_LOG_ALL_TYPES)) return;  /* check log levels */

		if (mask & HCL_LOG_STDOUT) logfd = 1;
		else
		{
			logfd = xtn->logfd;
			if (logfd <= -1) return;
		}
	}

/* TODO: beautify the log message.
 *       do classification based on mask. */
	if (!(mask & (HCL_LOG_STDOUT | HCL_LOG_STDERR)))
	{
		time_t now;
		char ts[32];
		size_t tslen;
		struct tm tm, *tmp;

		now = time(NULL);

	#if defined(__OS2__)
		tmp = _localtime(&now, &tm);
	#elif defined(HAVE_LOCALTIME_R)
		tmp = localtime_r(&now, &tm);
	#else
		tmp = localtime(&now);
	#endif
	#if defined(HAVE_STRFTIME_SMALL_Z)
		tslen = strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S %z ", tmp);
	#else
		tslen = strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S %Z ", tmp); 
	#endif
		if (tslen == 0) 
		{
			strcpy (ts, "0000-00-00 00:00:00 +0000");
			tslen = 25; 
		}

		write_log (json, logfd, ts, tslen);
	}

	if (logfd == xtn->logfd && xtn->logfd_istty)
	{
		if (mask & HCL_LOG_FATAL) write_log (json, logfd, "\x1B[1;31m", 7);
		else if (mask & HCL_LOG_ERROR) write_log (json, logfd, "\x1B[1;32m", 7);
		else if (mask & HCL_LOG_WARN) write_log (json, logfd, "\x1B[1;33m", 7);
	}

#if defined(HCL_OOCH_IS_UCH)
	msgidx = 0;
	while (len > 0)
	{
		ucslen = len;
		bcslen = HCL_COUNTOF(buf);

		n = hcl_conv_oochars_to_bchars_with_cmgr(&msg[msgidx], &ucslen, buf, &bcslen, hcl_get_utf8_cmgr());
		if (n == 0 || n == -2)
		{
			/* n = 0: 
			 *   converted all successfully 
			 * n == -2: 
			 *    buffer not sufficient. not all got converted yet.
			 *    write what have been converted this round. */

			/*HCL_ASSERT (hcl, ucslen > 0); */ /* if this fails, the buffer size must be increased */
			/*assert (ucslen > 0);*/

			/* attempt to write all converted characters */
			if (write_log(json, logfd, buf, bcslen) <= -1) break;

			if (n == 0) break;
			else
			{
				msgidx += ucslen;
				len -= ucslen;
			}
		}
		else if (n <= -1)
		{
			/* conversion error */
			if (bcslen <= 0) break;
			if (write_log(json, logfd, buf, bcslen) <= -1) break;
			msgidx += ucslen;
			len -= ucslen;
		}
	}
#else
	write_log (json, logfd, msg, len);
#endif

	if (logfd == xtn->logfd && xtn->logfd_istty)
	{
		if (mask & (HCL_LOG_FATAL | HCL_LOG_ERROR | HCL_LOG_WARN)) write_log (json, logfd, "\x1B[0m", 4);
	}

	flush_log (json, logfd);
}

static int instcb (hcl_json_t* json, hcl_json_inst_t it, const hcl_oocs_t* str)
{
	json_xtn_t* json_xtn;

	json_xtn = hcl_json_getxtn(json);

	switch (it)
	{
		case HCL_JSON_INST_START_ARRAY:
			json_xtn->depth++;
			hcl_json_logbfmt (json, HCL_LOG_INFO | HCL_LOG_APP,  "[\n");
			break;
		case HCL_JSON_INST_END_ARRAY:
			json_xtn->depth--;
			hcl_json_logbfmt (json, HCL_LOG_INFO | HCL_LOG_APP,  "]\n");
			break;
		case HCL_JSON_INST_START_DIC:
			json_xtn->depth++;
			hcl_json_logbfmt (json, HCL_LOG_INFO | HCL_LOG_APP,  "{\n");
			break;
		case HCL_JSON_INST_END_DIC:
			json_xtn->depth--;
			hcl_json_logbfmt (json, HCL_LOG_INFO | HCL_LOG_APP,  "}\n");
			break;

		case HCL_JSON_INST_KEY:
			hcl_json_logbfmt (json, HCL_LOG_INFO | HCL_LOG_APP,  "%.*js: ", str->len, str->ptr);
			break;

		case HCL_JSON_INST_CHARACTER:
		case HCL_JSON_INST_STRING:
		case HCL_JSON_INST_NUMBER:
		case HCL_JSON_INST_TRUE:
		case HCL_JSON_INST_FALSE:
		case HCL_JSON_INST_NIL:
			hcl_json_logbfmt (json, HCL_LOG_INFO | HCL_LOG_APP,  "%.*js\n", str->len, str->ptr);
			break;
	}

	return 0;
}
/* ========================================================================= */

int main (int argc, char* argv[])
{

	hcl_json_t* json;
	hcl_json_prim_t json_prim;
	json_xtn_t* json_xtn;
	hcl_oow_t xlen;
	const char* p;

	memset (&json_prim, 0, HCL_SIZEOF(json_prim));
	json_prim.log_write = log_write;
	json_prim.instcb = instcb;

	json = hcl_json_open (&sys_mmgr, HCL_SIZEOF(json_xtn_t), &json_prim, NULL);

	json_xtn = hcl_json_getxtn(json);
	json_xtn->logmask = HCL_LOG_ALL_LEVELS | HCL_LOG_ALL_TYPES;

	p = "[ \"ab\\xab\\uC88B\\uC544\\uC6A9c\", \"kaden\", \"iron\", true, { \"null\": \"a\\1bc\", \"123\": \"AA20AA\", \"10\": -0.123, \"way\": '\\uC88A' } ]";
	/*p = "{ \"result\": \"SUCCESS\", \"message\": \"1 clients\", \"sessions\": [] }";*/

	if (hcl_json_feed(json, p, strlen(p), &xlen) <= -1)
	{	
		hcl_json_logbfmt (json, HCL_LOG_FATAL | HCL_LOG_APP, "ERROR: unable to process - %js\n", hcl_json_geterrmsg(json));
	}
	else if (json_xtn->depth != 0)
	{
		hcl_json_logbfmt (json, HCL_LOG_FATAL | HCL_LOG_APP, "ERROR: incomplete input\n");
	}

	hcl_json_close (json);
	return 0;
}
