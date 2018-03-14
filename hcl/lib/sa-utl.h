static int str_to_ipv4 (const ooch_t* str, hcl_oow_t len, struct in_addr* inaddr)
{
	const ooch_t* end;
	int dots = 0, digits = 0;
	hcl_uint32_t acc = 0, addr = 0;	
	ooch_t c;

	end = str + len;

	do
	{
		if (str >= end)
		{
			if (dots < 3 || digits == 0) return -1;
			addr = (addr << 8) | acc;
			break;
		}

		c = *str++;

		if (c >= '0' && c <= '9') 
		{
			if (digits > 0 && acc == 0) return -1;
			acc = acc * 10 + (c - '0');
			if (acc > 255) return -1;
			digits++;
		}
		else if (c == '.') 
		{
			if (dots >= 3 || digits == 0) return -1;
			addr = (addr << 8) | acc;
			dots++; acc = 0; digits = 0;
		}
		else return -1;
	}
	while (1);

	inaddr->s_addr = hcl_hton32(addr);
	return 0;

}

#if (HCL_SIZEOF_STRUCT_SOCKADDR_IN6 > 0)
static int str_to_ipv6 (const ooch_t* src, hcl_oow_t len, struct in6_addr* inaddr)
{
	hcl_uint8_t* tp, * endp, * colonp;
	const ooch_t* curtok;
	ooch_t ch;
	int saw_xdigit;
	unsigned int val;
	const ooch_t* src_end;

	src_end = src + len;

	HCL_MEMSET (inaddr, 0, HCL_SIZEOF(*inaddr));
	tp = &inaddr->s6_addr[0];
	endp = &inaddr->s6_addr[HCL_COUNTOF(inaddr->s6_addr)];
	colonp = HCL_NULL;

	/* Leading :: requires some special handling. */
	if (src < src_end && *src == ':')
	{
		src++;
		if (src >= src_end || *src != ':') return -1;
	}

	curtok = src;
	saw_xdigit = 0;
	val = 0;

	while (src < src_end)
	{
		int v1;

		ch = *src++;

		if (ch >= '0' && ch <= '9')
			v1 = ch - '0';
		else if (ch >= 'A' && ch <= 'F')
			v1 = ch - 'A' + 10;
		else if (ch >= 'a' && ch <= 'f')
			v1 = ch - 'a' + 10;
		else v1 = -1;
		if (v1 >= 0)
		{
			val <<= 4;
			val |= v1;
			if (val > 0xffff) return -1;
			saw_xdigit = 1;
			continue;
		}

		if (ch == ':') 
		{
			curtok = src;
			if (!saw_xdigit) 
			{
				if (colonp) return -1;
				colonp = tp;
				continue;
			}
			else if (src >= src_end)
			{
				/* a colon can't be the last character */
				return -1;
			}

			*tp++ = (hcl_uint8_t)(val >> 8) & 0xff;
			*tp++ = (hcl_uint8_t)val & 0xff;
			saw_xdigit = 0;
			val = 0;
			continue;
		}

		if (ch == '.' && ((tp + HCL_SIZEOF(struct in_addr)) <= endp) &&
		    str_to_ipv4(curtok, src_end - curtok, (struct in_addr*)tp) == 0) 
		{
			tp += HCL_SIZEOF(struct in_addr*);
			saw_xdigit = 0;
			break; 
		}

		return -1;
	}

	if (saw_xdigit) 
	{
		if (tp + HCL_SIZEOF(hcl_uint16_t) > endp) return -1;
		*tp++ = (hcl_uint8_t)(val >> 8) & 0xff;
		*tp++ = (hcl_uint8_t)val & 0xff;
	}
	if (colonp != HCL_NULL) 
	{
		/*
		 * Since some memmove()'s erroneously fail to handle
		 * overlapping regions, we'll do the shift by hand.
		 */
		hcl_oow_t n = tp - colonp;
		hcl_oow_t i;
 
		for (i = 1; i <= n; i++) 
		{
			endp[-i] = colonp[n - i];
			colonp[n - i] = 0;
		}
		tp = endp;
	}

	if (tp != endp) return -1;

	return 0;
}
#endif


