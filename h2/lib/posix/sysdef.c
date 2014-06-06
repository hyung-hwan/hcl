#include <sys/types.h>
#include <fcntl.h>
#include <stdio.h>
#include <limits.h>

int main (int argc, char* argv[])
{
	if (argc != 2)
	{
		fprintf (stderr, "Usage: %s  package-name\n", argv[0]);
		return -1;
	}

	printf ("package %s is\n", argv[1]);
	printf ("\n");

	printf ("\ttype size_t   is mod 2 ** %d;\n", (int)(sizeof(size_t) * 8));
	printf ("\ttype ssize_t  is range -(2 ** (%d - 1)) .. +(2 ** (%d - 1)) - 1;\n", (int)(sizeof(size_t) * 8), (int)(sizeof(size_t) * 8));


	printf ("\ttype ushort_t is mod 2 ** %u;\n", (int)(sizeof(unsigned short) * 8));
	printf ("\ttype uint_t   is mod 2 ** %u;\n", (int)(sizeof(int) * 8));
	printf ("\ttype ulong_t  is mod 2 ** %u;\n", (int)(sizeof(unsigned long) * 8));
	printf ("\ttype short_t  is range %d .. %d;\n", SHRT_MIN, SHRT_MAX);
	printf ("\ttype int_t    is range %d .. %d;\n", INT_MIN, INT_MAX);
	printf ("\ttype long_t   is range %ld .. %ld;\n", LONG_MIN, LONG_MAX);
	printf ("\n");

	printf ("\tO_RDONLY: constant := %d;\n", O_RDONLY);
	printf ("\tO_WRONLY: constant := %d;\n", O_WRONLY);
	printf ("\tO_RDWR:   constant := %d;\n", O_RDWR);
	printf ("\tO_CREAT:  constant := %d;\n", O_CREAT);
	printf ("\tO_EXCL:   constant := %d;\n", O_EXCL);
	printf ("\tO_TRUNC:  constant := %d;\n", O_TRUNC);
	printf ("\tO_APPEND:  constant := %d;\n", O_APPEND);

#if !defined(O_NONBLOCK)
#	define O_NONBLOCK 0
#endif
	printf ("\tO_NONBLOCK:  constant := %d;\n", O_NONBLOCK);

#if !defined(O_SYNC)
#	define O_SYNC 0
#endif
	printf ("\tO_SYNC:   constant := %d;\n", O_SYNC);


	printf ("\n");
	printf ("end %s;\n", argv[1]);

	return 0;
}
