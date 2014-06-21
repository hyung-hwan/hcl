# This script requires QSEAWK.

BEGIN {
	printf ("-- Generated with ascii.txt and ascii.awk\n");
	printf ("-- Run qseawk -f ascii.awk ascii.txt > h2-ascii.ads for regeneration\n\n");
	printf ("generic\n");
	printf ("\ttype Slim_Character is (<>);\n");
	printf ("\ttype Wide_Character is (<>);\n");
	printf ("package H2.Ascii is\n\n");
	#printf ("\tpragma Preelaborate (Ascii);\n\n");
	printf ("\tpackage Code is\n");
}

{
	t = sprintf ("%c", NR - 1);
	if (str::isprint(t)) t = " -- " t;
	else t="";
	printf ("\t\t%-20s: constant := %d;%s\n", $1, NR-1, t);
	X[NR - 1] = $1; 
}

END {
	printf ("\tend Code;\n\n");

	printf ("\tpackage Slim is\n");
	for (i = 0; i < length(X); i++)
	{
		printf ("\t\t%-20s: constant Slim_Character := Slim_Character'Val(Code.%s);\n", X[i], X[i]);
	}
	printf ("\tend Slim;\n");

	printf ("\n");

	printf ("\tpackage Wide is\n");
	for (i = 0; i < length(X); i++)
	{
		printf ("\t\t%-20s: constant Wide_Character := Wide_Character'Val(Code.%s);\n", X[i], X[i]);
	}
	printf ("\tend Wide;\n");

	printf ("\nend H2.Ascii;\n");
}
