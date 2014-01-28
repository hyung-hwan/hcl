
separate (H2.Scheme.Execute)

procedure Apply is
	--pragma Inline (Apply);

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
		Put_Frame_Result (Interp, Interp.Stack, Get_Car(A));
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
		Put_Frame_Result (Interp, Interp.Stack, Get_Cdr(A));
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
		Put_Frame_Result (Interp, Interp.Stack, Ptr);
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
		Put_Frame_Result (Interp, Interp.Stack, A);
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
		Put_Frame_Result (Interp, Interp.Stack, A);
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
		Put_Frame_Result (Interp, Interp.Stack, Integer_To_Pointer(Num));
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
		Put_Frame_Result (Interp, Interp.Stack, Integer_To_Pointer(Num));
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
		Put_Frame_Result (Interp, Interp.Stack, Integer_To_Pointer(Num));
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
		Put_Frame_Result (Interp, Interp.Stack, Integer_To_Pointer(Num));
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
			Put_Frame_Result (Interp, Interp.Stack, Bool);
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

	-- -------------------------------------------------------------
	-- Closure
	-- -------------------------------------------------------------
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
		Envir := Make_Environment(Interp.Self, Get_Closure_Environment(Func));
		-- Update the environment of the frame to the one created above
		-- so as to put the arguments into the new environment.
		Set_Frame_Environment (Interp.Stack, Envir);

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
					Ada.Text_IO.Put_Line (">>>> TOO FEW ARGUMENTS FOR CLOSURE <<<<");	
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
			
		Set_Frame_Opcode (Interp.Stack, Opcode_Grouped_Call);
		Set_Frame_Operand (Interp.Stack, Fbody);
		Clear_Frame_Result (Interp.Stack);

		Pop_Tops (Interp, 4);
	end Apply_Closure;

	-- -------------------------------------------------------------
	-- Continuation
	-- -------------------------------------------------------------

	function Is_Callcc_Friendly (A: Object_Pointer) return Standard.Boolean is
		pragma Inline (Is_Callcc_Friendly);
	begin
		return Is_Closure(A) or else Is_Procedure(A) or else Is_Continuation(A);
	end Is_Callcc_Friendly;

	procedure Apply_Callcc_Procedure is
		C: aliased Object_Pointer;
	begin
		-- (call-with-current-continuation proc) 
		-- where proc is a procedure accepting one argument.
		--
		--   (define f (lambda (return) (return 2) 3))
		--   (f (lambda (x) x)) ; 3
		--   (call-with-current-continuation f) ; 2
		--
		--   (call-with-current-continuation (lambda (return) (return 2) 3))
		--
		--   (define c (call-with-current-continuation call-with-current-continuation))
		--   c ; continuation
		--   (c (+ 1 2 3)) ; 6 becomes the result of the frame that continuation remembers.
		--                 ; subsequently, its parent frames are executed. 
		--   c ; 6

		if not Is_Cons(Args) or else Get_Cdr(Args) /= Nil_Pointer then
			Ada.Text_IO.Put_Line ("WRONG NUMBER OF ARGUMETNS FOR CALL/CC"); 
			raise Syntax_Error;
		end if;

		if not Is_Callcc_Friendly(Get_Car(Args)) then
			ada.text_io.put_line ("NON CLOSURE/PROCEDURE/CONTINUATION FOR CALL/CC");
			raise Syntax_Error;
		end if; 

		Push_Top (Interp, C'Unchecked_Access);
		C := Get_Frame_Parent(Interp.Stack);
		if Get_Frame_Parent(C) = Nil_Pointer then
			C := Make_Continuation (Interp.Self, C, Nil_Pointer, Nil_Pointer);
		else

declare
w: object_word;
for w'address use c'address;
f: object_word;
for f'address use interp.stack'address;

r: object_pointer := get_frame_result(c);
rw: object_word;
for rw'address use r'address;
begin
ada.text_io.put ("Frame" & object_word'image(f) & " PUSH CONTINUATION CURRENT RESULT" & object_word'image(rw) & " ----> ");
print (interp, r);
end;

			--C := Make_Continuation (Interp.Self, C, Get_Frame_Result(Get_Frame_Parent(C)), Get_Frame_Operand(Get_Frame_Parent(C)));
			C := Make_Continuation (Interp.Self, C, Get_Frame_Result(Get_Frame_Parent(C)), Get_Frame_Result(C));
		end if;
		C := Make_Cons (Interp.Self, C, Nil_Pointer);
		C := Make_Cons (Interp.Self, Get_Car(Args), C);
declare
w: object_word;
for w'address use c'address;
f: object_word;
for f'address use interp.stack'address;
begin
ada.text_io.put ("                      PUSH CONTINUATION");
ada.text_io.put (object_word'image(w) & " ");
print (interp, c);
end;

		Set_Frame_Opcode (Interp.Stack, Opcode_Apply);
		Set_Frame_Operand (Interp.Stack, C);
		Clear_Frame_Result (Interp.Stack);
ada.text_io.put_line ("                      CLEARED RESULT BEFORE APPLYING");

		Pop_Tops (Interp, 1);
	end Apply_Callcc_Procedure;

	procedure Apply_Continuation is
		R: Object_Pointer;
	begin
declare
w: object_word;
for w'address use func'address;
f: object_word;
for f'address use interp.stack'address;
begin
ada.text_io.put ("Frame" & object_word'image(f) & " POPING APPLY CONTINUATION -----> ");
ada.text_io.put (object_word'image(w) & " ");
end;
Print (Interp, Args);
ada.text_io.put ("                      CURRENT FREME RESULT " );
Print (Interp, get_Frame_result(interp.stack));
		if not Is_Cons(Args) or else Get_Cdr(Args) /= Nil_Pointer then
			Ada.Text_IO.Put_Line ("WRONG NUMBER OF ARGUMETNS FOR CONTINUATION"); 
			raise Syntax_Error;
		end if;

-- Get the result of the continuation frame
--		R := Get_Frame_Result(Interp.Stack);

          -- Restore the frame to the remembered one
          Interp.Stack := Get_Continuation_Frame(Func);

declare
f: object_word;
for f'address use interp.stack'address;
begin
ada.text_io.put_line ("                      SWITCHED STACK TO FREME " & object_word'image(f) );
ada.text_io.put ("                      CURRENT RESULT " );
print (interp, get_Frame_result(interp.stack));
ada.text_io.put ("                      CURRENT OPERAND " );
print (interp, get_Frame_operand(interp.stack));
ada.text_io.put_line ("                      CURRENT OPCODE" & opcode_type'image(get_Frame_opcode(interp.stack)));
end;


declare
k: object_pointer := get_continuation_save2(func);
w: object_word;
for w'address use k'address;
begin
ada.text_io.put ("                      RESTORE FREME RESULT TO " & object_word'image(w) & " --> ");
print (interp, k);
end;
		--Set_Frame_Result (Interp.Stack, Get_Continuation_Save2(Func));

ada.text_io.put ("                      CHAIN NEW RESULT, TAKING THE FIRST ONLY FROM ");
print (interp, args);
		Put_Frame_Result (Interp, Interp.Stack, Get_Car(Args)); 

--		if R /= Nil_Pointer then
--ada.text_io.put ("                      CARRY OVER RESULT ");
--print (interp, get_car(r));
--			Chain_Frame_Result (Interp, Interp.Stack, Get_Car(R));
--		end if;

--Set_Frame_Result (Interp.Stack, R);
--Chain_Frame_Result (Interp, Interp.Stack, Get_Car(Args)); 


ada.text_io.put ("                      FINAL RESULT ");
print (interp, get_Frame_result(interp.stack));

--		if Get_Frame_Parent(Interp.Stack) /= Nil_Pointer then
--			Set_Frame_Result (Get_Frame_Parent(Interp.Stack), Get_Continuation_Save(Func));
--			--Set_Frame_Operand (Get_Frame_Parent(Interp.Stack), Get_Continuation_Save2(Func));
--		end if;

	end Apply_Continuation;

begin
	Push_Top (Interp, Operand'Unchecked_Access);
	Push_Top (Interp, Func'Unchecked_Access);
	Push_Top (Interp, Args'Unchecked_Access);

	Operand := Get_Frame_Operand(Interp.Stack);
	pragma Assert (Is_Cons(Operand));

declare
w: object_word;
for w'address use interp.stack'address;
begin
ada.text_io.put ("Frame" & object_word'image(w) & " OPERAND TO APPLY => ");
print (Interp, Operand);
ada.text_io.put ("                      CURRENT RESULT => ");
print (Interp, get_frame_result(interp.stack));
end;
	Func := Get_Car(Operand);
	if not Is_Normal_Pointer(Func) then
		Ada.Text_IO.Put_Line ("INVALID FUNCTION TYPE");
		raise Evaluation_Error;
	end if;

	Args := Get_Cdr(Operand);

	case Func.Tag is
		when Procedure_Object => 
			case Get_Procedure_Opcode(Func) is

				when Callcc_Procedure =>
					Apply_Callcc_Procedure;
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
			Apply_Continuation;

		when others =>
			Ada.Text_IO.Put_Line ("INVALID FUNCTION TYPE");
			raise Internal_Error;

	end case;

	Pop_Tops (Interp, 3);
end Apply;
