with H2.Slim;
with Ada.Text_IO;

package Slim_Stream is

	package S renames H2.Slim.Scheme;

	------------------------------------------------------------
	--type Object_Character_Array_Pointer is access all S.Object_Character_Array;
	type Object_Character_Array_Pointer is access constant S.Object_Character_Array;
	type String_Input_Stream_Record(Str: Object_Character_Array_Pointer) is new S.Stream_Record with record
		Pos: S.Object_Size := 0;	
	end record;

	procedure Open (Stream: in out String_Input_Stream_Record);
	procedure Close (Stream: in out String_Input_Stream_Record);
	procedure Read (Stream: in out String_Input_Stream_Record;
	                Data:   out    S.Object_Character_Array;
	                Last:   out    S.Object_Size);
	procedure Write (Stream: in out String_Input_Stream_Record;
	                 Data:   out    S.Object_Character_Array;
	                 Last:   out    S.Object_Size);

	------------------------------------------------------------

	type File_Stream_Record is new S.Stream_Record with record
		Name:   S.Constant_Object_Character_Array_Pointer;
		Handle: Ada.Text_IO.File_Type;
	end record;

	procedure Open (Stream: in out File_Stream_Record);
	procedure Close (Stream: in out File_Stream_Record);
	procedure Read (Stream: in out File_Stream_Record;
	                Data:   out    S.Object_Character_Array;
	                Last:   out    S.Object_Size);
	procedure Write (Stream: in out File_Stream_Record;
	                 Data:   out    S.Object_Character_Array;
	                 Last:   out    S.Object_Size);

	------------------------------------------------------------
	procedure Allocate_Stream (Interp: in out S.Interpreter_Record;
	                           Name:   access S.Object_Character_Array;
	                           Result: out    S.Stream_Pointer);

	procedure Deallocate_Stream (Interp: in out S.Interpreter_Record;
	                             Source: in out S.Stream_Pointer);

--private
--	type File_Stream_Record is new S.Stream_Record with record
--		Name:   S.Constant_Object_Character_Array_Pointer;
--		Handle: Ada.Text_IO.File_Type;
--	end record;

end Slim_Stream;

