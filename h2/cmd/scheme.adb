
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

begin
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

	declare
		subtype x is S.Object_Record (S.Moved_Object, 0);
		subtype y is S.Object_Record (S.Pointer_Object, 1);
		subtype z is S.Object_Record (S.Character_Object, 1);
		subtype q is S.Object_Record (S.Byte_Object, 1);
		a: x;
		b: y;
		c: z;
		d: q;
          w: S.Object_Word;
	begin
	Ada.Text_Io.Put_Line (S.Object_Word'Image(w'Size));
	Ada.Text_Io.Put_Line (S.Object_Word'Image(S.Object_Word'Size));
	Ada.Text_IO.Put_Line ("------");
	Ada.Text_Io.Put_Line (S.Object_Word'Image(x'Size));
	Ada.Text_Io.Put_Line (S.Object_Word'Image(y'Size));
	Ada.Text_Io.Put_Line (S.Object_Word'Image(z'Size));
	Ada.Text_Io.Put_Line (S.Object_Word'Image(q'Size));
	Ada.Text_Io.Put_Line (S.Object_Word'Image(x'Max_Size_In_Storage_Elements));
	Ada.Text_Io.Put_Line (S.Object_Word'Image(y'Max_Size_In_Storage_Elements));
	Ada.Text_Io.Put_Line (S.Object_Word'Image(z'Max_Size_In_Storage_Elements));
	Ada.Text_Io.Put_Line (S.Object_Word'Image(q'Max_Size_In_Storage_Elements));
	Ada.Text_Io.Put_Line (S.Object_Word'Image(a'Size));
	Ada.Text_Io.Put_Line (S.Object_Word'Image(b'Size));
	Ada.Text_Io.Put_Line (S.Object_Word'Image(c'Size));
	Ada.Text_Io.Put_Line (S.Object_Word'Image(c'Size));
	Ada.Text_Io.Put_Line (S.Object_Integer'Image(S.Object_Integer'First));
	Ada.Text_Io.Put_Line (S.Object_Integer'Image(S.Object_Integer'Last));
	end;

	Ada.Text_IO.Put_Line ("BYE...");

end scheme;
