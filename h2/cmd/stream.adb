
package body Stream is

	------------------------------------------------------------------

	procedure Open (Stream: in out String_Input_Stream_Record) is
	begin
Ada.Text_IO.Put_Line ("OPEN STRING STREAM");
		Stream.Pos := 0;
	end Open;

	procedure Close (Stream: in out String_Input_Stream_Record) is
	begin
Ada.Text_IO.Put_Line ("CLOSE STRING STREAM");
		Stream.Pos := Stream.Str'Last;
	end Close;

	procedure Read (Stream: in out String_Input_Stream_Record;
	                Data:   out    S.Object_String;
	                Last:   out    Standard.Natural) is
		Avail: Standard.Natural;
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
	                 Last:   out    Standard.Natural) is
	begin
		--raise S.Stream_Error;
		Last := Data'First - 1;
	end Write;

	------------------------------------------------------------------

	procedure Open (Stream: in out File_Stream_Record) is
	begin
Ada.Text_IO.Put_Line ("OPEN File STREAM");
		Ada.Text_IO.Open (Stream.Handle, Ada.Text_IO.In_File, Stream.Name.all);
	end Open;

	procedure Close (Stream: in out File_Stream_Record) is
	begin
Ada.Text_IO.Put_Line ("CLOSE File STREAM");
		Ada.Text_IO.Close (Stream.Handle);
	end Close;

	procedure Read (Stream: in out File_Stream_Record;
	                Data:   out    S.Object_String;
	                Last:   out    Standard.Natural) is
	begin
		for I in Data'First .. Data'Last loop
			begin
				Ada.Text_IO.Get_Immediate (Stream.Handle, Data(I));
			exception
				when Ada.Text_IO.End_Error =>
					Last := I - 1;
					return;
				-- other exceptions must be just raised to indicate errors
			end;
		end loop;
		Last := Data'Last;
	end Read;

	procedure Write (Stream: in out File_Stream_Record;
	                 Data:   out    S.Object_String;
	                 Last:   out    Standard.Natural) is
	begin
		--raise S.Stream_Error;
		Last := Data'First - 1;
	end Write;

	------------------------------------------------------------------

end Stream;
