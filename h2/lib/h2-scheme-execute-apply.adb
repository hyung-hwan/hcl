
separate (H2.Scheme.Execute)

procedure Apply is
	pragma Inline (Apply);

	Operand: aliased Object_Pointer;
	Func: aliased Object_Pointer;
	Args: aliased Object_Pointer;

	-- -------------------------------------------------------------
	-- List manipulation procedures
	-- -------------------------------------------------------------
	procedure Apply_Car_Procedure is
		Ptr: Object_Pointer := Args;
		A: Object_Pointer;
	begin
		if Ptr = Nil_Pointer or else Get_Cdr(Ptr) /= Nil_Pointer then
Ada.Text_IO.Put_Line ("WRONG NUMBER OF ARGUMETNS FOR CAR"); 
			raise Syntax_Error;
		end if;

		A := Get_Car(Ptr); -- the first argument

		Pop_Frame (Interp); -- Done with the current frame
		Chain_Frame_Result (Interp, Interp.Stack, Get_Car(A));
	end Apply_Car_Procedure;

	procedure Apply_Cdr_Procedure is
		Ptr: Object_Pointer := Args;
		A: Object_Pointer;
	begin
		if Ptr = Nil_Pointer or else Get_Cdr(Ptr) /= Nil_Pointer then
Ada.Text_IO.Put_Line ("WRONG NUMBER OF ARGUMETNS FOR CDR"); 
			raise Syntax_Error;
		end if;

		A := Get_Car(Ptr); -- the first argument

		Pop_Frame (Interp); -- Done with the current frame
		Chain_Frame_Result (Interp, Interp.Stack, Get_Cdr(A));
	end Apply_Cdr_Procedure;

	procedure Apply_Cons_Procedure is
		Ptr: Object_Pointer := Args;
		A: Object_Pointer;
		B: Object_Pointer;
	begin
		if Ptr = Nil_Pointer or else Get_Cdr(Ptr) = Nil_Pointer or else Get_Cdr(Get_Cdr(Ptr)) /= Nil_Pointer  then
Ada.Text_IO.Put_Line ("WRONG NUMBER OF ARGUMETNS FOR CONS"); 
			raise Syntax_Error;
		end if;

		A := Get_Car(Ptr); -- the first argument
		B := Get_Car(Get_Cdr(Ptr)); -- the second argument
		Ptr := Make_Cons (Interp.Self, A, B); -- change car

		Pop_Frame (Interp); -- Done with the current frame
		Chain_Frame_Result (Interp, Interp.Stack, Ptr);
	end Apply_Cons_Procedure;

	procedure Apply_Setcar_Procedure is
		Ptr: Object_Pointer := Args;
		A: Object_Pointer;
		B: Object_Pointer;
	begin
		if Ptr = Nil_Pointer or else Get_Cdr(Ptr) = Nil_Pointer or else Get_Cdr(Get_Cdr(Ptr)) /= Nil_Pointer  then
Ada.Text_IO.Put_Line ("WRONG NUMBER OF ARGUMETNS FOR SET-CAR!"); 
			raise Syntax_Error;
		end if;

		A := Get_Car(Ptr); -- the first argument
		B := Get_Car(Get_Cdr(Ptr)); -- the second argument
		Set_Car (A, B); -- change car

		Pop_Frame (Interp); -- Done with the current frame
		Chain_Frame_Result (Interp, Interp.Stack, A);
	end Apply_Setcar_Procedure;

	procedure Apply_Setcdr_Procedure is
		Ptr: Object_Pointer := Args;
		A: Object_Pointer;
		B: Object_Pointer;
	begin
		if Ptr = Nil_Pointer or else Get_Cdr(Ptr) = Nil_Pointer or else Get_Cdr(Get_Cdr(Ptr)) /= Nil_Pointer  then
