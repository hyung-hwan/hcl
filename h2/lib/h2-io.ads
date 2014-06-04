with H2.Sysapi;

generic
	type Slim_Character is (<>);
	type Wide_Character is (<>);
	type Slim_String is array(System_Index range<>) of Slim_Character;
	type Wide_String is array(System_Index range<>) of Wide_Character;
	with function Slim_To_Wide (Slim: in Slim_String) return Wide_String;
	with function Wide_To_Slim (Wide: in Wide_String) return Slim_String;


package H2.IO is

	package Sysapi is new H2.Sysapi (Slim_Character, Wide_Character, Slim_String, Wide_String, Slim_To_Wide, Wide_To_Slim);

	package File is

		subtype Flag_Record is Sysapi.File.Flag_Record;

		FLAG_READ:       constant := Sysapi.File.FLAG_READ;
		FLAG_WRITE:      constant := Sysapi.File.FLAG_WRITE;
		FLAG_CREATE:     constant := Sysapi.File.FLAG_CREATE;
		FLAG_EXCLUSIVE:  constant := Sysapi.File.FLAG_EXCLUSIVE;
		FLAG_TRUNCATE:   constant := Sysapi.File.FLAG_TRUNCATE;
		FLAG_APPEND:     constant := Sysapi.File.FLAG_APPEND;
		FLAG_NONBLOCK:   constant := Sysapi.File.FLAG_NONBLOCK;
		FLAG_SYNC:       constant := Sysapi.File.FLAG_SYNC;
		FLAG_NOFOLLOW:   constant := Sysapi.File.FLAG_NOFOLLOW;

		type File_Record is limited record
			File: Sysapi.File.File_Pointer := null;
			Buffer: System_Byte_Array (1 .. 2048);
			Last: System_Length := System_Length'First;
		end record;

		procedure Open (File: in out File_Record; 
					 Name: in     Slim_String;
		                Flag: in     Flag_Record;
					 Pool: in     Storage_Pool_Pointer := null);

		procedure Open (File: in out File_Record;
					 Name: in     Wide_String;
		                Flag: in     Flag_Record;
					 Pool: in     Storage_Pool_Pointer := null);

		procedure Close (File: in out File_Record);

		procedure Read (File:   in out File_Record; 
					 Buffer: in out Slim_String;
					 Last:   out    System_Length);

		procedure Read (File:   in out File_Record;
					 Buffer: in out Wide_String;
					 Last:   out    System_Length);

		procedure Write (File:   in out File_Record; 
					  Buffer: in     Slim_String;
					  Last:   out    System_Length);

		procedure Write (File:   in out File_Record;
					  Buffer: in     Wide_String;
					  Last:   out    System_Length);

		procedure Flush (File: in out File_Record);
	end File;

end H2.IO;

