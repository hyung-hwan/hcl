with ada.text_io;

package body H2.Utf8 is

	type Uint8 is mod 2 ** 8;
	type Uint32 is mod 2 ** 32;

	type Conv_Record is record
		Lower: Uint32;	
		Upper: Uint32;	
		Fbyte: Uint8; -- Mask to the first utf8 byte */
		Mask: Uint8;
		Fmask: Uint8;
		Length: Uint8; -- number of bytes 	
	end record;

	type Conv_Record_Array is array(System_Index range<>) of Conv_Record;

	Conv_Table: constant Conv_Record_Array := (
		(16#0000_0000#, 16#0000_007F#, 16#00#, 16#80#, 16#7F#, 1),
		(16#0000_0080#, 16#0000_07FF#, 16#C0#, 16#E0#, 16#1F#, 2),
		(16#0000_0800#, 16#0000_FFFF#, 16#E0#, 16#F0#, 16#0F#, 3),
		(16#0001_0000#, 16#001F_FFFF#, 16#F0#, 16#F8#, 16#07#, 4),
		(16#0020_0000#, 16#03FF_FFFF#, 16#F8#, 16#FC#, 16#03#, 5),
		(16#0400_0000#, 16#7FFF_FFFF#, 16#FC#, 16#FE#, 16#01#, 6)
	);

	function Get_Utf8_Slot (UV: in Uint32) return System_Size is
		pragma Inline (Get_Utf8_Slot);
	begin
		for I in Conv_Table'Range loop
			if UV >= Conv_Table(I).Lower and then UV <= Conv_Table(I).Upper then
				return I;
			end if;
		end loop;
		return System_Size'First;
	end  Get_Utf8_Slot;
	
	function Unicode_To_Utf8 (UC: in Unicode_Character) return Utf8_String is
		UV: Uint32;
		I: System_Size;
	begin
		UV := Unicode_Character'Pos(UC);
		
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
				Result(J) := Utf8_Character'Val((UV and 2#0011_1111#) or 2#1000_0000#);
				UV := UV / (2 ** 6); --UV := UV >> 6;
			end loop;

			Result(Result'First) := Utf8_Character'Val(UV or Uint32(Conv_Table(I).Fbyte));
			return Result;
		end;
	end Unicode_To_Utf8;


	function Unicode_To_Utf8 (US: in Unicode_String) return Utf8_String is
		-- this function has high stack pressur if the input string is too long
		-- TODO: create a procedure to overcome this problem.
		Tmp: System_Size;
	begin
		Tmp := 0;
		for I in US'Range loop
			declare
				Utf8: Utf8_String := Unicode_To_Utf8(US(I));
			begin
				Tmp := Tmp + Utf8'Length;
			end;
		end loop;

		declare
			Result: Utf8_String (1 .. Tmp);
		begin
			Tmp := Result'First;
			for I in US'Range loop
				declare
					Utf8: Utf8_String := Unicode_To_Utf8(US(I));	
				begin
					Result(Tmp .. Tmp + Utf8'Length - 1) := Utf8;
					Tmp := Tmp + Utf8'Length;
				end;
			end loop;
			return Result;
		end;
	end Unicode_To_Utf8;

     procedure Utf8_To_Unicode (Utf8: in Utf8_String;
                                UC:   out Unicode_Character) is
	begin
		null;
	end Utf8_To_Unicode;

     procedure Utf8_To_Unicode (Utf8: in Utf8_String;
                                US:   in out Unicode_String) is
	begin
		null;
	end Utf8_To_Unicode;

end H2.Utf8;
