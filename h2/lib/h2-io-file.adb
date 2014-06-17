with H2.Ascii;

separate (H2.IO)

package body File is

	package Slim_Ascii renames IO.Slim_Ascii;
	package Wide_Ascii renames IO.Wide_Ascii;

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


	Slim_Line_Terminator: Slim_String := Get_Line_Terminator;
	--Wide_Line_Terminator: Wide_String := Get_Line_Terminator;

	--|-----------------------------------------------------------------------
	--| OPEN AND CLOSE
	--|-----------------------------------------------------------------------
	function Is_Open (File: in File_Record) return Standard.Boolean is
	begin
		return OS.File."/="(File.File, null);
	end Is_Open;

	procedure Open (File: in out File_Record;
	                Name: in     Slim_String;
	                Flag: in     Flag_Record;
	                Pool: in     Storage_Pool_Pointer := null) is
		pragma Assert (not Is_Open(File));
	begin
		OS.File.Open (File.File, Name, Flag, Pool => Pool);
		File.Rbuf.First := 0;
		File.Rbuf.Last := 0;
		File.Rbuf.Length := 0;

		File.Wbuf.Length := 0;
		File.EOF := Standard.False;
		--File.Slim_Line_Break := Get_Line_Terminator;
		--File.Wide_Line_Break := Get_Line_Terminator;
	end Open;

	procedure Open (File: in out File_Record;
	                Name: in     Wide_String;
	                Flag: in     Flag_Record;
	                Pool: in     Storage_Pool_Pointer := null) is
		pragma Assert (not Is_Open(File));
	begin
		OS.File.Open (File.File, Name, Flag, Pool => Pool);
		File.Rbuf.Length := 0;
		File.Wbuf.Length := 0;
		File.EOF := Standard.False;
		--File.Slim_Line_Break := Get_Line_Terminator;
		--File.Wide_Line_Break := Get_Line_Terminator;
	end Open;

	procedure Close (File: in out File_Record) is
		pragma Assert (Is_Open(File));
	begin
		Flush (File);
		OS.File.Close (File.File);
		File.File := null;
	end Close;

	function Is_Empty (Buf: in File_Buffer) return Standard.Boolean is
		pragma Inline (Is_Empty);
	begin
		return Buf.First >= Buf.Last;
	end Is_Empty;

	procedure Set_Length (Buf: in out File_Buffer; Length: in System_Length) is
		pragma Inline (Set_Length);
	begin
		Buf.First := Buf.Data'First - 1; -- this should be 0
		Buf.Last := Buf.First + Length;
	end Set_Length;

	procedure Load_Bytes (File: in out File_Record) is
		pragma Assert (Is_Open(File));
		pragma Assert (Is_Empty(File.Rbuf));
		L1: System_Length;
	begin
		if File.EOF then
			-- raise EOF EXCEPTION. ???
			null;
		else
			-- Read bytes into the buffer
			OS_Read_File (File, File.Rbuf.Data, L1);
			Set_Length  (File.Rbuf, L1);
		end if;
	end Load_Bytes;

	procedure Fetch_Byte (File:      in out File_Record; 
	                      Item:      out    System_Byte;
	                      Available: out    Standard.Boolean) is
	begin
		-- NOTE: If no data is available, Item is not initialized in this procedure
		if Is_Empty(File.Rbuf) then
			Load_Bytes (File);
		end if;
			
		if Is_Empty(File.Rbuf) then
			Available := Standard.False;
		else
			-- Consume 1 byte
			Available := Standard.True;
			File.Rbuf.First := File.Rbuf.First + 1;
			Item := File.Rbuf.Data(File.Rbuf.First);
		end if;
	end Fetch_Byte;

	procedure Fetch_Bytes (File:   in out File_Record;
	                       Item:   out    System_Byte_Array;
	                       Length: out    System_Length) is
		pragma Assert (Is_Open(File));
		L1, L2: System_Length;
	begin
		if Is_Empty(File.Rbuf) and then File.EOF then
			-- raise EOF EXCEPTION. ???
			Length := 0;
		else
			L1 := File.Rbuf.Last - File.Rbuf.First;
			if L1 > 0 then
				-- Copy the residue over to the output buffer
				if Item'Length <= L1 then
					L2 := Item'Length;
				else
					L2 := L1;
				end if;
			
				Copy_Array (Item, File.Rbuf.Data(File.Rbuf.First + 1 .. File.Rbuf.Last), L2);
				File.Rbuf.First := File.Rbuf.First + L2;

				Length := L2;
			else
				Length := 0;
			end if;

			if Item'Length > L1 then
				-- Item is not full. the internal read buffer must be empty.
				pragma Assert (File.Rbuf.First >= File.Rbuf.Last); 

				L2 := Item'Length - Length; -- Remaining capacity
				If L2 >= File.Rbuf.Data'Length then
					-- The remaining capacity of the output buffer is
					-- higher than that of the internal buffer. So read
					-- directly into the output buffer.
					OS_Read_File (File, Item(Item'First + Length .. Item'Last), L2);
					Length := Length + L2;
				else
					-- Read into the internal buffer.
					OS_Read_File (File, File.Rbuf.Data, L1);
					Set_Length (File.Rbuf, L1);

					if L1 < L2 then
						-- the actual bytes read may be less than the remaining capacity
						L2 := L1; 
					end if;

					-- Copy as many bytes as needed into the output buffer.
					Copy_Array (Item(Item'First + Length .. Item'Last), File.Rbuf.Data, L2);
					Length := Length + L2;
					File.Rbuf.First := File.Rbuf.First + L2;
				end if;
			end if;
		end if;
	end Fetch_Bytes;

	--|-----------------------------------------------------------------------
	--| READ SLIM STRING
	--|-----------------------------------------------------------------------
	procedure Read (File:   in out File_Record; 
	                Buffer: out    Slim_String;
	                Length: out    System_Length) is
		pragma Assert (Is_Open(File));
		pragma Assert (Buffer'Length > 0);

		Outbuf: System_Byte_Array (Buffer'Range);
		for Outbuf'Address use Buffer'Address;
	begin
		Fetch_Bytes (File, Outbuf, Length);
	end Read;

	procedure Read_Line (File:   in out File_Record;
	                     Buffer: out    Slim_String;
	                     Length: out    System_Length) is
		pragma Assert (Is_Open(File));
		pragma Assert (Buffer'Length > 0);

		Outbuf: System_Byte_Array (Buffer'Range);
		for Outbuf'Address use Buffer'Address;

		K: System_Length;
	begin
		K := Outbuf'First - 1;

		outer: loop
			if Is_Empty(File.Rbuf) then
				Load_Bytes (File);
				exit when Is_Empty(File.Rbuf);
			end if;

			while File.Rbuf.First < File.Rbuf.Last loop
				K := K + 1;
				File.Rbuf.First := File.Rbuf.First + 1;
				Outbuf(K) := File.Rbuf.Data(File.Rbuf.First);
				if K >= Outbuf'Last or else Outbuf(K) = Slim_Ascii.Pos.LF then -- TODO: different line terminator
					exit outer; -- Done
				end if;
			end loop;
		end loop outer;

		Length := K + 1 - Outbuf'First;
	end Read_Line;

	procedure Get_Line (File:   in out File_Record;
	                    Buffer: out    Slim_String;
	                    Length: out    System_Length) is
		pragma Assert (Is_Open(File));
		pragma Assert (Buffer'Length > 0);

		Last: System_Length;
	begin
		Read_Line (File, Buffer, Length);

		if Length >= 1 then
			Last := Buffer'First + Length - 1;
			if Buffer(Last) = Slim_Ascii.LF then
				if Last > Buffer'First and then Buffer(Last - 1) = Slim_Ascii.CR then
					-- Switch CR/LF to LF
					Length := Length - 1;
					Buffer(Last - 1) := Slim_Ascii.LF;
				end if;
			elsif Buffer(Last) = Slim_Ascii.CR then
				if Is_Empty(File.Rbuf) then
					Load_Bytes (File);
				end if;

				if not Is_Empty(File.Rbuf) then
					if File.Rbuf.Data(File.Rbuf.First + 1) = Slim_Ascii.Pos.LF then
						-- Consume LF held in the internal read buffer.
						File.Rbuf.First := File.Rbuf.First + 1;
						-- Switch CR to LF (End-result: CR/LF to LF)
						Buffer(Last) := Slim_Ascii.LF;
					end if;
				end if;
			end if;
		end if;
	end Get_Line;

	--|-----------------------------------------------------------------------
	--| READ WIDE STRING
	--|-----------------------------------------------------------------------

	procedure Read_Wide (File:       in out File_Record; 
	                     Buffer:     out    Wide_String;
	                     Length:     out    System_Length;
	                     Terminator: in     Wide_String) is

		pragma Assert (Is_Open(File));
		pragma Assert (Buffer'Length > 0);

		Outbuf: Wide_String renames Buffer;
		Inbuf: Slim_String (File.Rbuf.Data'Range);
		for Inbuf'Address use File.Rbuf.Data'Address;
		
		L3, L4, I, J, K: System_Length;
	begin
		K := Outbuf'First - 1;

		outer: while K < Outbuf'Last loop
			if Is_Empty(File.Rbuf) then
				Load_Bytes (File);
				exit when Is_Empty(File.Rbuf);
			end if;

			while File.Rbuf.First < File.Rbuf.Last and K < Outbuf'Last loop
				I := File.Rbuf.First + 1;
				L3 := Sequence_Length(Inbuf(I)); -- Required number of bytes to compose a wide character

				if L3 <= 0 then
					-- Potentially illegal sequence 
					K := K + 1;
					Outbuf(K) := Wide_Ascii.Question;
					File.Rbuf.First := I;
				else
					L4 := File.Rbuf.Last - File.Rbuf.First;  -- Avaliable number of bytes available in the internal buffer
					if L4 < L3 then
						-- Insufficient data available. Exit the inner loop to read more.
						exit;
					end if;

					K := K + 1;
					begin
						J := File.Rbuf.First + L3;
						Outbuf(K..K) := Slim_To_Wide(Inbuf(I .. J));
					exception
						when others => 
							Outbuf(K) := Wide_Ascii.Question;
							J := I; -- Override J to skip 1 byte only.
					end;
					File.Rbuf.First := J;
				end if;

				if Terminator'Length > 0 and then
				   Outbuf(K) = Terminator(Terminator'First) then 
					-- TODO: compare more characters in terminator, not just the first charactrer
					exit outer;
				end if;
			end loop;
		end loop outer;

		Length := K + 1 - Outbuf'First;
	end Read_Wide;

	procedure Read (File:   in out File_Record; 
	                Buffer: out    Wide_String;
	                Length: out    System_Length) is
		Terminator: Wide_String(1..0);
	begin
		Read_Wide (File, Buffer, Length, Terminator);
	end Read;

	procedure Read_Line (File:   in out File_Record; 
	                     Buffer: out    Wide_String;
	                     Length: out    System_Length) is
		Terminator: constant Wide_String(1..1) := (1 => Wide_Ascii.LF);
	begin
		Read_Wide (File, Buffer, Length, Terminator);
	end Read_Line;

	procedure Get_Line (File:   in out File_Record;
	                    Buffer: out    Wide_String;
	                    Length: out    System_Length) is
		pragma Assert (Is_Open(File));
		pragma Assert (Buffer'Length > 0);

		Last: System_Length;
	begin
		Read_Line (File, Buffer, Length);

		if Length >= 1 then
			Last := Buffer'First + Length - 1;
			if Buffer(Last) = Wide_Ascii.LF then
				if Last > Buffer'First and then Buffer(Last - 1) = Wide_Ascii.CR then
					-- Switch CR/LF to LF
					Length := Length - 1;
					Buffer(Last - 1) := Wide_Ascii.LF;
				end if;
			elsif Buffer(Last) = Wide_Ascii.CR then
				-- if the last character in the output buffer is CR,
				-- i need to inspect the first character in the internal 
				-- read buffer to determine if it's CR/LF.
				if Is_Empty(File.Rbuf) then
					Load_Bytes (File);
				end if;

				if not Is_Empty(File.Rbuf) then
					declare
						Inbuf: Slim_String (File.Rbuf.Data'Range);
						for Inbuf'Address use File.Rbuf.Data'Address;
						L3, I, J: System_Length;
						W: Wide_String(1..1);
					begin
						I := File.Rbuf.First + 1;
						L3 := Sequence_Length(Inbuf(I)); -- Required number of bytes to compose a wide character
						if L3 in  1 .. File.Rbuf.Last - File.Rbuf.First then
							J := File.Rbuf.First + L3;
							begin
								W := Slim_To_Wide(Inbuf(I .. J));
							exception
								when others =>
									W(1) := Wide_Ascii.NUL;
							end;
							if W(1) = Wide_Ascii.LF then
								-- Consume LF held in the internal read buffer.
								File.Rbuf.First := J;
								-- Switch CR to LF (End-result: CR/LF to LF)
								Buffer(Last) := Wide_Ascii.LF;
							end if;
						end if;
					end;
				end if;
			end if;
		end if;
	end Get_Line;

	--|-----------------------------------------------------------------------
	--| WRITE SLIM STRING
	--|-----------------------------------------------------------------------
	procedure Write (File:   in out File_Record; 
	                 Buffer: in     Slim_String;
	                 Length: out    System_Length) is
		pragma Assert (Is_Open(File));

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
		pragma Assert (Is_Open(File));

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
		pragma Assert (Is_Open(File));

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
		pragma Assert (Is_Open(File));

		Wbuf: File_Buffer renames File.Wbuf;
		F, L, I, LF: System_Length;
	begin
		LF := Wbuf.Data'First - 1;
		I := Buffer'First - 1;
		while I < Buffer'Last loop
			I := I + 1;

			--if Buffer(I) = Wide_Ascii.LF then
			--else
			--end if;

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
		pragma Assert (Is_Open(File));

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
		pragma Assert (Is_Open(File));
	begin
		File.Wbuf.Length := 0;
	end Drain;

end File;
