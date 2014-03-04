with H2.Pool;

separate (H2.Scheme)

package body Bigint is

	use type System.Bit_Order;

	Big_Endian : constant := Standard.Boolean'Pos (
		System.Default_Bit_Order = System.High_Order_First
	);
	Little_Endian : constant := Standard.Boolean'Pos (
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

	-----------------------------------------------------------------------------

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

	-----------------------------------------------------------------------------

	function Is_Less_Unsigned_Array (X:  in Object_Half_Word_Array;
	                                 XS: in Half_Word_Object_Size;
	                                 Y:  in Object_Half_Word_Array;
	                                 YS: in Half_Word_Object_Size) return Standard.Boolean is
		pragma Inline (Is_Less_Unsigned_Array);
	begin
		if XS /= YS then
			return XS < YS;
		end if;

		for I in reverse X'Range loop
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
		return Is_Less_Unsigned_Array (X.Half_Word_Slot, X.Size, Y.Half_Word_Slot, Y.Size);
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

	-----------------------------------------------------------------------------
	function Copy_Upto (Interp: access Interpreter_Record;
	                    X:      in     Object_Pointer;
	                    Last:   in     Half_Word_Object_Size) return Object_Pointer is
		pragma Assert (Last < X.Size);
		A: aliased Object_Pointer := X;
		Z: Object_Pointer;
	begin
		Push_Top (Interp.all, A'Unchecked_Access);
		Z := Make_Bigint(Interp, Size => Last);	
		Pop_Tops  (Interp.all, 1);
		Z.Sign := A.Sign;
		Z.Half_Word_Slot := A.Half_Word_Slot(1 .. Last);
		return Z;
	end Copy_Upto;

	function Count_Effective_Slots (X: in Object_Pointer) return Half_Word_Object_Size is
		pragma Inline (Count_Effective_Slots);
		Last: Half_Word_Object_Size := 1;
	begin
		for I in reverse 1 .. X.Size loop
			if X.Half_Word_Slot(I) /= 0 then
				Last := I;
				exit;
			end if;
		end loop;
		return Last;
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
					W: Object_Word := Make_Word (X.Half_Word_Slot(1), X.Half_Word_Slot(2));
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

	-----------------------------------------------------------------------------

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
				G: Object_Integer := Pointer_To_Integer(A);
				H: Object_Integer := Pointer_To_Integer(B);
			begin
				X := A;
				Y := B;
				Z := Integer_To_Pointer(Operator(G, H));
				return;
			exception
				when Constraint_Error =>
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

	-----------------------------------------------------------------------------

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
		W: Object_Word;
		Borrowed_Word: constant Object_Word := Object_Word(Object_Half_Word'Last) + 1;
		Borrow: Object_Half_Word := 0; 
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

	procedure Divide_Unsigned (Interp: in out Interpreter_Record;
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
				
				-- Check if the divident is less than the multiplication result.
				if Is_Less_Unsigned_Array(Dend.Half_Word_Slot, Dend_Size, Tmp.Half_Word_Slot, Tmp_Size) then
					-- If so, decrement  the candidate by 1.
					Quo.Half_Word_Slot(I) := Cand(1) - 1;
					
					-- Dividend := Dividend - Tmp
					Subtract_Unsigned_Array (Dend.Half_Word_Slot, Dend_Size, Tmp.Half_Word_Slot, Tmp_Size, Dend.Half_Word_Slot);
					Dend_Size := Count_Effective_Slots(Dend);

					-- Divident := Dividdent - Divisor
					Subtract_Unsigned_Array (Dend.Half_Word_Slot, Dend_Size, Sor.Half_Word_Slot, Sor_Size, Dend.Half_Word_Slot);
					Dend_Size := Count_Effective_Slots(Dend);
				else
					-- If not, the candidate is the right guess.
					Quo.Half_Word_Slot(I) := Cand(1);

					-- Dividend := Dividend - Tmp
					Subtract_Unsigned_Array (Dend.Half_Word_Slot, Dend_Size, Tmp.Half_Word_Slot, Tmp_Size, Dend.Half_Word_Slot);
					Dend_Size := Count_Effective_Slots(Dend);
				end if;
			end if;
			
			-- Shift the divisor right by 1 slot
			pragma Assert (I = Sor_Size);
			Sor_Size := Sor_Size - 1;
			Sor.Half_Word_Slot(1 .. Sor_Size) := Sor.Half_Word_Slot(2 .. I);
			Sor.Half_Word_Slot(I) := 0;
		end loop;
		
		Q := Quo;
		R := Dend;
	end Divide_Unsigned;

	-----------------------------------------------------------------------------

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
					--Sign := Object_Sign'Val(not Object_Sign'Pos(A.Sign)); -- opposite A.Sign
					if A.Sign = Negative_Sign then
						Sign := Positive_Sign;
					else
						Sign := Negative_Sign;
					end if;
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
			-- remainder operation must succeed if division was ok.
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

	procedure To_String (Interp: in out Interpreter_Record;
	                     X:      in     Object_Pointer;
	                     Radix:  in     Object_Half_Word;
	                     Z:      out    Object_Pointer) is
	begin
		null;
	end To_String;
end Bigint;

