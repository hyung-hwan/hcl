with H2.Pool;
with Ada.Unchecked_Conversion;

with Ada.Text_IO; -- for debugging

package body Stream is

	------------------------------------------------------------------
	use type S.Object_String_Size;

	procedure Open (Stream: in out String_Input_Stream_Record) is
	begin
Ada.Text_IO.Put_Line ("****** OPEN STRING STREAM ******");
		Stream.Pos := 0;
	end Open;

	procedure Close (Stream: in out String_Input_Stream_Record) is
	begin
Ada.Text_IO.Put_Line ("****** CLOSE STRING STREAM ******");
		Stream.Pos := Stream.Str'Last;
	end Close;

	procedure Read (Stream: in out String_Input_Stream_Record;
	                Data:   out    S.Object_String;
	                Last:   out    S.Object_String_Size) is
		Avail: S.Object_String_Size;
	begin
		Avail := Stream.Str'Last - Stream.Pos;
		if Avail <= 0 then
			-- EOF
			Last := Data'First - 1;
		else
			if Avail > Data'Length then
				Avail := Data'Length;
			end if;

			Data(Data'First .. Avail) := Stream.Str(Stream.Pos + 1..Stream.Pos + Avail);
			Stream.Pos := Stream.Pos + Avail;
			Last := Data'First + Avail - 1;
		end if;
	end Read;

	procedure Write (Stream: in out String_Input_Stream_Record;
	                 Data:   out    S.Object_String;
	                 Last:   out    S.Object_String_Size) is
	begin
		--raise S.Stream_Error;
		Last := Data'First - 1;
	end Write;

	------------------------------------------------------------------

	procedure Open (Stream: in out File_Stream_Record) is
	begin
Ada.Text_IO.Put_Line (">>>>> OPEN File STREAM <<<<< " & Standard.String(UTF8.Unicode_To_Utf8(UTF8.Unicode_String(Stream.Name.all))));
		--Ada.Wide_Text_IO.Open (Stream.Handle, Ada.Wide_Text_IO.In_File, Ada.Characters.Conversions.To_String(Standard.Wide_String(Stream.Name.all)));
		Ada.Wide_Text_IO.Open (Stream.Handle, Ada.Wide_Text_IO.In_File, Standard.String(UTF8.Unicode_To_Utf8(UTF8.Unicode_String(Stream.Name.all))));
	end Open;

	procedure Close (Stream: in out File_Stream_Record) is
		subtype Wide_String is Standard.Wide_String(1 .. Standard.Natural(Stream.Name'Length));
		function To_Wide_String is new Ada.Unchecked_Conversion (S.Object_String, Wide_String);
	begin
--Ada.Wide_Text_IO.Put_Line (">>>>> CLOSE File STREAM <<<<< " & Standard.Wide_String(Stream.Name.all));
Ada.Text_IO.Put_Line (">>>>> CLOSE File STREAM <<<<< " & Standard.String(UTF8.Unicode_To_Utf8(UTF8.Unicode_String(Stream.Name.all))));
		Ada.Wide_Text_IO.Close (Stream.Handle);
	end Close;

	procedure Read (Stream: in out File_Stream_Record;
	                Data:   out    S.Object_String;
	                Last:   out    S.Object_String_Size) is
	begin
		for I in Data'First .. Data'Last loop
			begin
				if Ada.Wide_Text_IO.End_Of_File (Stream.Handle) then
					Last := I - 1;
					return;
				end if;
				Ada.Wide_Text_IO.Get_Immediate (Stream.Handle, Data(I));
			exception
				when Ada.Wide_Text_IO.End_Error =>
					Last := I - 1;
					return;
				-- other exceptions must be just raised to indicate errors
			end;
		end loop;
		Last := Data'Last;
	end Read;

	procedure Write (Stream: in out File_Stream_Record;
	                 Data:   out    S.Object_String;
	                 Last:   out    S.Object_String_Size) is
	begin
		--raise S.Stream_Error;
		Last := Data'First - 1;
	end Write;

	------------------------------------------------------------------

	procedure Allocate_Stream (Interp: in out S.Interpreter_Record;
	                           Name:   in     S.Constant_Object_String_Pointer;
	                           Result: out    S.Stream_Pointer) is
		subtype FSR is Stream.File_Stream_Record;
		type FSP is access all FSR;
		package P is new H2.Pool (FSR, FSP);

		X: FSP;
		for X'Address use Result'Address;
		pragma Import (Ada, X);
	begin
		X := P.Allocate (S.Get_Storage_Pool(Interp));
		X.Name := Name;
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
end Stream;
