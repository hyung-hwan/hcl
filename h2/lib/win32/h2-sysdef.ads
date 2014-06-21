with System;

package H2.Sysdef is

	type PVOID is System.Address;
	subtype HANDLE is PVOID;

	type DWORD is mod 2 ** 32;

end H2.Sysdef;
