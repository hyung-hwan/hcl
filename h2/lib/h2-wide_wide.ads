with H2.Scheme;
with H2.Utf8;
with H2.Slim;

package H2.Wide_Wide is

	type String is array(System_Index range<>) of Standard.Wide_Wide_Character;

	package Scheme is new H2.Scheme (Standard.Wide_Wide_Character);
	package Utf8 is new H2.Utf8 (Standard.Character, Standard.Wide_Wide_Character, H2.Slim.String, H2.Wide_Wide.String);

end H2.Wide_Wide;
