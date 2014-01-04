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

with System;
with System.Storage_Pools;
with Ada.Unchecked_Conversion;

package H2.Scheme is

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

	-- Object_Word is a numeric type as large as Object_Poinetr;
	type Object_Word is mod 2 ** Object_Pointer_Bits;
	for Object_Word'Size use Object_Pointer_Bits;

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
	--type Object_Size is new Object_Word range 0 .. 1000; -- TODO: remove this line and uncommect the live above
	type Object_Size is new Object_Word;
	for Object_Size'Size use Object_Pointer_Bits; -- for GC

	type Object_Byte is mod 2 ** System.Storage_Unit;
	for Object_Byte'Size use System.Storage_Unit;

	subtype Object_Character is Standard.Wide_Character;
	subtype Object_String is Standard.Wide_String;

	type Object_String_Pointer is access all Object_String;
	type Constant_Object_String_Pointer is access constant Object_String;

	type Object_Byte_Array is array (Object_Size range <>) of Object_Byte;
	type Object_Character_Array is array (Object_Size range <>) of Object_Character;
	type Object_Pointer_Array is array (Object_Size range <>) of Object_Pointer;
	type Object_Word_Array is array (Object_Size range <>) of Object_Word;

	type Object_Kind is (
		Moved_Object, -- internal use only
		Pointer_Object,
		Character_Object,
		Byte_Object,
		Word_Object
	);
	for Object_Kind use (
		Moved_Object => 0,
		Pointer_Object => 1,
		Character_Object => 2,
		Byte_Object => 3,
		Word_Object => 4
	);

	-- -----------------------------------------------------------------------

	-- Object_Record contains the Flags field that can be used
	-- freely for management purpose. The Object_Flags type
	-- represents the value that can be stored in this field.
	type Object_Flags is mod 2 ** 4;
	Syntax_Object: constant Object_Flags := Object_Flags'(2#0001#); 

	type Syntax_Code is mod 2 ** 4;
	And_Syntax:    constant Syntax_Code := Syntax_Code'(0);
	Begin_Syntax:  constant Syntax_Code := Syntax_Code'(1);
	Case_Syntax:   constant Syntax_Code := Syntax_Code'(2);
	Cond_Syntax:   constant Syntax_Code := Syntax_Code'(3);
	Define_Syntax: constant Syntax_Code := Syntax_Code'(4);
	If_Syntax:     constant Syntax_Code := Syntax_Code'(5);
	Lambda_Syntax: constant Syntax_Code := Syntax_Code'(6);
	Let_Syntax:    constant Syntax_Code := Syntax_Code'(7);
	Letast_Syntax: constant Syntax_Code := Syntax_Code'(8);
	Letrec_Syntax: constant Syntax_Code := Syntax_Code'(9);
	Or_Syntax:     constant Syntax_Code := Syntax_Code'(10);
	Quote_Syntax:  constant Syntax_Code := Syntax_Code'(11);
	Set_Syntax:    constant Syntax_Code := Syntax_Code'(12);

	subtype Procedure_Code is Object_Integer;
	Car_Procedure:      constant Procedure_Code := Procedure_Code'(0);
	Cdr_Procedure:      constant Procedure_Code := Procedure_Code'(1);
	Setcar_Procedure:   constant Procedure_Code := Procedure_Code'(2);
	Setcdr_Procedure:   constant Procedure_Code := Procedure_Code'(3);
	Add_Procedure:      constant Procedure_Code := Procedure_Code'(4);
	Subtract_Procedure: constant Procedure_Code := Procedure_Code'(5);
	Multiply_Procedure: constant Procedure_Code := Procedure_Code'(6);
	Divide_Procedure:   constant Procedure_Code := Procedure_Code'(7);

	type Object_Tag is (
		Unknown_Object, 
		Cons_Object,
		String_Object,
		Symbol_Object,
		Number_Object,
		Array_Object,
		Table_Object,
		Procedure_Object,
		Closure_Object,
		Continuation_Object,
		Frame_Object,
		Mark_Object
	);

	type Object_Record (Kind: Object_Kind; Size: Object_Size) is record
		Flags: Object_Flags := 0;
		Scode: Syntax_Code := 0;
		Tag: Object_Tag := Unknown_Object;

		-- Object payload:
		--  I assume that the smallest payload is able to hold an 
		--  object pointer by specifying the alignement attribute 
		--  to Object_Pointer_Bytes and checking the minimum allocation
		--  size in Allocate_Bytes_In_Heap().
		case Kind is
			when Moved_Object =>
				New_Pointer: Object_Pointer := null;
			when Pointer_Object =>
				Pointer_Slot: Object_Pointer_Array (1 .. Size) := (others => null);
			when Character_Object =>
				Character_Slot: Object_Character_Array (0 .. Size) := (others => Object_Character'First);
			when Byte_Object =>
				Byte_Slot: Object_Byte_Array (1 .. Size) := (others => 0);
			when Word_Object =>
				Word_Slot: Object_Word_Array (1 .. Size) := (others => 0);
		end case;
	end record;
	for Object_Record use record
		Kind  at 0 range 0 .. 3; -- 4 bits (0 .. 15)
		Flags at 0 range 4 .. 7; -- 4 bits 
		Scode at 0 range 8 .. 11; -- 4 bits (0 .. 15)
		Tag   at 0 range 12 .. 15; -- 4 bits (0 .. 15)
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

	-- -----------------------------------------------------------------------------
	-- Various pointer classification and conversion procedures
	-- -----------------------------------------------------------------------------
	function Is_Pointer (Pointer: in Object_Pointer) return Standard.Boolean;
	function Is_Special_Pointer (Pointer: in Object_Pointer) return Standard.Boolean;
	function Is_Normal_Pointer (Pointer: in Object_Pointer) return Standard.Boolean;
	function Is_Integer (Pointer: in Object_Pointer) return Standard.Boolean;
	function Is_Character (Pointer: in Object_Pointer) return Standard.Boolean;
	function Is_Byte (Pointer: in Object_Pointer) return Standard.Boolean;

	function Integer_To_Pointer (Int: in Object_Integer) return Object_Pointer;
	function Character_To_Pointer (Char: in Object_Character) return Object_Pointer;
	function Byte_To_Pointer (Byte: in Object_Byte) return Object_Pointer;

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
	pragma Inline (Pointer_To_Integer);
	pragma Inline (Pointer_To_Character);
	pragma Inline (Pointer_To_Byte);

	-- -----------------------------------------------------------------------------


	type Stream_Record is abstract tagged limited null record;

	procedure Open (Stream: in out Stream_Record) is abstract;

	procedure Close (Stream: in out Stream_Record) is abstract;

	procedure Read (Stream: in out Stream_Record;
	                Data:   out    Object_String;
	                Last:   out    Standard.Natural) is abstract;

	procedure Write (Stream: in out Stream_Record;
	                 Data:   out    Object_String;
	                 Last:   out    Standard.Natural) is abstract;

	type Stream_Pointer is access all Stream_Record'Class;

	type Stream_Allocator is access 
		procedure (Interp: in out Interpreter_Record; 
		           Name:          Constant_Object_String_Pointer;
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
		--Data: Object_String(1..2048) := (others => Object_Character'First);
		Data: Object_String(1..5) := (others => Object_Character'First);
		Last: Standard.Natural := 0;
		Pos: Standard.Natural := 0;
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
	-- are never allocated on one of these addresses. Addresses of 0, 4, 8 are
	-- very low, making the assumption pretty safe.
	Nil_Word: constant Object_Word := 2#0000#; -- 0
	--Nil_Pointer: constant Object_Pointer;
	--for Nil_Pointer'Address use Nil_Word'Address;
	--pragma Import (Ada, Nil_Pointer);

	True_Word: constant Object_Word := 2#0100#; -- 4
	--True_Pointer: constant Object_Pointer;
	--for True_Pointer'Address use True_Word'Address;
	--pragma Import (Ada, True_Pointer);

	False_Word: constant Object_Word := 2#1000#; -- 8
	--False_Pointer: constant Object_Pointer;
	--for False_Pointer'Address use False_Word'Address;
	--pragma Import (Ada, False_Pointer);

	function Object_Word_To_Pointer is new Ada.Unchecked_Conversion (Object_Word, Object_Pointer);
	function Object_Pointer_To_Word is new Ada.Unchecked_Conversion (Object_Pointer, Object_Word);
	Nil_Pointer: constant Object_Pointer := Object_Word_To_Pointer (Nil_Word);
	True_Pointer: constant Object_Pointer := Object_Word_To_Pointer (True_Word);
	False_Pointer: constant Object_Pointer := Object_Word_To_Pointer (False_Word);

	-- -----------------------------------------------------------------------------

procedure Make_Test_Object (Interp: in out Interpreter_Record; Result: out Object_Pointer);

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
	procedure Read (Interp: in out Interpreter_Record;
	                Result: out    Object_Pointer);

	procedure Evaluate (Interp: in out Interpreter_Record;
	                    Source: in     Object_Pointer;
	                    Result: out    Object_Pointer);

	procedure Print (Interp: in out Interpreter_Record;
	                 Source: in     Object_Pointer);

	procedure Run_Loop (Interp: in out Interpreter_Record;
	                    Result: out    Object_Pointer);

	-- -----------------------------------------------------------------------------

	subtype Thin_String is Object_String (Standard.Positive'Range);
	type Thin_String_Pointer is access all Thin_String;
	for Thin_String_Pointer'Size use Object_Pointer_Bits;
	type Buffer_Record is record
		Ptr: Thin_String_Pointer := null;
		Len: Standard.Natural := 0;
		Last: Standard.Natural := 0;
	end record;
private
	type Heap_Element_Array is array (Heap_Size range <>) of aliased Heap_Element;

	type Heap_Record (Size: Heap_Size) is record
		Space: Heap_Element_Array(1..Size) := (others => 0);
		Bound: Heap_Size := 0;
	end record;
	for Heap_Record'Alignment use Object_Pointer_Bytes;
	type Heap_Pointer is access all Heap_Record;

	type Heap_Number is mod 2 ** 1;
	type Heap_Pointer_Array is Array (Heap_Number'First .. Heap_Number'Last) of Heap_Pointer;



	type Token_Kind is (End_Token,
	                    Identifier_Token,
	                    Left_Parenthesis_Token,
	                    Right_Parenthesis_Token,
	                    Single_Quote_Token,
	                    String_Token
	);

	type Token_Record is record
		Kind: Token_Kind;
		Value: Buffer_Record;
	end record;

	--type Interpreter_Record is tagged limited record
	type Interpreter_Record is limited record
		--Self: Interpreter_Pointer := null;
		Self: Interpreter_Pointer := Interpreter_Record'Unchecked_Access; -- Current instance's pointer
		Storage_Pool: Storage_Pool_Pointer := null;
		Trait: Option_Record(Trait_Option);
		Stream: Option_Record(Stream_Option);

		Heap: Heap_Pointer_Array := (others => null);
		Current_Heap: Heap_Number := Heap_Number'First;

		Root_Table: Object_Pointer := Nil_Pointer;
		Symbol_Table: Object_Pointer := Nil_Pointer;
		Root_Environment: Object_Pointer := Nil_Pointer;
		Environment: Object_Pointer := Nil_Pointer;
		Stack: Object_Pointer := Nil_Pointer;
		Mark: Object_Pointer := Nil_Pointer;

		Base_Input: aliased IO_Record;
		Input: IO_Pointer := null;

		Token: Token_Record;
	end record;

end H2.Scheme;
