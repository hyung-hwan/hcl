with Interfaces;

package body H2.Utf8 is

--|----------------------------------------------------------------------------
--| From RFC 2279 UTF-8, a transformation format of ISO 10646
--|
--|    UCS-4 range (hex.) UTF-8 octet sequence (binary)
--| 1:2 00000000-0000007F 0xxxxxxx
--| 2:2 00000080-000007FF 110xxxxx 10xxxxxx
--| 3:2 00000800-0000FFFF 1110xxxx 10xxxxxx 10xxxxxx
--| 4:4 00010000-001FFFFF 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
--| inv 00200000-03FFFFFF 111110xx 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx
--| inv 04000000-7FFFFFFF 1111110x 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx
--|----------------------------------------------------------------------------

	--type Uint8 is mod 2 ** 8;
	--type Uint32 is mod 2 ** 32;
	use type Interfaces.Unsigned_8;
	use type Interfaces.Unsigned_32;
	subtype Uint8 is Interfaces.Unsigned_8;
	subtype Uint32 is Interfaces.Unsigned_32;

	type Conv_Record is record
		Lower: Uint32;
		Upper: Uint32;

		Fbyte: Uint8; 
		Mask: Uint8; -- Mask for getting the fixed bits in the first byte.
		             -- (First-Byte and Mask) = Fbyte

		Fmask: Uint8; -- Mask for getting the actual values bits off the first byte.

		Length: System_Length; -- Number of bytes
	end record;

	type Conv_Record_Array is array(System_Index range<>) of Conv_Record;

	Conv_Table: constant Conv_Record_Array := (
		(16#0000_0000#, 16#0000_007F#, 2#0000_0000#, 2#1000_0000#, 2#0111_1111#, 1),
		(16#0000_0080#, 16#0000_07FF#, 2#1100_0000#, 2#1110_0000#, 2#0001_1111#, 2),
		(16#0000_0800#, 16#0000_FFFF#, 2#1110_0000#, 2#1111_0000#, 2#0000_1111#, 3),
		(16#0001_0000#, 16#001F_FFFF#, 2#1111_0000#, 2#1111_1000#, 2#0000_0111#, 4),
		(16#0020_0000#, 16#03FF_FFFF#, 2#1111_1000#, 2#1111_1100#, 2#0000_0011#, 5),
		(16#0400_0000#, 16#7FFF_FFFF#, 2#1111_1100#, 2#1111_1110#, 2#0000_0001#, 6)
	);

	function Get_Utf8_Slot (UV: in Uint32) return System_Length is
		pragma Inline (Get_Utf8_Slot);
	begin
		for I in Conv_Table'Range loop
			if UV >= Conv_Table(I).Lower and then UV <= Conv_Table(I).Upper then
				return I;
			end if;
		end loop;
		return System_Length'First;
	end  Get_Utf8_Slot;

	function From_Unicode_Character (Chr: in Unicode_Character) return Utf8_String is
		UV: Uint32;
		I: System_Length;
	begin
		UV := Unicode_Character'Pos(Chr);

		I := Get_Utf8_Slot(UV);
		if I not in System_Index'Range then
			raise Invalid_Unicode_Character;
		end if;

		declare
			Result: Utf8_String (1 .. System_Index(Conv_Table(I).Length));
		begin
			for J in reverse Result'First + 1 .. Result'Last loop
				-- 2#0011_1111#: 16#3F#
				-- 2#1000_0000#: 16#80#
				Result(J) := Utf8_Character'Val((UV and Uint32'(2#0011_1111#)) or Uint32'(2#1000_0000#));
				--UV := UV / (2 ** 6); --UV := UV >> 6;
				UV := Interfaces.Shift_Right (UV, 6);
			end loop;

			Result(Result'First) := Utf8_Character'Val(UV or Uint32(Conv_Table(I).Fbyte));
			return Result;
		end;
	end From_Unicode_Character;

	function From_Unicode_String (Str: in Unicode_String) return Utf8_String is
		-- this function has high stack pressure if the input string is too long
		-- TODO: create a procedure to overcome this problem.
		Tmp: System_Length;
	begin
		-- Calculate the length first
		Tmp := 0;
		for I in Str'Range loop
			declare
				Utf8: Utf8_String := From_Unicode_Character(Chr => Str(I));
			begin
				Tmp := Tmp + Utf8'Length;
			end;
		end loop;

		declare
			Result: Utf8_String (1 .. Tmp);
		begin
			Tmp := Result'First;
			for I in Str'Range loop
				declare
					Utf8: Utf8_String := From_Unicode_Character(Str(I));
				begin
					Result(Tmp .. Tmp + Utf8'Length - 1) := Utf8;
					Tmp := Tmp + Utf8'Length;
				end;
			end loop;
			return Result;
		end;
	end From_Unicode_String;

	function Sequence_Length (Seq: in Utf8_Character) return System_Length is
	begin
		for I in Conv_Table'Range loop
			if (Utf8_Character'Pos(Seq) and Conv_Table(I).Mask) = Conv_Table(I).Fbyte then
				return Conv_Table(I).Length;
			end if;
		end loop;
		return System_Length'First;
	end Sequence_Length;

	procedure To_Unicode_Character (Seq:     in  Utf8_String; 
	                                Seq_Len: out System_Length;
	                                Chr:     out Unicode_Character) is
		W: Uint32;
	begin
		for I in Conv_Table'Range loop

			-- Check if the first byte matches the desired bit patterns.
			if (Utf8_Character'Pos(Seq(Seq'First)) and Conv_Table(I).Mask) = Conv_Table(I).Fbyte then
				
				if Seq'Length < Conv_Table(I).Length then
					raise Insufficient_Utf8_Sequence;
				end if;

				-- Get the values bits off the first byte.
				W := Utf8_Character'Pos(Seq(Seq'First)) and Uint32(Conv_Table(I).Fmask);

				-- Get the values bits off subsequent bytes.
				for J in 1 .. Conv_Table(I).Length - 1 loop
					if (Utf8_Character'Pos(Seq(Seq'First + J)) and Uint32'(2#1100_0000#)) /= Uint32'(2#1000_0000#) then
						-- Each UTF8 byte except the first must be set with 2#1000_0000.
						raise Invalid_Utf8_Sequence;
					end if;
					W := Interfaces.Shift_Left(W, 6) or (Utf8_Character'Pos(Seq(Seq'First + J)) and Uint32'(2#0011_1111#)); 
				end loop;

				-- Return the character matching the word
				Chr := Unicode_Character'Val(W);
				Seq_Len := Conv_Table(I).Length;
				return;
			end if;
		end loop;
		
		raise Invalid_Utf8_Sequence;
	end To_Unicode_Character;

	function To_Unicode_Character (Seq: in Utf8_String) return Unicode_Character is
		Seq_Len: System_Length;
		Chr: Unicode_Character;
	begin
		To_Unicode_Character (Seq, Seq_Len, Chr);
		return Chr;
	end To_Unicode_Character;

	procedure To_Unicode_String (Seq:     in     Utf8_String; 
	                             Seq_Len: out    System_Length;
	                             Str:     in out Unicode_String;
	                             Str_Len: out    System_Length) is
		Seq_Pos: System_Index := Seq'First;
		Str_Pos: System_Index := Str'First;
		Len: System_Length;
	begin
		while Seq_Pos <= Seq'Last and then Str_Pos <= Str'Last loop
			To_Unicode_Character(Seq(Seq_Pos .. Seq'Last), Len, Str(Str_Pos));
			Seq_Pos := Seq_Pos + Len;
			Str_Pos := Str_Pos + 1;
		end loop;

		Seq_Len := Seq_Pos - Seq'First;
		Str_Len := Str_Pos - Str'First;
	end To_Unicode_String;

	function To_Unicode_String (Seq: in Utf8_String) return Unicode_String is
		UL: System_Length := 0;
	begin
		declare
			Chr: Unicode_Character;
			Pos: System_Index := Seq'First;
			Seq_Len: System_Length;
		begin
			while Pos <= Seq'Last loop
				To_Unicode_Character(Seq(Pos .. Seq'Last), Seq_Len, Chr);
				UL := UL + 1;
				Pos := Pos + Seq_Len;
			end loop;
		end;

		declare
			Str: Unicode_String (1 .. UL);
			Pos: System_Index := Seq'First;
			Seq_Len: System_Length;
		begin
			for I in Str'Range loop
				To_Unicode_Character(Seq(Pos .. Seq'Last), Seq_Len, Str(I));
				Pos := Pos + Seq_Len;
			end loop;
			return Str;
		end;
	end To_Unicode_String;

end H2.Utf8;
