package body H2.Sysapi is

	package body File is separate;

	procedure Set_File_Flag_Bits (Flag: in out File_Flag_Record; Bits: in File_Flag_Bits) is
	begin
		Flag.Bits := Flag.Bits or Bits;
	end Set_File_Flag_Bits;

	procedure Clear_File_Flag_Bits (Flag: in out File_Flag_Record; Bits: in File_Flag_Bits) is
	begin
		Flag.Bits := Flag.Bits and not Bits;
	end Clear_File_Flag_Bits;

end H2.Sysapi;
