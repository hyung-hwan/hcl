
with H2.Scheme;
with H2.Pool;
with Storage;
with Stream;
with Ada.Text_IO;
with Ada.Unchecked_Deallocation;

procedure scheme is
	--package S renames H2.Scheme;
	--package S is new  H2.Scheme (Wide_Character, Wide_String);
	package S renames Stream.S;

	Pool: aliased Storage.Global_Pool;
	SI: S.Interpreter_Record;

	I: S.Object_Pointer;
	O: S.Object_Pointer;

	--String: aliased S.Object_String := "(car '(1 2 3))";
	String: aliased constant S.Object_Character_Array := "((lambda (x y) (+ x y))  9  7)";
	String_Stream: Stream.String_Input_Stream_Record (String'Unchecked_Access);
	--String_Stream: Stream.String_Input_Stream_Record := (Len => String'Length, Str => String, Pos => 0);
	

	--File_Name: aliased S.Object_Character_Array := "test.adb";
	File_Name: aliased constant S.Object_Character_Array := "test.scm";
	--File_Stream: Stream.File_Stream_Record (File_Name'Unchecked_Access);
	--File_Stream: Stream.File_Stream_Record := (Name => File_Name'Unchecked_Access);
	File_Stream: Stream.File_Stream_Record;

   --procedure h2init;
   --pragma Import (C, h2init, "h2init");


begin
	--h2init;
	Ada.Text_Io.Put_Line (S.Object_Word'Image(S.Object_Pointer_Bytes));

	S.Open (SI, 2_000_000, Pool'Unchecked_Access);
	--S.Open (SI, null);

	-- Specify the named stream handler
	S.Set_Option (SI, (S.Stream_Option, 
	                   Stream.Allocate_Stream'Access, 
	                   Stream.Deallocate_Stream'Access)
	);

S.Set_Option (SI, (S.Trait_Option, S.No_Optimization));

	File_Stream.Name := File_Name'Unchecked_Access;
	begin
		S.Set_Input_Stream (SI, File_Stream); -- specify main input stream
		--S.Set_Input_Stream (SI, String_Stream);
	exception
		when others =>
			Ada.Text_IO.Put_Line ("Cannot open Input Stream");
	end;
	--S.Set_Output_Stream (SI, Stream); -- specify main output stream.

Ada.Text_IO.Put_Line ("-------------------------------------------");
S.Run_Loop (SI, I);
S.Print (SI, I);
	S.Close (SI);

	Ada.Text_IO.Put_Line ("BYE...");

end scheme;
