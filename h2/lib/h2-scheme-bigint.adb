with H2.Pool;

separate (H2.Scheme)

-- The code here assumes that Half_Word_Slot'First is 1. 
-- The code breaks if you change the array range to something else, 

package body Bigint is

	use type System.Bit_Order;

	Big_Endian: constant := Standard.Boolean'Pos (
		System.Default_Bit_Order = System.High_Order_First
	);
	Little_Endian: constant := Standard.Boolean'Pos (
		System.Default_Bit_Order = System.Low_Order_First
	);

	--Half_Word_Bits: constant := Object_Pointer_Bits / 2;
	Half_Word_Bits: constant := Object_Half_Word'Size;
	Half_Word_Bytes: constant := Half_Word_Bits / System.Storage_Unit;

	type Word_Record is record
		Low: Object_Half_Word;
		High: Object_Half_Word;
	end record;
	for Word_Record use record
		--Low at 0 range 0 .. Half_Word_Bits - 1;
		--High at 0 range Half_Word_Bits .. Word_Bits - 1;
		Low  at Half_Word_Bytes * (0 * Little_Endian + 1 * Big_Endian)
		     range 0 .. Half_Word_Bits - 1;
		High at Half_Word_Bytes * (1 * Little_Endian + 0 * Big_Endian)
		     range 0 .. Half_Word_Bits - 1;
	end record;
	for Word_Record'Size use Object_Word'Size;
	--for Word_Record'Size use Object_Pointer_Bits;
	--for Word_Record'Alignment use Object_Word'Alignment;
	--for Word_Record'Scalar_Storage_Order use System.High_Order_First;
	--for Word_Record'Bit_Order use System.High_Order_First;
	--for Word_Record'Bit_Order use System.Low_Order_First;

	type Half_Word_Bit_Array is array(1 .. Half_Word_Bits) of Object_Bit;
	pragma Pack (Half_Word_Bit_Array);
	for Half_Word_Bit_Array'Size use Half_Word_Bits;

	type Block_Divisor_Record is record
		Low: Object_Half_Word; -- low half-word of divisor
		High: Object_Half_Word; -- high half-word of divisor
		Length: Object_Size; -- number of digits
	end record;
	Block_Divisors: array (Object_Radix) of Block_Divisor_Record;
	Block_Divisors_Initialized: Standard.Boolean := Standard.False;
	
	-------------------------------------------------------------------------

	function Get_Low (W: in Object_Word) return Object_Half_Word is
		R: Word_Record;
		for R'Address use W'Address;
	begin
		return R.Low;	
	end Get_Low;

	function Get_High (W: in Object_Word) return Object_Half_Word is
		R: Word_Record;
		for R'Address use W'Address;
	begin
		return R.High;	
	end Get_High;

	function Make_Word (L: in Object_Half_Word;
	                    H: in Object_Half_Word) return Object_Word is
		W: Object_Word;
		R: Word_Record;
		for R'Address use W'Address;
	begin
		R.Low := L;
		R.High := H;
		return W;	
	end Make_Word;

	function Decode_To_Word (X:    in     Object_Pointer;
	                         Word: access Object_Word;
	                         Sign: access Object_Sign) return Standard.Boolean is
	begin
		if Is_Integer(X) then
			declare
				I: Object_Integer := Pointer_To_Integer(X);
			begin
				if I < 0 then
					-- Convert the negative number to a positive word.
					Word.all := Object_Word(-(I + 1)) + 1;
					Sign.all := Negative_Sign; 
				else
					Word.all := Object_Word(I);
					Sign.all := Positive_Sign;
				end if;
			end;
		else
			case X.Size is
				when 1 =>
					Word.all := Object_Word(X.Half_Word_Slot(1));
				when 2 =>
					Word.all := Make_Word(X.Half_Word_Slot(1), X.Half_Word_Slot(2));
				when others =>
					return Standard.False;
			end case;
			Sign.all := X.Sign;
		end if;
		return Standard.True;
	end Decode_To_Word;

	procedure Convert_Word_To_Text (Word:   in     Object_Word; 
	                                Radix:  in     Object_Radix;
	                                Buffer: in out Object_Character_Array;
	                                Length: out    Object_Size) is
		V: Object_Word;
		W: Object_Word := Word;
		Len: Object_Size := 0;
	begin
		loop
			V := W rem Object_Word(Radix);
			
			if V in 0 .. 9 then
				Buffer(Buffer'First + Len) := Object_Character'Val(Object_Character'Pos(Ch.Zero) + V);
			else
				Buffer(Buffer'First + Len) := Object_Character'Val(Object_Character'Pos(Ch.UC_A) + V - 10);
			end if;
			Len := Len + 1;
			
			W := W / Object_Word(Radix);
			exit when W <= 0;
		end loop;
			
		Length := Len; 
	end Convert_Word_To_Text;
	-------------------------------------------------------------------------

	function Is_Less_Unsigned_Array (X:  in Object_Half_Word_Array;
	                                 XS: in Half_Word_Object_Size;
	                                 Y:  in Object_Half_Word_Array;
	                                 YS: in Half_Word_Object_Size) return Standard.Boolean is
		pragma Inline (Is_Less_Unsigned_Array);
	begin
		if XS /= YS then
			return XS < YS;
		end if;

		for I in reverse 1 .. XS loop
			if X(I) /= Y(I) then
				return X(I) < Y(I);
			end if;
		end loop;

		return Standard.False;
	end Is_Less_Unsigned_Array;
	
	function Is_Less_Unsigned (X: in Object_Pointer;
	                           Y: in Object_Pointer) return Standard.Boolean is
		pragma Inline (Is_Less_Unsigned);
	begin
		return Is_Less_Unsigned_Array(X.Half_Word_Slot, X.Size, Y.Half_Word_Slot, Y.Size);
	end Is_Less_Unsigned;

	function Is_Less (X: in Object_Pointer;
	                  Y: in Object_Pointer) return Standard.Boolean is
	begin
		if X.Sign /= Y.Sign then
			return X.Sign = Negative_Sign;
		end if;
		return Is_Less_Unsigned(X, Y);
	end Is_Less;

	function Is_Equal (X: in Object_Pointer;
	                   Y: in Object_Pointer) return Standard.Boolean is
	begin
		return X.Sign = Y.Sign and then 
		       X.Size = Y.Size and then
		       X.Half_Word_Slot = Y.Half_Word_Slot;
	end Is_Equal;

	function Is_Zero (X: in Object_Pointer) return Standard.Boolean is
		pragma Inline (Is_Zero);
	begin
		return X.Size = 1 and then X.Half_Word_Slot(1) = 0;
	end Is_Zero;

	function Is_One_Unsigned (X: in Object_Pointer) return Standard.Boolean is
		pragma Inline (Is_One_Unsigned);
	begin
		return X.Size = 1 and then X.Half_Word_Slot(1) = 1;
	end Is_One_Unsigned;

	-------------------------------------------------------------------------
	function Copy_Upto (Interp: access Interpreter_Record;
	                    X:      in     Object_Pointer;
	                    Last:   in     Half_Word_Object_Size) return Object_Pointer is
		pragma Assert (Last <= X.Size);
		A: aliased Object_Pointer := X;
		Z: Object_Pointer;
	begin
		Push_Top (Interp.all, A'Unchecked_Access);
		Z := Make_Bigint(Interp, Size => Last);
		Pop_Tops (Interp.all, 1);
		Z.Sign := A.Sign;
		Z.Half_Word_Slot := A.Half_Word_Slot(1 .. Last);
		return Z;
	end Copy_Upto;

	function Count_Effective_Array_Slots (X:  in Object_Half_Word_Array; 
	                                      XS: in Half_Word_Object_Size) return Half_Word_Object_Size is
		pragma Inline (Count_Effective_Array_Slots);
		Last: Half_Word_Object_Size := 1;
	begin
		for I in reverse 1 .. XS loop
			if X(I) /= 0 then
				Last := I;
				exit;
			end if;
		end loop;
		return Last;
	end Count_Effective_Array_Slots;
	
	function Count_Effective_Slots (X: in Object_Pointer) return Half_Word_Object_Size is
		pragma Inline (Count_Effective_Slots);
	begin
		return Count_Effective_Array_Slots(X.Half_Word_Slot, X.Size);
	end Count_Effective_Slots;

	function Normalize (Interp: access Interpreter_Record;
	                    X:      in     Object_Pointer) return Object_Pointer is
		Last: Half_Word_Object_Size;
	begin
		Last := Count_Effective_Slots(X);

		case Last is
			when 1 =>
				if X.Sign = Negative_Sign then
					return Integer_To_Pointer(-Object_Integer(X.Half_Word_Slot(1)));
				else
					return Integer_To_Pointer(Object_Integer(X.Half_Word_Slot(1)));
				end if;

			when 2 =>
				declare
					W: Object_Word := Make_Word(X.Half_Word_Slot(1), X.Half_Word_Slot(2));
				begin
					if X.Sign = Negative_Sign then
						if W in 0 .. Object_Word(-Object_Signed_Word(Object_Integer'First)) then
							return Integer_To_Pointer(-Object_Integer(W));
						end if;
					else
						if W in 0 .. Object_Word(Object_Integer'Last) then
							return Integer_To_Pointer(Object_Integer(W));
						end if;
					end if;
				end;

			when others =>
				null;
		end case;

		if X.Size = Last then
			-- No compaction is needed. return it as it is
			return X;
		end if;

		-- Remove unneeded slots and clone meaningful contents only.
		return Copy_Upto(Interp, X, Last);
	end Normalize;

	-------------------------------------------------------------------------

	generic
		with function Operator (X: in Object_Integer; 
		                        Y: in Object_Integer) return Object_Integer;
	procedure Plain_Integer_Op (Interp: in out Interpreter_Record;
	                            X:      in out Object_Pointer;
	                            Y:      in out Object_Pointer;
	                            Z:      out    Object_Pointer);

	procedure Plain_Integer_Op (Interp: in out Interpreter_Record;
	                            X:      in out Object_Pointer;
	                            Y:      in out Object_Pointer;
	                            Z:      out    Object_Pointer) is
		A: aliased Object_Pointer := X;
		B: aliased Object_Pointer := Y;
	begin
		if Is_Integer(A) and then Is_Integer(B) then
			declare
				--pragma Unsuppress (Range_Check);
				--pragma Unsuppress (Overflow_Check);

				G: Object_Integer := Pointer_To_Integer(A);
				H: Object_Integer := Pointer_To_Integer(B);
			begin
				X := A;
				Y := B;
				Z := Integer_To_Pointer(Operator(G, H));
				return;
			exception
				when Constraint_Error =>
-- TODO: don't count on Constraint_Error exception.
					Push_Top (Interp, A'Unchecked_Access);
					Push_Top (Interp, B'Unchecked_Access);
-- TODO: allocate A and B from a non-GC heap.
-- I know that pointers returned by Make_Bigint here are short-lived
-- and not needed after actual operation. non-GC heap is a better choice.
					A := Make_Bigint(Interp.Self, Value => G);
					B := Make_Bigint(Interp.Self, Value => H);
					Pop_Tops (Interp, 2);
			end;
		else
			Push_Top (Interp, A'Unchecked_Access);
			Push_Top (Interp, B'Unchecked_Access);
			if Is_Integer(A) then
				A := Make_Bigint(Interp.Self, Value => Pointer_To_Integer(A));
			end if;
			if Is_Integer(B) then
				B := Make_Bigint(Interp.Self, Value => Pointer_To_Integer(B));
			end if;
			Pop_Tops (Interp, 2);
		end if;

		X := A;
		Y := B;
		Z := null;
	end Plain_Integer_Op;

	procedure Add_Integers is new Plain_Integer_Op (Operator => "+");
	procedure Subtract_Integers is new Plain_Integer_Op (Operator => "-");
	procedure Multiply_Integers is new Plain_Integer_Op (Operator => "*");
	procedure Divide_Integers is new Plain_Integer_Op (Operator => "/");

	-------------------------------------------------------------------------

	function Half_Word_Bit_Position (Pos: in Standard.Positive) return Standard.Natural is
		pragma Inline (Half_Word_Bit_Position);
	begin
		return (Pos * Little_Endian) + ((Half_Word_Bits - Pos + 1) * Big_Endian);
	end Half_Word_Bit_Position;
	
	function Get_Half_Word_Bit (X:   in Object_Half_Word;
	                            Pos: in Standard.Positive) return Object_Bit is
		pragma Inline (Get_Half_Word_Bit);
		BA: Half_Word_Bit_Array;
		for BA'Address use X'Address;
	begin
		return BA(Half_Word_Bit_Position(Pos));
	end Get_Half_Word_Bit;
	
	procedure Set_Half_Word_Bit (X:   in out Object_Half_Word;
	                             Pos: in     Standard.Positive;
	                             Bit: in     Object_Bit) is
		pragma Inline (Set_Half_Word_Bit);
		BA: Half_Word_Bit_Array;
		for BA'Address use X'Address;
	begin
		BA(Half_Word_Bit_Position(Pos)) := Bit;
	end Set_Half_Word_Bit;
	
	-------------------------------------------------------------------------

	function Shift_Half_Word_Left (W:    in Object_Half_Word; 
	                               Bits: in Standard.Natural) return Object_Half_Word is
		pragma Inline (Shift_Half_Word_Left);
	begin
		--if Bits >= W'Size then 
		--	return 0;
		--end if;
		return W * (2 ** Bits);
	end Shift_Half_Word_Left;

	function Shift_Half_Word_Right (W:    in Object_Half_Word; 
	                                Bits: in Standard.Natural) return Object_Half_Word is
		pragma Inline (Shift_Half_Word_Right);
	begin
		if Bits >= W'Size then
			-- prevent divide-by-zero in case 2 ** Bits becomes 0 
			-- for overflow.
			return 0;
		end if;
		return W / (2 ** Bits);
	end Shift_Half_Word_Right;
	
	-------------------------------------------------------------------------

	procedure Shift_Left_Unsigned_Array (X:    in out Object_Half_Word_Array;
	                                     XS:   in     Half_Word_Object_Size;
	                                     Bits: in     Object_Size) is
		Word_Shifts: Object_Size; -- half-word shift count
		Bit_Shifts: Standard.Natural; -- bit shift count
		Bit_Shifts_Right: Standard.Natural;
		SI: Half_Word_Object_Size;
	begin
		-- This function doesn't grow/shrink the array. Shifting is performed
		-- within the given array size only.

		-- Get how many half-words to shift.
		Word_Shifts := Bits / Half_Word_Bits;
		if Word_Shifts >= XS then
			X(1 .. XS) := (others => 0);
			return;
		end if;

		-- Get how many remaining bits to shift
		Bit_Shifts := Standard.Natural(Bits rem Half_Word_Bits);
		Bit_Shifts_Right := Half_Word_Bits - Bit_Shifts;

		-- Shift words and bits
		SI := XS - Word_Shifts;
		X(XS) := Shift_Half_Word_Left(X(SI), Bit_Shifts);
		for DI in reverse Object_Size(Word_Shifts) + 1 .. XS - 1 loop
			SI := DI - Word_Shifts; -- Source Index
			X(DI + 1) := X(DI + 1) or Shift_Half_Word_Right(X(SI), Bit_Shifts_Right);
			X(DI) := Shift_Half_Word_Left(X(SI), Bit_Shifts);
		end loop;

		-- Fill the remaining part with zeros
		X(1 .. Object_Size(Word_Shifts)) := (others => 0);
	end Shift_Left_Unsigned_Array;

	procedure Shift_Right_Unsigned_Array (X:    in out Object_Half_Word_Array;
	                                      XS:   in     Half_Word_Object_Size;
	                                      Bits: in     Object_Size) is
	                                     
		Word_Shifts: Object_Size; -- half-word shift count
		Bit_Shifts: Standard.Natural; -- bit shift count
		Bit_Shifts_Left: Standard.Natural;
		SI: Half_Word_Object_Size;
	begin
		-- This function doesn't grow/shrink the array. Shifting is performed
		-- within the given array size only.

		-- Get how many half-words to shift.
		Word_Shifts := Bits / Half_Word_Bits;
		if Word_Shifts >= XS then
			X(1 .. XS) := (others => 0);
			return;
		end if;
		
		-- Get how many remaining bits to shift
		Bit_Shifts := Standard.Natural(Bits rem Half_Word_Bits);
		Bit_Shifts_Left := Half_Word_Bits - Bit_Shifts;

		-- Shift words and bits
		SI := 1 + Word_Shifts;
		X(1) := Shift_Half_Word_Right(X(SI), Bit_Shifts);
		for DI in 2 .. XS - 1 loop
			SI := DI + Word_Shifts; -- Source Index
			X(DI - 1) := X(DI - 1) or Shift_Half_Word_Right(X(SI), Bit_Shifts_Left);
			X(DI) := Shift_Half_Word_Right(X(SI), Bit_Shifts);
		end loop;

		-- Fill the remaining part with zeros
		X(XS - Half_Word_Object_Size(Word_Shifts) + 1 .. XS) := (others => 0);
	end Shift_Right_Unsigned_Array;

	-------------------------------------------------------------------------

	procedure Add_Unsigned_Array (X:      in     Object_Half_Word_Array;
	                              XS:     in     Half_Word_Object_Size;
	                              Y:      in     Object_Half_Word_Array;
	                              YS:     in     Half_Word_Object_Size;
	                              Z:      in out Object_Half_Word_Array) is
		pragma Inline (Add_Unsigned_Array);
		pragma Assert (XS >= YS);
		W: Object_Word;
		Carry: Object_Half_Word := 0;
	begin
		for I in 1 .. YS loop
			W := Object_Word(X(I)) + Object_Word(Y(I)) + Object_Word(Carry);
			Carry := Get_High(W);
			Z(I)	:= Get_Low(W);
		end loop;

		for I in YS + 1 .. XS loop
			W := Object_Word(X(I)) + Object_Word(Carry);
			Carry := Get_High(W);
			Z(I)	:= Get_Low(W);
		end loop;

		Z(XS + 1) := Carry;
	end Add_Unsigned_Array;
	
	function Add_Unsigned (Interp: access Interpreter_Record;
	                       X:      in     Object_Pointer;
	                       Y:      in     Object_Pointer) return Object_Pointer is
		A, B: aliased Object_Pointer;
		Z: Object_Pointer;
	begin
		if X.Size >= Y.Size then
			A := X;
			B := Y;
		else
			A := Y;
			B := X;
		end if;

		Push_Top (Interp.all, A'Unchecked_Access);
		Push_Top (Interp.all, B'Unchecked_Access);
		Z := Make_Bigint (Interp.Self, A.Size + 1);
		Pop_Tops (Interp.all, 2);

		Add_Unsigned_Array (A.Half_Word_Slot, A.Size, B.Half_Word_Slot, B.Size, Z.Half_Word_Slot);
		return Z;
	end Add_Unsigned;
	
	procedure Subtract_Unsigned_Array (X:      in     Object_Half_Word_Array;
	                                   XS:     in     Half_Word_Object_Size;
	                                   Y:      in     Object_Half_Word_Array;
	                                   YS:     in     Half_Word_Object_Size;
	                                   Z:      in out Object_Half_Word_Array) is
		W: Object_Word;
		Borrowed_Word: constant Object_Word := Object_Word(Object_Half_Word'Last) + 1;
		Borrow: Object_Half_Word := 0;
	begin
		pragma Assert (not Is_Less_Unsigned_Array(X, XS, Y, YS)); -- The caller must ensure that X >= Y
		
		for I in 1 .. YS loop
			W := Object_Word(Y(I)) + Object_Word(Borrow);
			if Object_Word(X(I)) >= W then
				Z(I) := X(I) - Object_Half_Word(W);
				Borrow := 0;
			else
				Z(I) := Object_Half_Word(Borrowed_Word + Object_Word(X(I)) - W);
				Borrow := 1;
			end if;
		end loop;

		for I in YS + 1 .. XS loop
			if X(I) >= Borrow then
				Z(I) := X(I) - Object_Half_Word(Borrow);
				Borrow := 0;
			else
				Z(I) := Object_Half_Word(Borrowed_Word + Object_Word(X(I)) - Object_Word(Borrow));
				Borrow := 1;
			end if;
		end loop;

		pragma Assert (Borrow = 0);
	end Subtract_Unsigned_Array;

	function Subtract_Unsigned (Interp: access Interpreter_Record;
	                            X:      in     Object_Pointer;
	                            Y:      in     Object_Pointer) return Object_Pointer is
		pragma Inline (Subtract_Unsigned);

		A: aliased Object_Pointer := X;
		B: aliased Object_Pointer := Y;
		Z: Object_Pointer;
	begin
		pragma Assert (not Is_Less_Unsigned(A, B)); -- The caller must ensure that X >= Y

		Push_Top (Interp.all, A'Unchecked_Access);
		Push_Top (Interp.all, B'Unchecked_Access);
		Z := Make_Bigint (Interp.Self, A.Size); -- Assume X.Size >= Y.Size.
		Pop_Tops (Interp.all, 2);

		Subtract_Unsigned_Array (A.Half_Word_Slot, A.Size, B.Half_Word_SLot, B.Size, Z.Half_Word_Slot);
		return Z;
	end Subtract_Unsigned;

	procedure Multiply_Unsigned_Array (X:      in     Object_Half_Word_Array;
	                                   XS:     in     Half_Word_Object_Size;
	                                   Y:      in     Object_Half_Word_Array;
	                                   YS:     in     Half_Word_Object_Size;
	                                   Z:      in out Object_Half_Word_Array) is
		W: Object_Word;
		Low, High: Object_Half_Word;
		Carry: Object_Half_Word;
		Index: Half_Word_Object_Size;
	begin
		for I in 1 .. YS loop
			if Y(I) = 0 then
				Z(XS + I) := 0;
			else
				Carry := 0;

				for J in 1 .. XS loop
					W := Object_Word(X(J)) * Object_Word(Y(I));
					Low := Get_Low(W);
					High := Get_High(W);

					Low := Low + Carry;
					if Low < Carry then
						High := High + 1;
					end if;

					Index := J + I - 1;
					Low := Low + Z(Index);
					if Low < Z(Index) then
						High := High + 1;
					end if;
					Z(Index) := Low;

					Carry := High;
				end loop;


				Z(XS + I) := Carry;
			end if;
		end loop;
	end Multiply_Unsigned_Array;
	
	function Multiply_Unsigned (Interp: access Interpreter_Record;
	                            X:      in     Object_Pointer;
	                            Y:      in     Object_Pointer) return Object_Pointer is
		pragma Inline (Multiply_Unsigned);
		
		A: aliased Object_Pointer := X;
		B: aliased Object_Pointer := Y;
		Z: Object_Pointer;
	begin
		Push_Top (Interp.all, A'Unchecked_Access);
		Push_Top (Interp.all, B'Unchecked_Access);
		Z := Make_Bigint (Interp.Self, A.Size + B.Size);
		Pop_Tops (Interp.all, 2);

		Multiply_Unsigned_Array (A.Half_Word_Slot, A.Size, B.Half_Word_Slot, B.Size, Z.Half_Word_Slot);
		return Z;
	end Multiply_Unsigned;

	procedure Divide_Unsigned_Array (X:  in     Object_Half_Word_Array;
	                                 XS: in     Half_Word_Object_Size;
	                                 Y:  in out Object_Half_Word_Array;
	                                 YS: in     Half_Word_Object_Size;
	                                 Q:  in out Object_Half_Word_Array;
	                                 R:  in out Object_Half_Word_Array) is
		Bits: constant Object_Size := XS * Half_Word_Bits;
		Word_Pos: Object_Size;
		Bit_Pos: Standard.Positive;
		RS: Half_Word_Object_Size;
	begin
		-- Perform binary long division.
		-- http://en.wikipedia.org/wiki/Division_algorithm
		--Q := 0                 initialize quotient and remainder to zero
		--R := 0                     
		--for i = n-1...0 do     where n is number of bits in N
		--  R := R << 1          left-shift R by 1 bit    
		--  R(0) := X(i)         set the least-significant bit of R equal to bit i of the numerator
		--  if R >= Y then
		--    R = R - Y               
		--    Q(i) := 1
		--  end
		--end 
		
		Q := (others => 0);
		R := (others => 0);

		for I in reverse 1 .. Bits loop
			Word_Pos := (I - 1) / Half_Word_Bits + 1;
			Bit_Pos := Standard.Positive((I - 1) rem Half_Word_Bits + 1);

			Shift_Left_Unsigned_Array (R, XS, 1);
			Set_Half_Word_Bit (R(1), 1, Get_Half_Word_Bit(X(Word_Pos), Bit_Pos));

			RS := Count_Effective_Array_Slots (R, XS);
			if not Is_Less_Unsigned_Array(R, RS, Y, YS) then
				Subtract_Unsigned_Array (R, RS, Y, YS, R);
				Set_Half_Word_Bit (Q(Word_Pos), Bit_Pos, 1);
			end if;
		end loop;
	end Divide_Unsigned_Array;
	
	                                 
	procedure Divide_Unsigned (Interp: in out Interpreter_Record;
	                           X:      in     Object_Pointer;
	                           Y:      in     Object_Pointer;
	                           Q:      out    Object_Pointer;
	                           R:      out    Object_Pointer) is
		A: aliased Object_Pointer := X;
		B: aliased Object_Pointer := Y;
		C: aliased Object_Pointer;
		D: aliased Object_Pointer;
	begin
		pragma Assert (not Is_Less_Unsigned(A, B)); -- The caller must ensure that X >= Y

		Push_Top (Interp, A'Unchecked_Access);
		Push_Top (Interp, B'Unchecked_Access);
		Push_Top (Interp, C'Unchecked_Access);
		Push_Top (Interp, D'Unchecked_Access);
		C := Make_Bigint(Interp.Self, Size => A.Size);
		D := Make_Bigint(Interp.Self, Size => A.Size);
		Pop_Tops (Interp, 4);

		Divide_Unsigned_Array (A.Half_Word_Slot, A.Size, B.Half_Word_Slot, B.Size, C.Half_Word_Slot, D.Half_Word_Slot);

		Q := C;
		R := D;
	end Divide_Unsigned;

	procedure Divide_Unsigned_2 (Interp: in out Interpreter_Record;
	                           X:      in     Object_Pointer;
	                           Y:      in     Object_Pointer;
	                           Q:      out    Object_Pointer;
	                           R:      out    Object_Pointer) is
		A: aliased Object_Pointer := X;
		B: aliased Object_Pointer := Y;

		Quo: aliased Object_Pointer;
		Dend: aliased Object_Pointer; -- Dividend
		Sor: aliased Object_Pointer; -- Divisor
		Tmp: Object_Pointer;

		Diff: Half_Word_Object_Size;
		Dend_Size: Half_Word_Object_Size;
		Sor_Size: Half_Word_Object_Size;
		Tmp_Size: Half_Word_Object_Size;

		Cand_W: Object_Word;
		Cand: Object_Half_Word_Array (1 .. 2);
		Cand_Size: Half_Word_Object_Size;
	begin
		pragma Assert (not Is_Less_Unsigned(A, B)); -- The caller must ensure that X >= Y

		Push_Top (Interp, A'Unchecked_Access);
		Push_Top (Interp, B'Unchecked_Access);
		Push_Top (Interp, Quo'Unchecked_Access);
		Push_Top (Interp, Dend'Unchecked_Access);
		Push_Top (Interp, Sor'Unchecked_Access);
		Quo := Make_Bigint (Interp.Self, A.Size);
		Dend := Make_Bigint (Interp.Self, A.Size);
		Sor := Make_Bigint (Interp.Self, A.Size);
		Tmp := Make_Bigint (Interp.Self, A.Size + 2); -- Is it enough? A.Size + B.Size is safer
		Pop_Tops (Interp, 5);

		Dend_Size := A.Size;
		Sor_Size := A.Size;
		Diff := A.Size - B.Size;
		Dend.Half_Word_Slot := A.Half_Word_Slot;
		Sor.Half_Word_Slot(1 + Diff .. B.Size + Diff) := B.Half_Word_Slot;

		for I in reverse B.Size .. A.Size loop
			-- TODO: Optimize the alogrighm further. the adjustment loop may take very long.
			if not Is_Less_Unsigned_Array(Dend.Half_Word_Slot, Dend_Size, Sor.Half_Word_Slot, Sor_Size) then
				if Dend_Size > Sor_Size then
					-- Take the 2 high digits from the dividend and 
					-- the highest digit from the divisor and guess the quotient digits.
					Cand_W := Make_Word(Dend.Half_Word_Slot(Dend_Size - 1), Dend.Half_Word_Slot(Dend_Size));
					Cand_W := Cand_W / Object_Word(Sor.Half_Word_Slot(Sor_Size));
					Cand(1) := Get_Low(Cand_W);
					Cand(2) := Get_High(Cand_W);
					if Cand(2) > 0 then
						Cand_Size := 2;
					else
						Cand_Size := 1;
					end if;
				else
					-- Take the highest digit from the dividend and the divisor 
					-- and guess the quotient digit.
					Cand(1) := Dend.Half_Word_Slot(Dend_Size) / Sor.Half_Word_Slot(Sor_Size);
					Cand_Size := 1;
				end if;

				-- Multiply the divisor and the quotient candidate.
				Tmp.Half_Word_Slot := (others => 0);
				Multiply_Unsigned_Array (Cand, Cand_Size, Sor.Half_Word_Slot, Sor_Size, Tmp.Half_Word_Slot);
				Tmp_Size := Count_Effective_Slots(Tmp);

				-- Adjust down the guess while the dividend is less than the multiplication result. 
				while Is_Less_Unsigned_Array(Dend.Half_Word_Slot, Dend_Size, Tmp.Half_Word_Slot, Tmp_Size) loop
					Cand(1) := Cand(1) - 1;

					-- Tmp := Tmp - Divisor		
					Subtract_Unsigned_Array (Tmp.Half_Word_Slot, Tmp_Size, Sor.Half_Word_Slot, Sor_Size, Tmp.Half_Word_Slot);
					Tmp_Size := Count_Effective_Slots(Tmp);
				end loop;

				-- Set the guess to the quotient.
				Quo.Half_Word_Slot(I - B.Size + 1) := Cand(1);
				
				-- Dividend := Dividend - Tmp			
				Subtract_Unsigned_Array (Dend.Half_Word_Slot, Dend_Size, Tmp.Half_Word_Slot, Tmp_Size, Dend.Half_Word_Slot);
				Dend_Size := Count_Effective_Slots(Dend);
			end if;
			
			-- Shift the divisor right by 1 slot
			pragma Assert (I = Sor_Size);
			Sor_Size := Sor_Size - 1;
			Sor.Half_Word_Slot(1 .. Sor_Size) := Sor.Half_Word_Slot(2 .. I);
			Sor.Half_Word_Slot(I) := 0;
		end loop;
		
		Q := Quo;
		R := Dend;
	end Divide_Unsigned_2;

	-------------------------------------------------------------------------

	procedure Add (Interp: in out Interpreter_Record;
	               X:      in     Object_Pointer;
	               Y:      in     Object_Pointer;
	               Z:      out    Object_Pointer) is
		A: Object_Pointer := X;
		B: Object_Pointer := Y;
		Sign: Object_Sign;
	begin
		Add_Integers (Interp, A, B, Z);
		if Z = null then
			if A.Sign /= B.Sign then
				if A.Sign = Negative_Sign then
					Subtract (Interp, B, A, Z);
				else
					Subtract (Interp, A, B, Z);
				end if;
			else
				Sign := A.Sign;
				Z := Add_Unsigned (Interp.Self, A, B);
				Z.Sign := Sign;
			end if;
			Z := Normalize(Interp.Self, Z);
		end if;
	end Add;

	procedure Subtract (Interp: in out Interpreter_Record;
	                    X:      in     Object_Pointer;
	                    Y:      in     Object_Pointer;
	                    Z:      out    Object_Pointer) is
		A: Object_Pointer := X;
		B: Object_Pointer := Y;
		Sign: Object_Sign;
	begin
		Subtract_Integers (Interp, A, B, Z);
		if Z = null then
			if A.Sign /= B.Sign then
				Sign := A.Sign;
				Z := Add_Unsigned(Interp.Self, A, B);
				Z.Sign := Sign;
			else
				if Is_Less_Unsigned(A, B) then
					Sign := Object_Sign'Val(Object_Bit(Object_Sign'Pos(A.Sign)) + 1); -- opposite A.Sign
					Z := Subtract_Unsigned(Interp.Self, B, A);
					Z.Sign := Sign;
				else
					Sign := A.Sign;
					Z := Subtract_Unsigned(Interp.Self, A, B);
					Z.Sign := Sign;
				end if;
			end if;
			Z := Normalize(Interp.Self, Z);
		end if;
	end Subtract;

	procedure Multiply (Interp: in out Interpreter_Record;
	                    X:      in     Object_Pointer;
	                    Y:      in     Object_Pointer;
	                    Z:      out    Object_Pointer) is
		A: Object_Pointer := X;
		B: Object_Pointer := Y;
		Sign: Object_Sign;
	begin
		Multiply_Integers (Interp, A, B, Z);
		if Z = null then
			-- Determine the sign earlier than any object allocation
			-- to avoid GC side-effects because A and B are not pushed
			-- as temporarry object pointers.
			if A.Sign = B.Sign then
				Sign := Positive_Sign;
			else
				Sign := Negative_Sign;
			end if;
			Z := Multiply_Unsigned (Interp.Self, A, B);
			Z.Sign := Sign;
			Z := Normalize(Interp.Self, Z);
		end if;
	end Multiply;

	procedure Divide (Interp: in out Interpreter_Record;
	                  X:      in     Object_Pointer;
	                  Y:      in     Object_Pointer;
	                  Q:      out    Object_Pointer;
	                  R:      out    Object_Pointer) is
		A: Object_Pointer := X;
		B: Object_Pointer := Y;
		C: aliased Object_Pointer;
		D: aliased Object_Pointer;
		Sign: Object_Sign;
	begin
		if (Is_Integer(Y) and then Pointer_To_Integer(Y) = 0) or else
		   (Is_Bigint(Y) and then Is_Zero(Y)) then
			raise Divide_By_Zero_Error;
		end if;

		Divide_Integers (Interp, A, B, Q);
		if Q /= null then
			-- Remainder operation must succeed if division was ok.
			R :=  Integer_To_Pointer(Pointer_To_Integer(A) rem Pointer_To_Integer(B));
			return;
		end if;

		if Is_Equal(A, B) then
			Q := Integer_To_Pointer(1);
			R := Integer_To_Pointer(0);
			return;
		elsif Is_Less_Unsigned(A, B) then
			Q := Integer_To_Pointer(0);
			R := A;
			return;
		end if;

		-- Determine the sign earlier than any object allocation
		-- to avoid GC side-effects because A and B are not pushed
		-- as temporarry object pointers.
		if A.Sign = B.Sign then
			Sign := Positive_Sign;
		else
			Sign := Negative_Sign;
		end if;

		Divide_Unsigned (Interp, A, B, C, D);

		C.Sign := Sign;
		D.Sign := Sign;

		Push_Top (Interp, C'Unchecked_Access);
		Push_Top (Interp, D'Unchecked_Access);
		C := Normalize(Interp.Self, C);
		D := Normalize(Interp.Self, D);
		Pop_Tops (Interp, 2);

		Q := C;
		R := D;
	end Divide;

	-------------------------------------------------------------------------

	function Compare_Bigint_And_Bigint (Interp: access Interpreter_Record;
	                                    X:      in     Object_Pointer;
	                                    Y:      in     Object_Pointer) return Standard.Integer is
	begin
		if Is_Equal(X, Y) then
			return 0;
		elsif Is_Less(X, Y) then
			return -1;
		else
			return 1;
		end if;
	end Compare_Bigint_And_Bigint;

	function Compare_Bigint_And_Integer (Interp: access Interpreter_Record;
	                                     X:      in     Object_Pointer;
	                                     Y:      in     Object_Pointer) return Standard.Integer is
		YW: Object_Word := Object_Word(Pointer_To_Integer(Y));
		Size: Object_Size;
	begin
		if YW > Object_Word(Object_Half_Word'Last) then
			Size := 2;
		else
			Size := 1;
		end if;

		declare
			YY: aliased Object_Record (Kind => Half_Word_Object, Size => Size);
		begin
			YY.Tag := Bigint_Object;
			YY.Half_Word_Slot(1) := Get_Low(YW);
			if YY.Size >= 2 then
				YY.Half_Word_Slot(2) := Get_High(YW);
			end if;
			return Compare_Bigint_And_Bigint (Interp, X, YY'Unchecked_Access);
		end;
	end Compare_Bigint_And_Integer;

	function Compare (Interp: access Interpreter_Record;
	                  X:      in     Object_Pointer;
	                  Y:      in     Object_Pointer) return Standard.Integer is
	begin
		if Is_Bigint(X) then
			if Is_Bigint(Y) then
				return Compare_Bigint_And_Bigint (Interp, X, Y);
			else
				return Compare_Bigint_And_Integer (Interp, X, Y);
			end if;
		else
			if Is_Bigint(Y) then
				return -Compare_Bigint_And_Integer (Interp, Y, X);
			else
				if Pointer_To_Integer(X) = Pointer_To_Integer(Y) then
					return 0;
				elsif Pointer_To_Integer(X) < Pointer_To_Integer(Y) then
					return -1;
				else
					return 1;
				end if;
			end if;
		end if;
	end Compare;

	-------------------------------------------------------------------------

	function To_String (Interp: access Interpreter_Record;
	                    X:      in     Object_Pointer;
	                    Radix:  in     Object_Radix) return Object_Pointer is
		W: aliased Object_Word;
		Sign: aliased Object_Sign;
	begin
		-- Perform simple conversion if the object can be decoded 
		-- to a single word.
		if Decode_To_Word(X, W'Access, Sign'Access) then
			declare
				-- Use a static buffer for simple conversion as the largest
				-- size is known. The largest buffer is required for radix 2.
				-- For a binary conversion(radix 2), the number of bits is
				-- the maximum number of digits that can be produced. +1 is
				-- needed for the sign.
				Buf: Object_Character_Array (1 .. Object_Word'Size + 1);
				Len: Object_Size;
			begin
				Convert_Word_To_Text (W, Radix, Buf, Len);
				if Sign = Negative_Sign then
					Len := Len + 1;
					Buf(Len) := Ch.Minus_Sign;
				end if;
				return Make_String(Interp, Source => Buf(1 .. Len), Invert => Standard.True);
			end;
		end if;
		
		-- Otherwise, do it in a hard way.
		declare
			B: aliased Object_Record (Kind => Half_Word_Object, Size => 2);
			A: aliased Object_Pointer;
			R: aliased Object_Pointer;
			Q: aliased Object_Pointer;
			Z: Object_Pointer;

			-- TODO: optimize the buffer size depending on the radix value.
			subtype Static_Buffer is Object_Character_Array (1 .. 16 * Half_Word_Bits + 1);
			subtype Dynamic_Buffer is Object_Character_Array (1 .. X.Size  * Half_Word_Bits + 1);
			type Static_Buffer_Pointer is access all Static_Buffer;
			type Dynamic_Buffer_Pointer is access all Dynamic_Buffer;
			package Pool is new H2.Pool (Dynamic_Buffer, Dynamic_Buffer_Pointer, Interp.Storage_Pool);
			Static_Buf: aliased Static_Buffer;
			Dynamic_Buf: Dynamic_Buffer_Pointer;
			Buf: Thin_Object_Character_Array_Pointer;
			
			Totlen: Object_Size := 0; -- Length of total conversion
			Seglen: Object_Size; -- Length of each word conversion
			AS: Half_Word_Object_Size;
			
			-- BD is the largest multiple of Radix that is less than or 
			-- equal to Object_Word'Last.
			--BD: constant Block_Divisor_Record := Get_Block_Divisor(Radix);
			BD: Block_Divisor_Record renames Block_Divisors(Radix);
		begin
			if X.Size <= 16 then
				declare
					function Conv is new Ada.Unchecked_Conversion (Static_Buffer_Pointer, Thin_Object_Character_Array_Pointer);
				begin
					Buf := Conv(Static_Buf'Access);
				end;
			else
			-- TODO: move this dynamic buffer to Interpreter_Record and let it sustained during the lifetime of Interpreer
				declare
					function Conv is new Ada.Unchecked_Conversion (Dynamic_Buffer_Pointer, Thin_Object_Character_Array_Pointer);
				begin
					Dynamic_Buf := Pool.Allocate;
					Buf := Conv(Dynamic_Buf);
				end;
			end if;

			-- Create a block divisor object.
			B.Tag := Bigint_Object;
			B.Half_Word_Slot := (1 => BD.Low, 2 => BD.High);

			Push_Top (Interp.all, Q'Unchecked_Access);
			Push_Top (Interp.all, R'Unchecked_Access);
			Push_Top (Interp.all, A'Unchecked_Access);
			
			-- Clone the value to convert
			A := Copy_Upto(Interp, X, X.Size);

			-- Remember the sign to produce the sign symbol later
			Sign := A.Sign;
			A.Sign := Positive_Sign;
			AS := A.Size;

			Q := Make_Bigint(Interp, Size => A.Size);
			R := Make_Bigint(Interp, Size => A.Size);

			loop
				-- Get a word block to convert
				if Is_Less_Unsigned_Array (B.Half_Word_Slot, B.Size, A.Half_Word_Slot, AS) then
					Divide_Unsigned_Array (A.Half_Word_Slot, AS, B.Half_Word_Slot, B.Size, Q.Half_Word_Slot, R.Half_Word_Slot);
					A.Half_Word_Slot := Q.Half_Word_Slot;
					AS := Count_Effective_Slots(A);
				else
					R := A; -- The last block
				end if;

				-- Translate up to 2 half-words to a full word.
				if R.Size = 1 then
					W := Object_Word(R.Half_Word_Slot(1));
				else
					W := Make_Word(R.Half_Word_Slot(1), R.Half_Word_Slot(2));
				end if;
				Convert_Word_To_Text (W, Radix, Buf(Totlen + 1 .. Buf'Last), Seglen);
				Totlen := Totlen + Seglen;

				exit when R = A; -- Reached the last block

				-- Fill unfilled leading digits with zeros if it's not the last block
				--for I in Seglen + 1 .. Block_Divisors(Radix).Length loop
				for I in Seglen + 1 .. BD.Length loop
					Totlen := Totlen + 1;
					Buf(Totlen) := Object_Character'Val(Object_Character'Pos(Ch.Zero));
				end loop;
			end loop;

			Pop_Tops (Interp.all, 3);
			
			if Sign = Negative_Sign then
				Totlen := Totlen + 1;
				Buf(Totlen) := Ch.Minus_Sign;
			end if;
			
			Z := Make_String(Interp.Self, Source => Buf(1 .. Totlen), Invert => Standard.True);

			-- TODO: Move dynamic_buf to interpreter_Record.
			if Dynamic_Buf /= null then
				Pool.Deallocate (Dynamic_Buf);
			end if;

			return Z;

		exception
			when others =>
				if Dynamic_Buf /= null then
					Pool.Deallocate (Dynamic_Buf);
				end if;
				raise;
		end;
	end To_String;
	
	
	function From_String (Interp: access Interpreter_Record;
	                      X:      in     Object_Character_Array;
	                      Radix:  in     Object_Radix) return Object_Pointer is
	        

		function Get_Digit_Value (C: in Object_Character) return Object_Integer is
			Pos: Object_Integer;
		begin
			Pos := Object_Character'Pos(C);
			case Pos is
				when Ch.Pos.Zero .. Ch.Pos.Nine =>
					Pos := Pos - Ch.Pos.Zero;

				when Ch.Pos.LC_A .. Ch.Pos.LC_Z =>
					Pos := Pos - Ch.Pos.LC_A + 10;

				when Ch.Pos.UC_A .. Ch.Pos.UC_Z =>
					Pos := Pos - Ch.Pos.UC_A + 10;

				when others =>
					Pos := -1;
			end case;

			if Pos not in 0 .. Object_Integer(Radix) - 1 then
				raise Numeric_String_Error;
			end if;

			return Pos;
		end Get_Digit_Value;

		Sign: Object_Sign;
		Idx: Object_Size;
		W: Object_Word;
		BDLen: Object_Size renames Block_Divisors(Radix).Length;
		NDigits: Object_Size;
		B: Object_Pointer;
	begin
		-- Find the first digit while remembering the sign
		Sign := Positive_Sign;
		Idx := X'First;
		if Idx <= X'Last then
			if X(Idx) = Ch.Plus_Sign then
				Idx := Idx + 1;
			elsif X(Idx) = Ch.Minus_Sign then
				Idx := Idx + 1;
				Sign := Negative_Sign;
			end if;
		end if;

		pragma Assert (Idx <= X'Last); -- the caller ensure at least 1 digit
		if Idx > X'Last then
			-- No digits in the string.
			--return Integer_To_Pointer(0);
			raise Numeric_String_Error;
		end if;
		
		-- Find the first non-zero digit
		while Idx <= X'Last loop
			exit when X(Idx) /= Ch.Zero;
			Idx := Idx + 1;
		end loop;
		if Idx > X'Last then
			-- All digits are zeros.
			return Integer_To_Pointer(0);
		end if;

		NDigits := X'Last - Idx + 1; -- number of effective digits

		-- Attempt to perform conversion within the range of Object_Integer.
		declare
			--pragma Unsuppress (Range_Check);
			--pragma Unsuppress (Overflow_Check);

			V1, V2: Object_Word;
			I: Object_Integer;
		begin
			W := 0;
			while Idx <= X'Last loop
				V1 := W * Radix;
				if V1 / Radix /= W then
					-- Overflow
					goto Huge;
				end if;

				V2 := V1 + Object_Word(Get_Digit_Value(X(Idx)));
				if V2 > Object_Word(Object_Integer'Last) or else V2 < V1 then
					-- Overflow
					goto Huge;
				end if;

				W := V2;
				Idx := Idx + 1;
			end loop;
			-- Processed all digits. The value can fit
			-- into an Object_Integer.
			I := Object_Integer(W);

			--I := 0;
			--while Idx <= X'Last loop
			--	begin
			--		I := I * Object_Integer(Radix) + Get_Digit_Value(X(Idx));
			--	exception
			--		when Constraint_Error =>
			--			W := Object_Word(I);
			--			goto Huge;
			--	end;
			--	Idx := Idx + 1;
			--end loop;

			if Sign = Negative_Sign then
				I := -I;
			end if;
			return Integer_To_Pointer(I);
		end;

	<<Huge>>
		-- TODO: Optimizations if Radix 2, 4, 16. For there radix, conversion can be done in chunk.

		-- The input string is too large to be converted to an Object_Integer.
		B := Make_Bigint(Interp, Size => ((NDigits + BDLen - 1) / BDLen) * 2 + 1); -- TODO: is it the right size?

		declare
			C: Object_Pointer;
			RB: aliased Object_Record (Kind => Half_Word_Object, Size => 1);
		begin
			RB.Tag := Bigint_Object;
			RB.Half_Word_Slot(1) := Object_Half_Word(Radix);

			C := Make_Bigint(Interp, Size => B.Size);

			B.Half_Word_Slot(1) := Get_Low(W);
			B.Half_Word_Slot(2) := Get_High(W);

			while Idx <= X'Last loop
				declare
					DVB: aliased Object_Record (Kind => Half_Word_Object, Size => 1);
				begin
					DVB.Tag := Bigint_Object;
					DVB.Half_Word_Slot(1) := Object_Half_Word(Get_Digit_Value(X(Idx)));

					Multiply_Unsigned_Array (B.Half_Word_Slot, Count_Effective_Array_Slots(B.Half_Word_Slot, B.Size), RB.Half_Word_Slot, RB.Size, C.Half_Word_Slot);
					B.Half_Word_Slot := (others => 0);
					Add_Unsigned_Array (C.Half_Word_Slot, Count_Effective_Array_Slots(C.Half_Word_Slot, B.Size), DVB.Half_Word_Slot, DVB.Size, B.Half_Word_Slot);
					C.Half_Word_Slot := (others => 0);
				end;

				Idx := Idx + 1;
			end loop;
		end;

		B.Sign := Sign;
		return Normalize(Interp.Self, B);
	end From_String;

	-------------------------------------------------------------------------

	function Get_Block_Divisor (Radix: in Object_Radix) return Block_Divisor_Record is
		V, W: Object_Word;
		Len: Object_Size;
	begin
		-- Get the largest multiples of Radix that can be represented
		-- in a single Object_Word.

		Len := 1;
		W := Object_Word(Radix);

		loop
			V := W * Object_Word(Radix);
			exit when V / Object_Word(Radix) /= W; -- Overflow
			Len := Len + 1;
			W := V;
		end loop;

		return (Low => Get_Low(W), High => Get_High(W), Length => Len);
	end Get_Block_Divisor;

	procedure Initialize is
	begin
		-- Initialize block divisors table
		if not Block_Divisors_Initialized then
			for Radix in Object_Radix'Range loop
				Block_Divisors(Radix) := Get_Block_Divisor(Radix);
			end loop;
			Block_Divisors_Initialized := Standard.True;
		end if;
	end Initialize;

begin
	Initialize;
end Bigint;
