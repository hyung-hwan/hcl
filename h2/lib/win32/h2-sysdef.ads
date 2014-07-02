with System;

package H2.Sysdef is

	subtype LPVOID is System.Address;
	subtype HANDLE is PVOID;

	type BOOL is (FALSE, TRUE);
	for BOOL use (FALSE => 0, TRUE => 1);
	for BOOL'Size use 32;

	type DWORD is mod 2 ** 32;
	type WORD is mod 2 ** 16;
	type BYTE is mod 2 ** 8;

	type LPDWORD is access all DWORD;
	pragma Convention (C, LPDWORD);

	INVALID_HANDLE_VALUE: constant HANDLE := HANDLE'Last;

end H2.Sysdef;
