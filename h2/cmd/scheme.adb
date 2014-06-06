
with H2.Wide;
with H2.Slim;
with H2.Pool;
with Storage;
with Slim_Stream;
with Wide_Stream;
with Ada.Text_IO;
with Ada.Wide_Text_IO;
with Ada.Unchecked_Deallocation;


with H2.OS;
with H2.IO;
use type H2.System_Length;

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
	package OS is new H2.OS (
		H2.Slim.Character,
		H2.Wide.Character,
		H2.Slim.String,
		H2.Wide.String,
		H2.Wide.Utf8.To_Unicode_String,
		H2.Wide.Utf8.From_Unicode_String);
	package File renames OS.File;

	F: File.File_Pointer;
	FL: File.Flag_Record;
	Length: H2.System_Length;
	Buffer: H2.System_Byte_Array (50 .. 100);
begin
	--OS.File.Set_Flag_Bits (FL, OS.File.FLAG_WRITE); 
	File.Set_Flag_Bits (FL, File.FLAG_READ);
	File.Open (F, H2.Wide.String'("/etc/passwd"), FL);
	File.Read (F, Buffer, Length);
	File.Close (F);

	File.Write (OS.File.Get_Stdout, Buffer(Buffer'First .. Buffer'First + Length - 1), Length);
end;

declare
	package IO is new H2.IO (
		H2.Slim.Character,
		H2.Wide.Character,
		H2.Slim.String,
		H2.Wide.String,
		H2.Wide.Utf8.To_Unicode_String,
		H2.Wide.Utf8.From_Unicode_String,
		H2.Wide.Utf8.Sequence_Length);

	package File renames IO.File;

	F, F2: File.File_Record;
	FL: File.Flag_Record;
	Buffer: H2.Slim.String (1 .. 200);
	BufferW: H2.Wide.String (1 .. 50);
	IL, OL: H2.System_Length;
begin
	--File.Open (F, H2.Slim.String'("/etc/passwd"), FL);
	--File.Read (F, Buffer, Length);
	--Ada.Text_IO.PUt_Line (Standard.String(Buffer(Buffer'First .. Buffer'First + Length - 1)));

	--File.Read (F, Buffer, Length);
	--Ada.Text_IO.PUt (Standard.String(Buffer(Buffer'First .. Buffer'First + Length - 1)));
	--File.Close (F);

ada.text_io.put_line ("------------------");
	File.Set_Flag_Bits (FL, File.FLAG_READ);
	File.Set_Flag_Bits (FL, File.FLAG_NONBLOCK);
	File.Open (F, H2.Slim.String'("/tmp/xxx"), FL);

	File.Clear_Flag_Bits (FL, FL.Bits);
	File.Set_Flag_Bits (FL, File.FLAG_WRITE);
	File.Set_Flag_Bits (FL, File.FLAG_CREATE);
	File.Set_Flag_Bits (FL, File.FLAG_TRUNCATE);
	File.Open (F2, H2.Wide.String'("/tmp/yyy"), FL);
	loop
		File.Read (F, Buffer, IL);
		--File.Read (F, BufferW, IL);
		exit when IL <= 0;

		File.Write_Line (F2, Buffer(Buffer'First .. Buffer'First + IL - 1), OL);
		pragma Assert (IL = OL);

		--Ada.Text_IO.PUt (Standard.String(Buffer(Buffer'First .. Buffer'First + IL - 1)));
		--Ada.Wide_Text_IO.Put_Line (Standard.Wide_String(BufferW(BufferW'First .. BufferW'First + IL - 1)));
	end loop;

	File.Write (F2, H2.Wide.String'("나는 피리부는 사나이 정말로 멋있는 사나이"), OL);
	File.Write_Line (F2, H2.Wide.String'("이세상에 문디없어면 무슨재미로 너도 나도 만세."), OL);
	File.Close (F2);
	File.Close (F);
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
