with H2.Scheme;
with Ada.Wide_Text_IO;

package Stream is

	--package S renames H2.Scheme;
	package S is new  H2.Scheme (Standard.Wide_Character, Standard.Wide_String);

	------------------------------------------------------------
	--type Object_String_Pointer is access all S.Object_String;
	type Object_String_Pointer is access constant S.Object_String;
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
	--	Handle: H2.Text_IO.File_Type;
	--end record;

	type File_Stream_Record is new S.Stream_Record with record
		Name:   S.Constant_Object_String_Pointer;
		Handle: Ada.Wide_Text_IO.File_Type;
	end record;


	procedure Open (Stream: in out File_Stream_Record);
	procedure Close (Stream: in out File_Stream_Record);
	procedure Read (Stream: in out File_Stream_Record;
	                Data:   out    S.Object_String;
	                Last:   out    Standard.Natural);
	procedure Write (Stream: in out File_Stream_Record;
	                 Data:   out    S.Object_String;
	                 Last:   out    Standard.Natural);

	------------------------------------------------------------
	procedure Allocate_Stream (Interp: in out S.Interpreter_Record;
	                           Name:   in     S.Constant_Object_String_Pointer;
	                           Result: out    S.Stream_Pointer);

	procedure Deallocate_Stream (Interp: in out S.Interpreter_Record;
	                             Source: in out S.Stream_Pointer);

--private
--	type File_Stream_Record is new S.Stream_Record with record
--		Name:   S.Constant_Object_String_Pointer;
--		Handle: Ada.Wide_Text_IO.File_Type;
--	end record;

end Stream;

