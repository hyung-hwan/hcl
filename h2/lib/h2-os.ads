
generic
	type Slim_Character is (<>);
	type Wide_Character is (<>);
	type Slim_String is array(System_Index range<>) of Slim_Character;
	type Wide_String is array(System_Index range<>) of Wide_Character;
	with function Slim_To_Wide (Slim: in Slim_String) return Wide_String;
	with function Wide_To_Slim (Wide: in Wide_String) return Slim_String;

package H2.OS is



	type File_Flag_Bits is new System_Word;
	type File_Flag_Record is record
		Bits: File_Flag_Bits := 0;
	end record;

	type File_Mode_Bits is new System_Word;
	type File_Mode_Record is record
		Bits: File_Mode_Bits := 0;
	end record;

	procedure Set_File_Flag_Bits (Flag: in out File_Flag_Record; 
	                              Bits: in     File_Flag_Bits);

	procedure Clear_File_Flag_Bits (Flag: in out File_Flag_Record;
	                                Bits: in     File_Flag_Bits);

	package File is
		type File_Record is tagged null record;
		type File_Pointer is access all File_Record'Class;

		subtype Flag_Bits is OS.File_Flag_Bits;
		subtype Mode_Bits is OS.File_Mode_Bits;
		subtype Flag_Record is OS.File_Flag_Record;
		subtype Mode_Record is OS.File_Mode_Record;

		FLAG_READ:       constant Flag_Bits := 2#0000_0000_0000_0001#;
		FLAG_WRITE:      constant Flag_Bits := 2#0000_0000_0000_0010#;
		FLAG_CREATE:     constant Flag_Bits := 2#0000_0000_0000_0100#;
		FLAG_EXCLUSIVE:  constant Flag_Bits := 2#0000_0000_0000_1000#;
		FLAG_TRUNCATE:   constant Flag_Bits := 2#0000_0000_0001_0000#;
		FLAG_APPEND:     constant Flag_Bits := 2#0000_0000_0010_0000#;
		FLAG_NONBLOCK:   constant Flag_Bits := 2#0000_0000_0100_0000#;
		FLAG_SYNC:       constant Flag_Bits := 2#0000_0000_1000_0000#;
		FLAG_NOFOLLOW:   constant Flag_Bits := 2#0000_0001_0000_0000#;
	--  	FLAG_NOSHREAD:   constant Flag_Bits := 2#0010_0000_0000_0000#;
	--  	FLAG_NOSHWRITE:  constant Flag_Bits := 2#0100_0000_0000_0000#;
	--  	FLAG_NOSHDELETE: constant Flag_Bits := 2#1000_0000_0000_0000#;

		MODE_OWNER_READ:  constant Mode_Bits := 2#100_000_000#;
		MODE_OWNER_WRITE: constant Mode_Bits := 2#010_000_000#;
		MODE_OWNER_EXEC:  constant Mode_Bits := 2#001_000_000#;
		MODE_GROUP_READ:  constant Mode_Bits := 2#000_100_000#;
		MODE_GROUP_WRITE: constant Mode_Bits := 2#000_010_000#;
		MODE_GROUP_EXEC:  constant Mode_Bits := 2#000_001_000#;
		MODE_OTHER_READ:  constant Mode_Bits := 2#000_000_100#;
		MODE_OTHER_WRITE: constant Mode_Bits := 2#000_000_010#;
		MODE_OTHER_EXEC:  constant Mode_Bits := 2#000_000_001#;

		DEFAULT_MODE: constant Mode_Record := ( Bits => 2#110_100_100# );

		procedure Set_Flag_Bits (Flag: in out Flag_Record;
		                         Bits: in     Flag_Bits) renames OS.Set_File_Flag_Bits;

		procedure Clear_Flag_Bits (Flag: in out Flag_Record;
		                           Bits: in     Flag_Bits) renames OS.Clear_File_Flag_Bits;

		function Get_Stdin return File_Pointer;
		function Get_Stdout return File_Pointer;
		function Get_Stderr return File_Pointer;

		procedure Open (File: out File_Pointer;
		                Name: in  Slim_String;
		                Flag: in  Flag_Record;
		                Mode: in  Mode_Record := DEFAULT_MODE;
		                Pool: in  Storage_Pool_Pointer := null);

		procedure Open (File: out File_Pointer;
		                Name: in  Wide_String;
		                Flag: in  Flag_Record;
		                Mode: in  Mode_Record := DEFAULT_MODE;
		                Pool: in  Storage_Pool_Pointer := null);

		procedure Close (File: in out File_Pointer);

		procedure Read (File:   in     File_Pointer; 
		                Buffer: in out System_Byte_Array; 
		                Length: out    System_Length);

		procedure Write (File:   in  File_Pointer; 
		                 Buffer: in  System_Byte_Array; 
		                 Length: out System_Length);

		pragma Inline (Get_Stdin);
		pragma Inline (Get_Stdout);
		pragma Inline (Get_Stderr);
	end File;

	--procedure Open_File (File: out File_Pointer;
	--                Flag: in  Flag_Record;
	--                Mode: in  Mode_Record) renames File.Open;
	--procedure Close_File (File: in out File_Pointer) renames File.Close;

end H2.OS;
