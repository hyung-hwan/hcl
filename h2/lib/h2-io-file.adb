with H2.Ascii;

separate (H2.IO)

package body File is

	package Slim_Ascii is new H2.Ascii (Slim_Character);
	package Wide_Ascii is new H2.Ascii (Wide_Character);

	--|-----------------------------------------------------------------------
	--| PRIVATE ROUTINES
	--|-----------------------------------------------------------------------
	procedure OS_Read_File (File:   in out File_Record;
	                        Buffer: in out System_Byte_Array;
	                        Length: out    System_Length) is
	begin
		OS.File.Read (File.File, Buffer, Length);
		File.EOF := (Length <= 0);
	end OS_Read_File;

	procedure Shift_Buffer (Buffer: in out File_Buffer;
	                        Length: in     System_Length) is
		pragma Inline (Shift_Buffer);
	begin
		Buffer.Length := Buffer.Length - Length;
		Buffer.Data(Buffer.Data'First .. Buffer.Data'First + Buffer.Length - 1) := Buffer.Data(Buffer.Data'First + Length .. Buffer.Data'First + Length + Buffer.Length - 1);
	end Shift_Buffer;

	procedure Copy_Array (Dst:    in out System_Byte_Array;
	                      Src:    in     System_Byte_Array;
	                      Length: in     System_Length) is
		pragma Inline (Copy_Array);
	begin
		Dst(Dst'First .. Dst'First + Length - 1) := Src(Src'First .. Src'First + Length - 1);
	end Copy_Array;

	--|-----------------------------------------------------------------------
	--| OPEN AND CLOSE
	--|-----------------------------------------------------------------------

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
		Flush (File);
		OS.File.Close (File.File);
		File.File := null;
	end Close;

	--|-----------------------------------------------------------------------
	--| READ SLIM STRING
	--|-----------------------------------------------------------------------
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
			Copy_Array (Outbuf, Rbuf.Data, L1);

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
			Copy_Array (Outbuf, Rbuf.Data, L2);

			-- Move the residue of the internal buffer to the head
			Shift_Buffer (Rbuf, L2);
			
			-- Set the output length
			Length := L2;
		end if;
	end Read;

	procedure Read_Line (File:   in out File_Record;
	                     Buffer: in out Slim_String;
	                     Length: out   System_Length) is

		pragma Assert (Buffer'Length > 0);

		Outbuf: System_Byte_Array (Buffer'Range);
		for Outbuf'Address use Buffer'Address;

		Rbuf: File_Buffer renames File.Rbuf;
		L1, L2, K: System_Length;

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
				Rbuf.Length := Rbuf.Length + L1;
			end if;

			exit when Rbuf.Length <= 0;

			L2 := Rbuf.Data'First + Rbuf.Length - 1; -- Last index of the internal buffer
			for I in Rbuf.Data'First .. L2 loop
				K := K + 1;
				Outbuf(K) := Rbuf.Data(I);
				if K >= Outbuf'Last or else Outbuf(K) = Slim_Ascii.Pos.LF then -- TODO: different line terminator
					L1 := I - Rbuf.Data'First + 1; -- Length of data copied to the output buffer.
					Shift_Buffer (Rbuf, L1); -- Shift the residue
					exit outer; -- Done
				end if;
			end loop;

			-- Empty the internal buffer;
			Rbuf.Length := 0;
		end loop outer;

		Length := K + 1 - Outbuf'First;
	end Read_Line;

	--|-----------------------------------------------------------------------
	--| READ WIDE STRING
	--|-----------------------------------------------------------------------
	procedure Read_Wide (File:       in out File_Record; 
	                     Buffer:     in out Wide_String;
	                     Length:     out    System_Length;
	                     Terminator: in     Wide_String) is
		pragma Assert (Buffer'Length > 0);
		Outbuf: Wide_String renames Buffer;

		Rbuf: File_Buffer renames File.Rbuf;
		Inbuf: Slim_String (Rbuf.Data'Range);
		for Inbuf'Address use Rbuf.Data'Address;
		
		L1, L2, L3, L4, I, J, K: System_Length;
	begin
		if Rbuf.Length <= 0 and then File.EOF then
			-- raise EOF EXCEPTION. ???
			Length := 0;
			return;
		end if;

		K := Outbuf'First - 1;

		outer: while K < Outbuf'Last loop

			if Rbuf.Length < Rbuf.Data'Length then
				-- Attempt to fill the internal buffer. It may not get full with a single read.
				OS_Read_File (File, Rbuf.Data(Rbuf.Data'First + Rbuf.Length .. Rbuf.Data'Last), L1);
				Rbuf.Length := Rbuf.Length + L1;
			end if;

			exit when Rbuf.Length <= 0;

			L2 := Rbuf.Data'First + Rbuf.Length - 1; -- Last index of the internal buffer
			I := Rbuf.Data'First;
			while I <= L2 and K < Outbuf'Last loop
				L3 := Sequence_Length(Inbuf(I)); -- Required number of bytes to compose a wide character

				if L3 <= 0 then
					-- Potentially illegal sequence 
					K := K + 1;
					Outbuf(K) := Wide_Ascii.Question;
					I := I + 1;
				else
					L4 := L2 - I + 1;  -- Avaliable number of bytes available in the internal buffer
					if L4 < L3 then
						-- Insufficient data available. Exit the inner loop to read more.
						exit;
					end if;

					K := K + 1;
					J := I + L3;
					begin
						Outbuf(K..K) := Slim_To_Wide(Inbuf(I .. J - 1));
					exception
						when others => 
							Outbuf(K) := Wide_Ascii.Question;
							J := I + 1; -- Override J to skip 1 byte only
					end;
					I := J;
				end if;

				if Terminator'Length > 0 and then
				   Outbuf(K) = Terminator(Terminator'First) then 
					-- TODO: compare more characters in terminator, not just the first charactrer
					Shift_Buffer (Rbuf, I - Rbuf.Data'First);
					exit outer;
				end if;
			end loop;

			-- Move the residue to the front.
			Shift_Buffer (Rbuf, I - Rbuf.Data'First);
		end loop outer;

		Length := K + 1 - Outbuf'First;
	end Read_Wide;

	procedure Read (File:   in out File_Record; 
	                Buffer: in out Wide_String;
	                Length: out    System_Length) is
		Terminator: Wide_String(1..0);
	begin
		Read_Wide (File, Buffer, Length, Terminator);
	end Read;

	procedure Read_Line (File:   in out File_Record; 
	                     Buffer: in out Wide_String;
	                     Length: out    System_Length) is
		Terminator: constant Wide_String(1..1) := (1 => Wide_Ascii.LF);
	begin
		Read_Wide (File, Buffer, Length, Terminator);
	end Read_Line;

	--|-----------------------------------------------------------------------
	--| WRITE SLIM STRING
	--|-----------------------------------------------------------------------
	procedure Write (File:   in out File_Record; 
	                 Buffer: in     Slim_String;
	                 Length: out    System_Length) is
		Wbuf: File_Buffer renames File.Wbuf;

		Inbuf: System_Byte_Array (Buffer'Range);
		for Inbuf'Address use Buffer'Address;

		F, L: System_Length;
	begin
		-- This procedure attempts to write as many bytes as requested.
		-- However, under a certain condition, it may not be able to 
		-- process the input buffer in full.

		if Wbuf.Length > 0 then
			-- Some residue data in the internal buffer.

			if Inbuf'Length <= Wbuf.Data'Length - Wbuf.Length then
				-- Copy the input to the internal buffer to reduce OS calls
				F := Wbuf.Data'First + Wbuf.Length - 1;
				L := F + Inbuf'Length - 1;
				Wbuf.Data(F .. L) := Inbuf;
				Flush (File);

				Length := Inbuf'Length;
				return;
			end if;

			-- Flush the residue first.
			Flush (File);
		end if;

		L := 0;
		while L < Inbuf'Length loop
			--begin
				OS.File.Write (File.File, Inbuf, F);
			--exception
			--	when OS.Would_Block_Exception =>
			--		-- Cannot write the input in full.
			--		-- Copy some to to the internal buffer
			--		L := L + as much as copied;
			--		exit;
			--	when others =>
			--		raise;
			--end;
			L := L + F;
		end loop;

		Length := L;
	end Write;

	procedure Write_Line (File:   in out File_Record; 
	                      Buffer: in     Slim_String;
	                      Length: out    System_Length) is
		Wbuf: File_Buffer renames File.Wbuf;

		Inbuf: System_Byte_Array (Buffer'Range);
		for Inbuf'Address use Buffer'Address;

		I, J, LF: System_Length;

	begin
		-- This procedure attempts to write the input up to the last line
		-- terminator. It buffers the remaining input after the terminator.
		-- The input may not include any line terminators. 

		LF := Wbuf.Data'First - 1;
		I := Inbuf'First - 1;
		while I < Inbuf'Last loop
			I := I + 1;
			J := Wbuf.Data'First + Wbuf.Length;
			Wbuf.Data(J) := Inbuf(I);
			Wbuf.Length := Wbuf.Length + 1;
			if Wbuf.Data(J) = Slim_Ascii.Pos.LF then -- TODO: different line terminator
				-- Remeber the index of the line terminator
				LF := J;
			end if;
			
			if Wbuf.Length >= Wbuf.Data'Length then
				-- The internal write buffer is full.
				Flush (File);
				LF := Wbuf.Data'First - 1;
			end if;
		end loop;

		-- The line terminator was found. Write up to the terminator.
		-- Keep the rest in the internal buffer.
		while LF in Wbuf.Data'First .. Wbuf.Length loop
			OS.File.Write (File.File, Wbuf.Data(Wbuf.Data'First .. LF), J);
			Shift_Buffer (Wbuf, J);
			LF := LF - J;
		end loop;

		Length := I - Inbuf'First + 1;
	end Write_Line;

	--|-----------------------------------------------------------------------
	--| WRITE WIDE STRING
	--|-----------------------------------------------------------------------
	procedure Write (File:   in out File_Record; 
	                 Buffer: in     Wide_String;
	                 Length: out    System_Length) is
		Wbuf: File_Buffer renames File.Wbuf;
		F, L, I: System_Length;
	begin
		I := Buffer'First - 1;
		while I < Buffer'Last loop
			I := I + 1;
			declare
				Tmp: Slim_String := Wide_To_Slim(Buffer(I..I));
				Tmp2: System_Byte_Array(Tmp'Range);
				for Tmp2'Address use Tmp'Address;
			begin
				F := Wbuf.Data'First + Wbuf.Length;
				L := F + Tmp2'Length - 1;

				if L > Wbuf.Data'Last then
					-- The multi-byte sequence for the current character
					-- can't fit into the internal buffer. Flush the
					-- buffer and attempt to fit it in.
					Flush (File);
					F := Wbuf.Data'First;
					L := F + Tmp2'Length - 1;
				end if;

				Wbuf.Data(F..L) := Tmp2;
				Wbuf.Length := Wbuf.Length + Tmp2'Length;
			end;
		end loop;

		Flush (File);
		Length := I - Buffer'First + 1;
	end Write;

	procedure Write_Line (File:   in out File_Record; 
	                      Buffer: in     Wide_String;
	                      Length: out    System_Length) is

		Wbuf: File_Buffer renames File.Wbuf;
		F, L, I, LF: System_Length;
	begin
		LF := Wbuf.Data'First - 1;
		I := Buffer'First - 1;
		while I < Buffer'Last loop
			I := I + 1;
			declare
				Tmp: Slim_String := Wide_To_Slim(Buffer(I..I));
				Tmp2: System_Byte_Array(Tmp'Range);
				for Tmp2'Address use Tmp'Address;
			begin
				F := Wbuf.Data'First + Wbuf.Length;
				L := F + Tmp2'Length - 1;

				if L > Wbuf.Data'Last then
					-- The multi-byte sequence for the current character
					-- can't fit into the internal buffer. Flush the
					-- buffer and attempt to fit it in.
					Flush (File);
					F := Wbuf.Data'First;
					L := F + Tmp2'Length - 1;
					LF := Wbuf.Data'First - 1;
				end if;

				Wbuf.Data(F..L) := Tmp2;
				Wbuf.Length := Wbuf.Length + Tmp2'Length;

				if Buffer(I) = Wide_Ascii.LF then -- TODO: different line terminator
					LF := L;
				end if;
			end;
		end loop;

		-- The line terminator was found. Write up to the terminator.
		-- Keep the rest in the internal buffer.
		while LF in Wbuf.Data'First .. Wbuf.Length loop
			OS.File.Write (File.File, Wbuf.Data(Wbuf.Data'First .. LF), L);
			Shift_Buffer (Wbuf, L);
			LF := LF - L;
		end loop;

		Length := I - Buffer'First + 1;
	end Write_Line;

	--|-----------------------------------------------------------------------
	--| FLUSH AND DRAIN
	--|-----------------------------------------------------------------------
	procedure Flush (File: in out File_Record) is
		Wbuf: File_Buffer renames File.Wbuf;
		Length: System_Length;
	begin
		while Wbuf.Length > 0 loop
			--begin
				OS.File.Write (File.File, Wbuf.Data(Wbuf.Data'First .. Wbuf.Length), Length);
			--exception
			--	when Would_Block_Exception =>
			--		-- Flush must write all it can.
			--		null;
			--	when others => 
			--		raise;
			--end;
			Shift_Buffer (Wbuf, Length);
		end loop;
	end Flush;
	                 
	procedure Drain (File: in out File_Record) is
	begin
		File.Wbuf.Length := 0;
	end Drain;
end File;
