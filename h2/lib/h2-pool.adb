with Ada.Unchecked_Conversion;
with Ada.Unchecked_Deallocation;

package body H2.Pool is

	function Allocate (Pool: in Storage_Pool_Pointer := null) return Pointer_Type is
		P: Storage_Pool_Pointer;

	begin
		if Pool = null then
			P := Storage_Pool;
		else
			P := Pool;
		end if;

		if P = null then
			return new Normal_Type;
		else
			declare
				type Pooled_Pointer is access Normal_Type;
				for Pooled_Pointer'Storage_Pool use P.all;
				function To_Pointer_Type is new Ada.Unchecked_Conversion (Pooled_Pointer, Pointer_Type);
				Tmp: Pooled_Pointer;
			begin
				Tmp := new Normal_Type;
				return To_Pointer_Type (Tmp);
			end; 
		end if;
	end Allocate;

--	function Allocate (Source: in Normal_Type; 
--	                   Pool:   in Storage_Pool_Pointer := null) return Pointer_Type is
--		P: Storage_Pool_Pointer;
--	begin
--		if Pool = null then
--			P := Storage_Pool;
--		else
--			P := Pool;
--		end if;
--
--		if P = null then
--			return new Normal_Type'(Source);
--		else
--			declare
--				type Pooled_Pointer is access Normal_Type;
--				for Pooled_Pointer'Storage_Pool use P.all;
--				function To_Pointer_Type is new Ada.Unchecked_Conversion (Pooled_Pointer, Pointer_Type);
--				Tmp: Pooled_Pointer;
--			begin
--				Tmp := new Normal_Type'(Source);
--				return To_Pointer_Type (Tmp);
--			end; 
--		end if;
--	end Allocate;

	procedure Deallocate (Target: in out Pointer_Type;
	                      Pool:   in Storage_Pool_Pointer := null) is
		P: Storage_Pool_Pointer;
	begin
		if Pool = null then
			P := Storage_Pool;
		else
			P := Pool;
		end if;

		if P = null then
			declare
				procedure Dealloc is new Ada.Unchecked_Deallocation (Normal_Type, Pointer_Type);
			begin
				Dealloc (Target);
			end;
		else
			declare
				type Pooled_Pointer is access Normal_Type;
				for Pooled_Pointer'Storage_Pool use P.all;
				function To_Pooled_Pointer is new Ada.Unchecked_Conversion (Pointer_Type, Pooled_Pointer);
				procedure Dealloc is new Ada.Unchecked_Deallocation (Normal_Type, Pooled_Pointer);
				Tmp: Pooled_Pointer := To_Pooled_Pointer (Target);
			begin
				Dealloc (Tmp);
				Target := null;	
			end;
		end if;
	end Deallocate;

end H2.Pool;
