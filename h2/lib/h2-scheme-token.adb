with H2.Pool;

separate (H2.Scheme)

package body Token is

	-----------------------------------------------------------------------------
	-- BUFFER MANAGEMENT
	-----------------------------------------------------------------------------
	procedure Clear_Buffer (Buffer: in out Buffer_Record) is
		pragma Inline (Clear_Buffer);
	begin
		Buffer.Last := 0;
	end Clear_Buffer;

	procedure Purge_Buffer (Interp: in out Interpreter_Record;
	                        Buffer: in out Buffer_Record) is
	begin
		if Buffer.Len > 0 then
			declare
				subtype New_String is Object_Character_Array (1 .. Buffer.Len);
				type New_String_Pointer is access all New_String;
				for New_String_Pointer'Size use Object_Pointer_Bits;

				package Pool is new H2.Pool (New_String, New_String_Pointer, Interp.Storage_Pool);

				-- Pointer overlay doesn't work well in gnat-3.15p
				-- The pointer is initialized to null despite pragma Import.
				--Tmp: New_String_Pointer;
				--for Tmp'Address use Buffer.Ptr'Address;
				--pragma Import (Ada, Tmp);

				-- So let me use unchecked conversion instead.
				function Conv1 is new Ada.Unchecked_Conversion (Thin_Object_Character_Array_Pointer, New_String_Pointer);	
				Tmp: New_String_Pointer := Conv1(Buffer.Ptr);

			begin
				Pool.Deallocate (Tmp);
			end;

			Buffer := (Ptr => null, Len => 0, Last => 0);
		end if;
	end Purge_Buffer;

	procedure Append_Buffer (Interp: in out Interpreter_Record;
	                         Buffer: in out Buffer_Record; 
	                         Source: in     Object_Character_Array) is
		Incr: Object_Size;
	begin
		if Buffer.Last >= Buffer.Len then
			if Buffer.Len <= 0 then
				Incr := 1; -- TODO: increase to 128
			else
				Incr := Buffer.Len;	
			end if;
			if Incr < Source'Length then	
				Incr := Source'Length;
			end if;
			
			declare
				subtype New_String is Object_Character_Array (1 .. Buffer.Len + Incr);
				type New_String_Pointer is access all New_String;
				for New_String_Pointer'Size use Object_Pointer_Bits;

				package Pool is new H2.Pool (New_String, New_String_Pointer, Interp.Storage_Pool);

				T1: New_String_Pointer;

				-- Pointer overlay doesn't work well in gnat-3.15p
				-- The pointer is initialized to null despite pragma Import.
				--T2: New_String_Pointer;
				--for T2'Address use Buffer.Ptr'Address;
				--pragma Import (Ada, T2);

				-- So let me use unchecked conversion instead.
				function Conv1 is new Ada.Unchecked_Conversion (Thin_Object_Character_Array_Pointer, New_String_Pointer);	
				function Conv2 is new Ada.Unchecked_Conversion (New_String_Pointer, Thin_Object_Character_Array_Pointer);
				T2: New_String_Pointer := Conv1(Buffer.Ptr);
			begin
				T1 := Pool.Allocate;
				if Buffer.Last > 0 then
					T1(1 .. Buffer.Last) := T2(1 .. Buffer.Last);
				end if;
				Pool.Deallocate (T2);

				--T2 := T1; -- uncomment this line if using overlay.
				Buffer.Ptr := Conv2(T1); -- uncomment this line if using unchecked conversion.
			end;

			Buffer.Len := Buffer.Len + Incr;
		end if;

		Buffer.Ptr(Buffer.Last + 1 .. Buffer.Last + Source'Length) := Source;
		Buffer.Last := Buffer.Last + Source'Length;
	end Append_Buffer;

	-----------------------------------------------------------------------------
	-- TOKEN MANAGEMENT
	-----------------------------------------------------------------------------
	procedure Purge (Interp: in out Interpreter_Record) is
	begin
		Purge_Buffer (Interp, Interp.Token.Value);
		Interp.Token := (End_Token, (null, 0, 0));
	end Purge;

	procedure Set (Interp: in out Interpreter_Record;
	               Kind:   in     Token_Kind) is
	begin
		Interp.Token.Kind := Kind;	
		Clear_Buffer (Interp.Token.Value);
	end Set;

	procedure Set (Interp: in out Interpreter_Record;
	               Kind:   in     Token_Kind;
	               Value:  in     Object_Character) is
		Tmp: Object_Character_Array(1..1);
	begin
		Interp.Token.Kind := Kind;	
		Clear_Buffer (Interp.Token.Value);
		Tmp(1) := Value;
		Append_Buffer (Interp, Interp.Token.Value, Tmp);
	end Set;

	procedure Set (Interp: in out Interpreter_Record;
	               Kind:   in     Token_Kind;
	               Value:  in     Object_Character_Array) is
	begin
		Interp.Token.Kind := Kind;	
		Clear_Buffer (Interp.Token.Value);
		if Value'Length > 0 then
			Append_Buffer (Interp, Interp.Token.Value, Value);
		end if;
	end Set;

	procedure Append_String (Interp: in out Interpreter_Record;
	                         Value:  in     Object_Character_Array) is
	begin
		if Value'Length > 0 then
			Append_Buffer (Interp, Interp.Token.Value, Value);	
		end if;
	end Append_String;

	procedure Append_Character (Interp: in out Interpreter_Record;
	                            Value:  in     Object_Character) is
		Tmp: Object_Character_Array(1..1) := (1 => Value);
	begin
		Append_Buffer (Interp, Interp.Token.Value, Tmp);
	end Append_Character;

end Token;
