with H2.Scheme;
with H2.Utf8;

package H2.Wide is

	package Scheme is new H2.Scheme (Standard.Wide_Character);
	package Utf8 is new H2.Utf8 (Standard.Character, Standard.Wide_Character);

end H2.Wide;
