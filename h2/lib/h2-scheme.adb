with H2.Pool;
with System.Address_To_Access_Conversions;

package body H2.Scheme is

	----------------------------------------------------------------------------------
	-- EXCEPTIONS
	----------------------------------------------------------------------------------
	Allocation_Error: exception;
	Size_Error: exception;
	Internal_Error: exception;

	----------------------------------------------------------------------------------
	-- INTERNALLY-USED TYPES
	----------------------------------------------------------------------------------
	type Memory_Element_Pointer is access all Memory_Element;
	for Memory_Element_Pointer'Size use Object_Pointer_Bits; -- ensure that it can be overlayed by an ObjectPointer

	type Thin_Memory_Element_Array is array (1 .. Memory_Size'Last) of Memory_Element;
	type Thin_Memory_Element_Array_Pointer is access all Thin_Memory_Element_Array;
	for Thin_Memory_Element_Array_Pointer'Size use Object_Pointer_Bits;

	subtype Opcode_Type is Object_Integer range 0 .. 5;
	Opcode_Exit:               constant Opcode_Type := Opcode_Type'(0);
	Opcode_Evaluate_Object:    constant Opcode_Type := Opcode_Type'(1);
	Opcode_Evaluate_Argument:  constant Opcode_Type := Opcode_Type'(2);
	Opcode_Evaluate_Syntax:    constant Opcode_Type := Opcode_Type'(3);
	Opcode_Evaluate_Procedure: constant Opcode_Type := Opcode_Type'(4);
	Opcode_Apply:              constant Opcode_Type := Opcode_Type'(5);

	----------------------------------------------------------------------------------
	-- COMMON OBJECTS
	----------------------------------------------------------------------------------
	Cons_Object_Size: constant Pointer_Object_Size := 2;
	Cons_Car_Index: constant Pointer_Object_Size := 1;
	Cons_Cdr_Index: constant Pointer_Object_Size := 2;

	Frame_Object_Size: constant Pointer_Object_Size := 5;
	Frame_Stack_Index: constant Pointer_Object_Size := 1;
	Frame_Opcode_Index: constant Pointer_Object_Size := 2;
	Frame_Operand_Index: constant Pointer_Object_Size := 3;
	Frame_Environment_Index: constant Pointer_Object_Size := 4;
	Frame_Return_Index: constant Pointer_Object_Size := 5;

	Procedure_Object_Size: constant Pointer_Object_Size := 1;
	Procedure_Opcode_Index: constant Pointer_Object_Size := 1;

	Closure_Object_Size: constant Pointer_Object_Size := 2;
	Closure_Code_Index: constant Pointer_Object_Size := 1;
	Closure_Environment_Index: constant Pointer_Object_Size := 2;

	procedure Set_New_Location (Object: in Object_Pointer; Ptr: in Memory_Element_Pointer);
	procedure Set_New_Location (Object: in Object_Pointer; Ptr: in Object_Pointer);
	pragma Inline (Set_New_Location);

	function Get_New_Location (Object: in Object_Pointer) return Object_Pointer;
	pragma Inline (Get_New_Location);

	----------------------------------------------------------------------------------
	-- POINTER AND DATA CONVERSION
	----------------------------------------------------------------------------------

	function Get_Pointer_Type (Pointer: in Object_Pointer) return Object_Pointer_Type is
		pragma Inline (Get_Pointer_Type);

		Word: Object_Word;
		for Word'Address use Pointer'Address;
	begin 
		return Object_Pointer_Type(Word and Object_Word(Object_Pointer_Type_Mask));
	end Get_Pointer_Type;

	function Is_Pointer (Pointer: in Object_Pointer) return Standard.Boolean is
	begin
		return Get_Pointer_Type(Pointer) = Object_Pointer_Type_Pointer;
	end Is_Pointer;

	function Is_Special_Pointer (Pointer: in Object_Pointer) return Standard.Boolean is
	begin
		-- though sepcial, these 3 pointers gets true for Is_Pointer.
		return Pointer = Nil_Pointer or else 
		       Pointer = True_Pointer or else
		       Pointer = False_Pointer;
	end Is_Special_Pointer;

	function Is_Normal_Pointer (Pointer: in Object_Pointer) return Standard.Boolean is
	begin
		return Is_Pointer(Pointer) and then 
	            not Is_Special_Pointer(Pointer);
	end Is_Normal_Pointer;

	function Is_Integer (Pointer: in Object_Pointer) return Standard.Boolean is
	begin
		return Get_Pointer_Type(Pointer) = Object_Pointer_Type_Integer;
	end Is_Integer;

	function Is_Character (Pointer: in Object_Pointer) return Standard.Boolean is
	begin
		return Get_Pointer_Type(Pointer) = Object_Pointer_Type_Character;
	end Is_Character;

	function Is_Byte (Pointer: in Object_Pointer) return Standard.Boolean is
	begin
		return Get_Pointer_Type(Pointer) = Object_Pointer_Type_Byte;
	end Is_Byte;


	function Integer_To_Pointer (Int: in Object_Integer) return Object_Pointer is
		Pointer: Object_Pointer;
		Word: Object_Word;
		for Word'Address use Pointer'Address;
	begin
		if Int < 0 then
			-- change the sign of a negative number.
			-- '-Int' may violate the range of Object_Integer
			-- if it is Object_Integer'First. So I add 1 to 'Int'
			-- first to make it fall between Object_Integer'First + 1
			-- .. 0 and typecast it with an extra increment.
			--Word := Object_Word (-(Int + 1)) + 1;

			-- Let me use Object_Signed_Word instead of the trick shown above
			Word := Object_Word (-Object_Signed_Word(Int));

			-- shift the number to the left by 2 and
			-- set the highest bit on by force.
			Word := (Word * (2 ** Object_Pointer_Type_Bits)) or Object_Word(Object_Pointer_Type_Integer) or (2 ** (Word'Size - 1));
		else
			Word := Object_Word (Int);
			-- Shift 'Word' to the left by 2 and set the integer mark.
			Word := (Word * (2 ** Object_Pointer_Type_Bits)) or Object_Word(Object_Pointer_Type_Integer);
		end if;

		--return Object_Word_To_Object_Pointer (Word);
		return Pointer;
	end Integer_To_Pointer;

	function Character_To_Pointer (Char: in Object_Character) return Object_Pointer is
		Pointer: Object_Pointer;
		Word: Object_Word;
		for Word'Address use Pointer'Address;
	begin
		-- Note: Object_Character may get defined to Wide_Wide_Character.
		--   and Wide_Wide_Character'Last is #16#7FFFFFFF#. Such a large value
		--   may get lost when it's shifted left by 2 if Object_Word is 32 bits long
		--   or short. In reality, the last Unicode code point assigned is far
		--   less than #16#7FFFFFFF# as of this writing. So I should not be
		--   worried about it for the time being.
		Word := Object_Character'Pos (Char);
		Word := (Word * (2 ** Object_Pointer_Type_Bits)) or Object_Word(Object_Pointer_Type_Character);
		--return Object_Word_To_Object_Pointer (Word);
		return Pointer;
	end Character_To_Pointer;

	function Byte_To_Pointer (Byte: in Object_Byte) return Object_Pointer is
		Pointer: Object_Pointer;
		Word: Object_Word;
		for Word'Address use Pointer'Address;
	begin
		Word := Object_Word(Byte);
		Word := (Word * (2 ** Object_Pointer_Type_Bits)) or Object_Word(Object_Pointer_Type_Byte);
		return Pointer;
	end Byte_To_Pointer;

	function Pointer_To_Word is new Ada.Unchecked_Conversion (Object_Pointer, Object_Word);
	--function Pointer_To_Word (Pointer: in Object_Pointer) return Object_Word is
	--	Word: Object_Word;
	--	for Word'Address use Pointer'Address;
	--begin
	--	return Word;
	--end Pointer_To_Word;
	pragma Inline (Pointer_To_Word);

	function Pointer_To_Integer (Pointer: in Object_Pointer) return Object_Integer is
		Word: Object_Word := Pointer_To_Word (Pointer);
	begin
		if (Word and (2 ** (Word'Size - 1))) /= 0 then
			-- if the highest bit is set, it's a negative number
			-- originally. strip it off and shift 'Word' to the right by 2.
			return Object_Integer (-Object_Signed_Word (Word and not (2 ** (Word'Size - 1))) / (2 ** Object_Pointer_Type_Bits));
		else
			-- shift Word to the right by Object_Pointer_Type_Bits.
			return Object_Integer (Word / (2 ** Object_Pointer_Type_Bits));
		end if;
	end Pointer_To_Integer;

	function Pointer_To_Character (Pointer: in Object_Pointer) return Object_Character is
		Word: Object_Word := Pointer_To_Word (Pointer);
	begin
		return Object_Character'Val (Word / (2 ** Object_Pointer_Type_Bits));
	end Pointer_To_Character;

	function Pointer_To_Byte (Pointer: in Object_Pointer) return Object_Byte is
		Word: Object_Word := Pointer_To_Word (Pointer);
	begin
		return Object_Byte(Word / (2 ** Object_Pointer_Type_Bits));
	end Pointer_To_Byte;

	-- Check if a character object contains a given string in the payload.
	function Match (Object: in Object_Pointer; 
	                Data:   in Object_String) return Standard.Boolean is
		Slot: Object_Character_Array renames Object.Character_Slot;
	begin
		return Slot(Slot'First .. Slot'Last - 1) = Object_Character_Array(Data);
	end;

	procedure Copy_String (Source: in  Object_String;
	                       Target: out Object_Character_Array) is
	begin
		-- This procedure is not generic. The size of the Source 
		-- and Target must be in the expected length.
		pragma Assert (Source'Length + 1 = Target'Length); 

		-- Method 1. Naive. It doesn't look Adaish.
		-- ---------------------------------------------------------------------
		--declare
		--	x: Storage_Count;
		--begin	
		--	x := Target'First;
		--	for index in Source'Range loop
		--		Target(x) := Source(index);
		--		x := x + 1;
		--	end loop;
		--	Target(x) := Object_Character'First; -- Object_Character'Val(0);
		--end;

		-- Method 2.
		-- ObjectAda complains that the member of Object_String is not 
		-- aliased because Object_Character_Array is an array of aliased 
		-- Object_Character.It points to LRM 4.6(12); The component subtypes
		-- shall statically match. 
		-- ---------------------------------------------------------------------
		--Target(Target'First .. Target'Last - 1) := Object_Character_Array (Source(Source'First .. Source'Last));
		--Target(Target'Last) := Object_Character'First; -- Object_Character'Val(0);

		-- Method 3. Use unchecked conversion
		declare
			subtype Character_Array is Object_Character_Array (Target'First .. Target'Last - 1);
			function To_Character_Array is new Ada.Unchecked_Conversion (Object_String, Character_Array);
		begin
			Target(Target'First .. Target'Last - 1) := To_Character_Array (Source);
			Target(Target'Last) := Object_Character'First; -- Object_Character'Val(0);
		end;
	end Copy_String;

	procedure Copy_String (Source: in  Object_Character_Array;
	                       Target: out Object_String) is
	begin
		pragma Assert (Source'Length = Target'Length + 1); 

		declare
			subtype Character_Array is Object_Character_Array (Source'First .. Source'Last - 1);
			subtype String_Array is Object_String (Target'Range);
			function To_Character_Array is new Ada.Unchecked_Conversion (Character_Array, String_Array);
		begin
			Target := To_Character_Array (Source (Source'First .. Source'Last - 1));
		end;
	end Copy_String;

	function To_String (Source: in Object_Character_Array) return Object_String is
	begin
		-- ObjectAda complains that the member of Object_String is not 
		-- aliased because Object_Character_Array is an array of aliased
		-- Object_Character. It points to LRM 4.6(12); The component subtypes
		-- shall statically match. So let me turn to unchecked conversion.
		declare
			subtype Character_Array is Object_Character_Array (Source'First .. Source'Last - 1);
			subtype String_Array is Object_String (1 .. Source'Length - 1);
			function To_Character_Array is new Ada.Unchecked_Conversion (Character_Array, String_Array);
		begin
			return To_Character_Array (Source (Source'First .. Source'Last - 1));
			--return String_Array (Source (Source'First .. Source'Last - 1));
		end;
	end To_String;

	type Thin_String is new Object_String (Standard.Positive'Range);
	type Thin_String_Pointer is access all Thin_String;
	for Thin_String_Pointer'Size use Object_Pointer_Bits;

-- TODO: move away these utilities routines
	--function To_Thin_String_Pointer (Source: in Object_Pointer) return Thin_String_Pointer is
	--	type Character_Pointer is access all Object_Character;
	--	Ptr: Thin_String_Pointer;

	--	X: Character_Pointer;
	--	for X'Address use Ptr'Address;
	--	pragma Import (Ada, X);
	--begin
		-- this method requires Object_Character_Array to have aliased Object_Character.
		-- So i've commented out this function and turn to a different method below.
	--	X := Source.Character_Slot(Source.Character_Slot'First)'Access; 
	--	return Ptr;
	--end To_Thin_String_Pointer;	

	--function To_Thin_String_Pointer (Source: in Object_Pointer) return Thin_String_Pointer is
	--	function To_Thin_Pointer is new Ada.Unchecked_Conversion (System.Address, Thin_String_Pointer);
	--begin
	--	return To_Thin_Pointer(Source.Character_Slot'Address);
	--end To_Thin_String_Pointer;	

	function To_Thin_String_Pointer (Source: in Object_Pointer) return Thin_String_Pointer is
		X: aliased Thin_String;
		for X'Address use Source.Character_Slot'Address;
	begin
		return X'Unchecked_Access;
	end To_Thin_String_Pointer;	

	procedure Put_String (TS: in Thin_String_Pointer);
	pragma Import (C, Put_String, "puts");

	-- TODO: delete this procedure 
	procedure Print_Object_Pointer (Msg: in Object_String; Source: in Object_Pointer) is
		W: Object_Word;
		for W'Address use Source'Address;

		Ptr_Type: Object_Pointer_Type;
	begin
		Ptr_Type := Get_Pointer_Type(Source);
		if Ptr_Type = Object_Pointer_Type_Character then
			Text_IO.Put_Line (Msg & Object_Character'Image(Pointer_To_Character(Source)));
		elsif Ptr_Type = Object_Pointer_Type_Integer then
			Text_IO.Put_Line (Msg & Object_Integer'Image(Pointer_To_Integer(Source)));
		elsif Is_Special_Pointer (Source) then
			Text_IO.Put_Line (Msg & " at " & Object_Word'Image(W));
		elsif Source.Kind = Character_Object then
			Text_IO.Put (Msg & " at " & Object_Word'Image(W) & " at " & Object_Kind'Image(Source.Kind) & " size " & Object_Size'Image(Source.Size) & " - ");
			if Source.Kind = Moved_Object then
				Text_IO.Put_Line (To_String (Get_New_Location(Source).Character_Slot));
			else
				Text_IO.Put_Line (To_String (Source.Character_Slot));
			end if;
		else
			Text_IO.Put_Line (Msg & " at " & Object_Word'Image(W) & " at " & Object_Kind'Image(Source.Kind));
		end if;
	end Print_Object_Pointer;

	----------------------------------------------------------------------------------
	-- MEMORY MANAGEMENT
	----------------------------------------------------------------------------------
-- (define x ())
-- (define x #())
-- (define x $())
-- (define x #( 
--              (#a . 10)  ; a is a synbol
--              (b . 20)   ; b is a variable. resolve b at the eval-time and use it.
--              ("c" . 30) ; "c" is a string
--            )
-- )
-- (clone x y) -- deep copy
-- (define y x) -- reference assignment
-- (set! x.a 20) -- syntaic sugar
-- (set! (get x #a) 20)
-- (define x (make-hash))


	-- I wanted to reuse the Size field to store the pointer to
	-- the new location. GCC-GNAT 3.2.3 suffered from various constraint
	-- check errors. So i gave up on this procedure.
	--------------------------------------------------------------------
	--procedure Set_New_Location (Object: in Object_Pointer; Ptr: in Memory_Element_Pointer) is
		--New_Addr: Memory_Element_Pointer;
		--for New_Addr'Address use Object.Size'Address;
		--pragma Import (Ada, New_Addr); 
	--begin
		--New_Addr := Ptr;
	--end Set_New_Location;
	--function Get_New_Location (Object: in Object_Pointer) return Object_Pointer is
		--New_Ptr: Object_Pointer;
		--for New_Ptr'Address use Object.Size'Address;
		--pragma Import (Ada, New_Ptr); 
	--begin
		--return New_Ptr;
	--end;

	-- Instead, I created a new object kind that indicates a moved object.	
	-- The original object is replaced by this special object. this special 
	-- object takes up the smallest space that a valid object can take. So
	-- it is safe to overlay it on any normal objects.
	procedure Set_New_Location (Object: in Object_Pointer; Ptr: in Memory_Element_Pointer) is
		subtype Moved_Object_Record is Object_Record (Moved_Object, 0);
		Moved_Object: Moved_Object_Record;
		for Moved_Object'Address use Object.all'Address;
		-- pramga Import must not be specified here as I'm counting
		-- on the default initialization of Moved_Object to overwrite
		-- the Kind discriminant in particular.
		--pragma Import (Ada, Moved_Object); -- this must not be used.
		function To_Object_Pointer is new Ada.Unchecked_Conversion (Memory_Element_Pointer, Object_Pointer);
	begin
		Moved_Object.New_Pointer := To_Object_Pointer (Ptr);
	end Set_New_Location;

	procedure Set_New_Location (Object: in Object_Pointer; Ptr: in Object_Pointer) is
		subtype Moved_Object_Record is Object_Record (Moved_Object, 0);
		Moved_Object: Moved_Object_Record;
		for Moved_Object'Address use Object.all'Address;
		--pragma Import (Ada, Moved_Object); -- this must not be used.
	begin
		Moved_Object.New_Pointer := Ptr;
	end Set_New_Location;

	function Get_New_Location (Object: in Object_Pointer) return Object_Pointer is
	begin
		return Object.New_Pointer;
	end Get_New_Location;

	procedure Allocate_Bytes_In_Heap (Heap:        in out Heap_Pointer; 
	                                  Heap_Bytes:  in     Memory_Size;
	                                  Heap_Result: out    Memory_Element_Pointer) is
		Avail: Memory_Size;
	begin
		Avail := Heap.Size - Heap.Bound;
		if Heap_Bytes > Avail then
			Heap_Result := null;
		else
			Heap_Result := Heap.Space(Heap.Bound + 1)'Unchecked_Access;
			Heap.Bound := Heap.Bound + Heap_Bytes;
		end if;
	end Allocate_Bytes_In_Heap;

	procedure Copy_Object (Source: in     Object_Pointer;
	                       Target: in out Memory_Element_Pointer) is
		pragma Inline (Copy_Object);
		subtype Target_Object_Record is Object_Record (Source.Kind, Source.Size);
		type Target_Object_Pointer is access all Target_Object_Record;

		Target_Object: Target_Object_Pointer;
		for Target_Object'Address use Target'Address;
		pragma Import (Ada, Target_Object); 
	begin
		-- This procedure should work. but gnat 4.3.2 on whiite(ppc32,wii)
		-- produced erroneous code when it was called from Move_One_Object().
		-- Target_Object_Record'Size, Target_Object.all'Size, and
		-- Target_Object_Record'Max_Size_In_Stroage_Elements were not
		-- always correct. For example, for a character object containing
		-- the string "lambda", Target_Object.all'Size returned 72 while
		-- it's supposed to be 96. 
		Target_Object.all := Source.all;
		pragma Assert (Source.all'Size = Target_Object.all'Size);
	end Copy_Object;

	procedure Copy_Object_With_Size (Source: in     Object_Pointer;
	                                 Target: in out Memory_Element_Pointer;
	                                 Bytes:  in     Memory_Size) is
		pragma Inline (Copy_Object_With_Size);
		-- This procedure uses a more crude type for copying objects.
		-- It's the result of an effort to work around some compiler
		-- issues mentioned above.
		Tgt: Thin_Memory_Element_Array_Pointer;
		for Tgt'Address use Target'Address;
		pragma Import (Ada, Tgt);

		Src: Thin_Memory_Element_Array_Pointer;
		for Src'Address use Source'Address;
		pragma Import (Ada, Src);
	begin
		Tgt(1..Bytes) := Src(1..Bytes);
	end Copy_Object_With_Size;

	procedure Collect_Garbage (Interp: in out Interpreter_Record) is

		Last_Pos: Memory_Size;
		New_Heap: Heap_Number;

		--function To_Object_Pointer is new Ada.Unchecked_Conversion (Memory_Element_Pointer, Object_Pointer);

		function Move_One_Object (Object: in Object_Pointer) return Object_Pointer is
		begin
			if Is_Special_Pointer (Object)  then
Print_Object_Pointer ("Moving special ...", Object);
				return Object;
			end if;

			if Object.Kind = Moved_Object then
Print_Object_Pointer ("Moving NOT ...", Object);
				-- the object has moved to the new heap.
				-- the size field has been updated to the new object
				-- in the 'else' block below. i can simply return it
				-- without further migration.
				return Get_New_Location (Object);
			else
Print_Object_Pointer ("Moving REALLY ...", Object);
				declare
					Bytes: Memory_Size;

					-- This variable holds the allocation result
					Ptr: Memory_Element_Pointer;

					-- Create an overlay for type conversion
					New_Object: Object_Pointer;
					for New_Object'Address use Ptr'Address;
					pragma Import (Ada, New_Object); 
				begin

					-- Target_Object_Record'Max_Size_In_Storage_Elements gave 
					-- some erroneous values when compiled with GNAT 4.3.2 on 
					-- WII(ppc) Debian.
					--Bytes := Target_Object_Record'Max_Size_In_Storage_Elements;
					Bytes := Object.all'Size / System.Storage_Unit;

					-- Allocate space in the new heap
					Allocate_Bytes_In_Heap (
						Heap => Interp.Heap(New_Heap),
						Heap_Bytes => Bytes,
						Heap_Result => Ptr
					);

					-- Allocation here must not fail because
					-- I'm allocating the new space in a new heap for
					-- moving an existing object in the current heap.
					-- It must not fail, assuming the new heap is as large
					-- as the old heap, and garbage collection doesn't
					-- allocate more objects than in the old heap.
					pragma Assert (Ptr /= null);

					-- Copy the payload to the new object
					--Copy_Object (Object, Ptr); -- not reliable with some compilers
					Copy_Object_With_Size (Object, Ptr, Bytes); -- use this instead
					pragma Assert (Object.all'Size = New_Object.all'Size);
					pragma Assert (Bytes = New_Object.all'Size / System.Storage_Unit);

					-- Let the size field of the old object point to the
					-- new object allocated in the new heap. It is returned
					-- in the 'if' block at the beginning of this function
					-- if the object is marked with FLAG_MOVED;
					Set_New_Location (Object, Ptr);

Text_IO.Put_Line (Object_Word'Image(Pointer_To_Word(Object)) & Object_Word'Image(Pointer_To_Word(New_Object)));
Text_IO.Put_Line ("  Flags....after " & Object_Kind'Image(Object.Kind) & " New Size " & Object_Size'Image(Object.Size) & " New Loc: " & Object_Word'Image(Pointer_To_Word(Object.New_Pointer)));
					-- Return the new object
					return New_Object;
				end;
			end if;
		end Move_One_Object;

		function Scan_New_Heap (Start_Position: in Memory_Size) return Memory_Size is
			Ptr: Memory_Element_Pointer;

			Position: Memory_Size;
		begin
			Position := Start_Position;

--Text_IO.Put_Line ("Start Scanning New Heap from " & Memory_Size'Image (Start_Position) & " Bound: " & Memory_Size'Image (Interp.Heap(New_Heap).Bound));
			while Position <= Interp.Heap(New_Heap).Bound loop

--Text_IO.Put_Line (">>> Scanning New Heap from " & Memory_Size'Image (Position) & " Bound: " & Memory_Size'Image (Interp.Heap(New_Heap).Bound));
				Ptr := Interp.Heap(New_Heap).Space(Position)'Unchecked_Access;

				declare
					Object: Object_Pointer;
					for Object'Address use Ptr'Address;
					pragma Import (Ada, Object); -- not really needed

					--subtype Target_Object_Record is Object_Record (Object.Kind, Object.Size);
					Bytes: Memory_Size;

				begin
					--Bytes := Target_Object_Record'Max_Size_In_Storage_Elements;
					Bytes := Object.all'Size / System.Storage_Unit;
				
--Text_IO.Put_Line (">>> Scanning Obj " & Object_Kind'Image (Object.Kind) & " size " & Object_Size'Image(Object.Size) & " at " & Object_Word'Image(Pointer_To_Word(Object)) & " Bytes " & Memory_Size'Image(Bytes));

					if Object.Kind = Pointer_Object then
						for i in Object.Pointer_Slot'Range loop
							if Is_Pointer (Object.Pointer_Slot(i)) then
								Object.Pointer_Slot(i) := Move_One_Object (Object.Pointer_Slot(i));
							end if;
						end loop;
					end if;

					Position := Position + Bytes;
				end;

			end loop;

			return Position;
		end Scan_New_Heap;

		procedure Compact_Symbol_Table is
			Pred: Object_Pointer;
			Cons: Object_Pointer;
		begin
-- TODO: Change code here if the symbol table structure is changed to a hash table.

			Pred := Nil_Pointer;
			Cons := Interp.Symbol_Table;
			while Cons /= Nil_Pointer loop
				pragma Assert (Cons.Tag = Cons_Object);	

				declare
					Car: Object_Pointer renames Cons.Pointer_Slot(Cons_Car_Index);
					Cdr: Object_Pointer renames Cons.Pointer_Slot(Cons_Cdr_Index);
				begin
					pragma Assert (Car.Kind = Moved_Object or else Car.Tag = Symbol_Object);

					if Car.Kind /= Moved_Object and then
					   (Car.Flags and Syntax_Object) = 0 then
						-- A non-syntax symbol has not been moved. 
						-- Unlink the cons cell from the symbol table.

Text_IO.Put_Line ("COMPACT_SYMBOL_TABLE Unlinking " & To_String (Car.Character_Slot));
						if Pred = Nil_Pointer then
							Interp.Symbol_Table := Cdr;
						else
							Pred.Pointer_Slot(Cons_Cdr_Index) := Cdr;
						end if;
					end if;
					
					Cons := Cdr;	
				end;
			end loop;
		end Compact_Symbol_Table;

	begin
		-- As the Heap_Number type is a modular type that can 
		-- represent 0 and 1, incrementing it gives the next value.
		New_Heap := Interp.Current_Heap + 1;

		-- Migrate objects in the root table
Print_Object_Pointer ("Root_Table ...", Interp.Root_Table);
		Interp.Root_Table := Move_One_Object (Interp.Root_Table);

		-- Scane the heap
		Last_Pos := Scan_New_Heap (Interp.Heap(New_Heap).Space'First);

		-- Traverse the symbol table for unreferenced symbols.
		-- If the symbol has not moved to the new heap, the symbol
		-- is not referenced by any other objects than the symbol 
		-- table itself 
Text_IO.Put_Line (">>> [COMPACTING SYMBOL TABLE]");
		Compact_Symbol_Table;

Print_Object_Pointer (">>> [MOVING SYMBOL TABLE]", Interp.Symbol_Table);
		-- Migrate the symbol table itself
		Interp.Symbol_Table := Move_One_Object (Interp.Symbol_Table);

Text_IO.Put_Line (">>> [SCANNING HEAP AGAIN AFTER SYMBOL TABLE MIGRATION]");
		-- Scan the new heap again from the end position of
		-- the previous scan to move referenced objects by 
		-- the symbol table. 
		Last_Pos := Scan_New_Heap (Last_Pos);

		-- Swap the current heap and the new heap
		Interp.Heap(Interp.Current_Heap).Bound := 0;
		Interp.Current_Heap := New_Heap;
Text_IO.Put_Line (">>> [GC DONE]");
	end Collect_Garbage;

	procedure Allocate_Bytes (Interp: in out Interpreter_Record;
	                          Bytes:  in     Memory_Size;
	                          Result: out    Memory_Element_Pointer) is

		-- I use this temporary variable not to change Result
		-- if Allocation_Error should be raised.
		Tmp: Memory_Element_Pointer;
	begin
		pragma Assert (Bytes > 0);

		Allocate_Bytes_In_Heap (Interp.Heap(Interp.Current_Heap), Bytes, Tmp);
		if Tmp = null and then (Interp.Trait.Trait_Bits and No_Garbage_Collection) = 0 then
			Collect_Garbage (Interp);
			Allocate_Bytes_In_Heap (Interp.Heap(Interp.Current_Heap), Bytes, Tmp);
			if Tmp = null then
				raise Allocation_Error;
			end if;
		end if;

		Result := Tmp;
	end Allocate_Bytes;

	procedure Allocate_Pointer_Object (Interp:  in out Interpreter_Record;
	                                   Size:    in     Pointer_Object_Size;
	                                   Initial: in     Object_Pointer;
	                                   Result:  out    Object_Pointer) is

		subtype Pointer_Object_Record is Object_Record (Pointer_Object, Size);
		type Pointer_Object_Pointer is access all Pointer_Object_Record;

		Ptr: Memory_Element_Pointer;
		for Ptr'Address use Result'Address;
		pragma Import (Ada, Ptr);
		Obj_Ptr: Pointer_Object_Pointer;
		for Obj_Ptr'Address use Result'Address;
		pragma Import (Ada, Obj_Ptr);
	begin
		Allocate_Bytes (
			Interp,
			Memory_Size'(Pointer_Object_Record'Max_Size_In_Storage_Elements),
			Ptr
		);

		Obj_Ptr.all := (
			Kind => Pointer_Object,
			Size => Size,
			Flags => 0,
			Scode => 0,
			Tag => Unknown_Object,
			Pointer_Slot => (others => Initial)
		);
	end Allocate_Pointer_Object;

	procedure Allocate_Character_Object (Interp: in out Interpreter_Record;
	                                     Size:   in     Character_Object_Size;
	                                     Result: out    Object_Pointer) is

		subtype Character_Object_Record is Object_Record (Character_Object, Size);
		type Character_Object_Pointer is access all Character_Object_Record;

		Ptr: Memory_Element_Pointer;
		for Ptr'Address use Result'Address;
		pragma Import (Ada, Ptr);
		Obj_Ptr: Character_Object_Pointer;
		for Obj_Ptr'Address use Result'Address;
		pragma Import (Ada, Obj_Ptr);
	begin
		Allocate_Bytes (
			Interp,
			Memory_Size'(Character_Object_Record'Max_Size_In_Storage_Elements), 
			Ptr
		);

		Obj_Ptr.all := (
			Kind => Character_Object,
			Size => Size,
			Flags => 0,
			Scode => 0,
			Tag => Unknown_Object,
			Character_Slot => (others => Object_Character'First)
		);
	end Allocate_Character_Object;

	procedure Allocate_Character_Object (Interp: in out Interpreter_Record;
	                                     Source: in     Object_String;
	                                     Result: out    Object_Pointer) is
	begin
		if Source'Length > Character_Object_Size'Last then
			raise Size_Error;
		end if;
		
		Allocate_Character_Object (Interp, Character_Object_Size'(Source'Length), Result);
		Copy_String (Source, Result.Character_Slot);
	end Allocate_Character_Object;

	procedure Allocate_Byte_Object (Interp: in out Interpreter_Record;
	                                Size:   in     Byte_Object_Size;
	                                Result: out    Object_Pointer) is

		subtype Byte_Object_Record is Object_Record (Byte_Object, Size);
		type Byte_Object_Pointer is access all Byte_Object_Record;

		Ptr: Memory_Element_Pointer;
		for Ptr'Address use Result'Address;
		pragma Import (Ada, Ptr);
		Obj_Ptr: Byte_Object_Pointer;
		for Obj_Ptr'Address use Result'Address;
		pragma Import (Ada, Obj_Ptr);
	begin
		Allocate_Bytes (Interp, Memory_Size'(Byte_Object_Record'Max_Size_In_Storage_Elements), Ptr);
		Obj_Ptr.all := (
			Kind => Byte_Object,
			Size => Size,
			Flags => 0,
			Scode => 0,
			Tag => Unknown_Object,
			Byte_Slot => (others => 0)
		);
	end Allocate_Byte_Object;

	----------------------------------------------------------------------------------

	function Is_Cons (Source: in Object_Pointer) return Standard.Boolean is
		pragma Inline (Is_Cons);
	begin
		return Is_Normal_Pointer (Source) and then 
		       Source.Tag = Cons_Object;
	end Is_Cons;

	function Get_Car (Source: in Object_Pointer) return Object_Pointer is
		pragma Inline (Get_Car);
	begin
		pragma Assert (Is_Cons (Source));
		pragma Assert (Source.Size = Cons_Object_Size);
		return Source.Pointer_Slot(Cons_Car_Index);
	end Get_Car;

	procedure Set_Car (Source: in out Object_Pointer;
	                   Value:  in     Object_Pointer) is
		pragma Inline (Set_Car);
	begin
		pragma Assert (Is_Cons (Source));
		pragma Assert (Source.Size = Cons_Object_Size);
		Source.Pointer_Slot(Cons_Car_Index) := Value;
	end Set_Car;

	function Get_Cdr (Source: in Object_Pointer) return Object_Pointer is
		pragma Inline (Get_Cdr);
	begin
		pragma Assert (Is_Cons (Source));
		pragma Assert (Source.Size = Cons_Object_Size);
		return Source.Pointer_Slot(Cons_Cdr_Index);
	end Get_Cdr;

	procedure Set_Cdr (Source: in out Object_Pointer;
	                   Value:  in     Object_Pointer) is
		pragma Inline (Set_Cdr);
	begin
		pragma Assert (Is_Cons (Source));
		pragma Assert (Source.Size = Cons_Object_Size);
		Source.Pointer_Slot(Cons_Cdr_Index) := Value;
	end Set_Cdr;

	procedure Make_Cons (Interp: in out Interpreter_Record;
	                     Car:    in     Object_Pointer;
	                     Cdr:    in     Object_Pointer;
	                     Result: out    Object_Pointer) is
	begin
		Allocate_Pointer_Object (Interp, Cons_Object_Size, Nil_Pointer, Result);
		Result.Pointer_Slot(Cons_Car_Index) := Car;
		Result.Pointer_Slot(Cons_Cdr_Index) := Cdr;
		Result.Tag := Cons_Object;
Print_Object_Pointer ("Make_Cons Result - ", Result);
	end Make_Cons;

	function Make_Cons (Interp: access Interpreter_Record;
	                    Car:    in     Object_Pointer;
	                    Cdr:    in     Object_Pointer) return Object_Pointer is
		Result: Object_Pointer;
	begin
		Make_Cons (Interp.all, Car, Cdr, Result);
		return Result;
	end Make_Cons;

	----------------------------------------------------------------------------------

	procedure Make_String (Interp: in out Interpreter_Record;
	                       Source: in     Object_String;
	                       Result: out    Object_Pointer) is
	begin
		Allocate_Character_Object (Interp, Source, Result);
		Result.Tag := String_Object;
Print_Object_Pointer ("Make_String Result - " & Source, Result);
	end Make_String;

	function Is_Symbol (Source: in Object_Pointer) return Standard.Boolean is
		pragma Inline (Is_Symbol);
	begin
		return Is_Normal_Pointer (Source) and then 
		       Source.Tag = Symbol_Object;
	end Is_Symbol;

	procedure Make_Symbol (Interp: in out Interpreter_Record;
	                       Source: in     Object_String;
	                       Result: out    Object_Pointer) is
		Cons: Object_Pointer;
	begin
		-- TODO: the current linked list implementation isn't efficient.
		--       change the symbol table to a hashable table.

		-- Find an existing symbol in the symbol table.	
		Cons := Interp.Symbol_Table;
		while Cons /= Nil_Pointer loop
			pragma Assert (Is_Normal_Pointer(Cons) and then Cons.Tag = Cons_Object);	

			declare
				Car: Object_Pointer renames Cons.Pointer_Slot(Cons_Car_Index);
				Cdr: Object_Pointer renames Cons.Pointer_Slot(Cons_Cdr_Index);
			begin
--Text_IO.Put_Line (Car.Kind'Img & Car.Tag'Img & Object_Word'Image(Pointer_To_Word(Car)));
				pragma Assert (Car.Tag = Symbol_Object);

				if Match (Car, Source) then
					Result := Car;
Print_Object_Pointer ("Make_Symbol Result (Existing) - " & Source, Result);
					return;
				end if;

				Cons := Cdr;	
			end;
		end loop;

Text_IO.Put_Line ("Creating a symbol .. " & Source);
		-- Create a symbol object
		Allocate_Character_Object (Interp, Source, Result);
		Result.Tag := Symbol_Object;

-- TODO: ensure that Result is not reclaimed by GC.

		-- Link the symbol to the symbol table. 
		Make_Cons (Interp, Result, Interp.Symbol_Table, Interp.Symbol_Table);
Print_Object_Pointer ("Make_Symbol Result - " & Source, Result);
	end Make_Symbol;

	function Make_Symbol (Interp: access Interpreter_Record;
	                      Source: in     Object_String) return Object_Pointer is
		Result: Object_Pointer;
	begin
		Make_Symbol (Interp.all, Source, Result);
		return Result;
	end Make_Symbol;

	----------------------------------------------------------------------------------

	-- TODO: change environment implementation to a table from a list
	procedure Add_To_Environment (Interp: in out Interpreter_Record;
	                              Envir:  in out Object_Pointer;
	                              Key:    in     Object_Pointer;
	                              Value:  in     Object_Pointer) is
		-- This performs no duplicate key check.
		-- TODO: make environemnt a table instead of a list.
		Pair: Object_Pointer;
	begin
		pragma Assert (Is_Symbol(Key));

		-- TODO: make temporaries GC-aware
		Pair := Make_Cons (Interp.Self, Key, Value);
		Envir := Make_Cons (Interp.Self, Pair, Envir);
	end Add_To_Environment;

	function Get_Environment_Cons (Interp: access Interpreter_Record;
	                               Envir:  in     Object_Pointer;
	                               Key:    in     Object_Pointer) return Object_Pointer is
		Ptr: Object_Pointer := Envir;
		Cons: Object_Pointer;
	begin
Print_Object_Pointer ("Get_Environment Key => ", Key);
		while Ptr /= Nil_Pointer loop
			pragma Assert (Is_Cons(Ptr));
			
			Cons := Get_Car(Ptr);
			pragma Assert (Is_Cons(Cons));
			if Get_Car(Cons) = Key then
				return Cons;
			end if;

			Ptr := Get_Cdr(Ptr);
		end loop;	

		return null; -- not found. note that it's not Nil_Pointer.
	end Get_Environment_Cons;

	function Get_Environment (Interp: access Interpreter_Record;
	                          Envir:  in     Object_Pointer;
	                          Key:    in     Object_Pointer) return Object_Pointer is
		Cons: Object_Pointer;
	begin
		Cons := Get_Environment_Cons(Interp, Envir, Key);
		if Cons = null then
			return null;
		end if;
		return Get_Cdr(Cons);
	end Get_Environment;

	procedure Set_Environment (Interp: in out Interpreter_Record;
	                           Envir:  in out Object_Pointer;
	                           Key:    in     Object_Pointer;
	                           Value:  in     Object_Pointer) is
		Cons: Object_Pointer;
	begin
		Cons := Get_Environment_Cons (Interp.Self, Envir, Key);
		if Cons = null then
			-- add a new pair
			Add_To_Environment (Interp, Envir, Key, Value);
		else
			-- overwrite an existing pair
			Set_Cdr (Cons, Value);		
		end if;
	end Set_Environment;

	----------------------------------------------------------------------------------

	function Is_Syntax (Source: in Object_Pointer) return Standard.Boolean is
	begin
		return Is_Symbol(Source) and then (Source.Flags and Syntax_Object) /= 0;
	end Is_Syntax;

	procedure Make_Syntax (Interp: in out Interpreter_Record;
	                       Opcode: in     Syntax_Code;
	                       Name:   in     Object_String;
	                       Result: out    Object_Pointer) is
	begin
		Make_Symbol (Interp, Name, Result);
		Result.Flags := Result.Flags or Syntax_Object;
		Result.Scode := Opcode;
Text_IO.Put ("Creating Syntax Symbol ");
Put_String (To_Thin_String_Pointer (Result));
	end Make_Syntax;

	procedure Make_Procedure (Interp: in out Interpreter_Record;
	                          Opcode: in     Procedure_Code;
	                          Name:   in     Object_String;
	                          Result: out    Object_Pointer) is
		-- this procedure is for internal use only
		Symbol: Object_Pointer;
		Proc:   Object_Pointer;
	begin
-- TODO: make temporaries GC-aware
		-- Make a symbol for the procedure
		Make_Symbol (Interp, Name, Symbol);

		-- Make the actual procedure object
		Allocate_Pointer_Object (Interp, Procedure_Object_Size, Nil_Pointer, Proc);
		Proc.Tag := Procedure_Object;
		Proc.Pointer_Slot(Procedure_Opcode_Index) := Integer_To_Pointer(Opcode);

		-- Link it to the top environement
		pragma Assert (Get_Environment (Interp.Self, Interp.Root_Environment, Symbol) = null);
		Set_Environment (Interp, Interp.Root_Environment, Symbol, Proc);

		-- Set the procudure to the result.
		Result := Proc;
	end Make_Procedure;

	
	----------------------------------------------------------------------------------

	function Is_Frame (Source: in Object_Pointer) return Standard.Boolean is
		pragma Inline (Is_Frame);
	begin
		return Is_Normal_Pointer (Source) and then 
		       Source.Tag = Frame_Object;
	end Is_Frame;

	procedure Make_Frame (Interp:  in out Interpreter_Record;
	                      Stack:   in     Object_Pointer; -- current stack pointer
	                      Opcode:  in     Object_Pointer;
	                      Operand: in     Object_Pointer;
	                      Envir:   in     Object_Pointer;
	                      Result:  out    Object_Pointer) is
	begin
		Allocate_Pointer_Object (Interp, Frame_Object_Size, Nil_Pointer, Result);
		Result.Tag := Frame_Object;
		Result.Pointer_Slot(Frame_Stack_Index) := Stack;
		Result.Pointer_Slot(Frame_Opcode_Index) := Opcode;
		Result.Pointer_Slot(Frame_Operand_Index) := Operand;
		Result.Pointer_Slot(Frame_Environment_Index) := Envir;
--Print_Object_Pointer ("Make_Frame Result - ", Result);
	end Make_Frame;

	function Make_Frame (Interp: access Interpreter_Record;
	                     Stack:   in     Object_Pointer;
	                     Opcode:  in     Object_Pointer;
	                     Operand: in     Object_Pointer;
	                     Envir:   in     Object_Pointer) return Object_Pointer is
		Frame: Object_Pointer;
	begin
		Make_Frame (Interp.all, Stack, Opcode, Operand, Envir, Frame);
		return Frame;
	end Make_Frame;

	function Get_Frame_Return (Frame: in Object_Pointer) return Object_Pointer is
		pragma Inline (Get_Frame_Return);
	begin
		pragma Assert (Is_Frame(Frame));
		return Frame.Pointer_Slot(Frame_Return_Index);
	end Get_Frame_Return;

	procedure Set_Frame_Return (Frame: in out Object_Pointer;
	                            Value: in     Object_Pointer) is
		pragma Inline (Set_Frame_Return);
	begin
		pragma Assert (Is_Frame(Frame));
		Frame.Pointer_Slot(Frame_Return_Index) := Value;
	end Set_Frame_Return;

	function Get_Frame_Environment (Frame: in Object_Pointer) return Object_Pointer is
		pragma Inline (Get_Frame_Environment);
	begin
		pragma Assert (Is_Frame(Frame));
		return Frame.Pointer_Slot(Frame_Environment_Index);
	end Get_Frame_Environment;

	function Get_Frame_Opcode (Frame: in Object_Pointer) return Opcode_Type is
		pragma Inline (Get_Frame_Opcode);
	begin
		pragma Assert (Is_Frame(Frame));
		return Pointer_To_Integer(Frame.Pointer_Slot(Frame_Opcode_Index));
	end Get_Frame_Opcode;

	procedure Set_Frame_Opcode (Frame:  in Object_Pointer; 
	                            OpcodE: in Opcode_Type) is
		pragma Inline (Set_Frame_Opcode);
	begin
		pragma Assert (Is_Frame(Frame));
		Frame.Pointer_Slot(Frame_Opcode_Index) := Integer_To_Pointer(Opcode);
	end Set_Frame_Opcode;

	function Get_Frame_Operand (Frame: in Object_Pointer) return Object_Pointer is
		pragma Inline (Get_Frame_Operand);
	begin
		pragma Assert (Is_Frame(Frame));
		return Frame.Pointer_Slot(Frame_Operand_Index);
	end Get_Frame_Operand;

	procedure Set_Frame_Operand (Frame: in out Object_Pointer;
	                            Value: in     Object_Pointer) is
		pragma Inline (Set_Frame_Operand);
	begin
		pragma Assert (Is_Frame(Frame));
		Frame.Pointer_Slot(Frame_Operand_Index) := Value;
	end Set_Frame_Operand;

	----------------------------------------------------------------------------------

	procedure Make_Closure (Interp:  in out Interpreter_Record;
	                        Code:    in     Object_Pointer;
	                        Envir:   in     Object_Pointer;
	                        Result:  out    Object_Pointer) is
	begin
		Allocate_Pointer_Object (Interp, Closure_Object_Size, Nil_Pointer, Result);
		Result.Tag := Closure_Object;
		Result.Pointer_Slot(Closure_Code_Index) := Code;
		Result.Pointer_Slot(Closure_Environment_Index) := Envir;
	end Make_Closure;

	function Make_Closure (Interp:  access Interpreter_Record;
	                       Code:    in     Object_Pointer;
	                       Envir:   in     Object_Pointer) return Object_Pointer is
		Closure: Object_Pointer;
	begin
		Make_Closure (Interp.all, Code, Envir, Closure);
		return Closure;
	end Make_Closure;
	
	----------------------------------------------------------------------------------
	procedure Deinitialize_Heap (Interp: in out Interpreter_Record) is
	begin
		for I in Interp.Heap'Range loop
			if Interp.Heap(I) /= null then
				declare
					subtype Target_Heap_Record is Heap_Record (Interp.Heap(I).Size);
					type Target_Heap_Pointer is access all Target_Heap_Record;
					package Pool is new H2.Pool (Target_Heap_Record, Target_Heap_Pointer, Interp.Storage_Pool);

					Heap: Target_Heap_Pointer;
					for Heap'Address use Interp.Heap(I)'Address;
					pragma Import (Ada, Heap); 
				begin
					Pool.Deallocate (Heap);
				end;
			end if;
		end loop;
	end Deinitialize_Heap;

	procedure Open (Interp:            in out Interpreter_Record;
	                Initial_Heap_Size: in     Memory_Size;
	                Storage_Pool:      in     Storage_Pool_Pointer := null) is

		procedure Initialize_Heap (Size: Memory_Size) is
			subtype Target_Heap_Record is Heap_Record (Size);
			type Target_Heap_Pointer is access all Target_Heap_Record;
			package Pool is new H2.Pool (Target_Heap_Record, Target_Heap_Pointer, Interp.Storage_Pool);
		begin
			for I in Interp.Heap'Range loop
				Interp.Heap(I) := null; -- just in case
			end loop;

			for I in Interp.Heap'Range loop
				declare
					Heap: Target_Heap_Pointer;
					for Heap'Address use Interp.Heap(I)'Address;
					pragma Import (Ada, Heap); 
				begin
					Heap := Pool.Allocate;
				end;
			end loop;
		exception
			when others =>
				Deinitialize_Heap (Interp);
				raise;
		end Initialize_Heap;

		procedure Make_Syntax_Objects is
			Dummy: Object_Pointer;
		begin
			Make_Syntax (Interp, And_Syntax,    "and",    Dummy);
			Make_Syntax (Interp, Begin_Syntax,  "begin",  Dummy);
			Make_Syntax (Interp, Case_Syntax,   "case",   Dummy);
			Make_Syntax (Interp, Cond_Syntax,   "cond",   Dummy);
			Make_Syntax (Interp, Define_Syntax, "define", Dummy);
			Make_Syntax (Interp, If_Syntax,     "if",     Dummy);
			Make_Syntax (Interp, Lambda_Syntax, "lambda", Dummy);
			Make_Syntax (Interp, Let_Syntax,    "let",    Dummy);
			Make_Syntax (Interp, Letast_Syntax, "let*",   Dummy);
			Make_Syntax (Interp, Letrec_Syntax, "letrec", Dummy);
			Make_Syntax (Interp, Or_Syntax,     "or",     Dummy);
			Make_Syntax (Interp, Quote_Syntax,  "quote",  Dummy);
			Make_Syntax (Interp, Set_Syntax,    "set!",   Dummy);
		end Make_Syntax_Objects;

		procedure Make_Procedure_Objects is
			Dummy: Object_Pointer;
		begin
			Make_Procedure (Interp, Car_Procedure,      "car",    Dummy);
			Make_Procedure (Interp, Cdr_Procedure,      "cdr",    Dummy);
			Make_Procedure (Interp, Setcar_Procedure,   "setcar", Dummy);
			Make_Procedure (Interp, Setcdr_Procedure,   "setcdr", Dummy);
			Make_Procedure (Interp, Add_Procedure,      "+",      Dummy);
			Make_Procedure (Interp, Subtract_Procedure, "-",      Dummy);
			Make_Procedure (Interp, Multiply_Procedure, "*",      Dummy);
			Make_Procedure (Interp, Divide_Procedure,   "/",      Dummy);
		end Make_Procedure_Objects;
	begin
		declare
			Aliased_Interp: aliased Interpreter_Record;
			for Aliased_Interp'Address use Interp'Address;
			pragma Import (Ada, Aliased_Interp);
		begin
			-- Store a pointer to the interpreter record itself.
			-- I use this pointer to call functions that accept the "access"
			-- type to work around the ada95 limitation of no "in out" as
			-- a function parameter. Accoring to Ada95 RM (6.2), both a 
			-- non-private limited record type and a private type whose
			-- full type is a by-reference type are by-rereference types.
			-- So i assume that it's safe to create this aliased overlay 
			-- to deceive the compiler. If Interpreter_Record is a tagged
			-- limited record type, this overlay is not needed since the
			-- type is considered aliased. Having this overlay, however,
			-- should be safe for both "tagged" and "non-tagged".
			-- Note: Making it a tagged limit record caused gnat 3.4.6 to
			--       crash with an internal bug report.
			--Interp.Self := Interp'Unchecked_Access; -- if tagged limited
			Interp.Self := Aliased_Interp'Unchecked_Access;
		end;

		Interp.Storage_Pool := Storage_Pool;
		Interp.Root_Table := Nil_Pointer;
		Interp.Symbol_Table := Nil_Pointer;
		Interp.Root_Environment := Nil_Pointer;
		Interp.Environment := Interp.Root_Environment;

		Interp.Line_Pos := Interp.Line'First - 1;
		Interp.Line_Last := Interp.Line'First - 1;

-- TODO: disallow garbage collecion during initialization.
		Initialize_Heap (Initial_Heap_Size);
		Make_Syntax_Objects;
		Make_Procedure_Objects;
	exception
		when others =>
			Deinitialize_Heap (Interp);
	end Open;

	procedure Close (Interp: in out Interpreter_Record) is
	begin
		Deinitialize_Heap (Interp);
	end Close;

	procedure Set_Option (Interp: in out Interpreter_Record;
	                      Option: in     Option_Record) is
	begin
		case Option.Kind  is
		when Trait_Option =>
				Interp.Trait := Option;
		end case;
	end Set_Option;

	procedure Get_Option (Interp: in out Interpreter_Record;
	                      Option: in out Option_Record) is
	begin
		case Option.Kind  is
		when Trait_Option =>
				Option := Interp.Trait;
		end case;
	end Get_Option;

	procedure Read (Interp: in out Interpreter_Record;
	                Result: out    Object_Pointer) is

		EOF_Error: exception;

		function Get_Character return Object_Character is
		begin
			if Interp.Line_Pos >= Interp.Line_Last then
				if Text_IO.End_Of_File then
					raise EOF_Error;
				end if;
				Text_IO.Get_Line (Interp.Line, Interp.Line_Last);
				Interp.Line_Pos := Interp.Line'First - 1;
			end if;

			Interp.Line_Pos := Interp.Line_Pos + 1;
			return Interp.Line(Interp.Line_Pos);
		end Get_Character;

		procedure Skip_Space is
		begin
			null;
		end Skip_Space;
		
		--function Get_Token is
		--begin
		--	null;	
		--end Get_Token;

		procedure Read_Atom (Atom: out Object_Pointer) is
		begin
			null;
		end Read_Atom; 	

		Stack: Object_Pointer;
		Opcode: Object_Integer;
		Operand: Object_Pointer;
	begin
		--Opcode := 1;
		--loop
		--	case Opcode is
		--		when 1 =>
		--end loop;
		null;
	end Read;
	          
	procedure Print (Interp: in out Interpreter_Record; 
	                 Source: in     Object_Pointer) is

		procedure Print_Atom (Atom: in Object_Pointer) is
			Ptr_Type: Object_Pointer_Type;

			procedure Print_Pointee is
				W: Object_Word;
				for W'Address use Atom'Address;
			begin
				case W is
				when Nil_Word =>
					Text_IO.Put ("()");

				when True_Word =>
					Text_IO.Put ("#t");

				when False_Word =>
					Text_IO.Put ("#f");

				when others => 
					case Atom.Tag is
					when Cons_Object =>
						-- Cons_Object must not reach here.
						raise Internal_Error;

					when Symbol_Object =>
						Text_IO.Put (To_String (Atom.Character_Slot));

					when String_Object =>
						Text_IO.Put ("""");	
						Text_IO.Put (To_String (Atom.Character_Slot));
						Text_IO.Put ("""");	

					when Continuation_Object =>
						Text_IO.Put ("#Continuation");

					when Procedure_Object =>
						Text_IO.Put ("#Procedure");

					when Others =>
						if Atom.Kind = Character_Object then
							Text_IO.Put (To_String (Atom.Character_Slot));
						else
							Text_IO.Put ("#NOIMPL#");
						end if;
					end case;
				end case;
			end Print_Pointee;

			procedure Print_Integer is
				 X: constant Object_Integer := Pointer_To_Integer (Atom);
			begin
				Text_IO.Put (Object_Integer'Image (X));
			end Print_Integer;

			procedure Print_Character is
				 X: constant Object_Character := Pointer_To_Character (Atom);
			begin
				Text_IO.Put (Object_Character'Image (X));
			end Print_Character;

			procedure Print_Byte is
				 X: constant Object_Byte := Pointer_To_Byte (Atom);
			begin
				Text_IO.Put (Object_Byte'Image (X));
			end Print_Byte;

		begin
			Ptr_Type := Get_Pointer_Type(Atom);
			case Ptr_Type is
			when Object_Pointer_Type_Pointer =>
				Print_Pointee;

			when Object_Pointer_Type_Integer =>
				Print_Integer;

			when Object_Pointer_Type_Character =>
				Print_Character;

			when Object_Pointer_Type_Byte =>
				Print_Byte;
			end case;
		end Print_Atom;

		procedure Print_Object (Obj: in Object_Pointer) is
			Cons: Object_Pointer;
			Car: Object_Pointer;
			Cdr: Object_Pointer;

		begin
			if Is_Cons (Obj) then
				Cons := Obj;
				Text_IO.Put ("(");

				loop
					Car := Get_Car(Cons);

					if Is_Cons (Car) then
						Print_Object (Car);
					else
						Print_Atom (Car);
					end if;

					Cdr := Get_Cdr(Cons);
					if Is_Cons (Cdr) then
						Text_IO.Put (" ");
						Cons := Cdr;
						exit when Cons = Nil_Pointer;
					else
						if Cdr /= Nil_Pointer then
							Text_IO.Put (" . ");
							Print_Atom (Cdr);
						end if;
						exit;
					end if;
				end loop;

				Text_IO.Put (")");
			else
				Print_Atom (Obj);
			end if;
		end Print_Object;


		Stack: Object_Pointer; -- TODO: make it into the interpreter_Record so that GC can workd
		Opcode: Object_Integer;
		Operand: Object_Pointer;

	begin
		-- TODO: Let Make_Frame use a dedicated stack space that's apart from the heap.
		--       This way, the stack frame doesn't have to be managed by GC.

		Stack := Make_Frame (Interp.Self, Nil_Pointer, Integer_To_Pointer(0), Nil_Pointer, Interp.Root_Environment);  -- just for get_frame_environment...

		Opcode := 1;
		Operand := Source;

		loop
			case Opcode is
			when 1 =>
				if Is_Cons(Operand) then
					-- push cdr
					Stack := Make_Frame (Interp.Self, Stack, Integer_To_Pointer(2), Get_Cdr(Operand), Get_Frame_Environment(Stack)); -- push cdr
					Text_IO.Put ("(");
					Operand := Get_Car(Operand);
					Opcode := 1;
				else
					Print_Atom (Operand);
					if Stack = Nil_Pointer then 
						Opcode := 0; -- stack empty. arrange to exit
						Operand := True_Pointer; -- return value
					else
						Opcode := Pointer_To_Integer(Stack.Pointer_Slot(Frame_Opcode_Index));
						Operand := Stack.Pointer_Slot(Frame_Operand_Index);
						Stack := Stack.Pointer_Slot(Frame_Stack_Index); -- pop
					end if;
				end if;

			when 2 =>

				if Is_Cons(Operand) then
					-- push cdr
					Stack := Make_Frame (Interp.Self, Stack, Integer_To_Pointer(2), Get_Cdr(Operand), Get_Frame_Environment(Stack)); -- push
					Text_IO.Put (" ");
					Operand := Get_Car(Operand); -- car
					Opcode := 1;
				else
					if Operand /= Nil_Pointer then
						-- cdr of the last cons cell is not null.
						Text_IO.Put (" . ");
						Print_Atom (Operand);
					end if;
					Text_IO.Put (")");

					if Stack = Nil_Pointer then
						Opcode := 0; -- stack empty. arrange to exit
					else
						Opcode := Pointer_To_Integer(Stack.Pointer_Slot(Frame_Opcode_Index));
						Operand := Stack.Pointer_Slot(Frame_Operand_Index);
						Stack := Stack.Pointer_Slot(Frame_Stack_Index); -- pop 
					end if;
				end if;

			when others =>
				exit;
			end case;
		end loop;

		--Print_Object (Source);
		Text_IO.New_Line;
	end Print;

	procedure Evaluatex (Interp: in out Interpreter_Record) is
		X: Object_Pointer;
	begin
		--Make_Cons (Interpreter, Nil_Pointer, Nil_Pointer, X);
		--Make_Cons (Interpreter, Nil_Pointer, X, X);
		--Make_Cons (Interpreter, Nil_Pointer, X, X);
		--Make_Cons (Interpreter, Nil_Pointer, X, X);
Make_Symbol (Interp, "lambda", Interp.Root_Table);
--Print_Object_Pointer (">>> Root_Table ...", Interp.Root_Table);

		Collect_Garbage (Interp);

		-- (define x 10)

		X := Make_Cons (
			Interp.Self,
			Make_Symbol (Interp.Self, "define"),
			Make_Cons (
				Interp.Self,
				Make_Symbol (Interp.Self, "x"),
				Make_Cons (
					Interp.Self,
					Integer_To_Pointer (10),
					--Nil_Pointer
					Integer_To_Pointer (10)
				)
			)
		);
		X := Make_Cons (Interp.Self, X, Make_Cons (Interp.Self, X, Integer_To_Pointer(10)));

		--X := Make_Cons (Interp.Self, Nil_Pointer, Make_Cons (Interp.Self, Nil_Pointer, Integer_To_Pointer(TEN)));
		--X := Make_Cons (Interp.Self, Nil_Pointer, Nil_Pointer);
		Read (Interp, X);
		Print (Interp, X);

	end Evaluatex;

procedure Make_Test_Object (Interp: in out Interpreter_Record; Result: out Object_Pointer) is
begin
		--(define x 10)
		--Result := Make_Cons (
		--	Interp.Self,
		--	Make_Symbol (Interp.Self, "define"),
		--	Make_Cons (
		--		Interp.Self,
		--		Make_Symbol (Interp.Self, "x"),
		--		Make_Cons (
		--			Interp.Self,
		--			Integer_To_Pointer (10),
		--			--Nil_Pointer
		--			Integer_To_Pointer (10)
		--		)
		--	)
		--);

		-- (+ 1 2 . 2)
		Result := Make_Cons (
			Interp.Self,
			Make_Symbol (Interp.Self, "+"),
			Make_Cons (
				Interp.Self,
				Integer_To_Pointer (10),
				Make_Cons (
					Interp.Self,
					Integer_To_Pointer (20),
					Integer_To_Pointer (2)
				)
			)
		);
end Make_Test_Object;


	function Pointer_To_Opcode (Pointer: in Object_Pointer) return Opcode_Type is
		pragma Inline (Pointer_To_Opcode);
	begin	
		return Pointer_To_Integer(Pointer);
	end Pointer_To_Opcode;

	function Opcode_To_Pointer (Opcode: in Opcode_Type) return Object_Pointer is
		pragma Inline (Opcode_To_Pointer);
	begin	
		return Integer_To_Pointer(Opcode);
	end Opcode_To_Pointer;

	procedure Evaluate (Interp: in out Interpreter_Record;
	                    Source: in     Object_Pointer;
	                    Result: out    Object_Pointer) is

		procedure Push_Frame (Stack:   in out Object_Pointer;
		                      Opcode:  in     Opcode_Type; 
		                      Operand: in     Object_Pointer;
		                      Envir:   in     Object_Pointer) is
			pragma Inline (Push_Frame);
		begin
			Stack := Make_Frame (Interp.Self, Stack, Opcode_To_Pointer(Opcode), Operand, Envir);
		end Push_Frame;

		--procedure Pop_Frame (Stack:   out Object_Pointer;
		--                     Opcode:  out Opcode_Type;
		--                     Operand: out Object_Pointer) is
		--	pragma Inline (Pop_Frame);
		--begin
		--	pragma Assert (Stack /= Nil_Pointer);
		--	Opcode := Pointer_To_Opcode(Stack.Pointer_Slot(Frame_Opcode_Index));
		--	Operand := Stack.Pointer_Slot(Frame_Operand_Index);
		--	Stack := Stack.Pointer_Slot(Frame_Stack_Index); -- pop 
		--end Pop_Frame;

		procedure Pop_Frame (Stack:   out Object_Pointer) is
			pragma Inline (Pop_Frame);
		begin
			pragma Assert (Stack /= Nil_Pointer);
			Stack := Stack.Pointer_Slot(Frame_Stack_Index); -- pop 
		end Pop_Frame;

		procedure Evaluate_Object (Stack: in out Object_Pointer) is
			pragma Inline (Evaluate_Object);

			Operand: Object_Pointer;
			Operand_Word: Object_Word;
			for Operand_Word'Address use Operand'Address;

			Tmp: Object_Pointer;
		begin
Text_IO.Put_Line ("Evaluate_Object...");
			Operand := Get_Frame_Operand (Stack);

			if Get_Pointer_Type(Operand) /= Object_Pointer_Type_Pointer then
Text_IO.Put_Line ("NON_POINTER...");
				goto Literal;
			end if;

Print_Object_Pointer ("Operand => ", Operand);
			case Operand_Word is
				when Nil_Word | True_Word | False_Word =>
					-- special literal object
Text_IO.Put_Line ("SPECIAL POINTER...");
					goto Literal;

				when others =>
Text_IO.Put_Line ("OTHER BDONE.." & Object_Tag'Image(Operand.Tag));

					case Operand.Tag is
						when Symbol_Object => -- Is_Symbol(Operand)
							Tmp := Get_Environment (Interp.Self, Get_Frame_Environment(Stack), Operand); -- TODO: use current environent
							if Tmp = null then
								-- unbound
								Text_IO.Put_Line ("Unbound symbol....");
							else
								-- symbol found in the environment
Text_IO.Put_Line ("SUMBOL BDONE..");
								Operand := Tmp; 
								goto Literal;  -- In fact, this is not a literal, but can be handled in the same way
							end if;

						when Cons_Object => -- Is_Cons(Operand)
							Tmp := Get_Car(Operand);
							if Is_Syntax(Tmp) then
Text_IO.Put_Line ("SYNTAX ..");
								-- special syntax symbol. normal evaluate rule doesn't 
								-- apply for special syntax objects.
								--Opcode := Syntax_To_Opcode(Operand);
								Set_Frame_Opcode (Stack, Opcode_Evaluate_Syntax);  -- switch to syntax evaluation
							else
Text_IO.Put_Line ("NON_SYNTAX ..");
								declare
									Cdr: Object_Pointer := Get_Cdr(Operand);
								begin
									if Is_Cons(Cdr) then
										Set_Frame_Operand (Stack, Cdr); -- change the operand for the next call
										Push_Frame (Stack, Opcode_Evaluate_Object, Tmp, Get_Frame_Environment(Stack)); 
									else
										if Cdr /= Nil_Pointer then
											Text_IO.Put_Line ("..................FUCKING CDR.....................");
										end if;
										Operand := Get_Frame_Return(Stack);
										Set_Frame_Opcode (Stack, Opcode_Apply); 
									end if;
								end;
							end if;

						when others =>
							-- normal literal object
Text_IO.Put_Line ("nORMAL LITERAL POINTER...");
							goto Literal;
					end case;
			end case;

			return;

		<<Literal>>
			Pop_Frame (Stack); -- done
Print_Object_Pointer ("Return => ", Operand);
			Set_Frame_Return (Stack, Operand);
		end Evaluate_Object;

		procedure Evaluate_Argument (Stack: in out Object_Pointer) is
		begin
			null;
		end Evaluate_Argument;

		procedure Evaluate_Syntax (Stack: in out Object_Pointer) is
			Scode: Syntax_Code;
		begin
			Scode := Get_Car(Get_Frame_Operand(Stack)).Scode;
			case Scode is
				when Define_Syntax =>
					Text_IO.Put_Line ("define syntax");	
				when others =>
					Text_IO.Put_Line ("Unknown syntax");	
			end case;
		end Evaluate_Syntax;

		procedure Evaluate_Procedure (Stack: in out Object_Pointer) is
		begin
			null;
		end Evaluate_Procedure;

		procedure Apply (Stack: in out Object_Pointer) is
		begin
			null;
		end Apply;

		Stack: Object_Pointer; -- TODO: make it into the interpreter_Record so that GC can workd

	begin
		Stack := Nil_Pointer;

		-- Push a pseudo-frame to terminate the evaluation loop
		Push_Frame (Stack, Opcode_Exit, Nil_Pointer, Interp.Root_Environment); 

		-- Push the actual frame for evaluation
		Push_Frame (Stack, Opcode_Evaluate_Object, Source, Interp.Root_Environment);

		loop
			case Get_Frame_Opcode(Stack) is
				when Opcode_Evaluate_Object =>
					Evaluate_Object (Stack);
					
				when Opcode_Evaluate_Argument =>
					Evaluate_Argument (Stack);
	
				when Opcode_Evaluate_Syntax =>
					Evaluate_Syntax (Stack);

				when Opcode_Evaluate_Procedure =>
					Evaluate_Procedure (Stack);

				when Opcode_Apply =>
					Apply (Stack);

				when Opcode_Exit =>
					Result := Get_Frame_Return (Stack);
					Pop_Frame (Stack);
					exit;
			end case;
		end loop;

		-- the stack must be empty when the loop is terminated
		pragma Assert (Stack = Nil_Pointer);

	end Evaluate;

end H2.Scheme;



--(+ (+ 1 2) (+ 1 2))
--push | eval | expr | result |


