
h2_size_t XFUN(strfmc) (
	xchar_t* buf, const xchar_t* fmt, const xchar_t* str[])
{
	xchar_t* b = buf;
	const xchar_t* f = fmt;

	while (*f != XCHAR('\0'))
	{
		if (*f == XCHAR('$'))
		{
			if (f[1] == XCHAR('{') && 
			    (f[2] >= XCHAR('0') && f[2] <= XCHAR('9')))
			{
				const xchar_t* tmp;
				h2_size_t idx = 0;

				tmp = f;
				f += 2;

				do idx = idx * 10 + (*f++ - XCHAR('0'));
				while (*f >= XCHAR('0') && *f <= XCHAR('9'));
	
				if (*f != XCHAR('}'))
				{
					f = tmp;
					goto normal;
				}

				f++;

				tmp = str[idx];
				while (*tmp != XCHAR('\0')) *b++ = *tmp++;
				continue;
			}
			else if (f[1] == XCHAR('$')) f++;
		}

	normal:
		*b++ = *f++;
	}

	*b = XCHAR('\0');
	return b - buf;
}

#if 0
h2_size_t XFUN(strfncpy) (
	xchar_t* buf, const xchar_t* fmt, const cstr_t str[])
{
	xchar_t* b = buf;
	const xchar_t* f = fmt;

	while (*f != XCHAR('\0'))
	{
		if (*f == XCHAR('\\'))
		{
			/* get the escaped character and treat it normally.
			 * if the escaper is the last character, treat it 
			 * normally also. */
			if (f[1] != XCHAR('\0')) f++;
		}
		else if (*f == XCHAR('$'))
		{
			if (f[1] == XCHAR('{') && 
			    (f[2] >= XCHAR('0') && f[2] <= XCHAR('9')))
			{
				const xchar_t* tmp, * tmpend;
				h2_size_t idx = 0;

				tmp = f;
				f += 2;

				do idx = idx * 10 + (*f++ - XCHAR('0'));
				while (*f >= XCHAR('0') && *f <= XCHAR('9'));
	
				if (*f != XCHAR('}'))
				{
					f = tmp;
					goto normal;
				}

				f++;
				
				tmp = str[idx].ptr;
				tmpend = tmp + str[idx].len;

				while (tmp < tmpend) *b++ = *tmp++;
				continue;
			}
			else if (f[1] == XCHAR('$')) f++;
		}

	normal:
		*b++ = *f++;
	}

	*b = XCHAR('\0');
	return b - buf;
}

h2_size_t XFUN(strxfmc) (
	xchar_t* buf, h2_size_t bsz, 
	const xchar_t* fmt, const xchar_t* str[])
{
	xchar_t* b = buf;
	xchar_t* end = buf + bsz - 1;
	const xchar_t* f = fmt;

	if (bsz <= 0) return 0;

	while (*f != XCHAR('\0'))
	{
		if (*f == XCHAR('\\'))
		{
			/* get the escaped character and treat it normally.
			 * if the escaper is the last character, treat it 
			 * normally also. */
			if (f[1] != XCHAR('\0')) f++;
		}
		else if (*f == XCHAR('$'))
		{
			if (f[1] == XCHAR('{') && 
			    (f[2] >= XCHAR('0') && f[2] <= XCHAR('9')))
			{
				const xchar_t* tmp;
				h2_size_t idx = 0;

				tmp = f;
				f += 2;

				do idx = idx * 10 + (*f++ - XCHAR('0'));
				while (*f >= XCHAR('0') && *f <= XCHAR('9'));
	
				if (*f != XCHAR('}'))
				{
					f = tmp;
					goto normal;
				}

				f++;
				
				tmp = str[idx];
				while (*tmp != XCHAR('\0')) 
				{
					if (b >= end) goto fini;
					*b++ = *tmp++;
				}
				continue;
			}
			else if (f[1] == XCHAR('$')) f++;
		}

	normal:
		if (b >= end) break;
		*b++ = *f++;
	}

fini:
	*b = XCHAR('\0');
	return b - buf;
}

h2_size_t XFUN(strxfncpy) (
	xchar_t* buf, h2_size_t bsz, 
	const xchar_t* fmt, const cstr_t str[])
{
	xchar_t* b = buf;
	xchar_t* end = buf + bsz - 1;
	const xchar_t* f = fmt;

	if (bsz <= 0) return 0;

	while (*f != XCHAR('\0'))
	{
		if (*f == XCHAR('\\'))
		{
			/* get the escaped character and treat it normally.
			 * if the escaper is the last character, treat it 
			 * normally also. */
			if (f[1] != XCHAR('\0')) f++;
		}
		else if (*f == XCHAR('$'))
		{
			if (f[1] == XCHAR('{') && 
			    (f[2] >= XCHAR('0') && f[2] <= XCHAR('9')))
			{
				const xchar_t* tmp, * tmpend;
				h2_size_t idx = 0;

				tmp = f;
				f += 2;

				do idx = idx * 10 + (*f++ - XCHAR('0'));
				while (*f >= XCHAR('0') && *f <= XCHAR('9'));
	
				if (*f != XCHAR('}'))
				{
					f = tmp;
					goto normal;
				}

				f++;
				
				tmp = str[idx].ptr;
				tmpend = tmp + str[idx].len;

				while (tmp < tmpend)
				{
					if (b >= end) goto fini;
					*b++ = *tmp++;
				}
				continue;
			}
			else if (f[1] == XCHAR('$')) f++;
		}

	normal:
		if (b >= end) break;
		*b++ = *f++;
	}

fini:
	*b = XCHAR('\0');
	return b - buf;
}

#endif
