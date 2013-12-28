with H2.Scheme;
with H2.Pool;
with Storage;
with Stream;
with Ada.Text_IO;
with Ada.Unchecked_Deallocation;

procedure scheme is
	package S renames H2.Scheme;

	Pool: aliased Storage.Global_Pool;
	SI: S.Interpreter_Record;

	I: S.Object_Pointer;
	O: S.Object_Pointer;

	--String: aliased S.Object_String := "(car '(1 2 3))";
	String: aliased S.Object_String := "((lambda (x y) (+ x y))  9  7)";
	String_Stream: Stream.String_Input_Stream_Record (String'Unchecked_Access);
	--String_Stream: Stream.String_Input_Stream_Record := (Len => String'Length, Str => String, Pos => 0);
	

	File_Name: aliased S.Object_String := "test.adb";
	--File_Stream: Stream.File_Stream_Record (File_Name'Unchecked_Access);
	--File_Stream: Stream.File_Stream_Record := (Name => File_Name'Unchecked_Access);
	File_Stream: Stream.File_Stream_Record;

	procedure Allocate_Stream (Interp: in out S.Interpreter_Record;
	                           Name:   access S.Object_String;
	                           Result: in out S.Stream_Pointer) is
		subtype FSR is Stream.File_Stream_Record;
		type FSP is access all FSR;
		package P is new H2.Pool (FSR, FSP);

		X: FSP;
		for X'Address use Result'Address;
		pragma Import (Ada, X);
	begin
		X := P.Allocate (S.Get_Storage_Pool(Interp));
		X.Name := Stream.Object_String_Pointer(Name);
	end Allocate_Stream;

	procedure Deallocate_Stream (Interp: in out S.Interpreter_Record;
	                             Source: in out S.Stream_Pointer) is
		subtype FSR is Stream.File_Stream_Record;
		type FSP is access all FSR;
		package P is new H2.Pool (FSR, FSP);

		X: FSP;
		for X'Address use Source'Address;
		pragma Import (Ada, X);
	begin
		P.Deallocate (X, S.Get_Storage_Pool(Interp));
	end Deallocate_Stream;

--   --procedure Dealloc_Stream is new Ada.Unchecked_Deallocation (Stream_Record'Class, Stream_Pointer);
--   --procedure Destroy_Stream (Stream: in out Stream_Pointer) renames Dealloc_Stream;


begin
	Ada.Text_Io.Put_Line (S.Object_Word'Image(S.Object_Pointer_Bytes));

	S.Open (SI, 2_000_000, Pool'Unchecked_Access);
	--S.Open (SI, null);

File_Stream.Name := File_Name'Unchecked_Access;
S.Set_Input_Stream (SI, File_Stream); -- specify main input stream
--S.Set_Output_Stream (SI, Stream); -- specify main output stream.
S.Read (SI, I);
S.Make_Test_Object (SI, I);

	S.Evaluate (SI, I, O);
S.Print (SI, I);
Ada.Text_IO.Put_Line ("-------------------------------------------");
S.Print (SI, O);
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
