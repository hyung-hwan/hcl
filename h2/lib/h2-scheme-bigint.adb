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

	function Is_Less_Unsigned (X: in Object_Pointer;
	                           Y: in Object_Pointer) return Standard.Boolean is
		pragma Inline (Is_Less_Unsigned);
	begin
		if X.Size /= Y.Size then
			return X.Size < Y.Size;
		end if;

		for I in reverse X.Half_Word_Slot'Range loop
			if X.Half_Word_Slot(I) /= Y.Half_Word_Slot(I) then
				return X.Half_Word_Slot(I) < Y.Half_Word_Slot(I);
			end if;	
		end loop;

		return Standard.False;
	end Is_Less_Unsigned;

	function Is_Less (X: in Object_Pointer;
	                  Y: in Object_Pointer) return Standard.Boolean is
	begin
		if X.Sign /= Y.Sign then
			return X.Sign = Negative_Sign;
		end if;
		return Is_Less_Unsigned (X, Y);
	end Is_Less;

	function Is_Equal (X: in Object_Pointer;
	                   Y: in Object_Pointer) return Standard.Boolean is
	begin
		return X.Sign = Y.Sign and then 
		       X.Size = Y.Size and then
		       X.Half_Word_Slot = Y.Half_Word_Slot;
	end Is_Equal;

	function Is_Zero (X: in Object_Pointer) return Standard.Boolean is
	begin
		return X.Size = 1 and then X.Half_Word_Slot(1) = 0;
	end Is_Zero;

	-----------------------------------------------------------------------------

	function Normalize (Interp: access Interpreter_Record;
	                    X:      in     Object_Pointer) return Object_Pointer is
		Last: Half_Word_Object_Size := 1;
	begin
		for I in reverse 1 .. X.Size loop
			if X.Half_Word_Slot(I) /= 0 then
				Last := I;
				exit;
			end if;
		end loop;

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
			return X;
		else
			return Make_Bigint(Interp, X, Last);
		end if;

	end Normalize;

	-----------------------------------------------------------------------------

	generic
		with function Operator (X: in Object_Integer; 
		                        Y: in Object_Integer) return Object_Integer;
	procedure Plain_Integer_Op (Interp: access Interpreter_Record;
	                            X:      in out Object_Pointer;
	                            Y:      in out Object_Pointer;
	                            Z:      out    Object_Pointer);

	procedure Plain_Integer_Op (Interp: access Interpreter_Record;
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
					Push_Top (Interp.all, A'Unchecked_Access);
					Push_Top (Interp.all, B'Unchecked_Access);
-- TODO: allocate A and B from a non-GC heap.
-- I know that pointers returned by Make_Bigint here are short-lived
-- and not needed after actual operation. non-GC heap is a better choice.
					A := Make_Bigint(Interp, Value => G);	
					B := Make_Bigint(Interp, Value => H);	
					Pop_Tops (Interp.all, 2);
			end;
		else
			Push_Top (Interp.all, A'Unchecked_Access);
			Push_Top (Interp.all, B'Unchecked_Access);
			if Is_Integer(A) then
				A := Make_Bigint(Interp, Value => Pointer_To_Integer(A));
			end if;
			if Is_Integer(B) then
				B := Make_Bigint(Interp, Value => Pointer_To_Integer(B));
			end if;		
			Pop_Tops (Interp.all, 2);
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

	function Add_Unsigned (Interp: access Interpreter_Record;
	                       X:      in     Object_Pointer;
	                       Y:      in     Object_Pointer) return Object_Pointer is
		pragma Assert (Is_Bigint(X));
		pragma Assert (Is_Bigint(Y));

		A, B, Z: aliased Object_Pointer;
		W: Object_Word;
		Carry: Object_Half_Word; 
		Last: Half_Word_Object_Size;
	begin
		Push_Top (Interp.all, A'Unchecked_Access);
		Push_Top (Interp.all, B'Unchecked_Access);
		Push_Top (Interp.all, Z'Unchecked_Access);

		if X.Size >= Y.Size then
			A := X;
			B := Y;
			Last := X.Size + 1;
		else
			A := Y;
			B := X;	
			Last := Y.Size + 1;
		end if;
			
		Z := Make_Bigint (Interp.Self, Last);
		Carry := 0;

		for I in 1 .. B.Size loop
			W := Object_Word(A.Half_Word_Slot(I)) + Object_Word(B.Half_Word_Slot(I)) + Object_Word(Carry);
			Carry := Get_High(W);
			Z.Half_Word_Slot(I)	:= Get_Low(W);
		end loop;

		for I in B.Size + 1 .. A.Size loop
			W := Object_Word(A.Half_Word_Slot(I)) + Object_Word(Carry);
			Carry := Get_High(W);
			Z.Half_Word_Slot(I)	:= Get_Low(W);
		end loop;

		Z.Half_Word_Slot(Last) := Carry;

		Pop_Tops (Interp.all, 3);
		return Z;
	end Add_Unsigned;

	function Subtract_Unsigned (Interp: access Interpreter_Record;
	                            X:      in     Object_Pointer;
	                            Y:      in     Object_Pointer) return Object_Pointer is
		A, B, Z: aliased Object_Pointer;
		T: Object_Word;
		Borrowed_Word: constant Object_Word := Object_Word(Object_Half_Word'Last) + 1;
		Borrow: Object_Half_Word := 0; 
	begin
		Push_Top (Interp.all, A'Unchecked_Access);
		Push_Top (Interp.all, B'Unchecked_Access);
		Push_Top (Interp.all, Z'Unchecked_Access);

		A := X;
		B := Y;
		pragma Assert (not Is_Less_Unsigned(A, B)); -- The caller must ensure that X >= Y
			
		Z := Make_Bigint (Interp.Self, A.Size); -- Assume X.Size >= Y.Size.

		for I in 1 .. B.Size loop
			T := Object_Word(B.Half_Word_Slot(I)) + Object_Word(Borrow);
			if Object_Word(A.Half_Word_Slot(I)) >= T then
				Z.Half_Word_Slot(I) := A.Half_Word_Slot(I) - Object_Half_Word(T);
				Borrow := 0;
			else
				Z.Half_Word_Slot(I) := Object_Half_Word(Borrowed_Word + Object_Word(A.Half_Word_Slot(I)) - T);
				Borrow := 1;
			end if;
		end loop;

		for I in B.Size + 1 .. A.Size loop
			if A.Half_Word_Slot(I) >= Borrow then
				Z.Half_Word_Slot(I) := A.Half_Word_Slot(I) - Object_Half_Word(Borrow);
				Borrow := 0;
			else
				Z.Half_Word_Slot(I) := Object_Half_Word(Borrowed_Word + Object_Word(A.Half_Word_Slot(I)) - Object_Word(Borrow));
				Borrow := 1;
			end if;
		end loop;

		pragma Assert (Borrow = 0);
		return Z;
	end Subtract_Unsigned;	

	function Multiply_Unsigned (Interp: access Interpreter_Record;
	                            X:      in     Object_Pointer;
	                            Y:      in     Object_Pointer) return Object_Pointer is
		A, B, Z: aliased Object_Pointer;
		W: Object_Word;
		Low, High: Object_Half_Word;
		Carry: Object_Half_Word;
		Index: Half_Word_Object_Size;
	begin
		Push_Top (Interp.all, A'Unchecked_Access);
		Push_Top (Interp.all, B'Unchecked_Access);
		Push_Top (Interp.all, Z'Unchecked_Access);

		A := X;
		B := Y;
		Z := Make_Bigint (Interp.Self, A.Size + B.Size);

		--for I in B.Half_Word_Slot'Range loop
		for I in 1 .. B.Size loop
			if B.Half_Word_Slot(I) = 0 then
				Z.Half_Word_Slot(A.Size + I) := 0;
			else
				Carry := 0;	

				--for J in A.Half_Word_Slot'Range loop
				for J in 1 .. A.Size loop
					W := Object_Word(A.Half_Word_Slot(J)) * Object_Word(B.Half_Word_Slot(I));
					Low := Get_Low(W);	
					High := Get_High(W);	

					Low := Low + Carry;
					if Low < Carry then
						High := High + 1;
					end if;
			
					Index := J + I - 1;
					Low := Low + Z.Half_Word_Slot(Index);
					if Low < Z.Half_Word_SLot(Index) then
						High := High + 1;
					end if;
					Z.Half_Word_Slot(Index) := Low;

					Carry := High;
				end loop;

				Z.Half_Word_Slot(A.Size + I) := Carry;
			end if;
		end loop;

		Pop_Tops (Interp.all, 3);
		return Z;
	end Multiply_Unsigned;

	function Divide_Unsigned (Interp: access Interpreter_Record;
	                          X:      in     Object_Pointer;
	                          Y:      in     Object_Pointer) return Object_Pointer is
	begin
		return null;
	end Divide_Unsigned;
	-----------------------------------------------------------------------------

	function Add (Interp: access Interpreter_Record;
	              X:      in     Object_Pointer;
	              Y:      in     Object_Pointer) return Object_Pointer is

		Z: Object_Pointer;
		A: Object_Pointer := X;
		B: Object_Pointer := Y;
		Sign: Object_Sign;
	begin
		Add_Integers (Interp, A, B, Z);
		if Z /= null then
			return Z;
		end if;

		if A.Sign /= B.Sign then
			if A.Sign = Negative_Sign then
				Z := Subtract (Interp, B, A);
			else
				Z := Subtract (Interp, A, B);
			end if;
		else
			Sign := A.Sign;
			Z := Add_Unsigned (Interp, A, B);
			Z.Sign := Sign;
		end if;

		return Normalize(Interp, Z);
	end Add;

	function Subtract (Interp: access Interpreter_Record;
	                   X:      in     Object_Pointer;
	                   Y:      in     Object_Pointer) return Object_Pointer is
		Z: Object_Pointer;
		A: Object_Pointer := X;
		B: Object_Pointer := Y;
		Sign: Object_Sign;
	begin
		Subtract_Integers (Interp, A, B, Z);
		if Z /= null then
			return Z;
		end if;

		if A.Sign /= B.Sign then
			Sign := A.Sign;
			Z := Add_Unsigned (Interp, A, B);
			Z.Sign := Sign;
		else
			if Is_Less_Unsigned(A, B) then	
				--Sign := Object_Sign'Val(not Object_Sign'Pos(A.Sign)); -- opposite A.Sign
				if A.Sign = Negative_Sign then
					Sign := Positive_Sign;
				else
					Sign := Negative_Sign;
				end if;
				Z := Subtract_Unsigned (Interp, B, A);
				Z.Sign := Sign;
			else
				Sign := A.Sign;
				Z := Subtract_Unsigned (Interp, A, B);
				Z.Sign := Sign;
			end if;
		end if;

		return Normalize(Interp, Z);
	end Subtract;

	function Multiply (Interp: access Interpreter_Record;
	                   X:      in     Object_Pointer;
	                   Y:      in     Object_Pointer) return Object_Pointer is

		Z: Object_Pointer;
		A: Object_Pointer := X;
		B: Object_Pointer := Y;
		Sign: Object_Sign;
	begin
		Multiply_Integers (Interp, A, B, Z);
		if Z /= null then
			return Z;
		end if;

		-- Determine the sign earlier than any object allocation
		-- to avoid GC side-effects because A and B are not pushed
		-- as temporarry object pointers.
		if A.Sign = B.Sign then
			Sign := Positive_Sign;
		else
			Sign := Negative_Sign;
		end if;
		Z := Multiply_Unsigned (Interp, A, B);
		Z.Sign := Sign;

		return Normalize(Interp, Z);
	end Multiply;

	function Divide (Interp: access Interpreter_Record;
	                 X:      in     Object_Pointer;
	                 Y:      in     Object_Pointer) return Object_Pointer is
		Z: Object_Pointer;
		A: Object_Pointer := X;
		B: Object_Pointer := Y;
		Sign: Object_Sign;
	begin
		Divide_Integers (Interp, A, B, Z);
		if Z /= null then
			return Z;
		end if;

		-- Determine the sign earlier than any object allocation
		-- to avoid GC side-effects because A and B are not pushed
		-- as temporarry object pointers.
		if A.Sign = B.Sign then
			Sign := Positive_Sign;
		else
			Sign := Negative_Sign;
		end if;
		Z := Divide_Unsigned (Interp, A, B);
		Z.Sign := Sign;

		return Normalize(Interp, Z);
	end Divide;

end Bigint;

