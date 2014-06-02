
with H2.Wide;
with H2.Slim;
with H2.Pool;
with Storage;
with Slim_Stream;
with Wide_Stream;
with Ada.Text_IO;
with Ada.Unchecked_Deallocation;
with H2.Sysapi;

with Interfaces.C;

procedure scheme is
	package Stream renames Wide_Stream;
	package Scheme renames H2.Wide.Scheme;
	--package Stream renames Slim_Stream;
	--package Scheme renames H2.Slim.Scheme;

	Pool: aliased Storage.Global_Pool;
	SI: Scheme.Interpreter_Record;

	I: Scheme.Object_Pointer;
	O: Scheme.Object_Pointer;

	--String: aliased S.Object_String := "(car '(1 2 3))";
	String: aliased constant Scheme.Object_Character_Array := "((lambda (x y) (+ x y))  9  7)";
	String_Stream: Stream.String_Input_Stream_Record (String'Unchecked_Access);
	--String_Stream: Stream.String_Input_Stream_Record := (Len => String'Length, Str => String, Pos => 0);

	--File_Name: aliased S.Object_Character_Array := "test.adb";
	File_Name: aliased constant Scheme.Object_Character_Array := "시험.scm";
	--File_Stream: Stream.File_Stream_Record (File_Name'Unchecked_Access);
	--File_Stream: Stream.File_Stream_Record := (Name => File_Name'Unchecked_Access);
	File_Stream: Stream.File_Stream_Record;

   --procedure h2init;
   --pragma Import (C, h2init, "h2init");

begin
	--h2init;

declare
	package Sysapi is new H2.Sysapi (
		H2.Slim.Character,
		H2.Wide.Character,
		H2.Slim.String,
		H2.Wide.String,
		H2.Wide.Utf8.To_Unicode_String,
		H2.Wide.Utf8.From_Unicode_String);

	F: Sysapi.File_Pointer;
	FL: Sysapi.File_Flag;
begin
	Sysapi.Set_File_Flag_Bits (FL, Sysapi.FILE_FLAG_WRITE);
	Sysapi.Set_File_Flag_Bits (FL, Sysapi.FILE_FLAG_READ);
	Sysapi.File.Open (F, H2.Slim.String'("/etc/passwd"), FL);
	Sysapi.File.Close (F);
end;


declare

	LC_ALL : constant Interfaces.C.int := 0;
	procedure setlocale (
		category : Interfaces.C.int;
		locale : Interfaces.C.char_array);
	pragma Import (C, setlocale);
	Empty : aliased Interfaces.C.char_array := (0 => Interfaces.C.nul);

begin
	setlocale (LC_ALL, Empty);
end;


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
