
separate (H2.Scheme.Execute)

procedure Apply is
	--pragma Inline (Apply);

	Func: aliased Object_Pointer;
	Args: aliased Object_Pointer;

	-- -------------------------------------------------------------
	-- Boolean procedures
	-- -------------------------------------------------------------
	procedure Apply_Not_Procedure is
	begin
		null;
	end Apply_Not_Procedure;

	procedure Apply_Q_Boolean_Procedure is
	begin
		null;
	end Apply_Q_Boolean_Procedure;

	-- -------------------------------------------------------------
	-- Equivalence predicates
	-- -------------------------------------------------------------
	procedure Apply_Q_Eqv_Procedure is
	begin
		null;
	end Apply_Q_Eqv_Procedure;
	
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

		Return_Frame (Interp, Get_Car(A)); 
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

		Return_Frame (Interp, Get_Cdr(A));
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

		Return_Frame (Interp, Ptr);
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

		Return_Frame (Interp, A);
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

		Return_Frame (Interp, A);
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

		Return_Frame (Interp, Integer_To_Pointer(Num));
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

		Return_Frame (Interp, Integer_To_Pointer(Num));
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

		Return_Frame (Interp, Integer_To_Pointer(Num));
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

		Return_Frame (Interp, Integer_To_Pointer(Num));
	end Apply_Quotient_Procedure;

	-- -------------------------------------------------------------
	-- Comparions procedures
	-- -------------------------------------------------------------

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

			Return_Frame (Interp, Bool);
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

	procedure Apply_N_EQ_Procedure is new Apply_Compare_Procedure (Validate_Numeric, Equal_To);
	procedure Apply_N_GT_Procedure is new Apply_Compare_Procedure (Validate_Numeric, Greater_Than);
	procedure Apply_N_LT_Procedure is new Apply_Compare_Procedure (Validate_Numeric, Less_Than);
	procedure Apply_N_GE_Procedure is new Apply_Compare_Procedure (Validate_Numeric, Greater_Or_Equal);
	procedure Apply_N_LE_Procedure is new Apply_Compare_Procedure (Validate_Numeric, Less_Or_Equal);

	-- -------------------------------------------------------------
	-- Questioning procedures
	-- -------------------------------------------------------------
	generic 
		with function Question (X: in Object_Pointer) return Standard.Boolean;
	procedure Apply_Question_Procedure;

	procedure Apply_Question_Procedure is
		Bool: Object_Pointer;
	begin
		if Is_Cons(Args) and then not Is_Cons(Get_Cdr(Args)) then
			-- 1 actual argument
			if Question(Get_Car(Args)) then
				Bool := True_Pointer;
			else
				Bool := False_Pointer;
			end if;
			Return_Frame (Interp, Bool);
		else
