with System;
with System.Storage_Pools;

package H2 is
	pragma Preelaborate (H2);

	System_Word_Bits: constant := System.Word_Size;
	System_Word_Bytes: constant := System_Word_Bits / System.Storage_Unit;

	--type System_Byte is mod 2 ** System.Storage_Unit;
	--for System_Byte'Size use System.Storage_Unit;

	type System_Word is mod 2 ** System_Word_Bits;
	--for System_Word'Size use System_Word_Bits;

	type System_Signed_Word is range -(2 ** (System_Word_Bits - 1)) ..
	                                 +(2 ** (System_Word_Bits - 1)) - 1;
	--for System_Signed_Word'Size use System_Word_Bits;

	type System_Size is new System_Word range 0 .. (2 ** System_Word_Bits) - 1;
	subtype System_Length is System_Size;
	subtype System_Index is System_Size range 1 .. System_Size'Last;

	type Storage_Pool_Pointer is
		access all System.Storage_Pools.Root_Storage_Pool'Class;

	
	-- TODO: move this to H2.XXXX???
	type File_Record is tagged null record;
	type File_Pointer is access all File_Record'Class;
	
	type Flag_Record is record
		x: integer;
	end record;

	type Mode_Record is record
		x: integer;
	end record;
end H2;
