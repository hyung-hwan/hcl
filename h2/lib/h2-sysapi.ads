
generic
	type Slim_Character is (<>);
	type Wide_Character is (<>);
	type Slim_String is array(System_Index range<>) of Slim_Character;
	type Wide_String is array(System_Index range<>) of Wide_Character;
	with function Slim_To_Wide (Slim: in Slim_String) return Wide_String;
	with function Wide_To_Slim (Wide: in Wide_String) return Slim_String;

package H2.Sysapi is

	type File_Record is tagged null record;
	type File_Pointer is access all File_Record'Class;

	type File_Flag_Bits is new System_Word;
	type File_Flag is record
		Bits: File_Flag_Bits := 0;
	end record;

	type File_Mode_Bits is new System_Word;
	type File_Mode is record
		Bits: File_Mode_Bits := 0;
	end record;

	FILE_FLAG_READ:       constant File_Flag_Bits := 2#0000_0000_0000_0001#;
	FILE_FLAG_WRITE:      constant File_Flag_Bits := 2#0000_0000_0000_0010#;
	FILE_FLAG_CREATE:     constant File_Flag_Bits := 2#0000_0000_0000_0100#;
	FILE_FLAG_EXCLUSIVE:  constant File_Flag_Bits := 2#0000_0000_0000_1000#;
	FILE_FLAG_TRUNCATE:   constant File_Flag_Bits := 2#0000_0000_0001_0000#;
	FILE_FLAG_APPEND:     constant File_Flag_Bits := 2#0000_0000_0010_0000#;
	FILE_FLAG_NONBLOCK:   constant File_Flag_Bits := 2#0000_0000_0100_0000#;
	FILE_FLAG_SYNC:       constant File_Flag_Bits := 2#0000_0000_1000_0000#;
	FILE_FLAG_NOFOLLOW:   constant File_Flag_Bits := 2#0000_0001_0000_0000#;
--  	FILE_FLAG_NOSHREAD:   constant File_Flag_Bits := 2#0010_0000_0000_0000#;
--  	FILE_FLAG_NOSHWRITE:  constant File_Flag_Bits := 2#0100_0000_0000_0000#;
--  	FILE_FLAG_NOSHDELETE: constant File_Flag_Bits := 2#1000_0000_0000_0000#;

	FILE_MODE_OWNER_READ:  constant File_Mode_Bits := 2#100_000_000#;
	FILE_MODE_OWNER_WRITE: constant File_Mode_Bits := 2#010_000_000#;
	FILE_MODE_OWNER_EXEC:  constant File_Mode_Bits := 2#001_000_000#;
	FILE_MODE_GROUP_READ:  constant File_Mode_Bits := 2#000_100_000#;
	FILE_MODE_GROUP_WRITE: constant File_Mode_Bits := 2#000_010_000#;
	FILE_MODE_GROUP_EXEC:  constant File_Mode_Bits := 2#000_001_000#;
	FILE_MODE_OTHER_READ:  constant File_Mode_Bits := 2#000_000_100#;
	FILE_MODE_OTHER_WRITE: constant File_Mode_Bits := 2#000_000_010#;
	FILE_MODE_OTHER_EXEC:  constant File_Mode_Bits := 2#000_000_001#;

	DEFAULT_FILE_MODE: constant File_Mode := ( Bits => 2#110_100_100# );

	procedure Set_File_Flag_Bits (Flag: in out File_Flag; Bits: in File_Flag_Bits);
	procedure Clear_File_Flag_Bits (Flag: in out File_Flag; Bits: in File_Flag_Bits);

	package File is
		--type Handle_Record is tagged null record;
		--type Handle_Pointer is access all Handle_Record'Class;


		procedure Open (File: out File_Pointer;
		                Name: in  Slim_String;
		                Flag: in  File_Flag;
		                Mode: in  File_Mode := DEFAULT_FILE_MODE;
		                Pool: in  Storage_Pool_Pointer := null);

		procedure Open (File: out File_Pointer;
		                Name: in  Wide_String;
		                Flag: in  File_Flag;
		                Mode: in  File_Mode := DEFAULT_FILE_MODE;
		                Pool: in  Storage_Pool_Pointer := null);

		procedure Close (File: in out File_Pointer);
	end File;

	--procedure Open_File (File: out File_Pointer;
	--                Flag: in  Flag_Record;
	--                Mode: in  Mode_Record) renames File.Open;
	--procedure Close_File (File: in out File_Pointer) renames File.Close;

end H2.Sysapi;
