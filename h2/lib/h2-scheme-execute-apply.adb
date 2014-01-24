
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
		if not Is_Cons(Ptr) or else Get_Cdr(Ptr) /= Nil_Pointer then
Ada.Text_IO.Put_Line ("WRONG NUMBER OF ARGUMETNS FOR CAR"); 
			raise Syntax_Error;
		end if;

		A := Get_Car(Ptr); -- the first argument
		if not Is_Cons(A) then
Ada.Text_IO.Put_Line ("EXPECTED CONS-CELL FOR CAR"); 
			raise Evaluation_Error;
		end if;

		Pop_Frame (Interp); -- Done with the current frame
		Chain_Frame_Result (Interp, Interp.Stack, Get_Car(A));
	end Apply_Car_Procedure;

	procedure Apply_Cdr_Procedure is
		Ptr: Object_Pointer := Args;
		A: Object_Pointer;
	begin
		if not Is_Cons(Ptr) or else Get_Cdr(Ptr) /= Nil_Pointer then
Ada.Text_IO.Put_Line ("WRONG NUMBER OF ARGUMETNS FOR CDR"); 
			raise Syntax_Error;
		end if;

		A := Get_Car(Ptr); -- the first argument
		if not Is_Cons(A) then
Ada.Text_IO.Put_Line ("EXPECTED CONS-CELL FOR CDR"); 
			raise Evaluation_Error;
		end if;

		Pop_Frame (Interp); -- Done with the current frame
		Chain_Frame_Result (Interp, Interp.Stack, Get_Cdr(A));
	end Apply_Cdr_Procedure;

	procedure Apply_Cons_Procedure is
		Ptr: Object_Pointer := Args;
		A: Object_Pointer;
		B: Object_Pointer;
	begin
		if not Is_Cons(Ptr) or else not Is_Cons(Get_Cdr(Ptr)) or else Get_Cdr(Get_Cdr(Ptr)) /= Nil_Pointer  then
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
		if not Is_Cons(Ptr) or else not Is_Cons(Get_Cdr(Ptr)) or else Get_Cdr(Get_Cdr(Ptr)) /= Nil_Pointer  then
Ada.Text_IO.Put_Line ("WRONG NUMBER OF ARGUMETNS FOR SET-CAR!"); 
			raise Syntax_Error;
		end if;

		A := Get_Car(Ptr); -- the first argument
		if not Is_Cons(A) then
Ada.Text_IO.Put_Line ("EXPECTED CONS-CELL FOR Setcar"); 
			raise Evaluation_Error;
		end if;
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
		if not Is_Cons(Ptr) or else not Is_Cons(Get_Cdr(Ptr)) or else Get_Cdr(Get_Cdr(Ptr)) /= Nil_Pointer  then
Ada.Text_IO.Put_Line ("WRONG NUMBER OF ARGUMETNS FOR SET-CDR!"); 
			raise Syntax_Error;
		end if;

		A := Get_Car(Ptr); -- the first argument
		if not Is_Cons(A) then
Ada.Text_IO.Put_Line ("EXPECTED CONS-CELL FOR Setcdr"); 
			raise Evaluation_Error;
		end if;
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
		while Is_Cons(Ptr) loop
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
		if Is_Cons(Ptr) then
			Car := Get_Car(Ptr);
			if not Is_Integer(Car) then
				raise Evaluation_Error;
			end if;
			Num := Pointer_To_Integer(Car);

			Ptr := Get_Cdr(Ptr);
			while Is_Cons(Ptr) loop
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
		while Is_Cons(Ptr) loop
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
		while Is_Cons(Ptr) loop
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

	generic 
		with function Validate (X: in Object_Pointer; Y: in Object_Pointer) return Standard.Boolean;
		with function Compare (X: in Object_Pointer; Y: in Object_Pointer) return Standard.Boolean;
	procedure Apply_Compare_Procedure;

	procedure Apply_Compare_Procedure is
		-- TODO: support other values
		Ptr: Object_Pointer := Args;
		X: Object_Pointer;
		Y: Object_Pointer;
		Bool: Object_Pointer := True_Pointer;
	begin
		if Is_Cons(Ptr) and then Is_Cons(Get_Cdr(Ptr)) then
			-- at least 2 actual arguments
			X := Get_Car(Ptr);
			
			Ptr := Get_Cdr(Ptr);
			while Is_Cons(Ptr) loop
				Y := Get_Car(Ptr);

				if not Validate(X, Y) then
					ADA.TEXT_IO.PUT_LINE ("NON INTEGER FOR COMPARISION");
					raise Evaluation_Error;
				end if;

				if not Compare(X, Y) then
					Bool := False_Pointer;
					exit;
				end if;

				X := Y;
				Ptr := Get_Cdr(Ptr);
			end loop;

			Pop_Frame (Interp); --  Done with the current frame
			Chain_Frame_Result (Interp, Interp.Stack, Bool);
		else
