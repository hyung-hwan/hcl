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

	----------------------------------------------------------------------------------
	-- COMMON OBJECTS
	----------------------------------------------------------------------------------
	Cons_Object_Size: constant Pointer_Object_Size := 2;
	Cons_Car_Index: constant Pointer_Object_Size := 1;
	Cons_Cdr_Index: constant Pointer_Object_Size := 2;

	Frame_Object_Size: constant Pointer_Object_Size := 3;
	Frame_Stack_Index: constant Pointer_Object_Size := 1;
	Frame_Opcode_Index: constant Pointer_Object_Size := 2;
	Frame_Operand_Index: constant Pointer_Object_Size := 3;

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
	begin
		if Is_Special_Pointer (Source) then
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

	procedure Copy_Object (Source: in     Object_Pointer;
	                       Target: in out Memory_Element_Pointer;
	                       Bytes:  in     Memory_Size) is
		pragma Inline (Copy_Object);
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
	end Copy_Object;

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
					Copy_Object (Object, Ptr, Bytes); -- use this version instead
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

	procedure Make_String (Interp: in out Interpreter_Record;
	                       Source: in     Object_String;
	                       Result: out    Object_Pointer) is
	begin
		Allocate_Character_Object (Interp, Source, Result);
		Result.Tag := String_Object;
Print_Object_Pointer ("Make_String Result - " & Source, Result);
	end Make_String;

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


	procedure Make_Syntax (Interp: in out Interpreter_Record;
	                       Scode:  in     Syntax_Code;
	                       Name:   in     Object_String;
	                       Result: out    Object_Pointer) is
	begin
		Make_Symbol (Interp, Name, Result);
		Result.Flags := Result.Flags or Syntax_Object;
		Result.Scode := Scode;
Text_IO.Put ("Creating Syntax Symbol ");
Put_String (To_Thin_String_Pointer (Result));
	end Make_Syntax;

	procedure Make_Procedure (Interp: in out Interpreter_Record;
	                          Name:   in     Object_String;
	                          Result: out    Object_Pointer) is
	begin
		null;
	end Make_Procedure;

	procedure Make_Frame (Interp:  in out Interpreter_Record;
	                      Stack:   in     Object_Pointer; -- current stack pointer
	                      Opcode:  in     Object_Pointer;
	                      Operand: in     Object_Pointer;
	                      Result:  out    Object_Pointer) is
	begin
		Allocate_Pointer_Object (Interp, Frame_Object_Size, Nil_Pointer, Result);
		Result.Tag := Frame_Object;
		Result.Pointer_Slot(Frame_Stack_Index) := Stack;
		Result.Pointer_Slot(Frame_Opcode_Index) := Opcode;
		Result.Pointer_Slot(Frame_Operand_Index) := Operand;
--Print_Object_Pointer ("Make_Frame Result - ", Result);
	end Make_Frame;

	function Make_Cons (Interp: access Interpreter_Record;
	                    Car:    in     Object_Pointer;
	                    Cdr:    in     Object_Pointer) return Object_Pointer is
		Result: Object_Pointer;
	begin
		Make_Cons (Interp.all, Car, Cdr, Result);
		return Result;
	end Make_Cons;
	                    
	function Make_Symbol (Interp: access Interpreter_Record;
	                      Source: in     Object_String) return Object_Pointer is
		Result: Object_Pointer;
	begin
		Make_Symbol (Interp.all, Source, Result);
		return Result;
	end Make_Symbol;

	function Make_Frame (Interp: access Interpreter_Record;
	                     Stack:   in     Object_Pointer;
	                     Opcode:  in     Object_Pointer;
	                     Operand: in     Object_Pointer) return Object_Pointer is
		Result: Object_Pointer;
	begin
		Make_Frame (Interp.all, Stack, Opcode, Operand, Result);
		return Result;
	end Make_Frame;

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
			Make_Syntax (Interp, AND_SYNTAX,    "and",    Dummy);
			Make_Syntax (Interp, BEGIN_SYNTAX,  "begin",  Dummy);
			Make_Syntax (Interp, CASE_SYNTAX,   "case",   Dummy);
			Make_Syntax (Interp, COND_SYNTAX,   "cond",   Dummy);
			Make_Syntax (Interp, DEFINE_SYNTAX, "define", Dummy);
			Make_Syntax (Interp, IF_SYNTAX,     "if",     Dummy);
			Make_Syntax (Interp, LAMBDA_SYNTAX, "lambda", Dummy);
			Make_Syntax (Interp, LET_SYNTAX,    "let",    Dummy);
			Make_Syntax (Interp, LETAST_SYNTAX, "let*",   Dummy);
			Make_Syntax (Interp, LETREC_SYNTAX, "letrec", Dummy);
			Make_Syntax (Interp, OR_SYNTAX,     "or",     Dummy);
			Make_Syntax (Interp, QUOTE_SYNTAX,  "quote",  Dummy);
			Make_Syntax (Interp, SET_SYNTAX,    "set!",   Dummy);
		end Make_Syntax_Objects;
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
		Interp.Environment := Nil_Pointer;

-- TODO: disallow garbage collecion during initialization.
		Initialize_Heap (Initial_Heap_Size);
		Make_Syntax_Objects;

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
			Ptr_Type := Get_Pointer_Type (Atom);
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
					Car := Cons.Pointer_Slot (Cons_Car_Index);	

					if Is_Cons (Car) then
						Print_Object (Car);
					else
						Print_Atom (Car);
					end if;

					Cdr := Cons.Pointer_Slot (Cons_Cdr_Index);	
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


		Stack: Object_Pointer;
		Opcode: Object_Integer;
		Operand: Object_Pointer;

	begin
		-- TODO: Let Make_Frame use a dedicated stack space that's apart from the heap.
		--       This way, the stack frame doesn't have to be managed by GC.

		Opcode := 1;
		Operand := Source;
		Stack := Nil_Pointer; -- make it to the interpreter so that GC can work

		loop
			case Opcode is
				when 1 =>
					if Is_Cons(Operand) then
						-- push cdr
						Stack := Make_Frame (Interp.Self, Stack, Integer_To_Pointer(2), Operand.Pointer_Slot(Cons_Cdr_Index)); -- push cdr
						Text_IO.Put ("(");
						Operand := Operand.Pointer_Slot(Cons_Car_Index); -- car
						Opcode := 1;
					else
						Print_Atom (Operand);
						if Stack = Nil_Pointer then 
							Opcode := 0;
						else
							Opcode := Pointer_To_Integer(Stack.Pointer_Slot(Frame_Opcode_Index));
							Operand := Stack.Pointer_Slot(Frame_Operand_Index);
							Stack := Stack.Pointer_Slot(Frame_Stack_Index); -- pop
						end if;
					end if;

				when 2 =>

					if Is_Cons(Operand) then
						-- push cdr
						Stack := Make_Frame (Interp.Self, Stack, Integer_To_Pointer(2), Operand.Pointer_Slot(Cons_Cdr_Index)); -- push
						Text_IO.Put (" ");
						Operand := Operand.Pointer_Slot(Cons_Car_Index); -- car
						Opcode := 1;
					else
						if Operand /= Nil_Pointer then
							-- cdr of the last cons cell is not null.
							Text_IO.Put (" . ");
							Print_Atom (Operand);
						end if;
						Text_IO.Put (")");

						if Stack = Nil_Pointer then
							Opcode := 0;
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

	procedure Evaluate (Interp: in out Interpreter_Record) is
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
		Print (Interp, X);

	end Evaluate;


end H2.Scheme;
