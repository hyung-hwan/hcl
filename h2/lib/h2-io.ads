with H2.OS;
with H2.Ascii;

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
	package Slim_Ascii is new H2.Ascii (Slim_Character);
	package Wide_Ascii is new H2.Ascii (Wide_Character);

	function Get_Line_Terminator return Slim_String;
	--function Get_Line_Terminator return Wide_String;

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

		type Option_Bits is new System_Word;
		type Option_Record is record
			Bits: Option_Bits := 0;
		end record;

		-- Convert LF to CR/LF in Put_Line
		OPTION_CRLF: constant Option_Bits := 2#0000_0000_0000_0001#;

		type File_Buffer is private;
		type File_Record is limited private;
			
		procedure Set_Flag_Bits (Flag: in out Flag_Record; 
		                         Bits: in     Flag_Bits) renames OS.File.Set_Flag_Bits;

		procedure Clear_Flag_Bits (Flag: in out Flag_Record;
		                           Bits: in     Flag_Bits) renames OS.File.Clear_Flag_Bits;

		procedure Set_Option_Bits (Option: in out Option_Record;
		                            Bits:   in     Option_Bits);

		procedure Clear_Option_Bits (Option: in out Option_Record;
		                             Bits:   in     Option_Bits);
	
		function Is_Open (File: in File_Record) return Standard.Boolean;
		pragma Inline (Is_Open);

		procedure Open (File: in out File_Record; 
		                Name: in     Slim_String;
		                Flag: in     Flag_Record;
		                Pool: in     Storage_Pool_Pointer := null);

		procedure Open (File: in out File_Record;
		                Name: in     Wide_String;
		                Flag: in     Flag_Record;
		                Pool: in     Storage_Pool_Pointer := null);

		procedure Close (File: in out File_Record);

		procedure Set_Option (File: in out File_Record; Option: in Option_Record);

		function Get_Option (File: in File_Record) return Option_Record;

		-- The Read procedure reads as many characters as the buffer 
		-- can hold with a single system call at most.
		procedure Read (File:   in out File_Record; 
		                Buffer: out    Slim_String;
		                Length: out    System_Length);

		-- The Read_Line procedure reads a single line into the bufer.
		-- If the buffer is not large enough, it may not contain a full line. 
		-- The remaining part can be returned in the next call.
		procedure Read_Line (File:   in out File_Record; 
		                     Buffer: out    Slim_String;
		                     Length: out    System_Length);


		-- The Get_Line procedure acts like Read_Line but the line terminator
		-- is translated to LF.
		procedure Get_Line (File:   in out File_Record;
		                    Buffer: out    Slim_String;
		                    Length: out    System_Length);

		procedure Read (File:   in out File_Record;
		                Buffer: out    Wide_String;
		                Length: out    System_Length);

		procedure Read_Line (File:   in out File_Record;
		                     Buffer: out    Wide_String;
		                     Length: out    System_Length);

		procedure Get_Line (File:   in out File_Record;
		                    Buffer: out    Wide_String;
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

		procedure Put_Line (File:   in out File_Record;
		                    Buffer: in     Slim_String;
		                    Length: out    System_Length);


		procedure Write (File:   in out File_Record;
		                 Buffer: in     Wide_String;
		                 Length: out    System_Length);

		procedure Write_Line (File:   in out File_Record;
		                      Buffer: in     Wide_String;
		                      Length: out    System_Length);

		procedure Put_Line (File:   in out File_Record;
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
			Pos: System_Length := 0;
			Last: System_Length := 0;
		end record;

		type File_Record is limited record
			File: OS.File.File_Pointer := null;
			Rbuf: File_Buffer;
			Wbuf: File_Buffer;
			EOF: Standard.Boolean := false;
			Option: Option_Record;
		end record;

	end File;

end H2.IO;

