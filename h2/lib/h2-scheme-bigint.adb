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

	Half_Word_Bits: constant := Object_Pointer_Bits / 2;
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

	function Is_Less (X: in Object_Pointer;
	                  Y: in Object_Pointer) return Standard.Boolean is
	begin
		if X.Sign /= Y.Sign then
			return X.Sign = Negative_Sign;
		end if;

		if X.Size /= Y.Size then
			return X.Size < Y.Size;
		end if;

		for I in reverse X.Half_Word_Slot'Range loop
			if X.Half_Word_Slot(I) /= Y.Half_Word_Slot(I) then
				return X.Half_Word_Slot(I) < Y.Half_Word_Slot(I);
			end if;	
		end loop;

		return Standard.False;
	end Is_Less;

	function Is_Equal (X: in Object_Pointer;
	                   Y: in Object_Pointer) return Standard.Boolean is
	begin
		return X.Sign = Y.Sign and then 
		       X.Size = Y.Size and then
		       X.Half_Word_Slot = Y.Half_Word_Slot;
	end Is_Equal;

	function Add (Interp: access Interpreter_Record;
	              X:      in     Object_Pointer;
	              Y:      in     Object_Pointer) return Object_Pointer is
		--pragma Assert (Is_Integer(X) or else Is_Bigint(X));
		--pragma Assert (Is_Integer(Y) or else Is_Bigint(Y));
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

		if X.Size > Y.Size then
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

		if Carry > 0 then
			Z.Half_Word_Slot(Last) := Carry;
		else
			declare
				T: Object_Pointer;
			begin
				T := Make_Bigint(Interp.Self, Last - 1);
				T.Half_Word_Slot := Z.Half_Word_Slot(1 .. Last - 1);
				Z := T;
			end;
		end if;

declare
package Int_IO is new ada.text_io.modular_IO(object_half_word); 
begin
for I in reverse Z.Half_Word_Slot'Range loop
int_io.put (Z.Half_Word_Slot(I), base=>16);
ada.text_io.new_line;
end loop;
end;
		Pop_Tops (Interp.all, 3);
		return Z;
	end Add;

	function Subtract (Interp: access Interpreter_Record;
	                   X:      in     Object_Pointer;
	                   Y:      in     Object_Pointer) return Object_Pointer is
		A, B, Z: aliased Object_Pointer;
		Last: Half_Word_Object_Size;
		Borrow: Object_Signed_Word; 
		T: Object_Signed_Word;
	begin
		Push_Top (Interp.all, A'Unchecked_Access);
		Push_Top (Interp.all, B'Unchecked_Access);
		Push_Top (Interp.all, Z'Unchecked_Access);

		if X.Size > Y.Size then
			A := X;
			B := Y;
			Last := X.Size;
		else
			A := Y;
			B := X;	
			Last := Y.Size;
		end if;
			
		Z := Make_Bigint (Interp.Self, Last);
		Borrow := 0;

		for I in 1 .. B.Size loop
			T := Object_Signed_Word(A.Half_Word_Slot(I)) - Object_Signed_Word(B.Half_Word_Slot(I)) - Borrow;
			if T < 0 then
				Borrow := 1;
				Z.Half_Word_Slot(I) := Object_Half_Word(-T);
			else
				Borrow := 0;
				Z.Half_Word_Slot(I) := Object_Half_Word(T);
			end if;
		end loop;

		for I in B.Size + 1 .. A.Size loop
			T := Object_Signed_Word(A.Half_Word_Slot(I)) - Borrow;
			if T < 0 then
				Borrow := 1;
				Z.Half_Word_Slot(I) := Object_Half_Word(-T);
			else
				Borrow := 0;
				Z.Half_Word_Slot(I) := Object_Half_Word(T);
			end if;
		end loop;
		
ada.text_io.put_line ("-------------");
declare
package Int_IO is new ada.text_io.modular_IO(object_half_word); 
begin
for I in reverse Z.Half_Word_Slot'Range loop
int_io.put (Z.Half_Word_Slot(I), base=>16);
ada.text_io.new_line;
end loop;
end;
		return Z;
	end Subtract;	

end Bigint;

