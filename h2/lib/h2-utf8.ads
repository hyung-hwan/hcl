generic 
	type Utf8_Character_Type is (<>);
	type Unicode_Character_Type is (<>);
package H2.Utf8 is

	Invalid_Unicode_Character: exception;

	subtype Unicode_Character is Unicode_Character_Type;
	subtype Utf8_Character is Utf8_Character_Type;

	type Utf8_String is array(System_Index range<>) of Utf8_Character;
	type Unicode_String is array(System_Index range<>) of Unicode_Character;

	function Unicode_To_Utf8 (UC: in Unicode_Character) return Utf8_String;
	function Unicode_To_Utf8 (US: in Unicode_String) return Utf8_String;

	--procedure Utf8_To_Unicode (Utf8: in Utf8_String;
	--                           UC:   out Unicode_Character_Type);

end H2.Utf8;
