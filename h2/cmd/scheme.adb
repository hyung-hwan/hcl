with H2.Scheme;
with Storage;
with Ada.Text_IO;

procedure scheme is
	package S renames H2.Scheme;

	Pool: aliased Storage.Global_Pool;
	SI: S.Interpreter_Record;

begin
	Ada.Text_Io.Put_Line (S.Object_Word'Image(S.Object_Pointer_Bytes));

	S.Open (SI, 2_000_000, Pool'Unchecked_Access);
	--S.Open (SI, null);
	S.Evaluate (SI);
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
