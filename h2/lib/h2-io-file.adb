separate (H2.IO)

package body File is

	procedure Open (File: in out File_Record;
	                Name: in     Slim_String;
	                Flag: in     Flag_Record;
	                Pool: in     Storage_Pool_Pointer := null) is
	begin
		Sysapi.File.Open (File.File, Name, flag, Pool => Pool);
	end Open;

	procedure Open (File: in out File_Record;
	                Name: in     Wide_String;
	                Flag: in     Flag_Record;
	                Pool: in     Storage_Pool_Pointer := null) is
	begin
		Sysapi.File.Open (File.File, Name, flag, Pool => Pool);
	end Open;


	procedure Close (File: in out File_Record) is
	begin
		Sysapi.File.Close (File.File);
		File.File := null;
		File.Last := System_Length'First;
	end Close;

	procedure Read (File:   in out File_Record; 
	                Buffer: in out Slim_String;
	                Last:   out    System_Length) is
	begin
		null;
	end Read;

	procedure Read (File:   in out File_Record; 
	                Buffer: in out Wide_String;
	                Last:   out    System_Length) is
	begin
		null;
	end Read;

	procedure Write (File:   in out File_Record; 
	                 Buffer: in     Slim_String;
	                 Last:   out    System_Length) is
	begin
		null;
	end Write;

	procedure Write (File:   in out File_Record; 
	                 Buffer: in     Wide_String;
	                 Last:   out    System_Length) is
	begin
		null;
	end Write;

	procedure Flush (File: in out File_Record) is
	begin
		null;
	end Flush;
	                 
end File;
