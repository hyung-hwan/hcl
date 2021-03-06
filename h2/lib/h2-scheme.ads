---------------------------------------------------------------------
--  #####                                           #     #  #####  
-- #     #  ####  #    # ###### #    # ######       #     # #     # 
-- #       #    # #    # #      ##  ## #            #     #       # 
--  #####  #      ###### #####  # ## # #####  ##### #######  #####  
--       # #      #    # #      #    # #            #     # #       
-- #     # #    # #    # #      #    # #            #     # #       
--  #####   ####  #    # ###### #    # ######       #     # ####### 
--
-- Literal
--  Number: 1, 10
--  String: "hello"
--
-- Environment
--  The environment holds the key/value pairs.
--
-- Procedure
--  Some builtin-procedure objects are registered to the top-level environment
--  upon start-up. You can break the mapping between a name and a procedure
--  as it's in the normal environment.
--
-- Syntax Object
--  Some syntax objects are registered upon start-up. They are handled 
--  very specially when the list containing one of them as the first argument
--  is evaluated.
--
-- Evaluation Rule
--   A literal object evaluates to itself. A Symbol object evaluates to 
--   a value found in the environment. List evaluation is slightly more 
--   complex. Each element of a list is evluated using the standard evaluation
--   rule. The first argument acts as a function and the rest of the arguments
--   are applied to the function. An element must evaluate to a closure to be
--   a function. The syntax object bypasses the normal evaluation rule and is
--   evaluated according to the object-specific rule.
--
---------------------------------------------------------------------

with Ada.Unchecked_Conversion;
with H2.Ascii;

generic
	type Character_Type is (<>);