Ada.Text_IO.Put_Line ("WRONG NUMBER OF ARGUMETNS FOR SET-CDR!"); 
			raise Syntax_Error;
		end if;

		A := Get_Car(Ptr); -- the first argument
		B := Get_Car(Get_Cdr(Ptr)); -- the second argument
		Set_Cdr (A, B); -- change cdr

		Pop_Frame (Interp); -- Done with the current frame
		Chain_Frame_Result (Interp, Interp.Stack, A);
	end Apply_Setcdr_Procedure;

	-- -------------------------------------------------------------
	-- Arithmetic procedures
	-- -------------------------------------------------------------
	procedure Apply_Add_Procedure is
		Ptr: Object_Pointer := Args;
		Num: Object_Integer := 0; -- TODO: support BIGNUM
		Car: Object_Pointer;
	begin
		while Ptr /= Nil_Pointer loop
			-- TODO: check if car is an integer or bignum or something else.
			--       if something else, error
			Car := Get_Car(Ptr);
			if not Is_Integer(Car) then
Ada.Text_IO.Put ("NOT INTEGER FOR ADD"); Print (Interp, Car);
				raise Evaluation_Error;
			end if;
			Num := Num + Pointer_To_Integer(Car);
			Ptr := Get_Cdr(Ptr);
		end loop;

		Pop_Frame (Interp); -- Done with the current frame
		Chain_Frame_Result (Interp, Interp.Stack, Integer_To_Pointer(Num));
	end Apply_Add_Procedure;

	procedure Apply_Subtract_Procedure is
		Ptr: Object_Pointer := Args;
		Num: Object_Integer := 0; -- TODO: support BIGNUM
		Car: Object_Pointer;
	begin
		if Ptr /= Nil_Pointer then
			Car := Get_Car(Ptr);
			if not Is_Integer(Car) then
				raise Evaluation_Error;
			end if;
			Num := Pointer_To_Integer(Car);

			Ptr := Get_Cdr(Ptr);
			while Ptr /= Nil_Pointer loop
				-- TODO: check if car is an integer or bignum or something else.
				--       if something else, error
				Car := Get_Car(Ptr);
				if not Is_Integer(Car) then
					raise Evaluation_Error;
				end if;
				Num := Num - Pointer_To_Integer(Car);
				Ptr := Get_Cdr(Ptr);
			end loop;
		end if;

		Pop_Frame (Interp); --  Done with the current frame
		Chain_Frame_Result (Interp, Interp.Stack, Integer_To_Pointer(Num));
	end Apply_Subtract_Procedure;

	procedure Apply_Multiply_Procedure is
		Ptr: Object_Pointer := Args;
		Num: Object_Integer := 1; -- TODO: support BIGNUM
		Car: Object_Pointer;
	begin
		while Ptr /= Nil_Pointer loop
			-- TODO: check if car is an integer or bignum or something else.
			--       if something else, error
			Car := Get_Car(Ptr);
			if not Is_Integer(Car) then
Ada.Text_IO.Put ("NOT INTEGER FOR MULTIPLY"); Print (Interp, Car);
				raise Evaluation_Error;
			end if;
			Num := Num * Pointer_To_Integer(Car);
			Ptr := Get_Cdr(Ptr);
		end loop;

		Pop_Frame (Interp); -- Done with the current frame
		Chain_Frame_Result (Interp, Interp.Stack, Integer_To_Pointer(Num));
	end Apply_Multiply_Procedure;

	procedure Apply_Quotient_Procedure is
		Ptr: Object_Pointer := Args;
		Num: Object_Integer := 1; -- TODO: support BIGNUM
		Car: Object_Pointer;
	begin
		while Ptr /= Nil_Pointer loop
			-- TODO: check if car is an integer or bignum or something else.
			--       if something else, error
			Car := Get_Car(Ptr);
			if not Is_Integer(Car) then
