with H2.Ascii;

separate (H2.IO)

package body File is

	procedure Open (File: in out File_Record;
	                Name: in     Slim_String;
	                Flag: in     Flag_Record;
	                Pool: in     Storage_Pool_Pointer := null) is
	begin
		OS.File.Open (File.File, Name, Flag, Pool => Pool);
		File.Rbuf.Length := 0;
		File.Wbuf.Length := 0;
		File.EOF := Standard.False;
	end Open;

	procedure Open (File: in out File_Record;
	                Name: in     Wide_String;
	                Flag: in     Flag_Record;
	                Pool: in     Storage_Pool_Pointer := null) is
	begin
		OS.File.Open (File.File, Name, Flag, Pool => Pool);
		File.Rbuf.Length := 0;
		File.Wbuf.Length := 0;
		File.EOF := Standard.False;
	end Open;

	procedure Close (File: in out File_Record) is
	begin
		OS.File.Close (File.File);
		File.File := null;
	end Close;

	procedure OS_Read_File (File:   in out File_Record;
	                        Buffer: in out System_Byte_Array;
	                        Length: out    System_Length) is
	begin
		OS.File.Read (File.File, Buffer, Length);
		File.EOF := (Length <= 0);
	end OS_Read_File;

	procedure Read (File:   in out File_Record; 
	                Buffer: in out Slim_String;
	                Length: out    System_Length) is
		pragma Assert (Buffer'Length > 0);

		Outbuf: System_Byte_Array (Buffer'Range);
		for Outbuf'Address use Buffer'Address;

		Rbuf: File_Buffer renames File.Rbuf;
		L1, L2: System_Length;
	begin
		if Rbuf.Length <= 0 and then File.EOF then
			-- raise EOF EXCEPTION. ???
			Length := 0;
			return;
		end if;

		if Outbuf'Length >= Rbuf.Data'Length then
			-- The output buffer size if greater than the internal buffer size.

			L1 := Rbuf.Length;
			if L1 < Outbuf'Length then
				-- Read into the tail of the output buffer if insufficient
				-- data is available in the internal buffer.
				OS_Read_File (File, Outbuf(Outbuf'First + L1 .. Outbuf'Last), L2);
			end if;

			-- Fill the head of the output buffer with the internal buffer contents
			Outbuf(Outbuf'First .. Outbuf'First + L1 - 1) := Rbuf.Data(Rbuf.Data'First .. Rbuf.Data'First + L1 - 1);

			-- Empty the internal buffer.
			Rbuf.Length := 0;

			-- Set the output length
			Length := L1 + L2;
		else
			if Rbuf.Length < Rbuf.Data'Length then
				-- Attempt to fill the internal buffer. It may not get full with a single read.
				OS_Read_File (File, Rbuf.Data(Rbuf.Data'First + Rbuf.Length .. Rbuf.Data'Last), L1);
				Rbuf.Length := RBuf.Length + L1;
			end if;

			-- Determine how much need to be copied to the output buffer.
			If Outbuf'Length < Rbuf.Length then
				L2 := Outbuf'Length;
			else
				L2 := Rbuf.Length;
			end if;

			-- Copy the head of the internal buffer to the output buffer
			Outbuf(Outbuf'First .. Outbuf'First + L2 - 1) := Rbuf.Data(Rbuf.Data'First .. Rbuf.Data'First + L2 - 1);

			-- Move the residue of the internal buffer to the head
			Rbuf.Length := Rbuf.Length - L2;
			Rbuf.Data(Rbuf.Data'First .. Rbuf.Data'First + Rbuf.Length - 1) := Rbuf.Data(Rbuf.Data'First + L2 .. Rbuf.Data'First + L2 + Rbuf.Length - 1);
			
			-- Set the output length
			Length := L2;
		end if;

	end Read;

	procedure Read (File:   in out File_Record; 
	                Buffer: in out Wide_String;
	                Length: out    System_Length) is
		pragma Assert (Buffer'Length > 0);
		Outbuf: Wide_String renames Buffer;

		Rbuf: File_Buffer renames File.Rbuf;
		Inbuf: Slim_String (Rbuf.Data'Range);
		for Inbuf'Address use Rbuf.Data'Address;
		
		L1, L2, L3, I, J, K: System_Length;

	begin
		if Rbuf.Length <= 0 and then File.EOF then
			-- raise EOF EXCEPTION. ???
			Length := 0;
			return;
		end if;

		K := Outbuf'First - 1;

		outer: loop
			if Rbuf.Length < Rbuf.Data'Length then
				-- Attempt to fill the internal buffer. It may not get full with a single read.
				OS_Read_File (File, Rbuf.Data(Rbuf.Data'First + Rbuf.Length .. Rbuf.Data'Last), L1);
				File.EOF := (L1 <= 0);
				Rbuf.Length := Rbuf.Length + L1;
			end if;

			if Rbuf.Length <= 0 then
				exit outer;
			end if;

			L2 := Rbuf.Data'First + Rbuf.Length - 1; -- Last index of the internal buffer
			I := Rbuf.Data'First;
			loop
				L3 := Sequence_Length (Inbuf(I));
				if L2 - I + 1 < L3 then
					exit;
				end if;

				K := K + 1;
				J := I + L3;
				Outbuf(K..K) := Slim_To_Wide(Inbuf(I .. J - 1));
				I := J;
				
				--if K >= Outbuf'Last or else Outbuf(K) = Ascii.Pos.LF then -- TODO: different line terminator
				--	L1 := I - Rbuf.Data'First + 1; -- Length of data copied to the output buffer.
				--	Rbuf.Length := Rbuf.Length - L1; -- Residue length
				--	Rbuf.Data(Rbuf.Data'First .. RBuf.Data'First + Rbuf.Length - 1) := Rbuf.Data(I + 1 .. L2); -- Copy residue
				--	exit outer; -- Done
				--end if;
			end loop;

			-- Empty the internal buffer;
			Rbuf.Length := 0;
		end loop outer;

		Length := K + 1 - Outbuf'First;
	end Read;

	procedure Read_Line (File:   in out File_Record;
	                     Buffer: in out Slim_String;
	                     Length: out   System_Length) is

		pragma Assert (Buffer'Length > 0);

		Outbuf: System_Byte_Array (Buffer'Range);
		for Outbuf'Address use Buffer'Address;

		Rbuf: File_Buffer renames File.Rbuf;
		L1, L2, K: System_Length;

		package Ascii is new H2.Ascii (Slim_Character);
	begin
		-- Unlike Read, this procedure should use the internal buffer
		-- regardless of the output buffer size as the position of
		-- the line terminator is unknown. 
		--
		-- If the buffer is not large enough to hold a line, the output
		-- is just truncated truncated to the buffer size.

		if Rbuf.Length <= 0 and then File.EOF then
			-- raise EOF EXCEPTION. ???
			Length := 0;
			return;
		end if;

		K := Outbuf'First - 1;

		outer: loop
			if Rbuf.Length < Rbuf.Data'Length then
				-- Attempt to fill the internal buffer. It may not get full with a single read.
				OS_Read_File (File, Rbuf.Data(Rbuf.Data'First + Rbuf.Length .. Rbuf.Data'Last), L1);
				File.EOF := (L1 <= 0);
				Rbuf.Length := Rbuf.Length + L1;
			end if;

			if Rbuf.Length <= 0 then
				exit outer;
			end if;

			L2 := Rbuf.Data'First + Rbuf.Length - 1; -- Last index of the internal buffer
			for I in Rbuf.Data'First .. L2 loop
				K := K + 1;
				Outbuf(K) := Rbuf.Data(I);
				if K >= Outbuf'Last or else Outbuf(K) = Ascii.Pos.LF then -- TODO: different line terminator
					L1 := I - Rbuf.Data'First + 1; -- Length of data copied to the output buffer.
					Rbuf.Length := Rbuf.Length - L1; -- Residue length
					Rbuf.Data(Rbuf.Data'First .. RBuf.Data'First + Rbuf.Length - 1) := Rbuf.Data(I + 1 .. L2); -- Copy residue
					exit outer; -- Done
				end if;
			end loop;

			-- Empty the internal buffer;
			Rbuf.Length := 0;
		end loop outer;

		Length := K + 1 - Outbuf'First;
	end Read_Line;

	procedure Read_Line (File:   in out File_Record;
	                     Buffer: in out Wide_String;
	                     Length: out   System_Length) is
	begin
		null;
	end Read_Line;

	procedure Write (File:   in out File_Record; 
	                 Buffer: in     Slim_String;
	                 Length: out    System_Length) is
	begin
		null;
	end Write;

	procedure Write (File:   in out File_Record; 
	                 Buffer: in     Wide_String;
	                 Length: out    System_Length) is
	begin
		null;
	end Write;

	procedure Flush (File: in out File_Record) is
	begin
		null;
	end Flush;
	                 
end File;
