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

	function Add (Interp: access Interpreter_Record;
	              X:      in     Object_Pointer;
	              Y:      in     Object_Pointer) return Object_Pointer is
		pragma Assert (Is_Integer(X) or else Is_Bigint(X));
		pragma Assert (Is_Integer(Y) or else Is_Bigint(Y));

		Z: Object_Pointer;
		
	begin
		--if X.Size > Y.Size then
		--end if;

		--Z := Make_Bigint (Interp,  X.Size
		return null;
	end Add;

end Bigint;
