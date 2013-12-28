--------------------------------------------------------------------
-- Instantantiate this package before using. To allocate integers,
--
--   package Integer_Pool is new Pool (Integer, Integer_Pointer, Storage_Pool);
--
--   Integer_Pool.Allocate (10);
--------------------------------------------------------------------

generic
	--type Normal_Type is private;
	type Normal_Type is limited private;
	type Pointer_Type is access all Normal_Type;
	Storage_Pool: in Storage_Pool_Pointer := null;

package H2.Pool is

	function Allocate (Pool: in Storage_Pool_Pointer := null) return Pointer_Type;

--	function Allocate (Source: in Normal_Type; 
--	                   Pool:   in Storage_Pool_Pointer := null) return Pointer_Type;

	procedure Deallocate (Target: in out Pointer_Type;
	                      Pool:   in     Storage_Pool_Pointer := null);

end H2.Pool;