Ada.Text_IO.Put ("NOT INTEGER FOR MULTIPLY"); Print (Interp, Car);
				raise Evaluation_Error;
			end if;
			Num := Num * Pointer_To_Integer(Car);
			Ptr := Get_Cdr(Ptr);
		end loop;

		Pop_Frame (Interp); -- Done with the current frame
		Chain_Frame_Result (Interp, Interp.Stack, Integer_To_Pointer(Num));
	end Apply_Quotient_Procedure;

	procedure Apply_Closure is
		Fbody: aliased Object_Pointer;
		Param: aliased Object_Pointer;
		Arg: aliased Object_Pointer;
	begin
		Push_Top (Interp, Fbody'Unchecked_Access);
		Push_Top (Interp, Param'Unchecked_Access);
		Push_Top (Interp, Arg'Unchecked_Access);

		-- For a closure created of "(lambda (x y) (+ x y) (* x y))"
		-- Get_Closure_Code(Func) returns "((x y) (+ x y) (* x y))"

		-- Push a new environmen for the closure
		Interp.Environment := Make_Environment (Interp.Self, Get_Closure_Environment(Func));

		Fbody := Get_Closure_Code(Func);
		pragma Assert (Is_Cons(Fbody)); -- the reader must ensure this.

		Param := Get_Car(Fbody); -- Parameter list
		--Arg := Get_Car(Args); -- Actual argument list
		Arg := Args; -- Actual argument list

		Fbody := Get_Cdr (Fbody); -- Real function body
		pragma Assert (Is_Cons(Fbody)); -- the reader must ensure this as wel..

		while Is_Cons(Param) loop

			if not Is_Cons(Arg) then
				Ada.Text_IO.Put_Line (">>>> Too few arguments <<<<");	
				raise Evaluation_Error;
			end if;

			-- Insert the key/value pair into the environment
			Set_Environment (Interp, Get_Car(Param), Get_Car(Arg));

			Param := Get_Cdr(Param);
			Arg := Get_Cdr(Arg);
		end loop;

		-- Perform cosmetic checks for the parameter list
		if Param /= Nil_Pointer then
			Ada.Text_IO.Put_Line (">>> GARBAGE IN PARAMETER LIST <<<");
			raise Syntax_Error;
		end if;

		-- Perform cosmetic checks for the argument list
		if Is_Cons(Arg) then
			Ada.Text_IO.Put_Line (">>>> Two many arguments <<<<");	
			raise Evaluation_Error;
		elsif Arg /= Nil_Pointer then	
			Ada.Text_IO.Put_Line (">>> GARBAGE IN ARGUMENT LIST <<<");
			raise Syntax_Error;
		end if;
			
-- TODO: is it correct to keep the environement in the frame?
		Set_Frame_Opcode (Interp.Stack, Opcode_Evaluate_Group);
		Set_Frame_Operand (Interp.Stack, Fbody);
		Clear_Frame_Result (Interp.Stack);

		Pop_Tops (Interp, 3);
	end Apply_Closure;

begin
	Push_Top (Interp, Operand'Unchecked_Access);
	Push_Top (Interp, Func'Unchecked_Access);
	Push_Top (Interp, Args'Unchecked_Access);

	Operand := Get_Frame_Operand(Interp.Stack);
	pragma Assert (Is_Cons(Operand));

ada.text_io.put ("OPERAND TO  APPLY => ");
Print (Interp, Operand);
	Func := Get_Car(Operand);
	if not Is_Normal_Pointer(Func) then
		Ada.Text_IO.Put_Line ("INVALID FUNCTION TYPE");
		raise Evaluation_Error;
	end if;

	Args := Get_Cdr(Operand);

	case Func.Tag is
		when Procedure_Object => 
			case Get_Procedure_Opcode(Func) is

				when Car_Procedure =>
					Apply_Car_Procedure;
				when Cdr_Procedure =>
					Apply_Cdr_Procedure;
				when Cons_Procedure =>
					Apply_Cons_Procedure;
				when Setcar_Procedure =>
					Apply_Setcar_Procedure;
				when Setcdr_Procedure =>
					Apply_Setcdr_Procedure;


				when Add_Procedure =>
					Apply_Add_Procedure;
				when Subtract_Procedure =>
					Apply_Subtract_Procedure;
				when Multiply_Procedure =>
					Apply_Multiply_Procedure;
				when Quotient_Procedure =>
					Apply_Quotient_Procedure;
				--when Remainder_Procedure =>
				--	Apply_Remainder_Procedure;

				when others =>
					raise Internal_Error;
			end case;	

		when Closure_Object =>
			Apply_Closure;

		when Continuation_Object =>
			null;

		when others =>
			Ada.Text_IO.Put_Line ("INVALID FUNCTION TYPE");
			raise Internal_Error;

	end case;

	Pop_Tops (Interp, 3);
end Apply;
