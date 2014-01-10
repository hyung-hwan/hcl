with H2.Scheme;
with Ada.Wide_Text_IO;

package Stream is

	package S is new H2.Scheme (Standard.Wide_Character);

	------------------------------------------------------------
	--type Object_String_Pointer is access all S.Object_String;
	type Object_String_Pointer is access constant S.Object_String;
	type String_Input_Stream_Record(Str: Object_String_Pointer) is new S.Stream_Record with record
		Pos: S.Object_String_Size := 0;	
	end record;

	procedure Open (Stream: in out String_Input_Stream_Record);
	procedure Close (Stream: in out String_Input_Stream_Record);
	procedure Read (Stream: in out String_Input_Stream_Record;
	                Data:   out    S.Object_String;
	                Last:   out    S.Object_String_Size);
	procedure Write (Stream: in out String_Input_Stream_Record;
	                 Data:   out    S.Object_String;
	                 Last:   out    S.Object_String_Size);

	------------------------------------------------------------

	type File_Stream_Record is new S.Stream_Record with record
		Name:   S.Constant_Object_String_Pointer;
		Handle: Ada.Wide_Text_IO.File_Type;
	end record;

	procedure Open (Stream: in out File_Stream_Record);
	procedure Close (Stream: in out File_Stream_Record);
	procedure Read (Stream: in out File_Stream_Record;
	                Data:   out    S.Object_String;
	                Last:   out    S.Object_String_Size);
	procedure Write (Stream: in out File_Stream_Record;
	                 Data:   out    S.Object_String;
	                 Last:   out    S.Object_String_Size);

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