Ada.Text_IO.Put_line ("WRONG NUMBER OF ARGUMETNS FOR COMPARISON");
			raise Syntax_Error;
		end if;
	end Apply_Question_Procedure;
	
	function Is_Null_Class (X: in Object_Pointer) return Standard.Boolean is
		pragma Inline (Is_Null_Class);
	begin
		return X = Nil_Pointer;
	end Is_Null_Class;

	function Is_Number_Class (X: in Object_Pointer) return Standard.Boolean is
		pragma Inline (Is_Number_Class);
	begin
		return Is_Integer(X); -- TODO bignum
	end Is_Number_Class;

	function Is_Procedure_Class (X: in Object_Pointer) return Standard.Boolean is
		pragma Inline (Is_Procedure_Class);
	begin
		return Is_Closure(X) or else Is_Procedure(X) or else Is_Continuation(X);
	end Is_Procedure_Class;

	function Is_String_Class (X: in Object_Pointer) return Standard.Boolean is
		pragma Inline (Is_String_Class);
	begin
		return Is_String(X);
	end Is_String_Class;

	function Is_Symbol_Class (X: in Object_Pointer) return Standard.Boolean is
		pragma Inline (Is_Symbol_Class);
	begin
		return Is_Symbol(X);
	end Is_Symbol_Class;


	procedure Apply_Q_Null_Procedure is new Apply_Question_Procedure (Is_Null_Class);
	procedure Apply_Q_Number_Procedure is new Apply_Question_Procedure (Is_Number_Class);
	procedure Apply_Q_Procedure_Procedure is new Apply_Question_Procedure (Is_Procedure_Class);
	procedure Apply_Q_String_Procedure is new Apply_Question_Procedure (Is_String_Class);
	procedure Apply_Q_Symbol_Procedure is new Apply_Question_Procedure (Is_Symbol_Class);

	procedure Apply_Q_Eq_Procedure is
	begin
		null;
	end Apply_Q_Eq_Procedure;
	
	
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
			Set_Current_Environment (Interp, Formal, Actual);
		else
			while Is_Cons(Formal) loop
				if not Is_Cons(Actual) then
					Ada.Text_IO.Put_Line (">>>> TOO FEW ARGUMENTS FOR CLOSURE <<<<");	
					raise Evaluation_Error;
				end if;

				-- Insert the key/value pair into the environment
				Set_Current_Environment (Interp, Get_Car(Formal), Get_Car(Actual));

				Formal := Get_Cdr(Formal);
				Actual := Get_Cdr(Actual);
			end loop;

			-- Perform cosmetic checks for the parameter list
			if Is_Symbol(Formal) then
				-- The last formal argument to the closure is in a CDR.
				-- Assign the remaining actual arguments to the last formal argument
				-- e.g) ((lambda (x y . z) z) 1 2 3 4 5)
				Set_Current_Environment (Interp, Formal, Actual);
			else
				-- The lambda evaluator must ensure all formal arguments are symbols.
				pragma Assert (Formal = Nil_Pointer); 

				if Actual /= Nil_Pointer then	
					Ada.Text_IO.Put_Line (">>>> TOO MANY ARGUMETNS FOR CLOSURE  <<<<");	
					raise Evaluation_Error;
				end if;
			end if;
		end if;
			
		Switch_Frame (Interp.Stack, Opcode_Grouped_Call, Fbody, Nil_Pointer);

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

		Push_Top (Interp, C'Unchecked_Access); -- this is not needed. TOOD: remove this
		C := Make_Continuation (Interp.Self, Get_Frame_Parent(Interp.Stack));

		Set_Frame_Opcode (Interp.Stack, Opcode_Apply);
		Set_Frame_Operand (Interp.Stack, Get_Car(Args)); -- (call/cc xxx), xxx becomes this.
		Set_Frame_Intermediate (Interp.Stack, Nil_Pointer); -- pass the continuation object
		Chain_Frame_Intermediate (Interp, Interp.Stack, C); -- as an actual parameter. (xxx #continuation)
		Clear_Frame_Result (Interp.Stack);

		Pop_Tops (Interp, 1);
	end Apply_Callcc_Procedure;

	procedure Apply_Continuation is
	begin
--declare
--w: object_word;
--for w'address use func'address;
--f: object_word;
--for f'address use interp.stack'address;
--begin
--ada.text_io.put_line ("Frame" & object_word'image(f) & " " & Opcode_Type'Image(Get_Frame_Opcode(Interp.Stack)));
--ada.text_io.put ("                      POPPING ...  APPLY CONTINUATION -->> ");
--ada.text_io.put (object_word'image(w) & " ");
--end;
--Print (Interp, Args);
--ada.text_io.put ("                      CURRENT FREME RESULT " );
--Print (Interp, get_Frame_result(interp.stack));
		if not Is_Cons(Args) or else Get_Cdr(Args) /= Nil_Pointer then
			Ada.Text_IO.Put_Line ("WRONG NUMBER OF ARGUMETNS FOR CONTINUATION"); 
			raise Syntax_Error;
		end if;

          -- Restore the frame to the remembered one
		Interp.Stack := Get_Continuation_Frame(Func);
	
--declare
--f: object_word;
--for f'address use interp.stack'address;
--begin
--ada.text_io.put_line ("                      SWITCHED STACK TO FREME " & object_word'image(f) );
--ada.text_io.put ("                      CURRENT RESULT " );
--print (interp, get_Frame_result(interp.stack));
--ada.text_io.put ("                      CURRENT OPERAND " );
--print (interp, get_Frame_operand(interp.stack));
--ada.text_io.put ("                      CURRENT INTERMEDIATE " );
--print (interp, get_Frame_intermediate(interp.stack));
--ada.text_io.put_line ("                      CURRENT OPCODE " & opcode_type'image(get_Frame_opcode(interp.stack)));
--end;

		Set_Frame_Result (Interp.Stack, Get_Car(Args)); 

--ada.text_io.put ("                      FINAL RESULT ");
--print (interp, get_Frame_result(interp.stack));

	end Apply_Continuation;

begin
	Push_Top (Interp, Func'Unchecked_Access);
	Push_Top (Interp, Args'Unchecked_Access);

	Func := Get_Frame_Operand(Interp.Stack);
	if not Is_Normal_Pointer(Func) then
		Ada.Text_IO.Put_Line ("INVALID FUNCTION TYPE");
		raise Evaluation_Error;
	end if;

	Args := Get_Frame_Intermediate(Interp.Stack);

--declare
--w: object_word;
--for w'address use interp.stack'address;
--begin
--ada.text_io.put_line ("Frame" & object_word'image(w) & " " & Opcode_Type'Image(Get_Frame_Opcode(Interp.Stack)));
--ada.text_io.put ("                      FUNCTION => ");
--print (Interp, Func);
--ada.text_io.put ("                      ARGUMENTS => ");
--print (Interp, Args);
--ada.text_io.put ("                      CURRENT RESULT => ");
--print (Interp, get_frame_result(interp.stack));
--end;


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
				when Not_Procedure =>
					Apply_Not_Procedure;

				when N_Add_Procedure =>
					Apply_Add_Procedure;
				when N_EQ_Procedure =>
					Apply_N_EQ_Procedure;
				when N_GT_Procedure =>
					Apply_N_GT_Procedure;
				when N_LT_Procedure =>
					Apply_N_LT_Procedure;
				when N_GE_Procedure =>
					Apply_N_GE_Procedure;
				when N_LE_Procedure =>
					Apply_N_LE_Procedure;
				when N_Multiply_Procedure =>
					Apply_Multiply_Procedure;
				when N_Quotient_Procedure =>
					Apply_Quotient_Procedure;
				when N_Remainder_Procedure =>
					--Apply_Remainder_Procedure;
					ada.text_io.put_line ("NOT IMPLEMENTED");
					raise Evaluation_Error;
				when N_Subtract_Procedure =>
					Apply_Subtract_Procedure;

				when Q_Boolean_Procedure =>
					Apply_Q_Boolean_Procedure;
				when Q_Eq_Procedure =>
					Apply_Q_Eq_Procedure;
				when Q_Eqv_Procedure =>
					Apply_Q_Eqv_Procedure;
				when Q_Null_Procedure =>
					Apply_Q_Null_Procedure;	
				when Q_Number_Procedure =>
					Apply_Q_Number_Procedure;	
				when Q_Procedure_Procedure =>
					Apply_Q_Procedure_Procedure;	
				when Q_String_Procedure =>
					Apply_Q_String_Procedure;	
				when Q_Symbol_Procedure =>
					Apply_Q_Symbol_Procedure;	


				when Setcar_Procedure =>
					Apply_Setcar_Procedure;
				when Setcdr_Procedure =>
					Apply_Setcdr_Procedure;

			end case;	

		when Closure_Object =>
			Apply_Closure;

		when Continuation_Object =>
			Apply_Continuation;

		when others =>
			Ada.Text_IO.Put_Line ("INVALID FUNCTION TYPE");
			raise Internal_Error;

	end case;

	Pop_Tops (Interp, 2);
end Apply;
