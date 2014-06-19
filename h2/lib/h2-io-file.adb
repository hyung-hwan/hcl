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

	procedure Compact_Buffer (Buffer: in out File_Buffer) is
		A, B, L: System_Length;
	begin
		A := Buffer.Pos;
		B := Buffer.Last;
		L := Buffer.Pos - Buffer.Data'First + 1;

		Buffer.Pos := Buffer.Pos - L; -- should be same as Buffer.Data'First - 1
		Buffer.Last := Buffer.Last - L;

		Buffer.Data(Buffer.Pos + 1 .. Buffer.Last) := Buffer.Data(A + 1 .. B);
	end Compact_Buffer;

	procedure Copy_Array (Dst:    in out System_Byte_Array;
	                      Src:    in     System_Byte_Array;
	                      Length: in     System_Length) is
		pragma Inline (Copy_Array);
	begin
		Dst(Dst'First .. Dst'First + Length - 1) := Src(Src'First .. Src'First + Length - 1);
	end Copy_Array;

	function Is_Empty (Buf: in File_Buffer) return Standard.Boolean is
		pragma Inline (Is_Empty);
	begin
		return Buf.Pos >= Buf.Last;
	end Is_Empty;

	procedure Set_Length (Buf: in out File_Buffer; Length: in System_Length) is
		pragma Inline (Set_Length);
	begin
		Buf.Pos := Buf.Data'First - 1; -- this should be 0
		Buf.Last := Buf.Pos + Length;
	end Set_Length;


	Slim_Line_Terminator: Slim_String := Get_Line_Terminator;
	--Wide_Line_Terminator: Wide_String := Get_Line_Terminator;


	procedure Set_Option_Bits (Option: in out Option_Record;
	                            Bits:   in     Option_Bits) is
	begin
		Option.Bits := Option.Bits or Bits;
	end Set_Option_Bits;

	procedure Clear_Option_Bits (Option: in out Option_Record;
	                             Bits:   in     Option_Bits) is
	begin
		Option.Bits := Option.Bits and not Bits;
	end Clear_Option_Bits;

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
	
		Set_Length (File.Rbuf, 0);
		Set_Length (File.Wbuf, 0);

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

		Set_Length (File.Rbuf, 0);
		Set_Length (File.Wbuf, 0);

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

	procedure Set_Option (File: in out File_Record; Option: in Option_Record) is
	begin
		File.Option := Option;
	end Set_Option;

	function Get_Option (File: in File_Record) return Option_Record is
	begin
		return File.Option;
	end Get_Option;

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
			File.Rbuf.Pos := File.Rbuf.Pos + 1;
			Item := File.Rbuf.Data(File.Rbuf.Pos);
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
			L1 := File.Rbuf.Last - File.Rbuf.Pos;
			if L1 > 0 then
				-- Copy the residue over to the output buffer
				if Item'Length <= L1 then
					L2 := Item'Length;
				else
					L2 := L1;
				end if;
			
				Copy_Array (Item, File.Rbuf.Data(File.Rbuf.Pos + 1 .. File.Rbuf.Last), L2);
				File.Rbuf.Pos := File.Rbuf.Pos + L2;

				Length := L2;
			else
				Length := 0;
			end if;

			if Item'Length > L1 then
				-- Item is not full. the internal read buffer must be empty.
				pragma Assert (File.Rbuf.Pos >= File.Rbuf.Last); 

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
					File.Rbuf.Pos := File.Rbuf.Pos + L2;
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

			while File.Rbuf.Pos < File.Rbuf.Last loop
				K := K + 1;
				File.Rbuf.Pos := File.Rbuf.Pos + 1;
				Outbuf(K) := File.Rbuf.Data(File.Rbuf.Pos);
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
					if File.Rbuf.Data(File.Rbuf.Pos + 1) = Slim_Ascii.Pos.LF then
						-- Consume LF held in the internal read buffer.
						File.Rbuf.Pos := File.Rbuf.Pos + 1;
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

			while File.Rbuf.Pos < File.Rbuf.Last and K < Outbuf'Last loop
				I := File.Rbuf.Pos + 1;
				L3 := Sequence_Length(Inbuf(I)); -- Required number of bytes to compose a wide character

				if L3 <= 0 then
					-- Potentially illegal sequence 
					K := K + 1;
					Outbuf(K) := Wide_Ascii.Question;
					File.Rbuf.Pos := I;
				else
					L4 := File.Rbuf.Last - File.Rbuf.Pos;  -- Avaliable number of bytes available in the internal buffer
					if L4 < L3 then
						-- Insufficient data available. Exit the inner loop to read more.
						exit;
					end if;

					K := K + 1;
					begin
						J := File.Rbuf.Pos + L3;
						Outbuf(K..K) := Slim_To_Wide(Inbuf(I .. J));
					exception
						when others => 
							Outbuf(K) := Wide_Ascii.Question;
							J := I; -- Override J to skip 1 byte only.
					end;
					File.Rbuf.Pos := J;
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
						I := File.Rbuf.Pos + 1;
						L3 := Sequence_Length(Inbuf(I)); -- Required number of bytes to compose a wide character
						if L3 in  1 .. File.Rbuf.Last - File.Rbuf.Pos then
							J := File.Rbuf.Pos + L3;
							begin
								W := Slim_To_Wide(Inbuf(I .. J));
							exception
								when others =>
									W(1) := Wide_Ascii.NUL;
							end;
							if W(1) = Wide_Ascii.LF then
								-- Consume LF held in the internal read buffer.
								File.Rbuf.Pos := J;
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

		Inbuf: System_Byte_Array (Buffer'Range);
		for Inbuf'Address use Buffer'Address;

		F, L: System_Length;
	begin
		-- This procedure attempts to write as many bytes as requested.
		-- However, under a certain condition, it may not be able to 
		-- process the input buffer in full.

		if not Is_Empty(File.Wbuf) then
			-- Some residue data in the internal buffer.

			if Inbuf'Length <= File.Wbuf.Data'Last - File.Wbuf.Last then
				-- Copy the input to the internal buffer to reduce OS calls

				F := File.Wbuf.Last + 1;
				L := F + Inbuf'Length - 1;
				File.Wbuf.Data(F .. L) := Inbuf;
				File.Wbuf.Last := L;
				Flush (File);

				-- The resulting length is the length  of input buffer given.
				-- The residue in the internal write buffer is not counted.
				Length := Inbuf'Length;
				return;
			end if;

			-- Flush the residue first.
			Flush (File);
		end if;

		L := 0;
		while L < Inbuf'Length loop
			--begin
				OS.File.Write (File.File, Inbuf(Inbuf'First + L .. Inbuf'Last), F);
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

		Inbuf: System_Byte_Array (Buffer'Range);
		for Inbuf'Address use Buffer'Address;

		L, I, LF: System_Length;
	begin
		-- This procedure attempts to write the input up to the last line
		-- terminator. It buffers the remaining input after the terminator.
		-- The input may not include any line terminators. 

		LF := File.Wbuf.Data'First - 1;
		I := Inbuf'First - 1;

		while I < Inbuf'Last loop
			if File.Wbuf.Last >= File.Wbuf.Data'Last then
				-- The internal write buffer is full.
				Flush (File);
				LF := File.Wbuf.Data'First - 1;
			end if;

			I := I + 1;
			File.Wbuf.Last := File.Wbuf.Last + 1;
			File.Wbuf.Data(File.Wbuf.Last) := Inbuf(I);
			if File.Wbuf.Data(File.Wbuf.Last) = Slim_Ascii.Pos.LF then -- TODO: different line terminator
				-- Remeber the index of the line terminator
				LF := File.Wbuf.Last;
			end if;
		end loop;

		-- The line terminator was found. Write up to the terminator.
		-- Keep the rest in the internal buffer.
		if LF in File.Wbuf.Data'Range then
			while File.Wbuf.Pos < LF loop
				OS.File.Write (File.File, File.Wbuf.Data(File.Wbuf.Pos + 1 .. LF), L);
				File.Wbuf.Pos := File.Wbuf.Pos + L;
			end loop;
		end if;

		if File.Wbuf.Pos >= File.Wbuf.Data'First then
			Compact_Buffer (File.Wbuf);
		end if;

		Length := I - Inbuf'First + 1;
	end Write_Line;

	procedure Put_Line (File:   in out File_Record; 
	                    Buffer: in     Slim_String;
	                    Length: out    System_Length) is
		pragma Assert (Is_Open(File));

		Inbuf: System_Byte_Array (Buffer'Range);
		for Inbuf'Address use Buffer'Address;

		L, I, LF: System_Length;
		X: System_Byte;
		Injected: Standard.Boolean := Standard.False;

	begin
		LF := File.Wbuf.Data'First - 1;
		I := Inbuf'First - 1;

		while I < Inbuf'Last loop
			if (File.Option.Bits and OPTION_CRLF) /= 0 and then 
			   not Injected and then Inbuf(I + 1) = Slim_Ascii.Pos.LF then
				X := Slim_Ascii.Pos.CR;
				Injected := Standard.True;
			else
				I := I + 1;
				X := Inbuf(I);
				Injected := Standard.False;
			end if;

			if File.Wbuf.Last >= File.Wbuf.Data'Last then
				-- The internal write buffer is full.
				Flush (File);
				LF := File.Wbuf.Data'First - 1;
			end if;

			File.Wbuf.Last := File.Wbuf.Last + 1;
			File.Wbuf.Data(File.Wbuf.Last) := X;
			if File.Wbuf.Data(File.Wbuf.Last) = Slim_Ascii.Pos.LF then -- TODO: different line terminator
				-- Remeber the index of the line terminator
				LF := File.Wbuf.Last;
			end if;
		end loop;

		-- The line terminator was found. Write up to the terminator.
		-- Keep the rest in the internal buffer.
		if LF in File.Wbuf.Data'Range then
			while File.Wbuf.Pos < LF loop
				OS.File.Write (File.File, File.Wbuf.Data(File.Wbuf.Pos + 1 .. LF), L);
				File.Wbuf.Pos := File.Wbuf.Pos + L;
			end loop;
		end if;

		if File.Wbuf.Pos >= File.Wbuf.Data'First then
			Compact_Buffer (File.Wbuf);
		end if;

		Length := I - Inbuf'First + 1;
	end Put_Line;

	--|-----------------------------------------------------------------------
	--| WRITE WIDE STRING
	--|-----------------------------------------------------------------------
	procedure Write (File:   in out File_Record; 
	                 Buffer: in     Wide_String;
	                 Length: out    System_Length) is
		pragma Assert (Is_Open(File));

		L, I: System_Length;
	begin
		I := Buffer'First - 1;
		while I < Buffer'Last loop
			I := I + 1;
			declare
				Tmp: Slim_String := Wide_To_Slim(Buffer(I..I));
				Tmp2: System_Byte_Array(Tmp'Range);
				for Tmp2'Address use Tmp'Address;
			begin
				L := File.Wbuf.Last + Tmp2'Length;

				if L > File.Wbuf.Data'Last then
					-- The multi-byte sequence for the current character
					-- can't fit into the internal buffer. Flush the
					-- buffer and attempt to fit it in.
					Flush (File);
					L := File.Wbuf.Pos + Tmp2'Length;
				end if;

				File.Wbuf.Data(File.Wbuf.Last + 1 .. L) := Tmp2;
				File.Wbuf.Last := L;
			end;
		end loop;

		Flush (File);
		Length := I - Buffer'First + 1;
	end Write;

	procedure Write_Line (File:   in out File_Record; 
	                      Buffer: in     Wide_String;
	                      Length: out    System_Length) is
		pragma Assert (Is_Open(File));

		L, I, LF: System_Length;
	begin

		LF := File.Wbuf.Data'First - 1;
		I := Buffer'First - 1;
		while I < Buffer'Last loop
			I := I + 1;

			declare
				Tmp: Slim_String := Wide_To_Slim(Buffer(I..I));
				Tmp2: System_Byte_Array(Tmp'Range);
				for Tmp2'Address use Tmp'Address;
			begin
				L := File.Wbuf.Last + Tmp2'Length;

				if L > File.Wbuf.Data'Last then
					-- The multi-byte sequence for the current character
					-- can't fit into the internal buffer. Flush the
					-- buffer and attempt to fit it in.
					Flush (File);

					L := File.Wbuf.Pos + Tmp2'Length;
					LF := File.Wbuf.Data'First - 1;
				end if;

				if Buffer(I) = Wide_Ascii.LF then -- TODO: different line terminator
					LF := L;
				end if;

				File.Wbuf.Data(File.Wbuf.Last + 1 .. L) := Tmp2;
				File.Wbuf.Last := L;
			end;
		end loop;

		if LF in File.Wbuf.Data'Range then
			while File.Wbuf.Pos < LF loop
				OS.File.Write (File.File, File.Wbuf.Data(File.Wbuf.Pos + 1 .. LF), L);
				File.Wbuf.Pos := File.Wbuf.Pos + L;
			end loop;
		end if;

		if File.Wbuf.Pos >= File.Wbuf.Data'First then
			Compact_Buffer (File.Wbuf);
		end if;

		Length := I - Buffer'First + 1;
	end Write_Line;

	procedure Put_Line (File:   in out File_Record; 
	                    Buffer: in     Wide_String;
	                    Length: out    System_Length) is
		pragma Assert (Is_Open(File));

		F, L, I, LF: System_Length;
		X: Wide_String(1..2) := (Wide_Ascii.CR, Wide_Ascii.LF);
	begin

		LF := File.Wbuf.Data'First - 1;
		I := Buffer'First - 1;
		while I < Buffer'Last loop
			I := I + 1;

			X(2) := Buffer(I);
			if (File.Option.Bits and OPTION_CRLF) /= 0 and then
			   Buffer(I) = Wide_Ascii.LF then
				F := 1;
			else
				F := 2;
			end if;

			declare
				Tmp: Slim_String := Wide_To_Slim(X(F..2));
				Tmp2: System_Byte_Array(Tmp'Range);
				for Tmp2'Address use Tmp'Address;
			begin
				L := File.Wbuf.Last + Tmp2'Length;

				if L > File.Wbuf.Data'Last then
					-- The multi-byte sequence for the current character
					-- can't fit into the internal buffer. Flush the
					-- buffer and attempt to fit it in.
					Flush (File);

					L := File.Wbuf.Pos + Tmp2'Length;
					LF := File.Wbuf.Data'First - 1;
				end if;

				if Buffer(I) = Wide_Ascii.LF then 
					LF := L;
				end if;

				File.Wbuf.Data(File.Wbuf.Last + 1 .. L) := Tmp2;
				File.Wbuf.Last := L;
			end;
		end loop;

		if LF in File.Wbuf.Data'Range then
			while File.Wbuf.Pos < LF loop
				OS.File.Write (File.File, File.Wbuf.Data(File.Wbuf.Pos + 1 .. LF), L);
				File.Wbuf.Pos := File.Wbuf.Pos + L;
			end loop;
		end if;

		if File.Wbuf.Pos >= File.Wbuf.Data'First then
			Compact_Buffer (File.Wbuf);
		end if;

		Length := I - Buffer'First + 1;
	end Put_Line;

	--|-----------------------------------------------------------------------
	--| FLUSH AND DRAIN
	--|-----------------------------------------------------------------------
	procedure Flush (File: in out File_Record) is
		pragma Assert (Is_Open(File));

		L: System_Length;
	begin
		while not Is_Empty(File.Wbuf)  loop
			--begin
				OS.File.Write (File.File, File.Wbuf.Data(File.Wbuf.Pos + 1 .. File.Wbuf.Last), L);
			--exception
			--	when Would_Block_Exception =>
			--		-- Flush must write all it can.
			--		null;
			--	when others => 
			--		raise;
			--end;
			File.Wbuf.Pos := File.Wbuf.Pos + L;
		end loop;

		Set_Length (File.Wbuf, 0);
	end Flush;
	                 
	procedure Drain (File: in out File_Record) is
		pragma Assert (Is_Open(File));
	begin
		Set_Length (File.Wbuf, 0);
	end Drain;

end File;
