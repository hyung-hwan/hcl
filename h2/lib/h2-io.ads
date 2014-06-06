with H2.OS;

generic
	type Slim_Character is (<>);
	type Wide_Character is (<>);
	type Slim_String is array(System_Index range<>) of Slim_Character;
	type Wide_String is array(System_Index range<>) of Wide_Character;
	with function Slim_To_Wide (Slim: in Slim_String) return Wide_String;
	with function Wide_To_Slim (Wide: in Wide_String) return Slim_String;
	with function Sequence_Length (Slim: in Slim_Character) return System_Length;

package H2.IO is

	package OS is new H2.OS (Slim_Character, Wide_Character, Slim_String, Wide_String, Slim_To_Wide, Wide_To_Slim);

	package File is

		subtype Flag_Record is OS.File.Flag_Record;
		subtype Flag_Bits is OS.File.Flag_Bits;

		FLAG_READ:       constant Flag_Bits := OS.File.FLAG_READ;
		FLAG_WRITE:      constant Flag_Bits := OS.File.FLAG_WRITE;
		FLAG_CREATE:     constant Flag_Bits := OS.File.FLAG_CREATE;
		FLAG_EXCLUSIVE:  constant Flag_Bits := OS.File.FLAG_EXCLUSIVE;
		FLAG_TRUNCATE:   constant Flag_Bits := OS.File.FLAG_TRUNCATE;
		FLAG_APPEND:     constant Flag_Bits := OS.File.FLAG_APPEND;
		FLAG_NONBLOCK:   constant Flag_Bits := OS.File.FLAG_NONBLOCK;
		FLAG_SYNC:       constant Flag_Bits := OS.File.FLAG_SYNC;
		FLAG_NOFOLLOW:   constant Flag_Bits := OS.File.FLAG_NOFOLLOW;

		type File_Buffer is private;
		type File_Record is limited private;
			

		procedure Set_Flag_Bits (Flag: in out Flag_Record; 
		                         Bits: in     Flag_Bits) renames OS.File.Set_Flag_Bits;

		procedure Clear_Flag_Bits (Flag: in out Flag_Record;
		                           Bits: in     Flag_Bits) renames OS.File.Clear_Flag_Bits;

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
		                Length: out    System_Length);

		procedure Read_Line (File:   in out File_Record; 
		                     Buffer: in out Slim_String;
		                     Length: out    System_Length);

		procedure Read (File:   in out File_Record;
		                Buffer: in out Wide_String;
		                Length: out    System_Length);

		procedure Read_Line (File:   in out File_Record;
		                     Buffer: in out Wide_String;
		                     Length: out    System_Length);

		procedure Write (File:   in out File_Record; 
		                 Buffer: in     Slim_String;
		                 Length: out    System_Length);

		-- The Write_Line procedure doesn't add a line terminator.
		-- It writes to the underlying file if the internal buffer
		-- is full or writes up to the last line terminator found.
		procedure Write_Line (File:   in out File_Record;
		                      Buffer: in     Slim_String;
		                      Length: out    System_Length);

		procedure Write (File:   in out File_Record;
		                 Buffer: in     Wide_String;
		                 Length: out    System_Length);

		procedure Write_Line (File:   in out File_Record;
		                      Buffer: in     Wide_String;
		                      Length: out    System_Length);

		
		procedure Flush (File: in out File_Record);
		procedure Drain (File: in out File_Record);

		--procedure Rewind (File: in out File_Record);
		--procedure Set_Position (File: in out File_Record; Position: Position_Record);
		--procedure Get_Position (File: in out File_Record; Position: Position_Record);

	private
		type File_Buffer is record
			-- TODO: determine the best buffer size.
			-- The Data array size must be as large as the longest 
			-- multi-byte sequence for a single wide character.
			Data: System_Byte_Array (1 .. 2048); 
			Length: System_Length := 0;
		end record;

		type File_Record is limited record
			File: OS.File.File_Pointer := null;
			Rbuf: File_Buffer;
			Wbuf: File_Buffer;
			EOF: Standard.Boolean := false;
		end record;

	end File;

end H2.IO;

