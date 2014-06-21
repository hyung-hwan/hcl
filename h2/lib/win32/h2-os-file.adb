
with H2.Pool;
with H2.Sysdef;

separate (H2.OS)

package body File is

	-- External functions and procedures
	function CreateFileA (lpFileName           : Slim_String;
					  dwDesiredAccess      : Sysdef.DWORD;
					  dwShareMode          : Sysdef.DWORD;
					  lpSecurityAttributes : Sysdef.PVOID; -- LPSECURITY_ATTRIBUTES;
					  dwCreationDisposition: Sysdef.DWORD;
					  dwFlagsAndAttributes : Sysdef.DWORD;
					  hTemplateFile        : Sysdef.HANDLE) return Sysdef.HANDLE;
	pragma Import (Stdcall, CreateFileA, "CreateFileA");

	function CreateFileW (lpFileName           : Wide_String;
					  dwDesiredAccess      : Sysdef.DWORD;
					  dwShareMode          : Sysdef.DWORD;
					  lpSecurityAttributes : Sysdef.PVOID; -- LPSECURITY_ATTRIBUTES;
					  dwCreationDisposition: Sysdef.DWORD;
					  dwFlagsAndAttributes : Sysdef.DWORD;
					  hTemplateFile        : Sysdef.HANDLE) return Sysdef.HANDLE;
	pragma Import (Stdcall, CreateFileW, "CreateFileW");

	procedure CloseFile (fd: Sysdef.HANDLE);
	pragma Import (Stdcall, CloseFile, "CloseFile");

	function ReadFile (fd: Sysdef.HANDLE; buf: in System.Address; count: in Sysdef.size_t) return Sysdef.ssize_t;
	pragma Import (Stdcall, ReadFile, "ReadFile");

	function Sys_Write (fd: Sysdef.HANDLE; buf: in System.Address; count: in Sysdef.size_t) return Sysdef.ssize_t;
	pragma Import (Stdcall, WriteFile, "WriteFile");

	-- Common constants
	INVALID_HANDLE: constant := -1;
	ERROR_RETURN: constant := -1;

	-- File record
	type Posix_File_Record is new File_Record with record
		Pool: Storage_Pool_Pointer := null;
		Handle: Sysdef.int_t := INVALID_HANDLE;
	end record;
	type Posix_File_Pointer is access all Posix_File_Record;

	-- Standard Files
	Stdin: aliased Posix_File_Record := (null, 0);
	Stdout: aliased Posix_File_Record := (null, 1);
	Stderr: aliased Posix_File_Record := (null, 2);

	function Flag_To_System (Bits: in Flag_Bits) return System_Word is
		V: System_Word := 0;
	begin
		if ((Bits and FLAG_READ) /= 0) and then 
		   ((Bits and FLAG_WRITE) /= 0) then
			V := V or Sysdef.O_RDWR;
		elsif ((Bits and FLAG_WRITE) /= 0) then
			V := V or Sysdef.O_WRONLY;
		else
			V := V or Sysdef.O_RDONLY;
		end if;

		if (Bits and FLAG_CREATE) /= 0 then
			V := V or Sysdef.O_CREAT;
		end if;

		if (Bits and FLAG_TRUNCATE) /= 0 then
			V := V or Sysdef.O_TRUNC;
		end if;

		if (Bits and FLAG_APPEND) /= 0 then
			V := V or Sysdef.O_APPEND;
		end if;

		if (Bits and FLAG_NONBLOCK) /= 0 then
			V := V or Sysdef.O_NONBLOCK;
		end if;

		if (Bits and FLAG_SYNC) /= 0 then
			V := V or Sysdef.O_SYNC;
		end if;

		return V;
	end Flag_To_System;

	function Get_Stdin return File_Pointer is
	begin
		--return File_Pointer'(Stdin'Access);
		return File_Record(Stdin)'Access;
	end Get_Stdin;

	function Get_Stdout return File_Pointer is
	begin
		--return File_Pointer'(Stdout'Access);
		return File_Record(Stdout)'Access;
	end Get_Stdout;

	function Get_Stderr return File_Pointer is
	begin
		--return File_Pointer'(Stderr'Access);
		return File_Record(Stdout)'Access;
	end Get_Stderr;

	procedure Open (File: out File_Pointer;
	                Name: in  Slim_String;
	                Flag: in  Flag_Record;
	                Mode: in  Mode_Record := DEFAULT_MODE;
	                Pool: in  Storage_Pool_Pointer := null) is

		package P is new H2.Pool (Posix_File_Record, Posix_File_Pointer, Pool);
		F: Posix_File_Pointer;

	begin
		F := P.Allocate;
		F.Pool := Pool;

		F.Handle := Sys_Open (Name & Slim_Character'Val(0), 
		                      Sysdef.int_t(Flag_To_System(Flag.Bits)),
		                      Sysdef.int_t(Mode.Bits));
		if Sysdef."<=" (F.Handle, INVALID_HANDLE) then
			raise Constraint_Error; -- TODO: raise a proper exception.
		end if;

		File := File_Pointer(F);
	end Open;

	procedure Open (File: out File_Pointer;
	                Name: in  Wide_String;
	                Flag: in  Flag_Record;
	                Mode: in  Mode_Record := DEFAULT_MODE;
	                Pool: in  Storage_Pool_Pointer := null) is
	begin
		Open (File, Wide_To_Slim(Name), Flag, Mode, Pool);
	end Open;

	procedure Close (File: in out File_Pointer) is
		F: Posix_File_Pointer := Posix_File_Pointer(File);
	begin
		if F /= Stdin'Access and then F /= Stdout'Access and then F /= Stderr'Access then
			-- Don't close standard files.

			Sys_Close (F.Handle);
			F.Handle := INVALID_HANDLE;

			declare
				package P is new H2.Pool (Posix_File_Record, Posix_File_Pointer, F.Pool);
			begin
				P.Deallocate (F);
			end;

			File := null;
		end if;
	end Close;

	procedure Read (File:   in     File_Pointer; 
	                Buffer: out    System_Byte_Array;
	                Length: out    System_Length) is
		pragma Assert (Buffer'Length > 0);
		F: Posix_File_Pointer := Posix_File_Pointer(File);
		N: Sysdef.ssize_t;
	begin
		N := Sys_Read (F.Handle, Buffer'Address, Buffer'Length);
		if Sysdef."<=" (N, ERROR_RETURN) then
			raise Constraint_Error; -- TODO rename exception
		else
			Length := System_Length(N);
		end if;
	end Read;

	procedure Write (File:   in  File_Pointer; 
	                 Buffer: in  System_Byte_Array;
	                 Length: out System_Length) is
		pragma Assert (Buffer'Length > 0);
		F: Posix_File_Pointer := Posix_File_Pointer(File);
		N: Sysdef.ssize_t;
	begin
		N := Sys_Write (F.Handle, Buffer'Address, Buffer'Length);
		if Sysdef."<=" (N, ERROR_RETURN) then
			raise Constraint_Error; -- TODO rename exception
		else
			Length := System_Length(N);
		end if;

	end Write;
end File;
