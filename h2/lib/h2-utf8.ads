generic 
	type Unicode_Character_Type is (<>);
	type UTF8_Character_Type is (<>);
package H2.UTF8 is

	Invalid_Unicode_Character: exception;

	subtype Unicode_Character is Unicode_Character_Type;
	subtype UTF8_Character is UTF8_Character_Type;

	type UTF8_String is array(System_Index range<>) of UTF8_Character;
	type Unicode_String is array(System_Index range<>) of Unicode_Character;

	function Unicode_To_UTF8 (UC: in Unicode_Character) return UTF8_String;
	function Unicode_To_UTF8 (US: in Unicode_String) return UTF8_String;

	--procedure UTF8_To_Unicode (UTF8: in UTF8_String;
	--                           UC:   out Unicode_Character_Type);

end H2.UTF8;
