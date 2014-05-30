
generic 
	type Slim_Character is (<>);
	type Wide_Character is (<>);
	type Slim_String is array(System_Index range<>) of Slim_Character;
	type Wide_String is array(System_Index range<>) of Wide_Character;
	with function Slim_To_Wide (Slim: in Slim_String) return Wide_String;
	with function Wide_To_Slim (Wide: in Wide_String) return Slim_String;

package H2.Sysapi is

	type Flag_Record is record
		x: integer;
	end record;

	type Mode_Record is record
		x: integer;
	end record;

	type File_Record is tagged null record;
	type File_Pointer is access all File_Record'Class;

	type File_Flag is (
		RDONLY,
		RDWR
	);

	package File is
		procedure Open (File: out File_Pointer;
		                Name: in  Slim_String;
		                Flag: in  Flag_Record;
		                Mode: in  Mode_Record;
		                Pool: in  Storage_Pool_Pointer := null);

		procedure Open (File: out File_Pointer;
		                Name: in  Wide_String;
		                Flag: in  Flag_Record;
		                Mode: in  Mode_Record;
		                Pool: in  Storage_Pool_Pointer := null);

		procedure Close (File: in out File_Pointer);
	end File;

	--procedure Open_File (File: out File_Pointer; 
	--                Flag: in  Flag_Record;
	--                Mode: in  Mode_Record) renames File.Open;
	--procedure Close_File (File: in out File_Pointer) renames File.Close;

end H2.Sysapi;
