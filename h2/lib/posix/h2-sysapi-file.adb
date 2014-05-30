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

	type Posix_File_Record is new File_Record with record
		Pool: Storage_Pool_Pointer := null;
		Handle: C.int := Interfaces.C."-"(1);
	end record;
	type Posix_File_Pointer is access all Posix_File_Record;

	function Flag_To_System (Flag: in Flag_Record) return C.int is
	begin
		return 0;
	end Flag_To_System;

	function Mode_To_System (Mode: in Mode_Record) return C.int is
	begin
		return 0;
	end Mode_To_System;

	procedure Open (File: out File_Pointer;
	                Name: in  Slim_String;
	                Flag: in  Flag_Record;
	                Mode: in  Mode_Record;
	                Pool: in  Storage_Pool_Pointer := null) is

		package P is new H2.Pool (Posix_File_Record, Posix_File_Pointer, Pool);
		F: Posix_File_Pointer;
		
	begin
		F := P.Allocate;
		F.Pool := Pool;

		--F.Handle := sys_open (Interfaces.C.char_array(Name & Slim.Character'Val(0)), 0, 0);
		F.Handle := sys_open (Name, Flag_To_System(Flag), Mode_To_System(Mode));
		if F.Handle <= -1 then
			raise Constraint_Error; -- TODO: raise a proper exception.
		end if;

		File := File_Pointer(F);
	end Open;

	procedure Open (File: out File_Pointer;
	                Name: in  Wide_String;
	                Flag: in  Flag_Record;
	                Mode: in  Mode_Record;
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
