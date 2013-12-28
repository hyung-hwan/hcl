with H2.Scheme;
with Ada.Text_IO;

package Stream is

	package S renames H2.Scheme;

	------------------------------------------------------------
	type Object_String_Pointer is access all S.Object_String;
	type String_Input_Stream_Record(Str: Object_String_Pointer) is new S.Stream_Record with record
		Pos: Standard.Natural := 0;	
	end record;

	--type String_Input_Stream_Record(Len: Standard.Natural) is new S.Stream_Record with record
	--	Pos: Standard.Natural := 0;	
	--	Str: S.Object_String (1 .. Len) := (others => ' ');
	--end record;

	procedure Open (Stream: in out String_Input_Stream_Record);
	procedure Close (Stream: in out String_Input_Stream_Record);
	procedure Read (Stream: in out String_Input_Stream_Record;
	                Data:   out    S.Object_String;
	                Last:   out    Standard.Natural);
	procedure Write (Stream: in out String_Input_Stream_Record;
	                 Data:   out    S.Object_String;
	                 Last:   out    Standard.Natural);

	------------------------------------------------------------
	--type File_Stream_Record(Name: Object_String_Pointer) is new S.Stream_Record with record
	--	Handle: Ada.Text_IO.File_Type;
	--end record;

	type File_Stream_Record is new S.Stream_Record with record
		Name:   Object_String_Pointer;
		Handle: Ada.Text_IO.File_Type;
	end record;

	procedure Open (Stream: in out File_Stream_Record);
	procedure Close (Stream: in out File_Stream_Record);
	procedure Read (Stream: in out File_Stream_Record;
	                Data:   out    S.Object_String;
	                Last:   out    Standard.Natural);
	procedure Write (Stream: in out File_Stream_Record;
	                 Data:   out    S.Object_String;
	                 Last:   out    Standard.Natural);

end Stream;

