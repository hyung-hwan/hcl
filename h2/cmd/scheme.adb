
with H2.Wide;
with H2.Slim;
with H2.Pool;
with Storage;
with Slim_Stream;
with Wide_Stream;
with Ada.Text_IO;
with Ada.Unchecked_Deallocation;

procedure scheme is
	--package Stream renames Wide_Stream;
	--package Scheme renames H2.Wide.Scheme;

	package Stream renames Slim_Stream;
	package Scheme renames H2.Slim.Scheme;
	
	Pool: aliased Storage.Global_Pool;
	SI: Scheme.Interpreter_Record;

	I: Scheme.Object_Pointer;
	O: Scheme.Object_Pointer;

	--String: aliased S.Object_String := "(car '(1 2 3))";
	String: aliased constant Scheme.Object_Character_Array := "((lambda (x y) (+ x y))  9  7)";
	String_Stream: Stream.String_Input_Stream_Record (String'Unchecked_Access);
	--String_Stream: Stream.String_Input_Stream_Record := (Len => String'Length, Str => String, Pos => 0);

	--File_Name: aliased S.Object_Character_Array := "test.adb";
	File_Name: aliased constant Scheme.Object_Character_Array := "test.scm";
	--File_Stream: Stream.File_Stream_Record (File_Name'Unchecked_Access);
	--File_Stream: Stream.File_Stream_Record := (Name => File_Name'Unchecked_Access);
	File_Stream: Stream.File_Stream_Record;

   --procedure h2init;
   --pragma Import (C, h2init, "h2init");


begin
	--h2init;

	Scheme.Open (SI, 2_000_000, Pool'Unchecked_Access);
	--Scheme.Open (SI, null);

	-- Specify the named stream handler
	Scheme.Set_Option (SI, (Scheme.Stream_Option, 
	                   Stream.Allocate_Stream'Access, 
	                   Stream.Deallocate_Stream'Access)
	);

Scheme.Set_Option (SI, (Scheme.Trait_Option, Scheme.No_Optimization));

	File_Stream.Name := File_Name'Unchecked_Access;
	begin
		Scheme.Set_Input_Stream (SI, File_Stream); -- specify main input stream
		--Schee.Set_Input_Stream (SI, String_Stream);
	exception
		when others =>
			Ada.Text_IO.Put_Line ("Cannot open Input Stream");
	end;
	--Scheme.Set_Output_Stream (SI, Stream); -- specify main output stream.

Ada.Text_IO.Put_Line ("-------------------------------------------");
Scheme.Run_Loop (SI, I);
Scheme.Print (SI, I);
	Scheme.Close (SI);

	Ada.Text_IO.Put_Line ("BYE...");

end scheme;