package H2.Scheme is

	-----------------------------------------------------------------------------
	-- EXCEPTIONS
	-----------------------------------------------------------------------------
	Allocation_Error: exception;
	Size_Error: exception;
	Syntax_Error: exception;
	Evaluation_Error: exception;
	Internal_Error: exception;
	IO_Error: exception;
	Divide_By_Zero_Error: exception;
	Numeric_String_Error: exception;

	type Interpreter_Record is limited private;
	type Interpreter_Pointer is access all Interpreter_Record;

	-- -----------------------------------------------------------------------------
	-- While I could define Heap_Element and Heap_Size to be
	-- the subtype of Object_Byte and Object_Size each, they are not
	-- logically the same thing.
	-- subtype Storage_Element is Object_Byte;
	-- subtype Storage_Count is Object_Size;
	type Heap_Element is mod 2 ** System.Storage_Unit;
	type Heap_Size is range 0 .. (2 ** (System.Word_Size - 1)) - 1;

	-- -----------------------------------------------------------------------

	-- An object pointer takes up as many bytes as a system word.
	Object_Pointer_Bits: constant := System.Word_Size;
	Object_Pointer_Bytes: constant := Object_Pointer_Bits / System.Storage_Unit;

	-- I use the lower 2 bits to indicate the type of an object pointer.
	-- A real object pointer is typically allocated on a word boundary.
	-- As a result, the lower 2 bits should always be 0. Using this
	-- property, I keep some other values at the lower 2 bits to indicate
	-- some other direct values like an integer or a character.
	Object_Pointer_Type_Bits: constant := 2;
	type Object_Pointer_Type is mod 2 ** Object_Pointer_Type_Bits;
	Object_Pointer_Type_Pointer:   constant Object_Pointer_Type := 2#00#;
	Object_Pointer_Type_Integer:   constant Object_Pointer_Type := 2#01#;
	Object_Pointer_Type_Character: constant Object_Pointer_Type := 2#10#;
	Object_Pointer_Type_Byte:      constant Object_Pointer_Type := 2#11#;
	Object_Pointer_Type_Mask:      constant Object_Pointer_Type := 2#11#;

	type Object_Record;
	type Object_Pointer is access all Object_Record;
	for Object_Pointer'Size use Object_Pointer_Bits;
	
	type Object_Bit is mod 2 ** 1;
	--for Object_Bit'Size use 1;
	
	-- Object_Word is a numeric type as large as Object_Poinetr;
	type Object_Word is mod 2 ** Object_Pointer_Bits;
	for Object_Word'Size use Object_Pointer_Bits;

	type Object_Half_Word is mod 2 ** (Object_Pointer_Bits / 2);
	for Object_Half_Word'Size use (Object_Pointer_Bits / 2);

	-- Object_Signed_Word is the signed version of Object_Word.
	-- Note Object_Word is a modular type while this is a signed range.
	type Object_Signed_Word is range -(2 ** (Object_Pointer_Bits - 1)) ..
	                                 +(2 ** (Object_Pointer_Bits - 1)) - 1;
	for Object_Signed_Word'Size use Object_Pointer_Bits;

	-- The actual number of bits for an integer the number of bits excluding
	-- the pointer type bits.
	Object_Integer_Bits: constant := Object_Pointer_Bits - Object_Pointer_Type_Bits;

	-- Object_Integer represents the range of SmallInteger.
	-- It defines an integer that can be held in the upper Object_Integer_Bits
	-- bits. Conversion functions betwen Object_Integer and Object_Pointer
	-- use the highest 1 bit to represent the sign after shifting. So, the 
	-- range is shrunk further by 1 bit, resulting in -2 in the foluma below.
	-- -----------------------------------------------------------------------
	--   type Object_Integer is range -(2 ** (Object_Integer_Bits - 2)) ..
	--                                +(2 ** (Object_Integer_Bits - 2)) - 1;
	-- -----------------------------------------------------------------------
	-- If i don't include -(2 ** (Object_Integer_Bits - 1)) into the range, 
	-- it can be extended to a larger range. That's because the excluded number
	-- conflicts with the highest sign bit during the conversion process.
	-- -----------------------------------------------------------------------
	type Object_Integer is range -(2 ** (Object_Integer_Bits - 1)) + 1 ..
	                             +(2 ** (Object_Integer_Bits - 1)) - 1;
	-- -----------------------------------------------------------------------
	-- What is a better choice? TODO: decide what to use
	-- -----------------------------------------------------------------------

	-- Let Object_Integer take up as large a space as Object_Pointer
	-- despite the actual range of Object_Integer.
	for Object_Integer'Size use Object_Pointer_Bits;

	-- The Object_Size type defines the size of object payload.
	-- It is the number of payload items for each object kind.
	--type Object_Size is new Object_Word range 0 .. (2 ** (System.Word_Size - 1)) - 1;
	--type Object_Size is new Object_Word range 0 .. 1000;
	--type Object_Size is new Object_Word;
	type Object_Size is new System_Size;
	for Object_Size'Size use Object_Pointer_Bits; -- for GC
	subtype Object_Index is Object_Size range Object_Size(System_Index'First) .. Object_Size(System_Index'Last);

	type Object_Byte is mod 2 ** System.Storage_Unit;
	for Object_Byte'Size use System.Storage_Unit;

	subtype Object_Character is Character_Type;

	type Object_Pointer_Array is array(Object_Index range <>) of Object_Pointer;
	type Object_Character_Array is array(Object_Index range <>) of Object_Character;
	type Object_Byte_Array is array(Object_Index range <>) of Object_Byte;
	type Object_Word_Array is array(Object_Index range <>) of Object_Word;
	type Object_Half_Word_Array is array(Object_Index range <>) of Object_Half_Word;

	type Object_Character_Array_Pointer is access all Object_Character_Array;
	for Object_Character_Array_Pointer'Size use Object_Pointer_Bits;
	type Constant_Object_Character_Array_Pointer is access constant Object_Character_Array;
	for Constant_Object_Character_Array_Pointer'Size use Object_Pointer_Bits;
	subtype Thin_Object_Character_Array is Object_Character_Array(Object_Index'Range);
	type Thin_Object_Character_Array_Pointer is access all Thin_Object_Character_Array;
	for Thin_Object_Character_Array_Pointer'Size use Object_Pointer_Bits;

	type Object_Kind is (
		Moved_Object, -- internal use only
		Pointer_Object,
		Character_Object,
		Byte_Object,
		Word_Object,
		Half_Word_Object
	);
	for Object_Kind use (
		Moved_Object => 0,
		Pointer_Object => 1,
		Character_Object => 2,
		Byte_Object => 3,
		Word_Object => 4,
		Half_Word_Object => 5
	);

	-- -----------------------------------------------------------------------

	-- Object_Record contains the Flags field that can be used
	-- freely for management purpose. The Object_Flags type
	-- represents the value that can be stored in this field.
	type Object_Flags is mod 2 ** 4;
	Syntax_Object: constant Object_Flags := Object_Flags'(2#0001#); 
	Syntax_Checked: constant Object_Flags := Object_Flags'(2#0010#);
	Argument_Checked: constant Object_Flags := Object_Flags'(2#0100#);

	type Object_Tag is (
		Unknown_Object, 
		Cons_Object,
		String_Object,
		Symbol_Object,
		Array_Object,
		Bigint_Object,
		Procedure_Object,
		Closure_Object,
		Continuation_Object,
		Frame_Object
	);

	type Syntax_Code is (
		And_Syntax,
		Begin_Syntax,
		Case_Syntax,
		Cond_Syntax,
		Define_Syntax,
		Do_Syntax,
		If_Syntax,
		Lambda_Syntax,
		Let_Syntax,
		Letast_Syntax,
		Letrec_Syntax,
		Or_Syntax,
		Quasiquote_Syntax,
		Quote_Syntax,
		Set_Syntax
	);

	type Object_Sign is (
		Positive_Sign,
		Negative_Sign
	);

	type Object_Record(Kind: Object_Kind; Size: Object_Size) is record
		Flags: Object_Flags := 0;
		Tag: Object_Tag := Unknown_Object;
		Scode: Syntax_Code := Syntax_Code'Val(0); -- Used if Flags contain Syntax_Object
		Sign: Object_Sign := Positive_Sign; -- Used for Bigint_Object

		-- Object payload:
		--  I assume that the smallest payload is able to hold an 
		--  object pointer by specifying the alignement attribute 
		--  to Object_Pointer_Bytes and checking the minimum allocation
		--  size in Allocate_Bytes_In_Heap().
		case Kind is
			when Moved_Object =>
				New_Pointer: Object_Pointer := null;

			when Pointer_Object =>
				Pointer_Slot: Object_Pointer_Array(1 .. Size) := (others => null);

			when Character_Object =>
				Character_Slot: Object_Character_Array(1 .. Size) := (others => Object_Character'First);
				-- The character terminator is to ease integration with 
				-- other languages using a terminating null.
				-- TODO: can this guarantee terminating NULL? is this 
				--       terminator guaranteed to be placed after the 
				--       character_slot without any gaps in between 
				--       under the current alignement condition?
				Character_Terminator: Object_Character := Object_Character'First; 

			when Byte_Object =>
				Byte_Slot: Object_Byte_Array(1 .. Size) := (others => 0);

			when Word_Object =>
				Word_Slot: Object_Word_Array(1 .. Size) := (others => 0);

			when Half_Word_Object =>
				Half_Word_Slot: Object_Half_Word_Array(1 .. Size) := (others => 0);
		end case;
	end record;
	for Object_Record use record
		Kind  at 0 range 0 .. 2; -- 3 bits (0 .. 7)
		Flags at 0 range 3 .. 6; -- 4 bits 
		Tag   at 0 range 7 .. 10; -- 4 bits (0 .. 15)
		Scode at 0 range 11 .. 14; -- 4 bits (0 .. 15)
		Sign  at 0 range 15 .. 15; -- 1 bit (0 or 1)
		-- there are still some space unused in the first word. What can i do?
	end record;
	for Object_Record'Alignment use Object_Pointer_Bytes;

	-- the following 3 size types are defined for limiting the object size range.
	subtype Empty_Object_Record is Object_Record (Byte_Object, 0);

	-- the number of bytes in an object header. this is fixed in size
	Object_Header_Bytes: constant Object_Size := Empty_Object_Record'Max_Size_In_Storage_Elements;
	-- the largest number of bytes that an object can hold after the header
	Object_Payload_Max_Bytes: constant Object_Size := Object_Size'Last - Object_Header_Bytes;

	-- the following types are defined to set the byte range of the object data.
	-- the upper bound is set to the maximum that don't cause overflow in calcuating the size in bits.
	-- the compiler doesn't seem to be able to return 'Size or 'Max_Size_In_Storage_Elements properly
	-- when the number of bits calculated overflows.
	subtype Byte_Object_Size is Object_Size range
		Object_Size'First .. (Object_Payload_Max_Bytes / (Object_Byte'Max_Size_In_Storage_Elements * System.Storage_Unit));
	subtype Character_Object_Size is Object_Size range
		Object_Size'First .. (Object_Payload_Max_Bytes / (Object_Character'Max_Size_In_Storage_Elements * System.Storage_Unit));
	subtype Pointer_Object_Size is Object_Size range
		Object_Size'First .. (Object_Payload_Max_Bytes / (Object_Pointer'Max_Size_In_Storage_Elements * System.Storage_Unit));
	subtype Word_Object_Size is Object_Size range
		Object_Size'First .. (Object_Payload_Max_Bytes / (Object_Word'Max_Size_In_Storage_Elements * System.Storage_Unit));
	subtype Half_Word_Object_Size is Object_Size range
		Object_Size'First .. (Object_Payload_Max_Bytes / (Object_Half_Word'Max_Size_In_Storage_Elements * System.Storage_Unit));

	-- -----------------------------------------------------------------------------
	-- Various pointer classification and conversion procedures
	-- -----------------------------------------------------------------------------
	function Is_Pointer (Pointer: in Object_Pointer) return Standard.Boolean;
	function Is_Special_Pointer (Pointer: in Object_Pointer) return Standard.Boolean;
	function Is_Normal_Pointer (Pointer: in Object_Pointer) return Standard.Boolean;
	function Is_Integer (Pointer: in Object_Pointer) return Standard.Boolean;
	function Is_Character (Pointer: in Object_Pointer) return Standard.Boolean;
	function Is_Byte (Pointer: in Object_Pointer) return Standard.Boolean;

	function Integer_To_Pointer (Value: in Object_Integer) return Object_Pointer;
	function Character_To_Pointer (Value: in Object_Character) return Object_Pointer;
	function Byte_To_Pointer (Value: in Object_Byte) return Object_Pointer;

	function Pointer_To_Integer (Pointer: in Object_Pointer) return Object_Integer;
	function Pointer_To_Character (Pointer: in Object_Pointer) return Object_Character;
	function Pointer_To_Byte (Pointer: in Object_Pointer) return Object_Byte;

	pragma Inline (Is_Special_Pointer);
	pragma Inline (Is_Pointer);
	pragma Inline (Is_Integer);
	pragma Inline (Is_Character);
	pragma Inline (Integer_To_Pointer);
	pragma Inline (Character_To_Pointer);
	pragma Inline (Byte_To_Pointer);

	-- this caused GNAT 4.6.3 to end up with an internal bug when used in  the generirc Plain_Integer_Op function.
	-- let me comment it out temporarily.
	--pragma Inline (Pointer_To_Integer); 

	pragma Inline (Pointer_To_Character);
	pragma Inline (Pointer_To_Byte);

	-- -----------------------------------------------------------------------------

	function Is_Cons (Source: in Object_Pointer) return Standard.Boolean;
	function Is_Bigint (Source: in Object_Pointer) return Standard.Boolean;

	pragma Inline (Is_Cons);
	pragma Inline (Is_Bigint);
	-- -----------------------------------------------------------------------------


	type Stream_Record is abstract tagged limited null record;

	procedure Open (Stream: in out Stream_Record) is abstract;

	procedure Close (Stream: in out Stream_Record) is abstract;

	procedure Read (Stream: in out Stream_Record;
	                Data:   out    Object_Character_Array;
	                Last:   out    Object_Size) is abstract;

	procedure Write (Stream: in out Stream_Record;
	                 Data:   out    Object_Character_Array;
	                 Last:   out    Object_Size) is abstract;

	type Stream_Pointer is access all Stream_Record'Class;

	type Stream_Allocator is access 
		procedure (Interp: in out Interpreter_Record; 
		           Name:   access Object_Character_Array;
		           Result: out    Stream_Pointer);

	type Stream_Deallocator is access 
		procedure (Interp: in out Interpreter_Record; 
		           Source: in out Stream_Pointer);


	type IO_Flags is mod 2 ** 4;
	IO_End_Reached: constant IO_Flags := IO_Flags'(2#0001#); 
	IO_Error_Occurred: constant IO_Flags := IO_Flags'(2#0001#); 

	type IO_Record;
	type IO_Pointer is access all IO_Record;

	type Character_Kind is (End_Character, Normal_Character, Error_Character);
	type IO_Character_Record is record
		Kind: Character_Kind := End_Character;
		Value: Object_Character := Object_Character'First;
	end record;
	--pragma Pack (IO_Character_Record);

	type IO_Record is record
	--type IO_Record is limited record
		Stream: Stream_Pointer := null;
		--Data: Object_Character_Array(1..2048) := (others => Object_Character'First);
		Data: Object_Character_Array(1..5) := (others => Object_Character'First);
		Last: Object_Size := 0;
		Pos: Object_Size := 0;
		Flags: IO_Flags := 0; -- EOF, ERROR
		Next: IO_Pointer := null;
		Iochar: IO_Character_Record; -- the last character read.	
	end record;

	-- -----------------------------------------------------------------------------

	type Trait_Mask is mod 2 ** System.Word_Size;
	No_Garbage_Collection: constant Trait_Mask := 2#0000_0000_0000_0001#;
	No_Optimization:       constant Trait_Mask := 2#0000_0000_0000_0010#;

	type Option_Kind is (Trait_Option, Stream_Option);
	type Option_Record (Kind: Option_Kind) is record
		case Kind is
			when Trait_Option =>
				Trait_Bits: Trait_Mask := 0;

			when Stream_Option =>
				Allocate: Stream_Allocator := null;
				Deallocate: Stream_Deallocator := null;
		end case;
	end record;  

	-- -----------------------------------------------------------------------------

	-- The nil/true/false object are represented by special pointer values.
	-- The special values are defined under the assumption that actual objects
	-- are never allocated on one of these addresses. Addresses of 4, 8, 12 are
	-- very low, making the assumption pretty safe. I don't use 0 for Nil_Word
	-- as it may conflict with ada's null.
	Nil_Word: constant Object_Word := 2#0100#; -- 4
	--Nil_Pointer: constant Object_Pointer;
	--for Nil_Pointer'Address use Nil_Word'Address;
	--pragma Import (Ada, Nil_Pointer);

	True_Word: constant Object_Word := 2#1000#; -- 8
	--True_Pointer: constant Object_Pointer;
	--for True_Pointer'Address use True_Word'Address;
	--pragma Import (Ada, True_Pointer);

	False_Word: constant Object_Word := 2#1100#; -- 12 
	--False_Pointer: constant Object_Pointer;
	--for False_Pointer'Address use False_Word'Address;
	--pragma Import (Ada, False_Pointer);

	function Object_Word_To_Pointer is new Ada.Unchecked_Conversion (Object_Word, Object_Pointer);
	function Object_Pointer_To_Word is new Ada.Unchecked_Conversion (Object_Pointer, Object_Word);
	Nil_Pointer: constant Object_Pointer := Object_Word_To_Pointer(Nil_Word);
	True_Pointer: constant Object_Pointer := Object_Word_To_Pointer(True_Word);
	False_Pointer: constant Object_Pointer := Object_Word_To_Pointer(False_Word);

	-- -----------------------------------------------------------------------------

	procedure Open (Interp:           in out Interpreter_Record;
	                Initial_Heap_Size:in     Heap_Size;
	                Storage_Pool:     in     Storage_Pool_Pointer := null);

	procedure Close (Interp: in out Interpreter_Record);

	function Get_Storage_Pool (Interp: in Interpreter_Record) return Storage_Pool_Pointer;

	procedure Set_Option (Interp: in out Interpreter_Record;
	                      Option: in     Option_Record);

	procedure Get_Option (Interp: in out Interpreter_Record;
	                      Option: in out Option_Record);
	
	procedure Set_Input_Stream  (Interp: in out Interpreter_Record;
	                             Stream: in out Stream_Record'Class);

	-- Source must be open for Read() to work.
	--procedure Read (Interp: in out Interpreter_Record;
	--                Result: out    Object_Pointer);

	procedure Evaluate (Interp: in out Interpreter_Record;
	                    Source: in     Object_Pointer;
	                    Result: out    Object_Pointer);

	procedure Print (Interp: in out Interpreter_Record;
	                 Source: in     Object_Pointer);

	procedure Run_Loop (Interp: in out Interpreter_Record;
	                    Result: out    Object_Pointer);


	procedure Collect_Garbage (Interp: in out Interpreter_Record);

	procedure Push_Top (Interp: in out Interpreter_Record;
	                    Source: access Object_Pointer);

	procedure Pop_Tops (Interp: in out Interpreter_Record;
	                    Count:  in     Object_Size);

	function Make_String (Interp: access  Interpreter_Record;
	                      Source: in      Object_Character_Array;
	                      Invert: in      Standard.Boolean := Standard.False) return Object_Pointer;

	function Make_Symbol (Interp: access  Interpreter_Record;
	                      Source: in      Object_Character_Array;
	                      Invert: in      Standard.Boolean := Standard.False) return Object_Pointer;

	function Make_Bigint (Interp: access Interpreter_Record;
	                      Size:   in     Half_Word_Object_Size) return Object_Pointer;

	function Make_Bigint (Interp: access Interpreter_Record;
	                      Value:  in     Object_Integer) return Object_Pointer;
	-- -----------------------------------------------------------------------------

private
	package Ch is new H2.Ascii(Object_Character, Object_Character);
	package Ch_Code renames Ch.Code;
	package Ch_Val renames Ch.Slim; -- Ch.Slim and Ch.Wide are the same as both are Object_Charater above.

	type Heap_Element_Array is array(Heap_Size range <>) of aliased Heap_Element;

	type Heap_Record(Size: Heap_Size) is record
		Space: Heap_Element_Array(1..Size) := (others => 0);
		Bound: Heap_Size := 0;
	end record;
	for Heap_Record'Alignment use Object_Pointer_Bytes;
	type Heap_Pointer is access all Heap_Record;

	type Heap_Number is mod 2 ** 1;
	type Heap_Pointer_Array is array(Heap_Number'First .. Heap_Number'Last) of Heap_Pointer;

	type Buffer_Record is record
		Ptr: Thin_Object_Character_Array_Pointer := null;
		Len: Object_Size := 0;
		Last: Object_Size := 0;
	end record;

	type Token_Kind is (End_Token,
	                    Identifier_Token,
	                    Left_Parenthesis_Token,
	                    Right_Parenthesis_Token,
	                    Period_Token,
	                    Single_Quote_Token,
	                    True_Token,
	                    False_Token,
	                    Character_Token,
	                    String_Token,
	                    Integer_Token
	);

	type Token_Record is record
		Kind: Token_Kind;
		Value: Buffer_Record;
	end record;
	
	-- Temporary Object Pointer to preserve during GC
	type Top_Datum is access all Object_Pointer;
	type Top_Array is array(Object_Index range<>) of Top_Datum;
	type Top_Record is record
		Last: Object_Size := 0;
		Data: Top_Array(1 .. 100) := (others => null);
	end record;

	type Interpreter_State is mod 2 ** 4;
	Force_Syntax_Check: constant Interpreter_State := Interpreter_State'(2#0001#); 

	--type Interpreter_Record is tagged limited record
	type Interpreter_Record is limited record
		Self: Interpreter_Pointer := Interpreter_Record'Unchecked_Access; -- Current instance's pointer
		State: Interpreter_State := 0; -- Internal housekeeping state

		Storage_Pool: Storage_Pool_Pointer := null;
		Trait: Option_Record(Trait_Option);
		Stream: Option_Record(Stream_Option);

		Heap: Heap_Pointer_Array := (others => null);
		Current_Heap: Heap_Number := Heap_Number'First;

		Symbol_Table: Object_Pointer := Nil_Pointer;
		Root_Environment: Object_Pointer := Nil_Pointer;
		Root_Frame: Object_Pointer := Nil_Pointer;
		Stack: Object_Pointer := Nil_Pointer;

		Arrow_Symbol:      Object_Pointer := Nil_Pointer;
		Else_Symbol:       Object_Pointer := Nil_Pointer;
		Quasiquote_Symbol: Object_Pointer := Nil_Pointer;
		Quote_Symbol:      Object_Pointer := Nil_Pointer;
		
		Top: Top_Record; -- temporary object pointers

		Base_Input: aliased IO_Record;
		Input: IO_Pointer := null;

		Token: Token_Record;
		LC_Unfetched: Standard.Boolean := Standard.False;
	end record;

	package Token is

		procedure Purge (Interp: in out Interpreter_Record);
		pragma Inline (Purge);

		procedure Set (Interp: in out Interpreter_Record;
		               Kind:   in     Token_Kind);

		procedure Set (Interp:  in out Interpreter_Record;
		               Kind:    in     Token_Kind;
		               Value:   in     Object_Character);

		procedure Set (Interp:  in out Interpreter_Record;
		               Kind:    in     Token_Kind;
		               Value:   in     Object_Character_Array);
	
		procedure Append_String (Interp: in out Interpreter_Record;
		                         Value:  in     Object_Character_Array);
		pragma Inline (Append_String);
	
		procedure Append_Character (Interp: in out Interpreter_Record;
		                            Value:  in     Object_Character);
		pragma Inline (Append_Character);

	end Token;

	package Bigint is
		
		
		subtype Object_Radix is Object_Word range 2 .. 36;
		
		function Get_Low (W: Object_Word) return Object_Half_Word;
		function Get_High (W: Object_Word) return Object_Half_Word;
		function Make_Word (L: Object_Half_Word;
		                    H: Object_Half_Word) return Object_Word;

		pragma Inline (Get_High);
		pragma Inline (Get_Low);
		pragma Inline (Make_Word);

		procedure Add (Interp: in out Interpreter_Record;
		               X:      in     Object_Pointer;
		               Y:      in     Object_Pointer;
		               Z:      out    Object_Pointer);

		procedure Subtract (Interp: in out Interpreter_Record;
		                    X:      in     Object_Pointer;
		                    Y:      in     Object_Pointer;
		                    Z:      out    Object_Pointer);

		procedure Multiply (Interp: in out Interpreter_Record;
		                    X:      in     Object_Pointer;
		                    Y:      in     Object_Pointer;
		                    Z:      out    Object_Pointer);

		procedure Divide (Interp: in out Interpreter_Record;
		                  X:      in     Object_Pointer;
		                  Y:      in     Object_Pointer;
		                  Q:      out    Object_Pointer;
		                  R:      out    Object_Pointer);
		                  
		function Compare (Interp: access Interpreter_Record;
		                  X:      in     Object_Pointer;
		                  Y:      in     Object_Pointer) return Standard.Integer;

		function To_String (Interp: access Interpreter_Record;
		                    X:      in     Object_Pointer;
		                    Radix:  in     Object_Radix) return Object_Pointer;

		function From_String (Interp: access Interpreter_Record;
		                      X:      in     Object_Character_Array;
		                      Radix:  in     Object_Radix) return Object_Pointer;

		procedure Initialize;
	end Bigint;

end H2.Scheme;
