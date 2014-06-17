with H2.Scheme;
with H2.Utf8;
with H2.Slim;

-- TODO: rename H2.Wide to H2.Wide_Utf8 or soemthing...

package H2.Wide is

	subtype Character is Standard.Wide_Character;
	type String is array(System_Index range<>) of Character;

	package Scheme is new H2.Scheme (Standard.Wide_Character);
	package Utf8 is new H2.Utf8 (H2.Slim.Character, Character, H2.Slim.String, H2.Wide.String);

	--package Stream is new H2.IO (
	--	Standard.Wide_Character,
	--	Standard.Character, 
	--	H2.System.Open,
	--	H2.System.Close,
	--	H2.System.Read,
	--	H2.System.Write
	--);
end H2.Wide;
