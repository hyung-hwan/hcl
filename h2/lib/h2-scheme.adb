with H2.Pool;

-- XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXx
-- TODO: delete these after debugging
with Ada.Unchecked_Deallocation; -- for h2scm c interface. TOOD: move it to a separate file
with Interfaces.C;
with ada.text_io;
with ada.wide_text_io;
with ada.exceptions;
-- TODO: delete above after debugging
-- XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXx

package body H2.Scheme is

	package body Bigint is separate;
	package body Token is separate;


	DEBUG_GC: Standard.Boolean := Standard.False;
	-----------------------------------------------------------------------------
	-- PRIMITIVE DEFINITIONS
	-----------------------------------------------------------------------------

	type Procedure_Code is (
		Callcc_Procedure,
		Car_Procedure,
		Cdr_Procedure,
		Cons_Procedure,
		Not_Procedure,

		N_Add_Procedure,
		N_EQ_Procedure,
		N_GT_Procedure,
		N_LT_Procedure,
		N_GE_Procedure,
		N_LE_Procedure,
		N_Multiply_Procedure,
		N_Quotient_Procedure,
		N_Remainder_Procedure,
		N_Subtract_Procedure,

		Q_Boolean_Procedure,
		Q_Eq_Procedure,
		Q_Eqv_Procedure,
		Q_Null_Procedure,
		Q_Number_Procedure,
		Q_Pair_Procedure,
		Q_Procedure_Procedure,
		Q_String_Procedure,
		Q_String_EQ_Procedure,
		Q_Symbol_Procedure,

		Setcar_Procedure,
		Setcdr_Procedure
	);
	for Procedure_Code'Size use Object_Integer'Size;

	-- I define these constants to word around the limitation of not being
	-- able to use a string literal when the string type is a generic parameter.
	-- Why doesn't ada include a formal type support for different character
	-- and string types? This limitation is caused because the generic 
	-- type I chosed to use to represent a character type is a discrete type.
	Label_And:        constant Object_Character_Array := (Ch.LC_A, Ch.LC_N, Ch.LC_D); -- "and"
	Label_Begin:      constant Object_Character_Array := (Ch.LC_B, Ch.LC_E, Ch.LC_G, Ch.LC_I, Ch.LC_N); -- "begin"
	Label_Case:       constant Object_Character_Array := (Ch.LC_C, Ch.LC_A, Ch.LC_S, Ch.LC_E); -- "case"
	Label_Cond:       constant Object_Character_Array := (Ch.LC_C, Ch.LC_O, Ch.LC_N, Ch.LC_D); -- "cond"
	Label_Define:     constant Object_Character_Array := (Ch.LC_D, Ch.LC_E, Ch.LC_F, Ch.LC_I, Ch.LC_N, Ch.LC_E); -- "define"
	Label_Do:         constant Object_Character_Array := (Ch.LC_D, Ch.LC_O); -- "do"
	Label_If:         constant Object_Character_Array := (Ch.LC_I, Ch.LC_F); -- "if"
	Label_Lambda:     constant Object_Character_Array := (Ch.LC_L, Ch.LC_A, Ch.LC_M, Ch.LC_B, Ch.LC_D, Ch.LC_A); -- "lambda"
	Label_Let:        constant Object_Character_Array := (Ch.LC_L, Ch.LC_E, Ch.LC_T); -- "let"
	Label_Letast:     constant Object_Character_Array := (Ch.LC_L, Ch.LC_E, Ch.LC_T, Ch.Asterisk); -- "let*"
	Label_Letrec:     constant Object_Character_Array := (Ch.LC_L, Ch.LC_E, Ch.LC_T, Ch.LC_R, Ch.LC_E, Ch.LC_C); -- "letrec"
	Label_Or:         constant Object_Character_Array := (Ch.LC_O, Ch.LC_R); -- "or"
	Label_Quasiquote: constant Object_Character_Array := (Ch.LC_Q, Ch.LC_U, Ch.LC_A, Ch.LC_S, Ch.LC_I, 
	                                                      Ch.LC_Q, Ch.LC_U, Ch.LC_O, Ch.LC_T, Ch.LC_E); -- "quasiquote"
	Label_Quote:      constant Object_Character_Array := (Ch.LC_Q, Ch.LC_U, Ch.LC_O, Ch.LC_T, Ch.LC_E); -- "quote"
	Label_Set:        constant Object_Character_Array := (Ch.LC_S, Ch.LC_E, Ch.LC_T, Ch.Exclamation); -- "set!"


	Label_Callcc:     constant Object_Character_Array := (Ch.LC_C, Ch.LC_A, Ch.LC_L, Ch.LC_L, Ch.Minus_Sign,
	                                                      Ch.LC_W, Ch.LC_I, Ch.LC_T, Ch.LC_H, Ch.Minus_Sign,
	                                                      Ch.LC_C, Ch.LC_U, Ch.LC_R, Ch.LC_R, Ch.LC_E, Ch.LC_N, Ch.LC_T, Ch.Minus_Sign,
	                                                      Ch.LC_C, Ch.LC_O, Ch.LC_N, Ch.LC_T, Ch.LC_I, Ch.LC_N, Ch.LC_U, Ch.LC_A, 
	                                                      Ch.LC_T, Ch.LC_I, Ch.LC_O, Ch.LC_N);  -- "call-with-current-continuation"
	Label_Car:        constant Object_Character_Array := (Ch.LC_C, Ch.LC_A, Ch.LC_R); -- "car"
	Label_Cdr:        constant Object_Character_Array := (Ch.LC_C, Ch.LC_D, Ch.LC_R); -- "cdr"
	Label_Cons:       constant Object_Character_Array := (Ch.LC_C, Ch.LC_O, Ch.LC_N, Ch.LC_S); -- "cons"
	Label_Not:        constant Object_Character_Array := (Ch.LC_N, Ch.LC_O, Ch.LC_T); -- "not"

	Label_N_Add:       constant Object_Character_Array := (1 => Ch.Plus_Sign); -- "+"
	Label_N_EQ:        constant Object_Character_Array := (1 => Ch.Equal_Sign); -- "="
	Label_N_GE:        constant Object_Character_Array := (Ch.Greater_Than_Sign, Ch.Equal_Sign); -- ">="
	Label_N_GT:        constant Object_Character_Array := (1 => Ch.Greater_Than_Sign); -- ">"
	Label_N_LE:        constant Object_Character_Array := (Ch.Less_Than_Sign, Ch.Equal_Sign); -- "<="
	Label_N_LT:        constant Object_Character_Array := (1 => Ch.Less_Than_Sign); -- "<"
	Label_N_Multiply:  constant Object_Character_Array := (1 => Ch.Asterisk); -- "*"
	Label_N_Quotient:  constant Object_Character_Array := (Ch.LC_Q, Ch.LC_U, Ch.LC_O, Ch.LC_T, Ch.LC_I, Ch.LC_E, Ch.LC_N, Ch.LC_T); -- "quotient"
	Label_N_Remainder: constant Object_Character_Array := (Ch.LC_R, Ch.LC_E, Ch.LC_M, Ch.LC_A, Ch.LC_I, Ch.LC_N, Ch.LC_D, Ch.LC_E, Ch.LC_R); -- "remainder"
	Label_N_Subtract:  constant Object_Character_Array := (1 => Ch.Minus_Sign); -- "-"
	
	Label_Q_Boolean:   constant Object_Character_Array := (Ch.LC_B, Ch.LC_O, Ch.LC_O, Ch.LC_L, Ch.LC_E, Ch.LC_A, Ch.LC_N, Ch.Question); -- "boolean?"
	Label_Q_Eq:        constant Object_Character_Array := (Ch.LC_E, Ch.LC_Q, Ch.Question); -- "eq?"
	Label_Q_Eqv:       constant Object_Character_Array := (Ch.LC_E, Ch.LC_Q, Ch.LC_V, Ch.Question); -- "eqv?"
	Label_Q_Null:      constant Object_Character_Array := (Ch.LC_N, Ch.LC_U, Ch.LC_L, Ch.LC_L, Ch.Question); -- "null?"
	Label_Q_Number:    constant Object_Character_Array := (Ch.LC_N, Ch.LC_U, Ch.LC_M, Ch.LC_B, Ch.LC_E, Ch.LC_R, Ch.Question); -- "number?"
	Label_Q_Pair:      constant Object_Character_Array := (Ch.LC_P, Ch.LC_A, Ch.LC_I, Ch.LC_R, Ch.Question); -- "pair?"
	Label_Q_Procedure: constant Object_Character_Array := (Ch.LC_P, Ch.LC_R, Ch.LC_O, Ch.LC_C, Ch.LC_E, Ch.LC_D, Ch.LC_U, Ch.LC_R, Ch.LC_E, Ch.Question); -- "procedure?"
	Label_Q_String:    constant Object_Character_Array := (Ch.LC_S, Ch.LC_T, Ch.LC_R, Ch.LC_I, Ch.LC_N, Ch.LC_G, Ch.Question); -- "string?"
	Label_Q_String_EQ: constant Object_Character_Array := (Ch.LC_S, Ch.LC_T, Ch.LC_R, Ch.LC_I, Ch.LC_N, Ch.LC_G, Ch.Equal_Sign, Ch.Question); -- "string=?"
	Label_Q_Symbol:    constant Object_Character_Array := (Ch.LC_S, Ch.LC_Y, Ch.LC_M, Ch.LC_B, Ch.LC_O, Ch.LC_L, Ch.Question); -- "symbol?"
	
	Label_Setcar:      constant Object_Character_Array := (Ch.LC_S, Ch.LC_E, Ch.LC_T, Ch.Minus_Sign, Ch.LC_C, Ch.LC_A, Ch.LC_R, Ch.Exclamation); -- "set-car!"
	Label_Setcdr:      constant Object_Character_Array := (Ch.LC_S, Ch.LC_E, Ch.LC_T, Ch.Minus_Sign, Ch.LC_C, Ch.LC_D, Ch.LC_R, Ch.Exclamation); -- "set-cdr!"

	Label_Newline:    constant Object_Character_Array := (Ch.LC_N, Ch.LC_E, Ch.LC_W, Ch.LC_L, Ch.LC_I, Ch.LC_N, Ch.LC_E); -- "newline"
	Label_Space:      constant Object_Character_Array := (Ch.LC_S, Ch.LC_P, Ch.LC_A, Ch.LC_C, Ch.LC_E); -- "space"

	Label_Arrow:      constant Object_Character_Array := (Ch.Equal_Sign, Ch.Greater_Than_Sign); -- "=>"
	Label_Else:       constant Object_Character_Array := (Ch.LC_E, Ch.LC_L, Ch.LC_S, Ch.LC_E); -- "else"

	-----------------------------------------------------------------------------
	-- INTERNAL EXCEPTIONS
	-----------------------------------------------------------------------------
	Stream_End_Error: exception;

	-----------------------------------------------------------------------------
	-- INTERNALLY-USED TYPES
	-----------------------------------------------------------------------------
	type Heap_Element_Pointer is access all Heap_Element;
	for Heap_Element_Pointer'Size use Object_Pointer_Bits; -- ensure that it can be overlaid by an ObjectPointer

	type Thin_Heap_Element_Array is array (1 .. Heap_Size'Last) of Heap_Element;
	type Thin_Heap_Element_Array_Pointer is access all Thin_Heap_Element_Array;
	for Thin_Heap_Element_Array_Pointer'Size use Object_Pointer_Bits;

	subtype Moved_Object_Record is Object_Record (Moved_Object, 0);

	type Opcode_Type is (
		Opcode_Exit,
		Opcode_Evaluate_Result,
		Opcode_Evaluate_Object,
	
		Opcode_And_Finish,
		Opcode_Or_Finish,
		Opcode_Case_Finish,
		Opcode_Cond_Finish,
		Opcode_Define_Finish,
		Opcode_Do_Binding,
		Opcode_Do_Break,
		Opcode_Do_Step,
		Opcode_Do_Test,
		Opcode_Do_Update,
		Opcode_Grouped_Call,  -- (begin ...), closure apply, let body
		Opcode_If_Finish,
		Opcode_Let_Binding,
		Opcode_Letast_Binding,
		Opcode_Letrec_Binding,
		Opcode_Procedure_Call,
		Opcode_Procedure_Call_Finish,
		Opcode_Set_Finish,
	
		Opcode_Apply,
		Opcode_Read_Object,
		Opcode_Read_List,
		Opcode_Read_List_Cdr,
		Opcode_Read_List_End,
		Opcode_Close_List,
		Opcode_Close_Quote,
		Opcode_Close_Quote_In_List
	);
	for Opcode_Type'Size use Object_Integer'Size;


	-----------------------------------------------------------------------------
	-- COMMON OBJECTS
	-----------------------------------------------------------------------------
	Cons_Object_Size: constant Pointer_Object_Size := 2;
	Cons_Car_Index: constant Pointer_Object_Size := 1;
	Cons_Cdr_Index: constant Pointer_Object_Size := 2;

	Frame_Object_Size: constant Pointer_Object_Size := 6;
	Frame_Parent_Index: constant Pointer_Object_Size := 1;
	Frame_Opcode_Index: constant Pointer_Object_Size := 2;
	Frame_Operand_Index: constant Pointer_Object_Size := 3;
	Frame_Environment_Index: constant Pointer_Object_Size := 4;
	Frame_Intermediate_Index: constant Pointer_Object_Size := 5;
	Frame_Result_Index: constant Pointer_Object_Size := 6;

	Procedure_Object_Size: constant Pointer_Object_Size := 1;
	Procedure_Opcode_Index: constant Pointer_Object_Size := 1;

	Closure_Object_Size: constant Pointer_Object_Size := 2;
	Closure_Code_Index: constant Pointer_Object_Size := 1;
	Closure_Environment_Index: constant Pointer_Object_Size := 2;
	Continuation_Object_Size: constant Pointer_Object_Size := 1;
	Continuation_Frame_Index: constant Pointer_Object_Size := 1;

	procedure Set_New_Location (Object: in Object_Pointer;
	                            Ptr:    in Heap_Element_Pointer);
	procedure Set_New_Location (Object: in Object_Pointer;
	                            Ptr:    in Object_Pointer);
	pragma Inline (Set_New_Location);

	function Get_New_Location (Object: in Object_Pointer) return Object_Pointer;
	pragma Inline (Get_New_Location);

	-----------------------------------------------------------------------------
	-- FOR DEBUGGING. REMVOE THESE LATER
	-----------------------------------------------------------------------------
	procedure Output_Character_Array (Source: in Object_Character_Array) is
		-- for debugging only.
	begin
		for I in Source'Range loop
			--Ada.Text_IO.Put (Source(I));
-- TODO: note this is a hack for quick printing.
			Ada.Text_IO.Put (Standard.Character'Val(Object_Character'Pos(Source(I))));
		end loop;
	end Output_Character_Array;

	-----------------------------------------------------------------------------
	-- POINTER AND DATA CONVERSION
	-----------------------------------------------------------------------------

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

	function Integer_To_Pointer (Value: in Object_Integer) return Object_Pointer is
		Pointer: Object_Pointer;
		Word: Object_Word;
		for Word'Address use Pointer'Address;
	begin
		if Value < 0 then
			-- change the sign of a negative number.
			-- '-Int' may violate the range of Object_Integer
			-- if it is Object_Integer'First. So I add 1 to 'Int'
			-- first to make it fall between Object_Integer'First + 1
			-- .. 0 and typecast it with an extra increment.
			--Word := Object_Word (-(Int + 1)) + 1;

			-- Let me use Object_Signed_Word instead of the trick shown above
			Word := Object_Word(-Object_Signed_Word(Value));

			-- shift the number to the left by 2 and
			-- set the highest bit on by force.
			Word := (Word * (2 ** Object_Pointer_Type_Bits)) or Object_Word(Object_Pointer_Type_Integer) or (2 ** (Word'Size - 1));
		else
			Word := Object_Word(Value);
			-- Shift 'Word' to the left by 2 and set the integer mark.
			Word := (Word * (2 ** Object_Pointer_Type_Bits)) or Object_Word(Object_Pointer_Type_Integer);
		end if;

		return Pointer;
	end Integer_To_Pointer;

	function Character_To_Pointer (Value: in Object_Character) return Object_Pointer is
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
		Word := Object_Character'Pos(Value);
		Word := (Word * (2 ** Object_Pointer_Type_Bits)) or Object_Word(Object_Pointer_Type_Character);
		return Pointer;
	end Character_To_Pointer;

	function Byte_To_Pointer (Value: in Object_Byte) return Object_Pointer is
		Pointer: Object_Pointer;
		Word: Object_Word;
		for Word'Address use Pointer'Address;
	begin
		Word := Object_Word(Value);
		Word := (Word * (2 ** Object_Pointer_Type_Bits)) or Object_Word(Object_Pointer_Type_Byte);
		return Pointer;
	end Byte_To_Pointer;

	function Pointer_To_Word is new Ada.Unchecked_Conversion (Object_Pointer, Object_Word);
	--function Pointer_To_Word (Pointer: in Object_Pointer) return Object_Word is
	--	pragma Inline (Pointer_To_Word);
	--	Word: Object_Word;
	--	for Word'Address use Pointer'Address;
	--begin
	--	return Word;
	--end Pointer_To_Word;

	function Pointer_To_Integer (Pointer: in Object_Pointer) return Object_Integer is
		Word: Object_Word := Pointer_To_Word(Pointer);
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
		return Object_Character'Val(Word / (2 ** Object_Pointer_Type_Bits));
	end Pointer_To_Character;

	function Pointer_To_Byte (Pointer: in Object_Pointer) return Object_Byte is
		Word: Object_Word := Pointer_To_Word (Pointer);
	begin
		return Object_Byte(Word / (2 ** Object_Pointer_Type_Bits));
	end Pointer_To_Byte;

	-- TODO: delete this procedure 
	procedure Print_Object_Pointer (Msg: in Standard.String; Source: in Object_Pointer) is
		W: Object_Word;
		for W'Address use Source'Address;

		Ptr_Type: Object_Pointer_Type;
	begin
		Ptr_Type := Get_Pointer_Type(Source);
		if Ptr_Type = Object_Pointer_Type_Character then
			Ada.Text_IO.Put_Line (Msg & Object_Character'Image(Pointer_To_Character(Source)));
		elsif Ptr_Type = Object_Pointer_Type_Integer then
			Ada.Text_IO.Put_Line (Msg & Object_Integer'Image(Pointer_To_Integer(Source)));
		elsif Is_Special_Pointer(Source) then
			Ada.Text_IO.Put_Line (Msg & " at " & Object_Word'Image(W));
		elsif Source.Kind = Character_Object then
			Ada.Text_IO.Put (Msg & " at " & Object_Word'Image(W) & 
			                   " at " & Object_Kind'Image(Source.Kind) & 
			                   " size " & Object_Size'Image(Source.Size) & " - ");
			if Source.Kind = Moved_Object then
				Output_Character_Array (Get_New_Location(Source).Character_Slot);
			else
				Output_Character_Array (Source.Character_Slot);
			end if;
		else
			Ada.Text_IO.Put_Line (Msg & " at " & Object_Word'Image(W) & 
			                            " kind: " & Object_Kind'Image(Source.Kind) &
			                            " size: " & Object_Size'Image(Source.Size) &
			                            " tag: " & Object_Tag'Image(Source.Tag));
		end if;
	end Print_Object_Pointer;

	function String_To_Integer_Pointer (Source: in Object_Character_Array) return Object_Pointer is
		V: Object_Integer := 0;
		Negative: Standard.Boolean := False;
		First: Object_Size;
	begin
		-- TODO: BIGNUM, RANGE CHECK, ETC
		pragma Assert (Source'Length > 0);

		First := Source'First;
		if Source(First) = Ch.Minus_Sign then
			First := First + 1;
			Negative := Standard.True;
		elsif Source(First) = Ch.Plus_Sign then
			First := First + 1;
		end if;
		for I in First .. Source'Last loop
			V := V * 10 + Object_Character'Pos(Source(I)) - Object_Character'Pos(Ch.Zero);
		end loop;	

		if Negative then	
			V := -V;
		end if;

		return Integer_To_Pointer(V);
	end String_To_Integer_Pointer;

	-----------------------------------------------------------------------------
	-- MORE CONVERSIONS
	-----------------------------------------------------------------------------
	--function Pointer_To_Opcode (Pointer: in Object_Pointer) return Opcode_Type renames Pointer_To_Integer;
	--function Opcode_To_Pointer (Opcode: in Opcode_Type) return Object_Pointer renames Integer_To_Pointer;
		
	function Pointer_To_Opcode (Pointer: in Object_Pointer) return Opcode_Type is
		pragma Inline (Pointer_To_Opcode);
	begin
		return Opcode_Type'Val(Pointer_To_Integer(Pointer));
	end Pointer_To_Opcode;

	function Opcode_To_Pointer (Opcode: in Opcode_Type) return Object_Pointer is
		pragma Inline (Opcode_To_Pointer);
	begin
		 return Integer_To_Pointer(Opcode_Type'Pos(Opcode));
	end Opcode_To_Pointer;

	function Pointer_To_Procedure_Code (Pointer: in Object_Pointer) return Procedure_Code is
		pragma Inline (Pointer_To_Procedure_Code);
	begin
		return Procedure_Code'Val(Pointer_To_Integer(Pointer));
	end Pointer_To_Procedure_Code;

	function Procedure_Code_To_Pointer (Opcode: in Procedure_Code) return Object_Pointer is
		pragma Inline (Procedure_Code_To_Pointer);
	begin
		 return Integer_To_Pointer(Procedure_Code'Pos(Opcode));
	end Procedure_Code_To_Pointer;


	function Token_To_Pointer (Interp: access Interpreter_Record; 
	                           Token:  in     Token_Record) return Object_Pointer is
	begin
		case Token.Kind is
			when Integer_Token =>
				-- TODO: bignum
				return String_To_Integer_Pointer(Token.Value.Ptr.all(1..Token.Value.Last));

			when Character_Token =>
				pragma Assert (Token.Value.Last = 1);
				return Character_To_Pointer(Token.Value.Ptr.all(1));
		
			when String_Token =>
				return Make_String (Interp, Token.Value.Ptr.all(1..Token.Value.Last));

			when Identifier_Token =>	
				return Make_Symbol (Interp, Token.Value.Ptr.all(1..Token.Value.Last));

			when True_Token =>
				return True_Pointer;

			when False_Token =>
				return False_Pointer;

			when others =>
				return null;
		end case;
	end Token_To_Pointer;

	-----------------------------------------------------------------------------
	-- COMPARISON
	-----------------------------------------------------------------------------

	function Equal_Values (X: in Object_Pointer; Y: in Object_Pointer) return Standard.Boolean is
		Ptr_Type: Object_Pointer_Type;
	begin
		if X = Y then
			return Standard.True;
		end if;
		
		Ptr_Type := Get_Pointer_Type(X);
		case Ptr_Type is
			when Object_Pointer_Type_Integer |
			     Object_Pointer_Type_Character |
			     Object_Pointer_Type_Byte =>
			     
				-- This part of the code won't be reached if two special
				-- pointers are the same. So False can be returned safely
				-- without further check. See the lines commented out.

				--if Get_Pointer_Type(Y) = Ptr_Type then
				--	return X = Y;
				--else
					return Standard.False;
				--end if;

			when others =>
				-- TODO: BIGNUM, OTHER NUMERIC DATA.
				if Is_Special_Pointer(X) then
					return X = Y;
				elsif Get_Pointer_Type(Y) /= Ptr_Type then
					return Standard.False;
				end if;

				case X.Kind is
					when Character_Object =>
						if Y.Kind = X.Kind then
							return X.Character_Slot = Y.Character_Slot;
						else
							return Standard.False;
						end if;

					when Byte_Object =>
						if Y.Kind = X.Kind then
							return X.Byte_Slot = Y.Byte_Slot;
						else
							return Standard.False;
						end if;

					when Word_Object =>
						if Y.Kind = X.Kind then
							return X.Word_Slot = Y.Word_Slot;
						else
							return Standard.False;
						end if;

					when Half_Word_Object =>
						if Y.Kind = X.Kind then
							return X.Half_Word_Slot = Y.Half_Word_Slot;
						else
							return Standard.False;
						end if;

					when Pointer_Object =>
						return X = Y;

					when Moved_Object =>
						raise Internal_Error;
				end case;
		end case;

	end Equal_Values;


	-----------------------------------------------------------------------------
	-- MEMORY MANAGEMENT
	-----------------------------------------------------------------------------
-- (define x ())
-- (define x #())
-- (define x $())
-- (define x #( 
--              (#a . 10)  ; a is a symbol
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
	--procedure Set_New_Location (Object: in Object_Pointer; Ptr: in Heap_Element_Pointer) is
		--New_Addr: Heap_Element_Pointer;
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
	procedure Set_New_Location (Object: in Object_Pointer; Ptr: in Heap_Element_Pointer) is
		Moved_Object: Moved_Object_Record;
		for Moved_Object'Address use Object.all'Address;
		-- pramga Import must not be specified here as I'm counting
		-- on the default initialization of Moved_Object to overwrite
		-- the Kind discriminant in particular.
		--pragma Import (Ada, Moved_Object); -- this must not be used.
		function To_Object_Pointer is new Ada.Unchecked_Conversion (Heap_Element_Pointer, Object_Pointer);
	begin
		Moved_Object.New_Pointer := To_Object_Pointer (Ptr);
	end Set_New_Location;

	procedure Set_New_Location (Object: in Object_Pointer; Ptr: in Object_Pointer) is
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

	function Verify_Pointer (Source: in Object_Pointer) return Object_Pointer is
		pragma Inline (Verify_Pointer);
	begin
		if not Is_Normal_Pointer(Source) or else
		   Source.Kind /= Moved_Object then
			return Source;
		else
			return Get_New_Location(Source);
		end if;
	end Verify_Pointer;

	function Allocate_Bytes_In_Heap (Heap:       access Heap_Record; 
	                                 Heap_Bytes: in     Heap_Size) return Heap_Element_Pointer is
		Avail: Heap_Size;
		Result: Heap_Element_Pointer;
		Real_Bytes: Heap_Size := Heap_Bytes;
	begin
		if Real_Bytes < Moved_Object_Record'Max_Size_In_Storage_Elements then
			-- Guarantee the minimum object size to be greater than or 
			-- equal to the size of a moved object for GC to work.
			Real_Bytes := Moved_Object_Record'Max_Size_In_Storage_Elements;	

			-- Note: Extra attention must be paid when calculating the 
			-- actual bytes allocated for an object. Scan_New_Heap() also 
			-- makes similar adjustment to skip actual allocated bytes.
		end if;

		Avail := Heap.Size - Heap.Bound;
		if Real_Bytes > Avail then
			return null;
		end if;
		
		Result := Heap.Space(Heap.Bound + 1)'Unchecked_Access;
		Heap.Bound := Heap.Bound + Real_Bytes;
		return Result;
	end Allocate_Bytes_In_Heap;

	function Get_Heap_Number (Interp: access Interpreter_Record;
	                          Source: in     Object_Pointer) return Heap_Number is
		-- for debugging
		SW: Object_Word;
		for SW'Address use Source'Address;

		H1: Heap_Element_Pointer := Interp.Heap(0).Space(1)'Unchecked_Access;
		H2: Heap_Element_Pointer := Interp.Heap(1).Space(1)'Unchecked_Access;

		HW1: Object_Word;
		for HW1'Address use H1'Address;

		HW2: Object_Word;
		for HW2'Address use H2'Address;
	begin
		if SW >= HW1 and then SW < HW1 + Object_Word(Interp.Heap(0).Size) then
			return 0;	
		end if;
		if SW >= HW2 and then SW < HW2 + Object_Word(Interp.Heap(1).Size) then
			return 1;	
		end if;

		if Source = Nil_Pointer then
ada.text_io.put_line ("HEAP SOURCE IS NIL");
			return 0;
		end if;

		raise Internal_Error;
	end Get_Heap_Number;


	procedure Copy_Object (Source: in     Object_Pointer;
	                       Target: in out Heap_Element_Pointer) is
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
		-- it's supposed to be 96. Use Copy_Object_With_Size() below instead.
		Target_Object.all := Source.all;
		pragma Assert (Source.all'Size = Target_Object.all'Size);
	end Copy_Object;

	procedure Copy_Object_With_Size (Source: in Object_Pointer;
	                                 Target: in Heap_Element_Pointer;
	                                 Bytes:  in Heap_Size) is
		pragma Inline (Copy_Object_With_Size);
		pragma Assert (Bytes > 0);
		-- This procedure uses a more crude type for copying objects.
		-- It's the result of an effort to work around some compiler
		-- issues mentioned above.


		-- The work around, however, still didn't work well with gnat-3.15p.
		-- The overlaying(thus overlaid)  pointer is initialized to null
		-- despite pragma Import.
		--Tgt: Thin_Heap_Element_Array_Pointer;
		--for Tgt'Address use Target'Address;
		--pragma Import (Ada, Tgt);
		--Src: Thin_Heap_Element_Array_Pointer;
		--for Src'Address use Source'Address;
		--pragma Import (Ada, Src);

		-- So let me turn to unchecked conversion instead.
		function Conv1 is new Ada.Unchecked_Conversion (Object_Pointer, Thin_Heap_Element_Array_Pointer);
		function Conv2 is new Ada.Unchecked_Conversion (Heap_Element_Pointer, Thin_Heap_Element_Array_Pointer);
		Src: Thin_Heap_Element_Array_Pointer := Conv1(Source);
		Tgt: Thin_Heap_Element_Array_Pointer := Conv2(Target);
	begin
		Tgt(Tgt'First .. Tgt'First + Bytes - 1) := Src(Src'First .. Src'First + Bytes - 1);
	end Copy_Object_With_Size;

	procedure Collect_Garbage (Interp: in out Interpreter_Record) is

		Last_Pos: Heap_Size;
		New_Heap: Heap_Number;
		Original_Symbol_Table: Object_Pointer;

		--function To_Object_Pointer is new Ada.Unchecked_Conversion (Heap_Element_Pointer, Object_Pointer);
		function Move_One_Object (Source: in Object_Pointer) return Object_Pointer is
		begin
			pragma Assert (Is_Normal_Pointer(Source));

			if Source.Kind = Moved_Object then
				-- the object has moved to the new heap.
				-- the size field has been updated to the new object
				-- in the 'else' block below. i can simply return it
				-- without further migration.
				return Get_New_Location (Source);
			else
				declare
					Bytes: Heap_Size;

					-- This variable holds the allocation result
					Ptr: Heap_Element_Pointer;

					-- Create an overlay for type conversion
					New_Object: Object_Pointer;
					for New_Object'Address use Ptr'Address;
					pragma Import (Ada, New_Object); 
				begin
					-- Target_Object_Record'Max_Size_In_Storage_Elements gave 
					-- some erroneous values when compiled with GNAT 4.3.2 on 
					-- WII(ppc) Debian.
					--Bytes := Target_Object_Record'Max_Size_In_Storage_Elements;
					Bytes := Source.all'Size / System.Storage_Unit;

					-- Allocate space in the new heap
					Ptr := Allocate_Bytes_In_Heap(Interp.Heap(New_Heap), Bytes);

					-- Allocation here must not fail because
					-- I'm allocating the new space in a new heap for
					-- moving an existing object in the current heap.
					-- It must not fail, assuming the new heap is as large
					-- as the old heap, and garbage collection doesn't
					-- allocate more objects than in the old heap.
					pragma Assert (Ptr /= null);

					-- This minimum size adjustment is not needed when copying
					-- an object as it's ok to have garbage in the trailing space.
					-- See Allocate_Bytes_In_Heap() and Scan_New_Heap() for more info.
					--if Bytes < Moved_Object_Record'Max_Size_In_Storage_Elements then
					--	Bytes := Moved_Object_Record'Max_Size_In_Storage_Elements;	
					--end  if;

					-- Copy the payload to the new object
					--Copy_Object (Object, Ptr); -- not reliable with some compilers
					Copy_Object_With_Size (Source, Ptr, Bytes); -- use this instead
					pragma Assert (Source.all'Size = New_Object.all'Size);
					pragma Assert (Bytes = New_Object.all'Size / System.Storage_Unit);

					-- Let the size field of the old object point to the
					-- new object allocated in the new heap. It is returned
					-- in the 'if' block at the beginning of this function
					-- if the object is marked with FLAG_MOVED;
					Set_New_Location (Source, Ptr);

					-- Return the new object
					return New_Object;
				end;
			end if;
		end Move_One_Object;

		function Scan_New_Heap (Start_Position: in Heap_Size) return Heap_Size is
			Ptr: Heap_Element_Pointer;
			Position: Heap_Size := Start_Position;
		begin

--Ada.Text_IO.Put_Line ("Start Scanning New Heap from " & Heap_Size'Image(Start_Position) & " Bound: " & Heap_Size'Image (Interp.Heap(New_Heap).Bound));
			while Position <= Interp.Heap(New_Heap).Bound loop

--Ada.Text_IO.Put_Line (">>> Scanning New Heap from " & Heap_Size'Image (Position) & " Bound: " & Heap_Size'Image (Interp.Heap(New_Heap).Bound));
				Ptr := Interp.Heap(New_Heap).Space(Position)'Unchecked_Access;

				declare
					-- There is a overlaid pointer initialization problem despite 
					-- "pragma Import()" in gnat-3.15p.
					--Object: Object_Pointer;
					--for Object'Address use Ptr'Address;
					--pragma Import (Ada, Object); 

					-- So let me turn to unchecked conversion.
					function Conv1 is new Ada.Unchecked_Conversion (Heap_Element_Pointer, Object_Pointer);
					Object: Object_Pointer := Conv1(Ptr);

					--subtype Target_Object_Record is Object_Record (Object.Kind, Object.Size);
					Bytes: Heap_Size;
				begin
					--Bytes := Target_Object_Record'Max_Size_In_Storage_Elements;
					Bytes := Object.all'Size / System.Storage_Unit;
					if Bytes < Moved_Object_Record'Max_Size_In_Storage_Elements then
						-- Allocate_Bytes_In_Heap() guarantee the minimum object size.
						-- The size must be guaranteed here when scanning a heap.
						Bytes := Moved_Object_Record'Max_Size_In_Storage_Elements;	
					end if;

					if Object.Kind = Pointer_Object then
--Ada.Text_IO.Put_Line (">>> Scanning Obj " & Object_Kind'Image(Object.Kind) & " Size: " & Object_Size'Image(Object.Size) & " At " & Object_Word'Image(Pointer_To_Word(Object)) & " Bytes " & Heap_Size'Image(Bytes));
--Print_Object_Pointer (">>> Scanning :", Object);
						for i in Object.Pointer_Slot'Range loop
							if Is_Normal_Pointer(Object.Pointer_Slot(i)) then
								Object.Pointer_Slot(i) := Move_One_Object(Object.Pointer_Slot(i));
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
			Car: Object_Pointer;
			Cdr: Object_Pointer;
		begin
-- TODO: Change code here if the symbol table structure is changed to a hash table.

			Pred := Nil_Pointer;
			Cons := Interp.Symbol_Table;
			while Cons /= Nil_Pointer loop
				pragma Assert (Cons.Tag = Cons_Object);	

				Car := Cons.Pointer_Slot(Cons_Car_Index);
				Cdr := Cons.Pointer_Slot(Cons_Cdr_Index);
				pragma Assert (Car.Kind = Moved_Object or else Car.Tag = Symbol_Object);

				if Car.Kind /= Moved_Object and then
				   (Car.Flags and Syntax_Object) = 0 then
					-- A non-syntax symbol has not been moved. 
					-- Unlink the cons cell from the symbol table.
					if Pred = Nil_Pointer then
						Interp.Symbol_Table := Cdr;
					else
						Pred.Pointer_Slot(Cons_Cdr_Index) := Cdr;
					end if;
				else
					Pred := Cons;
				end if;
				
				Cons := Cdr;	
			end loop;
		end Compact_Symbol_Table;

	begin

ada.text_io.put_line ("[GC BEGIN]");
--declare
--Avail: Heap_Size;
--begin
--Avail := Interp.Heap(Interp.Current_Heap).Size - Interp.Heap(Interp.Current_Heap).Bound;
--Ada.Text_IO.Put_Line (">>> [GC BEGIN] BOUND: " & Heap_Size'Image(Interp.Heap(Interp.Current_Heap).Bound) & " AVAIL: " & Heap_Size'Image(Avail));
--end;

		-- As the Heap_Number type is a modular type that can 
		-- represent 0 and 1, incrementing it gives the next value.
		New_Heap := Interp.Current_Heap + 1;

		-- Migrate some root objects
--Print_Object_Pointer (">>> [GC] Stack BEFORE ...", Interp.Stack);
		if Is_Normal_Pointer(Interp.Stack) then
			Interp.Stack := Move_One_Object(Interp.Stack);
		end if;

		Interp.Root_Environment := Move_One_Object(Interp.Root_Environment);
		Interp.Root_Frame := Move_One_Object(Interp.Root_Frame);

		-- Migrate temporary object pointers
		for I in Interp.Top.Data'First .. Interp.Top.Last loop
			if Interp.Top.Data(I).all = Interp.Symbol_Table then 
				-- The symbol table must stay before compaction.
				-- Skip migrating a temporary object pointer if it 
				-- is pointing to the symbol table. Remember that
				-- such skipping has happened.
				Original_Symbol_Table := Interp.Symbol_Table;	
			elsif Interp.Top.Data(I).all /= null and then
			      Is_Normal_Pointer(Interp.Top.Data(I).all) then
				Interp.Top.Data(I).all := Move_One_Object(Interp.Top.Data(I).all);
			end if;
		end loop;

		-- Migrate some known symbols
		Interp.Arrow_Symbol := Move_One_Object(Interp.Arrow_Symbol);
		Interp.Else_Symbol := Move_One_Object(Interp.Else_Symbol);
		Interp.Quasiquote_Symbol := Move_One_Object(Interp.Quasiquote_Symbol);
		Interp.Quote_Symbol := Move_One_Object(Interp.Quote_Symbol);

--Ada.Text_IO.Put_Line (">>> [GC SCANNING NEW HEAP]");
		-- Scan the heap
		Last_Pos := Scan_New_Heap(Interp.Heap(New_Heap).Space'First);

		-- Traverse the symbol table for unreferenced symbols.
		-- If the symbol has not moved to the new heap, the symbol
		-- is not referenced by any other objects than the symbol 
		-- table itself 
--Ada.Text_IO.Put_Line (">>> [GC COMPACTING SYMBOL TABLE]");
		Compact_Symbol_Table;

--Print_Object_Pointer (">>> [GC MOVING SYMBOL TABLE]", Interp.Symbol_Table);
		-- Migrate the symbol table itself
		Interp.Symbol_Table := Move_One_Object(Interp.Symbol_Table);

		-- Update temporary object pointers that were pointing to the symbol table
		if Original_Symbol_Table /= null then
			for I in Interp.Top.Data'First .. Interp.Top.Last loop
				if Interp.Top.Data(I).all = Original_Symbol_Table then 
					-- update to the new symbol table
					Interp.Top.Data(I).all := Interp.Symbol_Table; 
				end if;
			end loop;
		end if;

--Ada.Text_IO.Put_Line (">>> [GC SCANNING HEAP AGAIN AFTER SYMBOL TABLE MIGRATION]");
		-- Scan the new heap again from the end position of
		-- the previous scan to move referenced objects by 
		-- the symbol table. 
		Last_Pos := Scan_New_Heap(Last_Pos);

		-- Swap the current heap and the new heap
		Interp.Heap(Interp.Current_Heap).Bound := 0;
		Interp.Current_Heap := New_Heap;
--declare
--Avail: Heap_Size;
--begin
--Avail := Interp.Heap(Interp.Current_Heap).Size - Interp.Heap(Interp.Current_Heap).Bound;
--Print_Object_Pointer (">>> [GC DONE] Stack ...", Interp.Stack);
--Ada.Text_IO.Put_Line (">>> [GC DONE] BOUND: " & Heap_Size'Image(Interp.Heap(Interp.Current_Heap).Bound) & " AVAIL: " & Heap_Size'Image(Avail));
--Ada.Text_IO.Put_Line (">>> [GC DONE] ----------------------------------------------------------");
--end;
ada.text_io.put_line ("[GC END]");
	end Collect_Garbage;

	function Allocate_Bytes (Interp: access Interpreter_Record;
	                         Bytes:  in     Heap_Size) return Heap_Element_Pointer is

		-- I use this temporary variable not to change Result
		-- if Allocation_Error should be raised.
		Tmp: Heap_Element_Pointer;
	begin
		pragma Assert (Bytes > 0);

-- XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
if DEBUG_GC then
	Collect_Garbage (Interp.all);
end if;
-- XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX

		Tmp := Allocate_Bytes_In_Heap (Interp.Heap(Interp.Current_Heap), Bytes);
		if Tmp = null and then (Interp.Trait.Trait_Bits and No_Garbage_Collection) = 0 then
			Collect_Garbage (Interp.all);
			Tmp := Allocate_Bytes_In_Heap (Interp.Heap(Interp.Current_Heap), Bytes);
			if Tmp = null then
				raise Allocation_Error;
			end if;
		end if;

		return Tmp;
	end Allocate_Bytes;

	function Allocate_Pointer_Object (Interp:  access Interpreter_Record;
	                                  Size:    in     Pointer_Object_Size;
	                                  Initial: in     Object_Pointer) return Object_Pointer is

		subtype Pointer_Object_Record is Object_Record (Pointer_Object, Size);
		type Pointer_Object_Pointer is access all Pointer_Object_Record;

		Ptr: Heap_Element_Pointer;
		Obj_Ptr: Pointer_Object_Pointer;
		for Obj_Ptr'Address use Ptr'Address;
		pragma Import (Ada, Obj_Ptr);
		Result: Object_Pointer;
		for Result'Address use Ptr'Address;
		pragma Import (Ada, Result);
	begin
		Ptr := Allocate_Bytes (
			Interp,
			Heap_Size'(Pointer_Object_Record'Max_Size_In_Storage_Elements)
		);

		Obj_Ptr.all := (
			Kind => Pointer_Object,
			Size => Size,
			Flags => 0,
			Tag => Unknown_Object,
			Scode => Syntax_Code'Val(0),
			Sign => Positive_Sign,
			Pointer_Slot => (others => Initial)
		);

		return Result;
	end Allocate_Pointer_Object;

	function Allocate_Character_Object (Interp: access Interpreter_Record;
	                                    Size:   in     Character_Object_Size) return Object_Pointer is

		subtype Character_Object_Record is Object_Record (Character_Object, Size);
		type Character_Object_Pointer is access all Character_Object_Record;

		Ptr: Heap_Element_Pointer;
		Obj_Ptr: Character_Object_Pointer;
		for Obj_Ptr'Address use Ptr'Address;
		pragma Import (Ada, Obj_Ptr);
		Result: Object_Pointer;
		for Result'Address use Ptr'Address;
		pragma Import (Ada, Result);
	begin
		Ptr := Allocate_Bytes (
			Interp.Self,
			Heap_Size'(Character_Object_Record'Max_Size_In_Storage_Elements)
		);

		Obj_Ptr.all := (
			Kind => Character_Object,
			Size => Size,
			Flags => 0,
			Tag => Unknown_Object,
			Scode => Syntax_Code'Val(0),
			Sign => Positive_Sign,
			Character_Slot => (others => Ch.NUL),
			Character_Terminator => Ch.NUL
		);

		return Result;
	end Allocate_Character_Object;

	function Allocate_Character_Object (Interp: access Interpreter_Record;
	                                    Source: in     Object_Character_Array) return Object_Pointer is
		Result: Object_Pointer;
	begin
		if Source'Length > Character_Object_Size'Last then
			raise Size_Error;
		end if;
		
		Result := Allocate_Character_Object (Interp, Character_Object_Size'(Source'Length));
		Result.Character_Slot := Source;
		return Result;
	end Allocate_Character_Object;

	function Allocate_Byte_Object (Interp: access Interpreter_Record;
	                               Size:   in     Byte_Object_Size) return Object_Pointer is

		subtype Byte_Object_Record is Object_Record (Byte_Object, Size);
		type Byte_Object_Pointer is access all Byte_Object_Record;

		Ptr: Heap_Element_Pointer;
		Obj_Ptr: Byte_Object_Pointer;
		for Obj_Ptr'Address use Ptr'Address;
		pragma Import (Ada, Obj_Ptr);
		Result: Object_Pointer;
		for Result'Address use Ptr'Address;
		pragma Import (Ada, Result);
	begin
		Ptr := Allocate_Bytes (Interp.Self, Heap_Size'(Byte_Object_Record'Max_Size_In_Storage_Elements));
		Obj_Ptr.all := (
			Kind => Byte_Object,
			Size => Size,
			Flags => 0,
			Tag => Unknown_Object,
			Scode => Syntax_Code'Val(0),
			Sign => Positive_Sign,
			Byte_Slot => (others => 0)
		);
		return Result;
	end Allocate_Byte_Object;

	function Allocate_Word_Object (Interp: access Interpreter_Record;
	                               Size:   in     Word_Object_Size) return Object_Pointer is

		subtype Word_Object_Record is Object_Record (Word_Object, Size);
		type Word_Object_Pointer is access all Word_Object_Record;

		Ptr: Heap_Element_Pointer;
		Obj_Ptr: Word_Object_Pointer;
		for Obj_Ptr'Address use Ptr'Address;
		pragma Import (Ada, Obj_Ptr);
		Result: Object_Pointer;
		for Result'Address use Ptr'Address;
		pragma Import (Ada, Result);
	begin
		Ptr := Allocate_Bytes (Interp.Self, Heap_Size'(Word_Object_Record'Max_Size_In_Storage_Elements));
		Obj_Ptr.all := (
			Kind => Word_Object,
			Size => Size,
			Flags => 0,
			Tag => Unknown_Object,
			Scode => Syntax_Code'Val(0),
			Sign => Positive_Sign,
			Word_Slot => (others => 0)
		);
		return Result;
	end Allocate_Word_Object;

	function Allocate_Half_Word_Object (Interp: access Interpreter_Record;
	                                    Size:   in     Half_Word_Object_Size) return Object_Pointer is

		subtype Half_Word_Object_Record is Object_Record (Half_Word_Object, Size);
		type Half_Word_Object_Pointer is access all Half_Word_Object_Record;

		Ptr: Heap_Element_Pointer;
		Obj_Ptr: Half_Word_Object_Pointer;
		for Obj_Ptr'Address use Ptr'Address;
		pragma Import (Ada, Obj_Ptr);
		Result: Object_Pointer;
		for Result'Address use Ptr'Address;
		pragma Import (Ada, Result);
	begin
		Ptr := Allocate_Bytes (Interp.Self, Heap_Size'(Half_Word_Object_Record'Max_Size_In_Storage_Elements));
		Obj_Ptr.all := (
			Kind => Half_Word_Object,
			Size => Size,
			Flags => 0,
			Tag => Unknown_Object,
			Scode => Syntax_Code'Val(0),
			Sign => Positive_Sign,
			Half_Word_Slot => (others => 0)
		);
		return Result;
	end Allocate_Half_Word_Object;

	-----------------------------------------------------------------------------

	procedure Push_Top (Interp: in out Interpreter_Record;
	                    Source: access Object_Pointer) is
		Top: Top_Record renames Interp.Top;
	begin
		if Top.Last >= Top.Data'Last then
			-- Something is wrong. Too many temporary object pointers
			raise Internal_Error; -- TODO: change the exception to something else.
		end if; 

		Top.Last := Top.Last + 1;
		Top.Data(Top.Last) := Top_Datum(Source);
	end Push_Top;

	procedure Pop_Tops (Interp: in out Interpreter_Record; 
	                    Count:  in     Object_Size) is
		Top: Top_Record renames Interp.Top;
	begin
		if Top.Last < Count then
			-- Something is wrong. Too few temporary object pointers
			raise Internal_Error; -- TODO: change the exception to something else.
		end if;
		Top.Last := Top.Last - Count;
	end Pop_Tops;

	procedure Clear_Tops (Interp: in out Interpreter_Record) is
		pragma Inline (Clear_Tops);
		Top: Top_Record renames Interp.Top;
	begin
		Top.Last := Top.Data'First - 1;
	end Clear_Tops;

	-----------------------------------------------------------------------------

	function Make_Cons (Interp:  access Interpreter_Record;
	                    Car:     in     Object_Pointer;
	                    Cdr:     in     Object_Pointer) return Object_Pointer is
		Cons: Object_Pointer;
		Aliased_Car: aliased Object_Pointer := Car;
		Aliased_Cdr: aliased Object_Pointer := Cdr;
	begin
		Push_Top (Interp.all, Aliased_Car'Unchecked_Access);
		Push_Top (Interp.all, Aliased_Cdr'Unchecked_Access);

		Cons := Allocate_Pointer_Object (Interp, Cons_Object_Size, Nil_Pointer);
		Cons.Pointer_Slot(Cons_Car_Index) := Aliased_Car;
		Cons.Pointer_Slot(Cons_Cdr_Index) := Aliased_Cdr;
		Cons.Tag := Cons_Object;

		Pop_Tops (Interp.all, 2);
		return Cons;
	end Make_Cons;

	function Is_Cons (Source: in Object_Pointer) return Standard.Boolean is
		pragma Inline (Is_Cons);
	begin
		return Is_Normal_Pointer(Source) and then 
		       Source.Tag = Cons_Object;
	end Is_Cons;

	function Get_Car (Source: in Object_Pointer) return Object_Pointer is
		pragma Inline (Get_Car);
		pragma Assert (Is_Cons(Source));
	begin
		return Source.Pointer_Slot(Cons_Car_Index);
	end Get_Car;

	procedure Set_Car (Source: in Object_Pointer;
	                   Value:  in Object_Pointer) is
		pragma Inline (Set_Car);
		pragma Assert (Is_Cons(Source));
	begin
		Source.Pointer_Slot(Cons_Car_Index) := Value;
	end Set_Car;

	function Get_Cdr (Source: in Object_Pointer) return Object_Pointer is
		pragma Inline (Get_Cdr);
		pragma Assert (Is_Cons(Source));
	begin
		return Source.Pointer_Slot(Cons_Cdr_Index);
	end Get_Cdr;

	procedure Set_Cdr (Source: in Object_Pointer;
	                   Value:  in Object_Pointer) is
		pragma Inline (Set_Cdr);
		pragma Assert (Is_Cons(Source));
	begin
		Source.Pointer_Slot(Cons_Cdr_Index) := Value;
	end Set_Cdr;

	function Get_Last_Cdr (Source: in Object_Pointer) return Object_Pointer is
		pragma Assert (Is_Cons(Source));
		Ptr: Object_Pointer := Source;
	begin
		loop
			Ptr := Get_Cdr(Ptr);
			exit when not Is_Cons(Ptr);
		end loop;
		return Ptr;
	end Get_Last_Cdr;

	function Reverse_Cons (Source:   in Object_Pointer; 
	                       Last_Cdr: in Object_Pointer := Nil_Pointer) return Object_Pointer is
		pragma Assert (Is_Cons(Source));

		-- Note: The non-nil cdr in the last cons cell gets lost.
		--       e.g.) Reversing (1 2 3 . 4) results in (3 2 1)
		Ptr: Object_Pointer;
		Next: Object_Pointer;
		Prev: Object_Pointer;
	begin
		Prev := Last_Cdr;
		Ptr := Source;
		loop
			Next := Get_Cdr(Ptr);
			Set_Cdr (Ptr, Prev);
			Prev := Ptr;
			exit when not Is_Cons(Next);
			Ptr := Next;
		end loop;
		return Ptr;
	end Reverse_Cons;
	-----------------------------------------------------------------------------

	function Is_String (Source: in Object_Pointer) return Standard.Boolean is
		pragma Inline (Is_String);
	begin
		return Is_Normal_Pointer(Source) and then 
		       Source.Tag = String_Object;
	end Is_String;

	function Make_String (Interp: access  Interpreter_Record;
	                      Source: in      Object_Character_Array) return Object_Pointer is
		Result: Object_Pointer;
	begin
		Result := Allocate_Character_Object(Interp, Source);
		Result.Tag := String_Object;
		return Result;
	end Make_String;

	function Is_Symbol (Source: in Object_Pointer) return Standard.Boolean is
		pragma Inline (Is_Symbol);
	begin
		return Is_Normal_Pointer(Source) and then 
		       Source.Tag = Symbol_Object;
	end Is_Symbol;

	function Make_Symbol (Interp: access Interpreter_Record;
	                      Source: in     Object_Character_Array) return Object_Pointer is
		Ptr: aliased Object_Pointer;
	begin
		-- TODO: the current linked list implementation isn't efficient.
		--       change the symbol table to a hashable table.

		-- Find an existing symbol in the symbol table.	
		Ptr := Interp.Symbol_Table;
		while Ptr /= Nil_Pointer loop
			pragma Assert (Is_Cons(Ptr));

			declare
				Car: Object_Pointer renames Ptr.Pointer_Slot(Cons_Car_Index);
				Cdr: Object_Pointer renames Ptr.Pointer_Slot(Cons_Cdr_Index);
			begin
				pragma Assert (Car.Tag = Symbol_Object);

				if Car.Character_Slot = Source then
					-- the character string contents are the same.
					return Car;
				end if;

				Ptr := Cdr;	
			end;
		end loop;

		-- Create a symbol object
		Ptr := Allocate_Character_Object(Interp, Source);
		Ptr.Tag := Symbol_Object;

		-- Make Ptr safe from GC
		Push_Top (Interp.all, Ptr'Unchecked_Access);

		-- Link the symbol to the symbol table. 
		Interp.Symbol_Table := Make_Cons(Interp.Self, Ptr, Interp.Symbol_Table);

		Pop_Tops (Interp.all, 1);

		return Ptr;
	end Make_Symbol;

	-----------------------------------------------------------------------------

	function Make_Array (Interp: access Interpreter_Record;
	                     Size:   in     Pointer_Object_Size) return Object_Pointer is
		Ptr: Object_Pointer;
	begin
		Ptr := Allocate_Pointer_Object(Interp, Size, Nil_Pointer);
		Ptr.Tag := Array_Object;
		return Ptr;
	end Make_Array;

	function Is_Array (Source: in Object_Pointer) return Standard.Boolean is
		pragma Inline (Is_Array);
	begin
		return Is_Normal_Pointer(Source) and then 
		       Source.Tag = Array_Object;
	end Is_Array;

	-----------------------------------------------------------------------------

	function Make_Bigint (Interp: access Interpreter_Record;
	                      Size:   in     Half_Word_Object_Size) return Object_Pointer is
		Ptr: Object_Pointer;
	begin
		Ptr := Allocate_Half_Word_Object(Interp, Size);
		Ptr.Tag := Bigint_Object;
		return Ptr;
	end Make_Bigint;

	function Make_Bigint (Interp: access Interpreter_Record;
	                      Value:  in     Object_Integer) return Object_Pointer is
		Size: Pointer_Object_Size;
		Ptr: Object_Pointer;
		W: Object_Word;
		H: Object_Half_Word;
	begin
		if Value < 0 then
			W := Object_Word(-(Object_Signed_Word(Value)));
		else 
			W := Object_Word(Value);
		end if;
		
		H := Bigint.Get_High(W);
		if H > 0 then
			Size := 2;
		else
			Size := 1;
		end if;

		Ptr := Allocate_Half_Word_Object(Interp, Size);
		Ptr.Tag := Bigint_Object;
		Ptr.Half_Word_Slot(1) := Bigint.Get_Low(W);

		if H > 0 then
			Ptr.Half_Word_Slot(2) := H;
		end if;

		if Value < 0 then
			Ptr.Sign := Negative_Sign;
		end if;

		return Ptr;
	end Make_Bigint;

	function Is_Bigint (Source: in Object_Pointer) return Standard.Boolean is
		pragma Inline (Is_Bigint);
	begin
		return Is_Normal_Pointer(Source) and then 
		       Source.Tag = Bigint_Object;
	end Is_Bigint;

	-----------------------------------------------------------------------------

	function Make_Frame (Interp:  access Interpreter_Record;
	                     Parent:   in     Object_Pointer; -- current stack pointer
	                     Opcode:  in     Object_Pointer;
	                     Operand: in     Object_Pointer;
	                     Envir:   in     Object_Pointer;
	                     Interm:  in     Object_Pointer) return Object_Pointer is
		Frame: Object_Pointer;
		Aliased_Parent: aliased Object_Pointer := Parent;
		Aliased_Opcode: aliased Object_Pointer := Opcode;
		Aliased_Operand: aliased Object_Pointer := Operand;
		Aliased_Envir: aliased Object_Pointer := Envir;
		Aliased_Interm: aliased Object_Pointer := Interm;

	begin
		Push_Top (Interp.all, Aliased_Parent'Unchecked_Access);
		Push_Top (Interp.all, Aliased_Opcode'Unchecked_Access);
		Push_Top (Interp.all, Aliased_Operand'Unchecked_Access);
		Push_Top (Interp.all, Aliased_Envir'Unchecked_Access);
		Push_Top (Interp.all, Aliased_Interm'Unchecked_Access);

-- TODO: create a Frame in a special memory rather than in Heap Memory.
--       Since it's used for stack, it can be made special.
		Frame := Allocate_Pointer_Object (Interp, Frame_Object_Size, Nil_Pointer);
		Frame.Tag := Frame_Object;
		Frame.Pointer_Slot(Frame_Parent_Index) := Aliased_Parent;
		Frame.Pointer_Slot(Frame_Opcode_Index) := Aliased_Opcode;
		Frame.Pointer_Slot(Frame_Operand_Index) := Aliased_Operand;
		Frame.Pointer_Slot(Frame_Environment_Index) := Aliased_Envir;
		Frame.Pointer_Slot(Frame_Intermediate_Index) := Aliased_Interm;

		Pop_Tops (Interp.all, 5);
		return Frame;
	end Make_Frame;

	function Is_Frame (Source: in Object_Pointer) return Standard.Boolean is
		pragma Inline (Is_Frame);
	begin
		return Is_Normal_Pointer(Source) and then 
		       Source.Tag = Frame_Object;
	end Is_Frame;

	function Get_Frame_Intermediate (Frame: in Object_Pointer) return Object_Pointer is
		pragma Inline (Get_Frame_Intermediate);
		pragma Assert (Is_Frame(Frame));
	begin
		return Frame.Pointer_Slot(Frame_Intermediate_Index);
	end Get_Frame_Intermediate;

	procedure Set_Frame_Intermediate (Frame: in Object_Pointer;
	                                  Value: in Object_Pointer) is
		pragma Inline (Set_Frame_Intermediate);
		pragma Assert (Is_Frame(Frame));

		-- This procedure is not to set a single result,
		-- but to set the result chain. so it can be useful
		-- if you want to migrate a result chain from one frame
		-- to another. It's what this assertion is for.
		pragma Assert (Value = Nil_Pointer or else Is_Cons(Value));
	begin
		Frame.Pointer_Slot(Frame_Intermediate_Index) := Value;
	end Set_Frame_Intermediate;

	procedure Chain_Frame_Intermediate (Interp: in out Interpreter_Record;
	                                    Frame:  in     Object_Pointer;
	                                    Value:  in     Object_Pointer) is
		pragma Inline (Chain_Frame_Intermediate);
		pragma Assert (Is_Frame(Frame));
		V: Object_Pointer;
	begin
		-- Add a new cons cell to the front

		--Push_Top (Interp, Frame'Unchecked_Access);
		--Frame.Pointer_Slot(Frame_Intermediate_Index) :=  
		--	Make_Cons(Interp.Self, Value, Frame.Pointer_Slot(Frame_Intermediate_Index));
		--Pop_Tops (Interp, 1);

		-- This seems to cause a problem if Interp.Stack changes in Make_Cons().
		--Interp.Stack.Pointer_Slot(Frame_Intermediate_Index) :=  
		--	Make_Cons(Interp.Self, Value, Interp.Stack.Pointer_Slot(Frame_Intermediate_Index));

		-- So, let's separate the evaluation and the assignment.
		V := Make_Cons(Interp.Self, Value, Interp.Stack.Pointer_Slot(Frame_Intermediate_Index));
		Interp.Stack.Pointer_Slot(Frame_Intermediate_Index) := V;
	end Chain_Frame_Intermediate;

	function Get_Frame_Result (Frame: in Object_Pointer) return Object_Pointer is
		pragma Inline (Get_Frame_Result);
		pragma Assert (Is_Frame(Frame));
	begin
		return Frame.Pointer_Slot(Frame_Result_Index);
	end Get_Frame_Result;

	procedure Set_Frame_Result (Frame: in Object_Pointer;
	                            Value: in Object_Pointer) is
		pragma Inline (Set_Frame_Result);
		pragma Assert (Is_Frame(Frame));
	begin
		Frame.Pointer_Slot(Frame_Result_Index) := Value;
	end Set_Frame_Result;

	procedure Clear_Frame_Result (Frame: in Object_Pointer) is
	begin
		Frame.Pointer_Slot(Frame_Result_Index) := Nil_Pointer;
	end Clear_Frame_Result;

	function Get_Frame_Environment (Frame: in Object_Pointer) return Object_Pointer is
		pragma Inline (Get_Frame_Environment);
		pragma Assert (Is_Frame(Frame));
	begin
		return Frame.Pointer_Slot(Frame_Environment_Index);
	end Get_Frame_Environment;

	procedure Set_Frame_Environment (Frame: in Object_Pointer;
	                                 Value: in Object_Pointer) is
		pragma Inline (Set_Frame_Environment);
		pragma Assert (Is_Frame(Frame));
	begin
		Frame.Pointer_Slot(Frame_Environment_Index) := Value;
	end Set_Frame_Environment;

	function Get_Frame_Opcode (Frame: in Object_Pointer) return Opcode_Type is
		pragma Inline (Get_Frame_Opcode);
		pragma Assert (Is_Frame(Frame));
	begin
		return Pointer_To_Opcode(Frame.Pointer_Slot(Frame_Opcode_Index));
	end Get_Frame_Opcode;

	procedure Set_Frame_Opcode (Frame:  in Object_Pointer; 
	                            Opcode: in Opcode_Type) is
		pragma Inline (Set_Frame_Opcode);
		pragma Assert (Is_Frame(Frame));
	begin
		Frame.Pointer_Slot(Frame_Opcode_Index) := Opcode_To_Pointer(Opcode);
	end Set_Frame_Opcode;

	function Get_Frame_Operand (Frame: in Object_Pointer) return Object_Pointer is
		pragma Inline (Get_Frame_Operand);
		pragma Assert (Is_Frame(Frame));
	begin
		return Frame.Pointer_Slot(Frame_Operand_Index);
	end Get_Frame_Operand;

	procedure Set_Frame_Operand (Frame: in Object_Pointer;
	                             Value: in Object_Pointer) is
		pragma Inline (Set_Frame_Operand);
		pragma Assert (Is_Frame(Frame));
	begin
		Frame.Pointer_Slot(Frame_Operand_Index) := Value;
	end Set_Frame_Operand;

	function Get_Frame_Parent (Frame: in Object_Pointer) return Object_Pointer is
		pragma Inline (Get_Frame_Parent);
		pragma Assert (Is_Frame(Frame));
	begin
		return Frame.Pointer_Slot(Frame_Parent_Index);
	end Get_Frame_Parent;

	procedure Set_Frame_Parent (Frame: in Object_Pointer;
	                            Value: in Object_Pointer) is
		pragma Inline (Set_Frame_Parent);
		pragma Assert (Is_Frame(Frame));
	begin
		Frame.Pointer_Slot(Frame_Parent_Index) := Value;
	end Set_Frame_Parent;

	procedure Switch_Frame (Frame:   in Object_Pointer;
	                        Opcode:  in Opcode_Type;
	                        Operand: in Object_Pointer;
	                        Interm:  in Object_Pointer) is
	begin
		Set_Frame_Opcode (Frame, Opcode);	
		Set_Frame_Operand (Frame, Operand);	
		Set_Frame_Intermediate (Frame, Interm);
		Set_Frame_Result (Frame, Nil_Pointer);
	end Switch_Frame;

	-----------------------------------------------------------------------------

	--
	-- Environment is a cons cell whose slots represents:
	--   Car: Point to the first key/value pair.
	--   Cdr: Point to Parent environment
	-- 
	-- A key/value pair is held in an array object consisting of 3 slots.
	--   #1: Key
	--   #2: Value
	--   #3: Link to the next key/value array.
	--
	-- Frame.Environment     Interp.Root_Environment
     --   |                      |
	--   |                      V
	--   |     +----+----+    +----+----+
	--   +---> | |  |   ----> | |  | Nil|
	--         +-|--+-----    +-|--+-----
	--           |              |
	--           |              +--> another list
	--           V
	--         +----+----+----+   +----+----+----+   +----+----+----+   +----+----+----+
	--   list: | |  | |  |  ----> | |  | |  | -----> | |  | |  | -----> | |  | |  | Nil|
	--         +-|--+-|-------+   +-|--+-|-------+   +-|--+-|-------+   +-|--+-|-------+
	--           |    |             |    |             |    |             |    |
	--           V    V             V    V             V    V             V    V
	--          Key  Value          Key  Value        Key  Value         Key  Value
	--
	-- Upon initialization, Root_Frame.Environment is equal to Interp.Root_Environment.
	-- CDR(Interp.Root_Environment) is Nil_Pointer.
	--
	-- TODO: Change environment implementation to a hash table or something similar

	function Make_Environment (Interp: access Interpreter_Record;
	                           Parent: in     Object_Pointer) return Object_Pointer is
		pragma Inline (Make_Environment);
	begin
		return Make_Cons(Interp, Nil_Pointer, Parent);
	end Make_Environment;

	function Find_In_Environment_List (Interp: access Interpreter_Record;
	                                   List:   in     Object_Pointer;
	                                   Key:    in     Object_Pointer) return Object_Pointer is
		Arr: Object_Pointer;
	begin
		Arr := List;
		while Arr /= Nil_Pointer loop
			pragma Assert (Is_Array(Arr));
			pragma Assert (Arr.Size = 3);
			
			if Arr.Pointer_Slot(1) = Key then
				return Arr;
			end if;

			Arr := Arr.Pointer_Slot(3);
		end loop;	
		return null; -- not found. 
	end Find_In_Environment_List;

	function Get_Environment (Interp: access Interpreter_Record;
	                          Key:    in     Object_Pointer) return Object_Pointer is
		Envir: Object_Pointer;
		Arr: Object_Pointer;
	begin
		pragma Assert (Is_Symbol(Key));

		Envir := Get_Frame_Environment(Interp.Stack);

		while Envir /= Nil_Pointer loop
			pragma Assert (Is_Cons(Envir));

			Arr := Find_In_Environment_List(Interp, Get_Car(Envir), Key);
			if Arr /= null then
				return Arr.Pointer_Slot(2);
			end if;

			-- Move on to the parent environment
			Envir := Get_Cdr(Envir);
		end loop;
		return null; -- not found
	end Get_Environment;

	function Set_Environment (Interp: access Interpreter_Record;
	                          Key:    in     Object_Pointer;
	                          Value:  in     Object_Pointer) return Object_Pointer is
		Envir: Object_Pointer;
		Arr: Object_Pointer;
	begin
		-- Search the whole environment chain unlike Set_Current_Environment().
		-- It is mainly for set!.
		pragma Assert (Is_Symbol(Key));

		Envir := Get_Frame_Environment(Interp.Stack);
		while Envir /= Nil_Pointer loop
			pragma Assert (Is_Cons(Envir));

			Arr := Find_In_Environment_List(Interp, Get_Car(Envir), Key);
			if Arr /= null then
				-- Overwrite an existing pair
				Arr.Pointer_Slot(2) := Value;
				return Value;
			end if;

			-- Move on to the parent environment
			Envir := Get_Cdr(Envir);
		end loop;
		return null; -- not found. not set
	end Set_Environment;

	procedure Put_Environment (Interp: in out Interpreter_Record;
	                           Envir:  in     Object_Pointer;
	                           Key:    in     Object_Pointer;
	                           Value:  in     Object_Pointer) is
		Arr: Object_Pointer;
	begin
		-- Search the current environment only. It doesn't search the 
		-- environment. If no key is found, add a new pair
		-- This is mainly for define.
		pragma Assert (Is_Symbol(Key));
		pragma Assert (Is_Cons(Envir));

		Arr := Find_In_Environment_List(Interp.Self, Get_Car(Envir), Key);
		if Arr /= null then
			-- Found. Update the existing one
			Arr.Pointer_Slot(2) := Value;
		else
			-- Add a new key/value pair in the current environment
			-- if no existing pair has been found.
			declare 
				Aliased_Envir: aliased Object_Pointer := Envir;
				Aliased_Key: aliased Object_Pointer := Key;
				Aliased_Value: aliased Object_Pointer := Value;
			begin
				Push_Top (Interp, Aliased_Envir'Unchecked_Access);
				Push_Top (Interp, Aliased_Key'Unchecked_Access);
				Push_Top (Interp, Aliased_Value'Unchecked_Access);

				Arr := Make_Array(Interp.Self, 3);
				Arr.Pointer_Slot(1) := Aliased_Key;
				Arr.Pointer_Slot(2) := Aliased_Value;

				-- Chain the pair to the head of the list
				Arr.Pointer_Slot(3) := Get_Car(Aliased_Envir);	
				Set_Car (Aliased_Envir, Arr);
	
				Pop_Tops (Interp, 3);
			end;
		end if;
	end Put_Environment;

	procedure Set_Current_Environment (Interp: in out Interpreter_Record;
	                                   Key:    in     Object_Pointer;
	                                   Value:  in     Object_Pointer) is
		pragma Inline (Set_Current_Environment);
	begin
		Put_Environment (Interp, Get_Frame_Environment(Interp.Stack), Key, Value);
	end Set_Current_Environment;

	procedure Set_Parent_Environment (Interp: in out Interpreter_Record;
	                                   Key:    in     Object_Pointer;
	                                   Value:  in     Object_Pointer) is
		pragma Inline (Set_Parent_Environment);
	begin
		Put_Environment (Interp, Get_Frame_Environment(Get_Frame_Parent(Interp.Stack)), Key, Value);
	end Set_Parent_Environment;

	-----------------------------------------------------------------------------

	function Make_Syntax (Interp: access Interpreter_Record;
	                      Opcode: in     Syntax_Code;
	                      Name:   in     Object_Character_Array) return Object_Pointer is
		Result: Object_Pointer;
	begin
		Result := Make_Symbol(Interp, Name);
		Result.Flags := Result.Flags or Syntax_Object;
		Result.Scode := Opcode;
--Ada.Text_IO.Put ("Creating Syntax Symbol ");
--Put_String (To_Thin_Object_String_Pointer (Result));
		return Result;
	end Make_Syntax;

	function Is_Syntax (Source: in Object_Pointer) return Standard.Boolean is
		pragma Inline (Is_Syntax);
	begin
		return Is_Symbol(Source) and then (Source.Flags and Syntax_Object) /= 0;
	end Is_Syntax;

	function Make_Procedure (Interp: access Interpreter_Record;
	                         Code:   in     Procedure_Code;
	                         Name:   in     Object_Character_Array) return Object_Pointer is
		-- this procedure is for internal use only
		Symbol: aliased Object_Pointer;
		Proc:   aliased Object_Pointer;
	begin
		Push_Top (Interp.all, Symbol'Unchecked_Access);
		Push_Top (Interp.all, Proc'Unchecked_Access);

		-- Make a symbol for the procedure
		Symbol := Make_Symbol(Interp, Name);

		-- Make the actual procedure object
		Proc := Allocate_Pointer_Object(Interp, Procedure_Object_Size, Nil_Pointer);
		Proc.Tag := Procedure_Object;
		Proc.Pointer_Slot(Procedure_Opcode_Index) := Procedure_Code_To_Pointer(Code);

		-- Link it to the top environement
		pragma Assert (Get_Frame_Environment(Interp.Stack) = Interp.Root_Environment); 
		pragma Assert (Get_Environment(Interp.Self, Symbol) = null);
		Set_Current_Environment (Interp.all, Symbol, Proc);

		Pop_Tops (Interp.all, 2);
		return Proc;
	end Make_Procedure;

	function Is_Procedure (Source: in Object_Pointer) return Standard.Boolean is
		pragma Inline (Is_Procedure);
	begin
		return Is_Normal_Pointer(Source) and then 
		       Source.Tag = Procedure_Object;
	end Is_Procedure;

	function Get_Procedure_Opcode (Proc: in Object_Pointer) return Procedure_Code is
		pragma Inline (Get_Procedure_Opcode);
		pragma Assert (Is_Procedure(Proc));
		pragma Assert (Proc.Size = Procedure_Object_Size);
	begin
		return Pointer_To_Procedure_Code(Proc.Pointer_Slot(Procedure_Opcode_Index));
	end Get_Procedure_Opcode;
	
	-----------------------------------------------------------------------------

	function Make_Closure (Interp: access Interpreter_Record;
	                       Code:   in     Object_Pointer;
	                       Envir:  in     Object_Pointer) return Object_Pointer is
		Closure: Object_Pointer;
		Aliased_Code: aliased Object_Pointer := Code;
		Aliased_Envir: aliased Object_Pointer := Envir;
	begin
		Push_Top (Interp.all, Aliased_Code'Unchecked_Access);
		Push_Top (Interp.all, Aliased_Envir'Unchecked_Access);

		Closure := Allocate_Pointer_Object (Interp, Closure_Object_Size, Nil_Pointer);
		Closure.Tag := Closure_Object;
		Closure.Pointer_Slot(Closure_Code_Index) := Aliased_Code;
		Closure.Pointer_Slot(Closure_Environment_Index) := Aliased_Envir;

		Pop_Tops (Interp.all, 2);
		return Closure;
	end Make_Closure;

	function Is_Closure (Source: in Object_Pointer) return Standard.Boolean is
		pragma Inline (Is_Closure);
	begin
		return Is_Normal_Pointer(Source) and then 
		       Source.Tag = Closure_Object;
	end Is_Closure;

	function Get_Closure_Code (Closure: in Object_Pointer) return Object_Pointer is
		pragma Inline (Get_Closure_Code);
		pragma Assert (Is_Closure(Closure));
	begin
		return Closure.Pointer_Slot(Closure_Code_Index);
	end Get_Closure_Code;

	function Get_Closure_Environment (Closure: in Object_Pointer) return Object_Pointer is
		pragma Inline (Get_Closure_Environment);
		pragma Assert (Is_Closure(Closure));
	begin
		return Closure.Pointer_Slot(Closure_Environment_Index);
	end Get_Closure_Environment;

	-----------------------------------------------------------------------------
	function Make_Continuation (Interp: access Interpreter_Record;
	                            Frame:  in     Object_Pointer) return Object_Pointer is
		Cont: Object_Pointer;
		Aliased_Frame: aliased Object_Pointer := Frame;
	begin
		Push_Top (Interp.all, Aliased_Frame'Unchecked_Access);
		Cont := Allocate_Pointer_Object (Interp, Continuation_Object_Size, Nil_Pointer);
		Cont.Tag := Continuation_Object;
		Cont.Pointer_Slot(Continuation_Frame_Index) := Aliased_Frame;
		Pop_Tops (Interp.all, 1);
		return Cont;
	end Make_Continuation;

	function Is_Continuation (Source: in Object_Pointer) return Standard.Boolean is
		pragma Inline (Is_Continuation);
	begin
		return Is_Normal_Pointer(Source) and then 
		       Source.Tag = Continuation_Object;
	end Is_Continuation;

	function Get_Continuation_Frame (Cont: in Object_Pointer) return Object_Pointer is
		pragma Inline (Get_Continuation_Frame);
		pragma Assert (Is_Continuation(Cont));
	begin
		return Cont.Pointer_Slot(Continuation_Frame_Index);
	end Get_Continuation_Frame;

	-----------------------------------------------------------------------------
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

	procedure Close_Stream (Stream: in out Stream_Pointer) is
	begin
		Close (Stream.all);
		Stream := null;
	exception
		when others =>
			Stream := null; -- ignore exception
	end Close_Stream;

	procedure Start_Named_Input_Stream (Interp: in out Interpreter_Record;
	                                    Name:   access Object_Character_Array) is
		package IO_Pool is new H2.Pool (IO_Record, IO_Pointer, Interp.Storage_Pool);

		IO: IO_Pointer := null;
		Stream: Stream_Pointer := null;
	begin
		begin
			IO := IO_Pool.Allocate;
			Interp.Stream.Allocate (Interp, Name, Stream);
		exception
			when others =>	
				if IO /= null then
					if Stream /= null then
						Interp.Stream.Deallocate (Interp, Stream);
					end if;
					IO_Pool.Deallocate (IO);
				end if;
				raise;
		end;

		--IO.Stream := Stream;
		--IO.Pos := IO.Data'First - 1;
		--IO.Last := IO.Data'First - 1;
		--IO.Flags := 0;
		--IO.Next := Interp.Input;
		--Interp.Input := IO;

		IO.all := IO_Record'(
			Stream => Stream,
			Data => (others => Object_Character'First),
			Pos | Last => IO.Data'First - 1,
			Flags => 0,
			Next => Interp.Input,
			Iochar => IO_Character_Record'(End_Character, Object_Character'First)
		);
		Interp.Input := IO;
	end Start_Named_Input_Stream;

	procedure Stop_Named_Input_Stream (Interp: in out Interpreter_Record) is
		package IO_Pool is new H2.Pool (IO_Record, IO_Pointer, Interp.Storage_Pool);
		IO: IO_Pointer;
	begin
		pragma Assert (Interp.Input /= Interp.Base_Input'Unchecked_Access);
		IO := Interp.Input;
		Interp.Input := IO.Next;

		pragma Assert (IO.Stream /= null);
		Close_Stream (IO.Stream);
		Interp.Stream.Deallocate (Interp, IO.Stream);
		IO_Pool.Deallocate (IO);
	end Stop_Named_Input_Stream;

	-----------------------------------------------------------------------------

	procedure Open (Interp:            in out Interpreter_Record;
	                Initial_Heap_Size: in     Heap_Size;
	                Storage_Pool:      in     Storage_Pool_Pointer := null) is

		procedure Initialize_Heap (Size: Heap_Size) is
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
			Dummy := Make_Syntax (Interp.Self, And_Syntax,    Label_And); -- "and"
			Dummy := Make_Syntax (Interp.Self, Begin_Syntax,  Label_Begin); -- "begin"
			Dummy := Make_Syntax (Interp.Self, Case_Syntax,   Label_Case); -- "case"
			Dummy := Make_Syntax (Interp.Self, Cond_Syntax,   Label_Cond); -- "cond"
			Dummy := Make_Syntax (Interp.Self, Define_Syntax, Label_Define); -- "define"
			Dummy := Make_Syntax (Interp.Self, Do_Syntax,     Label_Do); -- "do"
			Dummy := Make_Syntax (Interp.Self, If_Syntax,     Label_If); -- "if"
			Dummy := Make_Syntax (Interp.Self, Lambda_Syntax, Label_Lambda); -- "lamba"
			Dummy := Make_Syntax (Interp.Self, Let_Syntax,    Label_Let); -- "let"
			Dummy := Make_Syntax (Interp.Self, Letast_Syntax, Label_Letast); -- "let*"
			Dummy := Make_Syntax (Interp.Self, Letrec_Syntax, Label_Letrec); -- "letrec"
			Dummy := Make_Syntax (Interp.Self, Or_Syntax,     Label_Or); -- "or"
			Interp.Quote_Symbol := Make_Syntax (Interp.Self, Quote_Syntax,  Label_Quote); -- "quote"
			Interp.Quasiquote_Symbol := Make_Syntax (Interp.Self, Quasiquote_Syntax,  Label_Quasiquote); -- "quasiquote"
			Dummy := Make_Syntax (Interp.Self, Set_Syntax,    Label_Set); -- "set!"
		end Make_Syntax_Objects;

		procedure Make_Procedure_Objects is
			Dummy: Object_Pointer;
		begin

			Dummy := Make_Procedure (Interp.Self, Callcc_Procedure,       Label_Callcc); -- "call-with-current-continuation"
			Dummy := Make_Procedure (Interp.Self, Car_Procedure,          Label_Car); -- "car"
			Dummy := Make_Procedure (Interp.Self, Cdr_Procedure,          Label_Cdr); -- "cdr"
			Dummy := Make_Procedure (Interp.Self, Cons_Procedure,         Label_Cons); -- "cons"
			Dummy := Make_Procedure (Interp.Self, Not_Procedure,          Label_Not); -- "not"


			Dummy := Make_Procedure (Interp.Self, N_Add_Procedure,        Label_N_Add); -- "+"
			Dummy := Make_Procedure (Interp.Self, N_EQ_Procedure,         Label_N_EQ); -- "="
			Dummy := Make_Procedure (Interp.Self, N_GE_Procedure,         Label_N_GE); -- ">="
			Dummy := Make_Procedure (Interp.Self, N_GT_Procedure,         Label_N_GT); -- ">"
			Dummy := Make_Procedure (Interp.Self, N_LE_Procedure,         Label_N_LE); -- "<="
			Dummy := Make_Procedure (Interp.Self, N_LT_Procedure,         Label_N_LT); -- "<"
			Dummy := Make_Procedure (Interp.Self, N_Multiply_Procedure,   Label_N_Multiply); -- "*"
			Dummy := Make_Procedure (Interp.Self, N_Quotient_Procedure,   Label_N_Quotient); -- "quotient"
			Dummy := Make_Procedure (Interp.Self, N_Remainder_Procedure,  Label_N_Remainder); -- "remainder"
			Dummy := Make_Procedure (Interp.Self, N_Subtract_Procedure,   Label_N_Subtract); -- "-"

			Dummy := Make_Procedure (Interp.Self, Q_Boolean_Procedure,    Label_Q_Boolean); -- "boolean?"
			Dummy := Make_Procedure (Interp.Self, Q_Eq_Procedure,         Label_Q_Eq); -- "eq?"
			Dummy := Make_Procedure (Interp.Self, Q_Eqv_Procedure,        Label_Q_Eqv); -- "eqv?"
			Dummy := Make_Procedure (Interp.Self, Q_Null_Procedure,       Label_Q_Null); -- "null?"
			Dummy := Make_Procedure (Interp.Self, Q_Number_Procedure,     Label_Q_Number); -- "number?"
			Dummy := Make_Procedure (Interp.Self, Q_Pair_Procedure,       Label_Q_Pair); -- "pair?"
			Dummy := Make_Procedure (Interp.Self, Q_Procedure_Procedure,  Label_Q_Procedure); -- "procedure?"
			Dummy := Make_Procedure (Interp.Self, Q_String_Procedure,     Label_Q_String); -- "string?"
			Dummy := Make_Procedure (Interp.Self, Q_String_EQ_Procedure,  Label_Q_String_EQ); -- "string=?"
			Dummy := Make_Procedure (Interp.Self, Q_Symbol_Procedure,     Label_Q_Symbol); -- "symbol?"

			Dummy := Make_Procedure (Interp.Self, Setcar_Procedure,       Label_Setcar); -- "set-car!"
			Dummy := Make_Procedure (Interp.Self, Setcdr_Procedure,       Label_Setcdr); -- "set-cdr!"

		end Make_Procedure_Objects;

		procedure Make_Common_Symbol_Objects is
		begin
			Interp.Arrow_Symbol := Make_Symbol (Interp.Self, Label_Arrow);	
			Interp.Else_Symbol := Make_Symbol (Interp.Self, Label_Else);	
		end Make_Common_Symbol_Objects;
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

		Interp.State := 0;
		Interp.Storage_Pool := Storage_Pool;
		Interp.Symbol_Table := Nil_Pointer;

		Interp.Base_Input.Stream := null;
		Interp.Input := Interp.Base_Input'Unchecked_Access;
		Interp.Token := (End_Token, (null, 0, 0));

		Interp.Top := (Interp.Top.Data'First - 1, (others => null));

-- TODO: disallow garbage collecion during initialization.
		Initialize_Heap (Initial_Heap_Size);

		Interp.Root_Environment := Make_Environment(Interp.Self, Nil_Pointer);
		Interp.Root_Frame := Make_Frame(Interp.Self, Nil_Pointer, Opcode_To_Pointer(Opcode_Exit), Nil_Pointer, Interp.Root_Environment, Nil_Pointer);
		Interp.Stack := Interp.Root_Frame;

		Make_Syntax_Objects;
		Make_Procedure_Objects;
		Make_Common_Symbol_Objects;

	exception
		when others =>
			Deinitialize_Heap (Interp);
	end Open;

	procedure Close (Interp: in out Interpreter_Record) is
	begin
		-- Destroy all unstacked named input streams 
		while Interp.Input /= Interp.Base_Input'Unchecked_Access loop
			Stop_Named_Input_Stream (Interp);
		end loop;

		if Interp.Base_Input.Stream /= null then
			-- Close the main input stream.
			Close_Stream (Interp.Base_Input.Stream);
		end if;

		Deinitialize_Heap (Interp);
		Token.Purge (Interp);
	end Close;

	function Get_Storage_Pool (Interp: in Interpreter_Record) return Storage_Pool_Pointer is
	begin
		return Interp.Storage_Pool;	
	end Get_Storage_Pool;

	procedure Set_Option (Interp: in out Interpreter_Record;
	                      Option: in     Option_Record) is
	begin
		case Option.Kind  is
			when Trait_Option =>
				Interp.Trait := Option;
			when Stream_Option =>
				Interp.Stream := Option;
		end case;
	end Set_Option;

	procedure Get_Option (Interp: in out Interpreter_Record;
	                      Option: in out Option_Record) is
	begin
		case Option.Kind  is
			when Trait_Option =>
				Option := Interp.Trait;
			when Stream_Option =>
				Option := Interp.Stream;
		end case;
	end Get_Option;

	procedure Set_Input_Stream  (Interp: in out Interpreter_Record;
	                             Stream: in out Stream_Record'Class) is
	begin
		--Open (Stream, Interp);		
		Open (Stream);

		-- if Open raised an exception, it wouldn't reach here.
		-- so the existing stream still remains intact.
		if Interp.Base_Input.Stream /= null then
			Close_Stream (Interp.Base_Input.Stream);
		end if;

		Interp.Base_Input := IO_Record'(
			Stream => Stream'Unchecked_Access,
			Data => (others => Object_Character'First),
			Pos | Last => Interp.Base_Input.Data'First - 1,
			Flags => 0,
			Next => null,
			Iochar => IO_Character_Record'(End_Character, Object_Character'First)
		);
	end Set_Input_Stream;

	--procedure Set_Output_Stream (Interp: in out Interpreter_Record;
	--                             Stream: in out Stream_Record'Class) is
	--begin
	--	
	--end Set_Output_Stream;

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
					Ada.Text_IO.Put ("()");

				when True_Word =>
					Ada.Text_IO.Put ("#t");

				when False_Word =>
					Ada.Text_IO.Put ("#f");

				when others => 
					case Atom.Tag is
						when Cons_Object =>
							-- Cons_Object must not reach here.
							raise Internal_Error;

						when Symbol_Object =>
							Output_Character_Array (Atom.Character_Slot);

						when String_Object =>
							Ada.Text_IO.Put ("""");	
							Output_Character_Array (Atom.Character_Slot);
							Ada.Text_IO.Put ("""");	
	
						when Closure_Object =>
							Ada.Text_IO.Put ("#Closure");
	
						when Continuation_Object =>
							declare
								w: object_word;
								for w'address use Atom'address;
							begin
								Ada.Text_IO.Put ("#Continuation[" & object_word'image(w) & "]");
							end;
	
						when Procedure_Object =>
							Ada.Text_IO.Put ("#Procedure");
	
						when Array_Object =>
							Ada.Text_IO.Put ("#Array");
				

						when Bigint_Object => 
							Ada.Text_IO.Put ("#Bigint(");
declare
package Int_IO is new ada.text_io.modular_IO(object_half_word);
begin
if Atom.Sign = Negative_Sign then
ada.text_io.put ("-");
else
ada.text_io.put ("+");
end if;
for I in reverse Atom.Half_Word_Slot'Range loop
ada.text_io.put (" ");
int_io.put (Atom.Half_Word_Slot(I), base=>16);
end loop;
end;

							Ada.Text_IO.Put(")");


						when Others =>
							if Atom.Kind = Character_Object then
								Output_Character_Array (Atom.Character_Slot);
							else
								Ada.Text_IO.Put ("#NOIMPL# => " & Object_Tag'Image(Atom.Tag));
							end if;
						end case;
				end case;
			end Print_Pointee;

			procedure Print_Integer is
				 X: constant Object_Integer := Pointer_To_Integer (Atom);
			begin
				Ada.Text_IO.Put (Object_Integer'Image(X));
			end Print_Integer;

			procedure Print_Character is
				 X: constant Object_Character := Pointer_To_Character (Atom);
			begin
				Ada.Text_IO.Put (Object_Character'Image(X));
			end Print_Character;

			procedure Print_Byte is
				 X: constant Object_Byte := Pointer_To_Byte (Atom);
			begin
				Ada.Text_IO.Put (Object_Byte'Image(X));
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
			if Is_Cons(Obj) then
				Cons := Obj;
				Ada.Text_IO.Put ("(");

				loop
					Car := Get_Car(Cons);

					if Is_Cons(Car)  or else Is_Array(Car) then
						Print_Object (Car);
					else
						Print_Atom (Car);
					end if;

					Cdr := Get_Cdr(Cons);
					if Is_Cons(Cdr) then
						Ada.Text_IO.Put (" ");
						Cons := Cdr;
						exit when Cons = Nil_Pointer;
					else
						if Cdr /= Nil_Pointer then
							Ada.Text_IO.Put (" . ");
							Print_Atom (Cdr);
						end if;
						exit;
					end if;
				end loop;

				Ada.Text_IO.Put (")");
			elsif Is_Array(Obj) then
				Ada.Text_IO.Put (" #(");
				for X in Obj.Pointer_Slot'Range loop
					if Is_Cons(Obj.Pointer_Slot(X)) or else Is_Array(Obj.Pointer_Slot(X)) then
						Print_Object (Obj.Pointer_Slot(X));
					else
						Print_Atom (Obj.Pointer_Slot(X));
					end if;
				end loop;
				Ada.Text_IO.Put (") ");
			else
				Print_Atom (Obj);
			end if;
		end Print_Object;


		Stack: Object_Pointer; -- TODO: make it into the interpreter_Record so that GC can workd
		Opcode: Object_Integer;
		Operand: Object_Pointer;

	begin

if DEBUG_GC then
Print_Object (Source); -- use a recursive version 
Ada.Text_IO.New_Line;
return;
end if;

		-- TODO: Let Make_Frame use a dedicated stack space that's apart from the heap.
		--       This way, the stack frame doesn't have to be managed by GC.

-- TODO: use a interp.Stack.
-- TODO: use Push_Frame
		Stack := Make_Frame (Interp.Self, Nil_Pointer, Integer_To_Pointer(0), Nil_Pointer, Nil_Pointer, Nil_Pointer);  -- just for get_frame_environment...

		Opcode := 1;
		Operand := Source;

		loop
			case Opcode is
				when 1 =>
					if Is_Cons(Operand) then
						-- push cdr
						Stack := Make_Frame (Interp.Self, Stack, Integer_To_Pointer(2), Get_Cdr(Operand), Nil_Pointer, Nil_Pointer); -- push cdr
						Ada.Text_IO.Put ("(");
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
							Stack := Stack.Pointer_Slot(Frame_Parent_Index); -- pop
						end if;
					end if;
	
				when 2 =>
	
					if Is_Cons(Operand) then
						-- push cdr
						Stack := Make_Frame (Interp.Self, Stack, Integer_To_Pointer(2), Get_Cdr(Operand), Nil_Pointer, Nil_Pointer); -- push
						Ada.Text_IO.Put (" ");
						Operand := Get_Car(Operand); -- car
						Opcode := 1;
					else
						if Operand /= Nil_Pointer then
							-- cdr of the last cons cell is not null.
							Ada.Text_IO.Put (" . ");
							Print_Atom (Operand);
						end if;
						Ada.Text_IO.Put (")");
	
						if Stack = Nil_Pointer then
							Opcode := 0; -- stack empty. arrange to exit
						else
							Opcode := Pointer_To_Integer(Stack.Pointer_Slot(Frame_Opcode_Index));
							Operand := Stack.Pointer_Slot(Frame_Operand_Index);
							Stack := Stack.Pointer_Slot(Frame_Parent_Index); -- pop 
						end if;
					end if;
	
				when others =>
					exit;
			end case;
		end loop;

		Ada.Text_IO.New_Line;
	end Print;

	function Insert_Frame (Interp:  access Interpreter_Record;
	                       Parent:  in     Object_Pointer;
	                       Opcode:  in     Opcode_Type; 
	                       Operand: in     Object_Pointer;
	                       Envir:   in     Object_Pointer;
	                       Interm:  in     Object_Pointer) return Object_Pointer is
		pragma Inline (Insert_Frame);
		pragma Assert (Parent = Nil_Pointer or else Is_Frame(Parent));
	begin
		return Make_Frame(Interp, Parent, Opcode_To_Pointer(Opcode), Operand, Envir, Interm);
	end Insert_Frame;

	procedure Push_Frame (Interp:  in out Interpreter_Record;
	                      Opcode:  in     Opcode_Type; 
	                      Operand: in     Object_Pointer) is
		pragma Inline (Push_Frame);
	begin
		Interp.Stack := Insert_Frame(Interp.Self, Interp.Stack, Opcode, Operand, Get_Frame_Environment(Interp.Stack), Nil_Pointer);
	end Push_Frame;

	procedure Push_Frame_With_Environment (Interp:  in out Interpreter_Record;
	                                       Opcode:  in     Opcode_Type; 
	                                       Operand: in     Object_Pointer;
	                                       Envir:   in     Object_Pointer) is
		pragma Inline (Push_Frame_With_Environment);
	begin
		Interp.Stack := Insert_Frame(Interp.Self, Interp.Stack, Opcode, Operand, Envir, Nil_Pointer);
	end Push_Frame_With_Environment;

	procedure Push_Frame_With_Environment_And_Intermediate (Interp:  in out Interpreter_Record;
	                                                        Opcode:  in     Opcode_Type; 
	                                                        Operand: in     Object_Pointer;
	                                                        Envir:   in     Object_Pointer;
	                                                        Interm:  in     Object_Pointer) is
		pragma Inline (Push_Frame_With_Environment_And_Intermediate);
	begin
		Interp.Stack := Insert_Frame(Interp.Self, Interp.Stack, Opcode, Operand, Envir, Interm);
	end Push_Frame_With_Environment_And_Intermediate;

	procedure Push_Frame_With_Intermediate (Interp:  in out Interpreter_Record;
	                                        Opcode:  in     Opcode_Type; 
	                                        Operand: in     Object_Pointer;
	                                        Interm:  in     Object_Pointer) is
		pragma Inline (Push_Frame_With_Intermediate);
	begin
		-- Place a new frame below the existing top frame.
		Interp.Stack := Insert_Frame (Interp.Self, Interp.Stack, Opcode, Operand, Get_Frame_Environment(Interp.Stack), Interm);
	end Push_Frame_With_Intermediate;

	procedure Push_Subframe (Interp:  in out Interpreter_Record;
	                         Opcode:  in     Opcode_Type; 
	                         Operand: in     Object_Pointer) is
		pragma Inline (Push_Subframe);
	begin
		-- Place a new frame below the existing top frame.
		Set_Frame_Parent (Interp.Stack, Insert_Frame(Interp.Self, Get_Frame_Parent(Interp.Stack), Opcode, Operand, Get_Frame_Environment(Interp.Stack), Nil_Pointer));
	end Push_Subframe;

	procedure Push_Subframe_With_Environment (Interp:  in out Interpreter_Record;
	                                          Opcode:  in     Opcode_Type; 
	                                          Operand: in     Object_Pointer;
	                                          Envir:   in     Object_Pointer) is
		pragma Inline (Push_Subframe_With_Environment);
	begin
		-- Place a new frame below the existing top frame.
		Set_Frame_Parent (Interp.Stack, Insert_Frame(Interp.Self, Get_Frame_Parent(Interp.Stack), Opcode, Operand, Envir, Nil_Pointer));
	end Push_Subframe_With_Environment;

	procedure Push_Subframe_With_Intermediate (Interp:  in out Interpreter_Record;
	                                           Opcode:  in     Opcode_Type; 
	                                           Operand: in     Object_Pointer;
	                                           Interm:  in     Object_Pointer) is
		pragma Inline (Push_Subframe_With_Intermediate);
	begin
		-- Place a new frame below the existing top frame.
		Set_Frame_Parent (Interp.Stack, Insert_Frame(Interp.Self, Get_Frame_Parent(Interp.Stack), Opcode, Operand, Get_Frame_Environment(Interp.Stack), Interm));
	end Push_Subframe_With_Intermediate;

	procedure Push_Subframe_With_Environment_And_Intermediate (Interp:  in out Interpreter_Record;
	                                                           Opcode:  in     Opcode_Type; 
	                                                           Operand: in     Object_Pointer;
	                                                           Envir:   in     Object_Pointer;
	                                                           Interm:  in     Object_Pointer) is
		pragma Inline (Push_Subframe_With_Environment_And_Intermediate);
	begin
		-- Place a new frame below the existing top frame.
		Set_Frame_Parent (Interp.Stack, Insert_Frame(Interp.Self, Get_Frame_Parent(Interp.Stack), Opcode, Operand, Envir, Interm));
	end Push_Subframe_With_Environment_And_Intermediate;

	procedure Pop_Frame (Interp: in out Interpreter_Record) is
		pragma Inline (Pop_Frame);
	begin
		pragma Assert (Interp.Stack /= Interp.Root_Frame);
		pragma Assert (Interp.Stack /= Nil_Pointer);
		Interp.Stack := Interp.Stack.Pointer_Slot(Frame_Parent_Index); -- pop 
	end Pop_Frame;

	procedure Return_Frame (Interp: in out Interpreter_Record;
	                        Value:  in     Object_Pointer) is
		pragma Inline (Return_Frame);
	begin
		-- Remove the current frame and return a value 
		-- to a new active(top) frame.
		Pop_Frame (Interp);
		Set_Frame_Result (Interp.Stack, Value);
	end Return_Frame;

	procedure Reload_Frame (Interp:  in out Interpreter_Record;
	                        Opcode:  in     Opcode_Type; 
	                        Operand: in     Object_Pointer) is
		pragma Inline (Reload_Frame);
		Envir: Object_Pointer;
	begin
		-- Change various frame fields keeping the environment.
		Envir := Get_Frame_Environment(Interp.Stack);
		Pop_Frame (Interp);
		Push_Frame_With_Environment (Interp, Opcode, Operand, Envir);
	end Reload_Frame;

	procedure Reload_Frame_With_Environment (Interp:  in out Interpreter_Record;
	                                         Opcode:  in     Opcode_Type; 
	                                         Operand: in     Object_Pointer;
	                                         Envir:   in     Object_Pointer) is
		pragma Inline (Reload_Frame_With_Environment);
	begin
		-- Change various frame fields
		Pop_Frame (Interp);
		Push_Frame_With_Environment (Interp, Opcode, Operand, Envir);
	end Reload_Frame_With_Environment;

	procedure Reload_Frame_With_Intermediate (Interp:  in out Interpreter_Record;
	                                          Opcode:  in     Opcode_Type; 
	                                          Operand: in     Object_Pointer;
	                                          Interm:  in     Object_Pointer) is
		pragma Inline (Reload_Frame_With_Intermediate);
		Envir: Object_Pointer;
	begin
		-- Change various frame fields keeping the environment.
		Envir := Get_Frame_Environment(Interp.Stack);
		Pop_Frame (Interp);
		Push_Frame_With_Environment_And_Intermediate (Interp, Opcode, Operand, Envir, Interm);
	end Reload_Frame_With_Intermediate;

	procedure Execute (Interp: in out Interpreter_Record) is separate;

	procedure Evaluate (Interp: in out Interpreter_Record;
	                    Source: in     Object_Pointer;
	                    Result: out    Object_Pointer) is
	begin
		Result := Nil_Pointer;

		-- Perform some clean ups in case the procedure is called
		-- again after an exception is raised
		Clear_Tops (Interp);
		Interp.Stack := Interp.Root_Frame;
		Clear_Frame_Result (Interp.Stack);

		-- Push an actual frame for evaluation
		Push_Frame (Interp, Opcode_Evaluate_Object, Source);

		Execute (Interp);

		pragma Assert (Interp.Stack = Interp.Root_Frame);
		pragma Assert (Get_Frame_Opcode(Interp.Stack) = Opcode_Exit);

		Result := Get_Frame_Result(Interp.Stack);
		Clear_Frame_Result (Interp.Stack); 
	end Evaluate;

	procedure Run_Loop (Interp: in out Interpreter_Record;
	                    Result: out    Object_Pointer) is
		-- standard read-eval-print loop
		Aliased_Result: aliased Object_Pointer;
	begin
		pragma Assert (Interp.Base_Input.Stream /= null);

DEBUG_GC := Standard.True;

		Result := Nil_Pointer;

		-- Perform some clean ups in case the procedure is called
		-- again after an exception is raised
		Clear_Tops (Interp);
		Interp.Stack := Interp.Root_Frame;
		Clear_Frame_Result (Interp.Stack);

		Push_Top (Interp, Aliased_Result'Unchecked_Access);
		loop
			pragma Assert (Interp.Stack = Interp.Root_Frame);
			pragma Assert (Get_Frame_Result(Interp.Stack) = Nil_Pointer);

			--Push_Frame (Interp, Opcode_Print_Result, Nil_Pointer);
			Push_Frame (Interp, Opcode_Evaluate_Result, Nil_Pointer);
			Push_Frame (Interp, Opcode_Read_Object, Nil_Pointer);
			Execute (Interp);

			pragma Assert (Interp.Stack = Interp.Root_Frame);
			pragma Assert (Get_Frame_Opcode(Interp.Stack) = Opcode_Exit);

			Aliased_Result := Get_Frame_Result(Interp.Stack); 
			Clear_Frame_Result (Interp.Stack);

Ada.Text_IO.Put ("RESULT: ");
Print (Interp, Aliased_Result);
Ada.Text_IO.Put_Line (">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> LOOP ITERATION XXXXXX CHECKPOINT"); 
		end loop;


		-- Jump into the exception handler not to repeat the same code here.
		-- In fact, this part must not be reached since the loop above can't
		-- be broken.
		raise Stream_End_Error;

	exception
		when Stream_End_Error =>
			-- this is not a real error. this indicates the end of input stream.
declare
A: aliased Object_Pointer;
B: aliased Object_Pointer;
begin
Push_Top (Interp, A'Unchecked_Access);
Push_Top (Interp, B'Unchecked_Access);
--A := Make_Bigint(Interp.Self, Value => 16#0FFFFFFF_FFFFFFFF#);
--B := Make_Bigint(Interp.Self, Value => 16#0FFFFFFF_FFFFFFFF#);
--for I in 1 .. 11 loop
--A := Bigint.Add(Interp.Self, A, B);
--end loop;
A := Make_Bigint(Interp.Self, Value => 16#FFFF_00000001#);
--B := Make_Bigint(Interp.Self, Value => 16#FFFF_0000000F#);
B := Make_Bigint(Interp.Self, Value => 16#FFFFFF_00000001#);
B.sign := Negative_Sign;

A := Make_Bigint(Interp.Self, Size => 4);
A.Half_Word_Slot(4) := 16#11FFFFFF#;
Bigint.Multiply(Interp, A, integer_to_pointer(2), A);
Bigint.Add(Interp, A, A, A);

B := Make_Bigint(Interp.Self, Size => 4);
B.Half_Word_Slot(4) := 16#22FFFFFF#;
Bigint.Subtract(Interp, B, integer_to_pointer(1), B);
--A := Bigint.Divide(Interp, A, integer_to_pointer(0));

print (interp, A);
print (interp, B);
declare
q, r: object_Pointer;
begin
	--Bigint.Divide (Interp, integer_to_pointer(-10), integer_to_pointer(6), Q, R);
	
	Bigint.Divide (Interp, A, B, Q, R);
ada.text_io.put ("Q => "); print (interp, Q);
ada.text_io.put ("R => "); print (interp, R);

bigint.to_string (interp, r, 16, r);
--bigint.to_string (interp, r, 10, r);

end;
Pop_tops (Interp, 2);
end;
			Ada.Text_IO.Put_LINE ("=== BYE ===");
			Pop_Tops (Interp, 1);
			if Aliased_Result /= null then
				Result := Aliased_Result;
			end if;

		when X: others =>
			Ada.TEXT_IO.PUT_LINE ("ERROR ERROR ERROR -> " & Ada.Exceptions.Exception_Name(X));
			raise;
	end Run_Loop;
	
	-----------------------------------------------------------------------------
--
--	function h2scm_open return Interpreter_Pointer;
--	pragma Export (C, h2scm_open, "h2scm_open");
--
--	procedure h2scm_close (Interp: in out Interpreter_Pointer);
--	pragma Export (C, h2scm_close, "h2scm_close");
--
--	function h2scm_evaluate (Interp: access Interpreter_Record;
--	                         Source: in     Object_Pointer) return Interfaces.C.int;
--	pragma Export (C, h2scm_evaluate, "h2scm_evaluate");
--
--	procedure h2scm_dealloc is new 
--		Ada.Unchecked_Deallocation (Interpreter_Record, Interpreter_Pointer);
--
--	function h2scm_open return Interpreter_Pointer is
--		Interp: Interpreter_Pointer;	
--	begin
--		begin
--			Interp := new Interpreter_Record;
--		exception
--			when others =>
--				return null;
--		end;
--
--		begin
--			Open (Interp.all, 1_000_000, null);
--		exception
--			when others =>
--				h2scm_dealloc (Interp);
--				return null;
--		end;
--
--		return Interp;
--	end h2scm_open;
--
--	procedure h2scm_close (Interp: in out Interpreter_Pointer) is
--	begin
--Text_IO.Put_Line ("h2scm_close");
--		Close (Interp.all);	
--		h2scm_dealloc (Interp);
--	end h2scm_close;
--
--	function h2scm_evaluate (Interp: access Interpreter_Record;
--	                         Source: in     Object_Pointer) return Interfaces.C.int is
--	begin
--		return Interfaces.C.int(Interp.Heap(Interp.Current_Heap).Size);
--	end h2scm_evaluate;
	
end H2.Scheme;
