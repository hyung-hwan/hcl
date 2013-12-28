with System.Storage_Pools;

package H2 is

	--subtype Character is Standard.Wide_Character;

	type Storage_Pool_Pointer is 
		access all System.Storage_Pools.Root_Storage_Pool'Class;

end H2;
