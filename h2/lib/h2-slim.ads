with H2.Scheme;

package H2.Slim is

	subtype Character is Standard.Character;
	type String is array(System_Index range<>) of Character;
	package Scheme is new H2.Scheme (Character);

	pragma Assert (Character'Size = System_Byte'Size);
end H2.Slim;
