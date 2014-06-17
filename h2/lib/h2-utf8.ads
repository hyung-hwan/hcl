generic 
	type Slim_Character is (<>);
	type Wide_Character is (<>);
	type Slim_String is array(System_Index range<>) of Slim_Character;
	type Wide_String is array(System_Index range<>) of Wide_Character;
package H2.Utf8 is
	--pragma Preelaborate (Utf8);

	--Invalid_Unicode_Character: exception renames Invalid_Wide_Character;
	--Invalid_Utf8_Sequence: exception renames Invalid_Slim_Sequence;
	--Insufficient_Utf8_Sequence: exception renames Insifficient_Slim_Sequence;
	Invalid_Unicode_Character: exception;
	Invalid_Utf8_Sequence: exception;
	Insufficient_Utf8_Sequence: exception;

	subtype Utf8_Character is Slim_Character;
	subtype Unicode_Character is Wide_Character;
	subtype Utf8_String is Slim_String;
	subtype Unicode_String is Wide_String;
	subtype Utf8_Sequence is Utf8_String;

	--type Unicode_Character_Kit is record
	--	Seq: System_Length; -- sequence length
	--	Chr: Unicode_Character;
	--end record;

	--type Unicode_String_Kit(Length: System_Length) is record
	--	Seq: System_Length;
	--	Str: Unicode_String(System_Index'First .. Length);
	--end record;

	function From_Unicode_Character (Chr: in Unicode_Character) return Utf8_String;
	function From_Unicode_String (Str: in Unicode_String) return Utf8_String;

	--| The Sequence_Length function returns the length of a full UTF8 
	--| sequence representing a single Unicode character given the first
	--| sequence byte. It returns 0 if the first byte is invalid.
	function Sequence_Length (Seq: in Utf8_Character) return System_Length;

	procedure To_Unicode_Character (Seq:     in  Utf8_String; 
	                                Seq_Len: out System_Length;
	                                Chr:     out Unicode_Character);

	procedure To_Unicode_String (Seq:     in  Utf8_String; 
	                             Seq_Len: out System_Length;
	                             Str:     out Unicode_String;
	                             Str_Len: out System_Length);

	function To_Unicode_Character (Seq: in Utf8_String) return Unicode_Character;
	function To_Unicode_String (Seq: in Utf8_String) return Unicode_String;

end H2.Utf8;