Ada.Text_IO.Put_line ("TOO FEW ARGUMETNS FOR COMPARISON");
			raise Syntax_Error;
		end if;
	end Apply_Compare_Procedure;

	function Validate_Numeric (X: in Object_Pointer; Y: in Object_Pointer) return Standard.Boolean is
		-- TODO: support BIGNUM, OTHER DATA TYPES
	begin
		return Is_Integer(X) and then Is_Integer(Y);
	end Validate_Numeric;

	function Equal_To (X: in Object_Pointer; Y: in Object_Pointer) return Standard.Boolean is
		-- TODO: support BIGNUM, OTHER DATA TYPES
	begin
		return Pointer_To_Integer(X) = Pointer_To_Integer(Y);
	end Equal_To;

	function Greater_Than (X: in Object_Pointer; Y: in Object_Pointer) return Standard.Boolean is
		-- TODO: support BIGNUM, OTHER DATA TYPES
	begin
		return Pointer_To_Integer(X) > Pointer_To_Integer(Y);
	end Greater_Than;

	function Less_Than (X: in Object_Pointer; Y: in Object_Pointer) return Standard.Boolean is
		-- TODO: support BIGNUM, OTHER DATA TYPES
	begin
		return Pointer_To_Integer(X) < Pointer_To_Integer(Y);
	end Less_Than;

	function Greater_Or_Equal (X: in Object_Pointer; Y: in Object_Pointer) return Standard.Boolean is
		-- TODO: support BIGNUM, OTHER DATA TYPES
	begin
		return Pointer_To_Integer(X) >= Pointer_To_Integer(Y);
	end Greater_Or_Equal;

	function Less_Or_Equal (X: in Object_Pointer; Y: in Object_Pointer) return Standard.Boolean is
		-- TODO: support BIGNUM, OTHER DATA TYPES
	begin
		return Pointer_To_Integer(X) <= Pointer_To_Integer(Y);
	end Less_Or_Equal;

	procedure Apply_EQ_Procedure is new Apply_Compare_Procedure (Validate_Numeric, Equal_To);
	procedure Apply_GT_Procedure is new Apply_Compare_Procedure (Validate_Numeric, Greater_Than);
	procedure Apply_LT_Procedure is new Apply_Compare_Procedure (Validate_Numeric, Less_Than);
	procedure Apply_GE_Procedure is new Apply_Compare_Procedure (Validate_Numeric, Greater_Or_Equal);
	procedure Apply_LE_Procedure is new Apply_Compare_Procedure (Validate_Numeric, Less_Or_Equal);

	procedure Apply_Closure is
		Fbody: aliased Object_Pointer;
		Formal: aliased Object_Pointer;
		Actual: aliased Object_Pointer;
		Envir: aliased Object_Pointer;
	begin
		Push_Top (Interp, Fbody'Unchecked_Access);
		Push_Top (Interp, Formal'Unchecked_Access);
		Push_Top (Interp, Actual'Unchecked_Access);
		Push_Top (Interp, Envir'Unchecked_Access);

		-- For a closure created of "(lambda (x y) (+ x y) (* x y))"
		-- Get_Closure_Code(Func) returns "((x y) (+ x y) (* x y))"

		-- Create a new environment for the closure
		--Interp.Environment := Make_Environment(Interp.Self, Get_Closure_Environment(Func));
		Envir := Make_Environment(Interp.Self, Get_Closure_Environment(Func));

		Fbody := Get_Closure_Code(Func);
		pragma Assert (Is_Cons(Fbody)); -- the lambda evaluator must ensure this.

		Formal := Get_Car(Fbody); -- Formal argument list
		Actual := Args; -- Actual argument list

		Fbody := Get_Cdr(Fbody); -- Real function body
		pragma Assert (Is_Cons(Fbody)); -- the lambda evaluator must ensure this.

		if Is_Symbol(Formal) then
			-- Closure made of a lambda expression with a single formal argument
			-- e.g) (lambda x (car x))
			-- Apply the whole actual argument list to the closure.
			Put_Environment (Interp, Formal, Actual);
		else
			while Is_Cons(Formal) loop
				if not Is_Cons(Actual) then
					Ada.Text_IO.Put_Line (">>>> Too few arguments for CLOSURE <<<<");	
					raise Evaluation_Error;
				end if;

				-- Insert the key/value pair into the environment
				Put_Environment (Interp, Get_Car(Formal), Get_Car(Actual));

				Formal := Get_Cdr(Formal);
				Actual := Get_Cdr(Actual);
			end loop;

			-- Perform cosmetic checks for the parameter list
			if Is_Symbol(Formal) then
				-- The last formal argument to the closure is in a CDR.
				-- Assign the remaining actual arguments to the last formal argument
				-- e.g) ((lambda (x y . z) z) 1 2 3 4 5)
				Put_Environment (Interp, Formal, Actual);
			else
				-- The lambda evaluator must ensure all formal arguments are symbols.
				pragma Assert (Formal = Nil_Pointer); 

				if Actual /= Nil_Pointer then	
					Ada.Text_IO.Put_Line (">>>> TOO MANY ARGUMETNS FOR CLOSURE  <<<<");	
					raise Evaluation_Error;
				end if;
			end if;
		end if;
			
-- TODO: is it correct to keep the environement in the frame?
		Set_Frame_Opcode (Interp.Stack, Opcode_Evaluate_Group);
		Set_Frame_Operand (Interp.Stack, Fbody);
		Clear_Frame_Result (Interp.Stack);

		-- Update the environment of the frame so as to perform
		-- body evaluation in the new environment.
		Set_Frame_Environment (Interp.Stack, Envir);

		Pop_Tops (Interp, 4);
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

				when EQ_Procedure =>
					Apply_EQ_Procedure;
				when GT_Procedure =>
					Apply_GT_Procedure;
				when LT_Procedure =>
					Apply_LT_Procedure;
				when GE_Procedure =>
					Apply_GE_Procedure;
				when LE_Procedure =>
					Apply_LE_Procedure;
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
