with Interfaces.C;
with H2.Pool;

separate (H2.Sysapi)

package body File is

	package C renames Interfaces.C;
	use type C.int;

	--function sys_open (path: ; flags: C.int; mode: C.int) return C.int;
	function sys_open (path: Slim_String; flags: C.int; mode: C.int) return C.int;
	pragma Import (C, sys_open, "open");

	procedure sys_close (fd: C.int);
	pragma Import (C, sys_close, "close");

	INVALID_HANDLE: constant C.int := -1;

	type Posix_File_Record is new File_Record with record
		Pool: Storage_Pool_Pointer := null;
		Handle: C.int := INVALID_HANDLE;
	end record;
	type Posix_File_Pointer is access all Posix_File_Record;

	function Flag_To_System (Bits: in File_Flag_Bits) return C.int is
		V: C.int := 0;
	begin
--  		if Bits and File_Flag_Read /= 0 then
--  			V := V or 0;
--  		end if;
--  		if Bits and File_Flag_Write /= 0 then
--  			V := V or 1;
--  		end if;

		return V;
	end Flag_To_System;

	procedure Open (File: out File_Pointer;
	                Name: in  Slim_String;
	                Flag: in  File_Flag;
	                Mode: in  File_Mode := DEFAULT_FILE_MODE;
	                Pool: in  Storage_Pool_Pointer := null) is

		package P is new H2.Pool (Posix_File_Record, Posix_File_Pointer, Pool);
		F: Posix_File_Pointer;

	begin
		F := P.Allocate;
		F.Pool := Pool;

		--F.Handle := sys_open (Interfaces.C.char_array(Name & Slim.Character'Val(0)), 0, 0);
		F.Handle := sys_open (Name, Flag_To_System(Flag.Bits), C.int(Mode.Bits));
		if F.Handle <= -1 then
			raise Constraint_Error; -- TODO: raise a proper exception.
		end if;

		File := File_Pointer(F);
	end Open;

	procedure Open (File: out File_Pointer;
	                Name: in  Wide_String;
	                Flag: in  File_Flag;
	                Mode: in  File_Mode := DEFAULT_FILE_MODE;
	                Pool: in  Storage_Pool_Pointer := null) is
	begin
		Open (File, Wide_To_Slim(Name), Flag, Mode, Pool);
	end Open;

	procedure Close (File: in out File_Pointer) is
		F: Posix_File_Pointer;
	begin
		F := Posix_File_Pointer(File);
		sys_close (F.Handle);
		F.Handle := Interfaces.C."-"(1);

		declare
			package P is new H2.Pool (Posix_File_Record, Posix_File_Pointer, F.Pool);
		begin
			P.Deallocate (F);
		end;

		File := null;
	end Close;

end File;