static int str_to_sockaddr (hcl_server_t* server, const ooch_t* str, hcl_oow_t len, sockaddr_t* nwad, socklen_t* socklen)
{
	const ooch_t* p;
	const ooch_t* end;
	oocs_t tmp;

	p = str;
	end = str + len;

	if (p >= end) 
	{
		hcl_server_seterrbfmt (server, HCL_EINVAL, "blank address");
		return -1;
	}

	HCL_MEMSET (nwad, 0, HCL_SIZEOF(*nwad));

#if (HCL_SIZEOF_STRUCT_SOCKADDR_IN6 > 0)
	if (*p == '[')
	{
		/* IPv6 address */
		tmp.ptr = (ooch_t*)++p; /* skip [ and remember the position */
		while (p < end && *p != '%' && *p != ']') p++;

		if (p >= end) goto no_rbrack;

		tmp.len = p - tmp.ptr;
		if (*p == '%')
		{
			/* handle scope id */
			hcl_uint32_t x;

			p++; /* skip % */

			if (p >= end)
			{
				/* premature end */
				hcl_server_seterrbfmt (server, HCL_EINVAL, "scope id blank");
				return -1;
			}

			if (*p >= '0' && *p <= '9') 
			{
				/* numeric scope id */
				nwad->in6.sin6_scope_id = 0;
				do
				{
					x = nwad->in6.sin6_scope_id * 10 + (*p - '0');
					if (x < nwad->in6.sin6_scope_id) 
					{
						hcl_server_seterrbfmt (server, HCL_EINVAL, "scope id too large");
						return -1; /* overflow */
					}
					nwad->in6.sin6_scope_id = x;
					p++;
				}
				while (p < end && *p >= '0' && *p <= '9');
			}
			else
			{
#if 0
TODO:
				/* interface name as a scope id? */
				const ooch_t* stmp = p;
				unsigned int index;
				do p++; while (p < end && *p != ']');
				if (hcl_nwifwcsntoindex (stmp, p - stmp, &index) <= -1) return -1;
				tmpad.u.in6.scope = index;
#endif
			}

			if (p >= end || *p != ']') goto no_rbrack;
		}
		p++; /* skip ] */

		if (str_to_ipv6(tmp.ptr, tmp.len, &nwad->in6.sin6_addr) <= -1) goto unrecog;
		nwad->in6.sin6_family = AF_INET6;
	}
	else
	{
#endif
		/* IPv4 address */
		tmp.ptr = (ooch_t*)p;
		while (p < end && *p != ':') p++;
		tmp.len = p - tmp.ptr;

		if (str_to_ipv4(tmp.ptr, tmp.len, &nwad->in4.sin_addr) <= -1)
		{
		#if (HCL_SIZEOF_STRUCT_SOCKADDR_IN6 > 0)
			/* check if it is an IPv6 address not enclosed in []. 
			 * the port number can't be specified in this format. */
			if (p >= end || *p != ':') 
			{
				/* without :, it can't be an ipv6 address */
				goto unrecog;
			}


			while (p < end && *p != '%') p++;
			tmp.len = p - tmp.ptr;

			if (str_to_ipv6(tmp.ptr, tmp.len, &nwad->in6.sin6_addr) <= -1) goto unrecog;

			if (p < end && *p == '%')
			{
				/* handle scope id */
				hcl_uint32_t x;

				p++; /* skip % */

				if (p >= end)
				{
					/* premature end */
					hcl_server_seterrbfmt (server, HCL_EINVAL, "scope id blank");
					return -1;
				}

				if (*p >= '0' && *p <= '9') 
				{
					/* numeric scope id */
					nwad->in6.sin6_scope_id = 0;
					do
					{
						x = nwad->in6.sin6_scope_id * 10 + (*p - '0');
						if (x < nwad->in6.sin6_scope_id) 
						{
							hcl_server_seterrbfmt (server, HCL_EINVAL, "scope id too large");
							return -1; /* overflow */
						}
						nwad->in6.sin6_scope_id = x;
						p++;
					}
					while (p < end && *p >= '0' && *p <= '9');
				}
				else
				{
#if 0
TODO
					/* interface name as a scope id? */
					const ooch_t* stmp = p;
					unsigned int index;
					do p++; while (p < end);
					if (hcl_nwifwcsntoindex(stmp, p - stmp, &index) <= -1) return -1;
					nwad->in6.sin6_scope_id = index;
#endif
				}
			}

			if (p < end) goto unrecog; /* some gargage after the end? */

			nwad->in6.sin6_family = AF_INET6;
			*socklen = HCL_SIZEOF(nwad->in6);
			return nwad->in6.sin6_family;
		#else
			goto unrecog;
		#endif	
		}

		nwad->in4.sin_family = AF_INET;
#if (HCL_SIZEOF_STRUCT_SOCKADDR_IN6 > 0)
	}
#endif

	if (p < end && *p == ':') 
	{
		/* port number */
		hcl_uint32_t port = 0;

		p++; /* skip : */

		tmp.ptr = (ooch_t*)p;
		while (p < end && *p >= '0' && *p <= '9')
		{
			port = port * 10 + (*p - '0');
			p++;
		}

		tmp.len = p - tmp.ptr;
		if (tmp.len <= 0 || tmp.len >= 6 || 
		    port > HCL_TYPE_MAX(hcl_uint16_t)) 
		{
			hcl_server_seterrbfmt (server, HCL_EINVAL, "port number blank or too large");
			return -1;
		}

	#if (HCL_SIZEOF_STRUCT_SOCKADDR_IN6 > 0)
		if (nwad->in4.sin_family == AF_INET)
			nwad->in4.sin_port = hcl_hton16(port);
		else
			nwad->in6.sin6_port = hcl_hton16(port);
	#else
		nwad->in4.sin_port = hcl_hton16(port);
	#endif
	}

	*socklen = (nwad->in4.sin_family == AF_INET)? HCL_SIZEOF(nwad->in4): HCL_SIZEOF(nwad->in6);
	return nwad->in4.sin_family;
	
	
unrecog:
	hcl_server_seterrbfmt (server, HCL_EINVAL, "unrecognized address");
	return -1;
	
no_rbrack:
	hcl_server_seterrbfmt (server, HCL_EINVAL, "missing right bracket");
	return -1;
}
