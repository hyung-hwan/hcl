separate (H2.Scheme.Execute)

procedure Evaluate is
	--pragma Inline (Evaluate);

	Operand: aliased Object_Pointer;
	Car: aliased Object_Pointer;
	Cdr: aliased Object_Pointer;

	generic
		V: Object_Pointer;
		Opcode: Opcode_Type;
	procedure Generic_And_Or_Syntax;

	procedure Generic_And_Or_Syntax is
	begin
		-- (and <test1> <test2> ...)
		--   (and (= 2 2) (> 2 1))                  ==>  #t
		--   (and (= 2 2) (< 2 1))                  ==>  #f
		--   (and (= 2 2) (< 2 1) (= 3 3))                  ==>  #f
		--   (and 1 2 'c '(f g))                    ==>  (f g)
		--   (and)                                  ==>  #t

		Operand := Cdr;  -- Skip "And"
		if Operand = Nil_Pointer then
			-- (and)
			Pop_Frame (Interp); 
			Put_Frame_Result (Interp, Interp.Stack, V);
		elsif not Is_Cons(Operand) or else Get_Last_Cdr(Operand) /= Nil_Pointer then
			-- (and . 10)
			-- (and 1 2 . 10)
			Ada.Text_IO.Put_LINE ("FUCKING cDR FOR DEFINE");
			raise Syntax_Error;	
		else
			Set_Frame_Opcode (Interp.Stack, Opcode);
			Set_Frame_Operand (Interp.Stack, Get_Cdr(Operand)); -- <test2> onwards
			Clear_Frame_Result (Interp.Stack);

			-- arrange to evaluate <test1>
			Push_Frame (Interp, Opcode_Evaluate_Object, Get_Car(Operand)); 
		end if;
	end Generic_And_Or_Syntax;

	procedure Evaluate_And_Syntax is new Generic_And_Or_Syntax(True_Pointer, Opcode_Finish_And_Syntax);
	procedure Evaluate_Or_Syntax is new Generic_And_Or_Syntax(False_Pointer, Opcode_Finish_Or_Syntax);

	procedure Evaluate_Define_Syntax is
		pragma Inline (Evaluate_Define_Syntax);
	begin
-- TODO: limit the context where define can be used.

		-- (define x 10) 
		-- (define (add x y) (+ x y)) -> (define add (lambda (x y) (+ x y)))
		Operand := Cdr; -- Skip "define"

		if not Is_Cons(Operand) or else not Is_Cons(Get_Cdr(Operand)) then
			-- e.g) (define)
			--      (define . 10)
			--      (define x . 10)
			Ada.Text_IO.Put_LINE ("TOO FEW ARGUMENTS FOR DEFINE");
			raise Syntax_Error;
		end if;
		
		Car := Get_Car(Operand);
		Cdr := Get_Cdr(Operand);
		if Is_Cons(Car) then
			-- define a function:  (define (add x y) ...) 
			null;
		elsif Is_Symbol(Car) then
			-- define a symbol: (define x ...)
			if Get_Cdr(Cdr) /= Nil_Pointer then
				Ada.Text_IO.Put_LINE ("TOO MANY ARGUMENTS FOR DEFINE");
				raise Syntax_Error;
			end if;
			Cdr := Get_Car(Cdr); -- Value

			-- Arrange to finish defining after value evaluation.
			Set_Frame_Opcode (Interp.Stack, Opcode_Finish_Define_Symbol);
			Set_Frame_Operand (Interp.Stack, Car); 
			Clear_Frame_Result (Interp.Stack);

			-- Arrange to evalaute the value part
			Push_Frame (Interp, Opcode_Evaluate_Object, Cdr); 
		else
			Ada.Text_IO.Put_LINE ("NO SYMBOL NOR ARGUMENT LIST AFTER DEFINE");
			raise Syntax_Error;	
		end if;
	end Evaluate_Define_Syntax;

	procedure Evaluate_If_Syntax is
		pragma Inline (Evaluate_If_Syntax);
	begin
		-- (if <test> <consequent>)
		-- (if <test> <consequent> <alternate>)
		-- e.g) (if (> 3 2) 'yes)
		--      (if (> 3 2) 'yes 'no)
		--      (if (> 3 2) (- 3 2) (+ 3 2))      
		Operand := Cdr;  -- Skip "if"
		if Not Is_Cons(Operand) then
			-- e.g) (if)
			--      (if . 10)
			Ada.Text_IO.Put_LINE ("NO CONDITIONAL FOR IF");
			raise Syntax_Error;
		end if;

		Car := Get_Car(Operand); -- <test>

		Operand := Get_Cdr(Operand); -- cons cell containg <consequent>
		if not Is_Cons(Operand) then
			Ada.Text_IO.Put_Line ("NO ACTION FOR IF");
			raise Syntax_Error;
		end if;

		Cdr := Get_Cdr(Operand); -- cons cell containing <alternate>
		if Cdr = Nil_Pointer then
			-- no <alternate>. it's ok
Ada.Text_IO.Put_Line ("NO ALTERNATE");
			null;
		elsif not Is_Cons(Cdr) then
			-- no <alternate> but reduncant cdr.	
			-- (if (> 3 2) 3 . 99)
			Ada.Text_IO.Put_Line ("FUCKING CDR FOR IF");
			raise Syntax_Error;
			
		elsif Get_Cdr(Cdr) /= Nil_Pointer then
			-- (if (> 3 2) 3 2 . 99) 
			-- (if (> 3 2) 3 2 99) 
			Ada.Text_IO.Put_Line ("TOO MANY ARGUMENTS FOR IF");
			raise Syntax_Error;
		end if;

		-- Switch the current frame to execute action after <test> evaluation.
		Set_Frame_Opcode (Interp.Stack, Opcode_Finish_If_Syntax);
		Set_Frame_Operand (Interp.Stack, Operand); 
		Clear_Frame_Result (Interp.Stack);

		-- Arrange to evalaute the conditional
		Push_Frame (Interp, Opcode_Evaluate_Object, Car); 

	end Evaluate_If_Syntax;

	procedure Evaluate_Lambda_Syntax is
		pragma Inline (Evaluate_Lambda_Syntax);
	begin
		-- (lambda <formals> <body>)
		-- e.g)  (lambda (x y) (+ x y))
		-- e.g)  (lambda (x y . z) z)
		-- e.g)  (lambda x (car x))
		Operand := Cdr; -- Skip "lambda". cons cell pointing to <formals>
		if not Is_Cons(Operand) then
			-- e.g) (lambda)
			--      (lambda . 10)
			Ada.Text_IO.Put_LINE ("FUCKNING CDR FOR LAMBDA");
			raise Syntax_Error;
		end if;

		Car := Get_Car(Operand);  -- <formals>
		if Car = Nil_Pointer or else Is_Symbol(Car) then
			-- (lambda () ...) or (lambda x ...)
			-- nothing to do	
			null;
		elsif Is_Cons(Car) then 
			declare
				Formals: Object_Pointer := Car;
				V: Object_Pointer;
			begin
				Cdr := Formals;
				loop
					Car := Get_Car(Cdr); -- <formal argument>
					if not Is_Symbol(Car) then
						Ada.Text_IO.Put_Line ("WRONG FORMAL FOR LAMBDA");
						raise Syntax_Error;
					end if;

					-- Check for a duplication formal argument
-- TODO: make duplication check optional or change the implementation more efficient so that this check is not repeated 
					V := Formals;
					loop
						exit when V = Cdr;

						if Get_Car(V) = Car then
							Ada.Text_IO.Put_Line ("DUPLICATE FORMAL FOR LAMBDA");
							raise Syntax_Error;
						end if;

						V := Get_Cdr(V);
					end loop;

					-- Move on to the next formal argument
					Cdr := Get_Cdr(Cdr);
					exit when not Is_Cons(Cdr);
				end loop;
			end;

			if Cdr /= Nil_Pointer and then not Is_Symbol(Cdr) then
				Ada.Text_IO.Put_Line ("FUCKING CDR IN FORMALS FOR LAMBDA");
				raise Syntax_Error;
			end if;
		else 
			Ada.Text_IO.Put_Line ("INVALID FORMALS FOR LAMBDA");
			raise Syntax_Error;
		end if;

		Cdr := Get_Cdr(Operand); -- cons cell containing <body>
		if not Is_Cons(Cdr) then
			Ada.Text_IO.Put_Line ("NO BODY");
			raise Syntax_Error;
		end if;

		if Get_Last_Cdr(Cdr) /= Nil_Pointer then
			-- (lambda (x y) (+ x y) . 99)
			Ada.Text_IO.Put_Line ("FUCKING CDR IN BODY FOR LAMBDA");
			raise Syntax_Error;
		end if;

		declare
			Closure: Object_Pointer;
		begin
			Closure := Make_Closure(Interp.Self, Operand, Get_Frame_Environment(Interp.Stack));
			Pop_Frame (Interp);  -- Done
			Put_Frame_Result (Interp, Interp.Stack, Closure);
		end;
	end Evaluate_Lambda_Syntax;

	procedure Check_Let_Syntax is
		pragma Inline (Check_Let_Syntax);

		Bindings: Object_Pointer;
		LetBody: Object_Pointer;
	begin
		-- let <bindings> <body>
		Operand := Cdr; -- Skip "let".
		if not Is_Cons(Operand) then
			-- e.g) (let)
			--      (let . 10)
			Ada.Text_IO.Put_LINE ("FUCKNING CDR FOR LET");
			raise Syntax_Error;
		end if;

		Bindings := Get_Car(Operand);  -- <bindings>
		if Bindings /= Nil_Pointer and then not Is_Cons(Bindings) then
			Ada.Text_IO.Put_Line ("INVALID BINDINGS FOR LET");
			raise Syntax_Error;
		end if;

		Letbody := Get_Cdr(Operand); -- Cons cell to <body>
		if not Is_Cons(Letbody) or else Get_Last_Cdr(Letbody) /= Nil_Pointer then
			-- (let ((x 2)) )
			-- (let ((x 2)) . 99)
			-- (let ((x 2)) (+ x 2) . 99)
			Ada.Text_IO.Put_Line ("INVALID BODY FOR LET");
			raise Syntax_Error;
		end if;

		if Is_Cons(Bindings) then
			Cdr := Bindings;
			loop
				Car := Get_Car(Cdr); -- <binding>
				if not Is_Cons(Car) or else not Is_Cons(Get_Cdr(Car)) or else Get_Cdr(Get_Cdr(Car)) /= Nil_Pointer then
					-- no binding name or no binding value or garbage after that
					Ada.Text_IO.Put_Line ("WRONG BINDING FOR LET");
					raise Syntax_Error;
				end if;
	
				if not Is_Symbol(Get_Car(Car)) then
					Ada.Text_IO.Put_Line ("WRONG BINDING NAME FOR LET");
					raise Syntax_Error;
				end if;
	
				-- Check for a duplicate binding name
-- TODO: make duplication check optional or change the implementation more efficient so that this check is not repeated 
				declare
					V: Object_Pointer;
				begin
					V := Bindings;
					loop
						exit when V = Cdr;
	
						if Get_Car(Get_Car(V)) = Get_Car(Car) then
							Ada.Text_IO.Put_Line ("DUPLICATE BINDING FOR LET");
							raise Syntax_Error;
						end if;
	
						V := Get_Cdr(V);
					end loop;
				end;
	
				-- Move on to the next binding
				Cdr := Get_Cdr(Cdr);
				exit when not Is_Cons(Cdr);
			end loop;
	
			if Cdr /= Nil_Pointer then
				-- The last cdr is not nil.
				Ada.Text_IO.Put_Line ("FUCKING CDR FOR LET BINDING");
				raise Syntax_Error;
			end if;
		end  if;

		-- To avoid problems of temporary object pointer problems.
		Car := Bindings;
		Cdr := LetBody;
	end Check_Let_Syntax;

	procedure Evaluate_Let_Syntax is
		pragma Inline (Evaluate_Let_Syntax);
		Envir: aliased Object_Pointer;
	begin
		Check_Let_Syntax;
		-- Car: <bindings>, Cdr: <body>

		Set_Frame_Opcode (Interp.Stack, Opcode_Let_Finish);
		Set_Frame_Operand (Interp.Stack, Cdr); 
		Clear_Frame_Result (Interp.Stack);

		-- Push a new environment onto the current frame.
		-- It's pushed even if <bindings> is empty because
		-- the new environment is still needed in such a case
		-- as shown in the first sample below.
		Envir := Make_Environment(Interp.Self, Get_Frame_Environment(Interp.Stack));
		Set_Frame_Environment (Interp.Stack, Envir);

		-- Some let samples:
		-- #1.
		--    (define x 99) ; define x in the root environment
		--    (let () (define x 100)) ; x is defined in the new environment.
		--    x ; this must be 99.
		--
		-- #2.
		--   (define x 10) ; x-outer
		--   (define y (let ((x (+ x 1))) x)) ; x-inner := x-outer + 1, y := x-inner
		--   y ; 11 
		--   x ; 10
		--    
		-- #3.
		--   (define x (let ((x x)) x))
		--    

		if Car /= Nil_Pointer then
			-- <bindings> is not empty
			Push_Top (Interp, Envir'Unchecked_Access);
			Envir := Get_Frame_Environment(Get_Frame_Parent(Interp.Stack));

			-- The actual binding after evaluation must be performed in the 
			-- new environment.
			Push_Frame (Interp, Opcode_Let_Binding, Car);

			-- But evaluation must be done in the current environment which is 
			-- the environment before the environment update above.
			Push_Frame_With_Environment (Interp, Opcode_Let_Evaluation, Car, Envir); 
			Pop_Tops (Interp, 1);
		end if;
	end Evaluate_Let_Syntax;

	procedure Evaluate_Letast_Syntax is
		pragma Inline (Evaluate_Letast_Syntax);
		Envir: Object_Pointer;
	begin
		Check_Let_Syntax;
		-- Car: <bindings>, Cdr: <body>

		Set_Frame_Opcode (Interp.Stack, Opcode_Let_Finish);
		Set_Frame_Operand (Interp.Stack, Cdr); 
		Clear_Frame_Result (Interp.Stack);

		if Car /= Nil_Pointer then
			-- <bindings> is not empty
			Push_Frame (Interp, Opcode_Letast_Binding, Car);
		else
			-- <bindings> is empty. push the new environment
			-- for <body> evaluation.
			Envir := Make_Environment(Interp.Self, Get_Frame_Environment(Interp.Stack));
			Set_Frame_Environment (Interp.Stack, Envir);
		end if;
	end Evaluate_Letast_Syntax;

	procedure Evaluate_Letrec_Syntax is
		pragma Inline (Evaluate_Letrec_Syntax);
		Envir: Object_Pointer;
	begin
		Check_Let_Syntax;
		-- Car: <bindings>, Cdr: <body>

		Set_Frame_Opcode (Interp.Stack, Opcode_Let_Finish);
		Set_Frame_Operand (Interp.Stack, Cdr); 
		Clear_Frame_Result (Interp.Stack);

		-- Push a new environment.
		Envir := Make_Environment(Interp.Self, Get_Frame_Environment(Interp.Stack));
		Set_Frame_Environment (Interp.Stack, Envir);

		if Car /= Nil_Pointer then
			-- <bindings> is not empty
			-- Arrange to perform evaluataion and binding in the
			-- new environment created.
			Push_Frame (Interp, Opcode_Let_Binding, Car);
			Push_Frame (Interp, Opcode_Let_Evaluation, Car); 
		end if;
	end Evaluate_Letrec_Syntax;

	procedure Evaluate_Quote_Syntax is
		pragma Inline (Evaluate_Quote_Syntax);
	begin
		Operand := Cdr; -- Skip "quote". Get the first argument.
		if not Is_Cons(Operand) then
			-- e.g) (quote)
			--      (quote . 10)
			Ada.Text_IO.Put_LINE ("FUCKNING CDR FOR QUOTE");
			raise Syntax_Error;
		elsif Get_Cdr(Operand) /= Nil_Pointer then
			Ada.Text_IO.Put_LINE ("WRONG NUMBER OF ARGUMENTS FOR QUOTE");
			raise Syntax_Error;
		end if;
		Pop_Frame (Interp);	 -- Done
		Put_Frame_Result (Interp, Interp.Stack, Get_Car(Operand));
	end Evaluate_Quote_Syntax;

	procedure Evaluate_Set_Syntax is
		pragma Inline (Evaluate_Set_Syntax);
	begin
		-- (set! <variable> <expression>)
		-- e.g) (set! x 10) 

		Operand := Cdr; -- Skip "set!"

		if not Is_Cons(Operand) or else not Is_Cons(Get_Cdr(Operand)) then
			-- e.g) (set!)
			--      (set . 10)
			--      (set x . 10)
			Ada.Text_IO.Put_LINE ("TOO FEW ARGUMENTS FOR SET!");
			raise Syntax_Error;
		end if;

		Car := Get_Car(Operand); -- <variable>
		Cdr := Get_Cdr(Operand); -- cons cell to <expression>
		if Is_Symbol(Car) then
			if Get_Cdr(Cdr) /= Nil_Pointer then
				Ada.Text_IO.Put_LINE ("TOO MANY ARGUMENTS FOR SET!");
				raise Syntax_Error;
			end if;
			Cdr := Get_Car(Cdr); -- <expression>

			-- Arrange to finish setting a variable after <expression> evaluation.
			Set_Frame_Opcode (Interp.Stack, Opcode_Set_Finish);
			Set_Frame_Operand (Interp.Stack, Car); 
			Clear_Frame_Result (Interp.Stack);

			-- Arrange to evalaute the value part
			Push_Frame (Interp, Opcode_Evaluate_Object, Cdr); 
		else
			Ada.Text_IO.Put_LINE ("INVALID SYMBOL AFTER SET!");
			raise Syntax_Error;	
		end if;
	end Evaluate_Set_Syntax;

begin
	Push_Top (Interp, Operand'Unchecked_Access);
	Push_Top (Interp, Car'Unchecked_Access);
	Push_Top (Interp, Cdr'Unchecked_Access);

<<Start_Over>>
	Operand := Get_Frame_Operand(Interp.Stack);

declare
f: object_word;
for f'address use interp.stack'address;
o: object_word;
for o'address use operand'address;
begin
ada.text_io.put ("Frame" & object_word'image(f) & " EVALUATE OPERAND" & object_word'image(o) & " ");
print (interp, operand);
ada.text_io.put ("                      CURRENT RESULT ");
print (interp, get_Frame_result(interp.stack));
end;

	if not Is_Normal_Pointer(Operand) then
		-- integer, character, specal pointers
		-- TODO: some normal pointers may point to literal objects. e.g.) bignum
		goto Literal;
	end if;

	case Operand.Tag is
		when Symbol_Object => -- Is_Symbol(Operand)
			-- TODO: find it in the Environment hierarchy.. not in the current environemnt.
			Car := Get_Environment (Interp.Self, Operand);
			if Car = null then
				-- unbound
				Ada.Text_IO.Put_Line ("Unbound symbol....");
				Print (Interp, Operand);		
				raise Evaluation_Error;
			else
				-- symbol found in the environment
				Operand := Car; 
				goto Literal;  -- In fact, this is not a literal, but can be handled in the same way
			end if;

		when Cons_Object => -- Is_Cons(Operand)
			Car := Get_Car(Operand);
			Cdr := Get_Cdr(Operand);
			if Is_Syntax(Car) then
				-- special syntax symbol. normal evaluate rule doesn't 
				-- apply for special syntax objects.

				case Car.Scode is
					when And_Syntax =>
						Evaluate_And_Syntax;
						
					when Begin_Syntax =>
						Operand := Cdr; -- Skip "begin"

						if Operand = Nil_Pointer then
							-- (begin)
							Pop_Frame (Interp);	
							-- Return nil to the upper frame for (begin).
							Put_Frame_Result (Interp, Interp.Stack, Nil_Pointer);
						else
							if Get_Last_Cdr(Operand) /= Nil_Pointer then
								Ada.Text_IO.Put_LINE ("FUCKNING CDR FOR BEGIN");
								raise Syntax_Error;
							end if;

							Set_Frame_Opcode (Interp.Stack, Opcode_Grouped_Call);
							Set_Frame_Operand (Interp.Stack, Operand);
							Clear_Frame_Result (Interp.Stack);
						end if;

						--if (Interp.Trait.Trait_Bits and No_Optimization) = 0 then
						--	-- I call Evaluate_Group for optimization here.
						--	Evaluate_Group; -- for optimization only. not really needed.
						--	-- I can jump to Start_Over because Evaluate_Group called 
						--	-- above pushes an Opcode_Evaluate_Object frame.
						--	pragma Assert (Get_Frame_Opcode(Interp.Stack) = Opcode_Evaluate_Object);
						--	goto Start_Over; -- for optimization only. not really needed.
						--end if;

					when Define_Syntax =>
						Evaluate_Define_Syntax;

					when If_Syntax =>
						Evaluate_If_Syntax;

					when Lambda_Syntax =>
						Evaluate_Lambda_Syntax;

					when Let_Syntax =>
						Evaluate_Let_Syntax;

					when Letast_Syntax =>
						Evaluate_Letast_Syntax;

					when Letrec_Syntax =>
						Evaluate_Letrec_Syntax;

					when Or_Syntax =>
						Evaluate_Or_Syntax;

					when Quote_Syntax =>
						Evaluate_Quote_Syntax;

					when Set_Syntax => -- set!
						Evaluate_Set_Syntax;

					when others =>
						Ada.Text_IO.Put_Line ("Unknown syntax");	
						--Set_Frame_Opcode (Interp.Stack, Opcode_Evaluate_Syntax);  -- Switch to syntax evaluation
						raise Internal_Error;
				end case;
			else
				-- procedure call
				-- (<operator> <operand1> ...)
				if Get_Last_Cdr(Operand) /= Nil_Pointer then
					Ada.Text_IO.Put_Line ("ERROR: FUCKING CDR FOR PROCEDURE CALL.$$$$");
					raise Syntax_Error;
				end if;

				-- Create a cons cell whose 'car' holds arguments and 
				-- 'cdr' holds evaluation results before applying them.
				Cdr := Make_Cons (Interp.Self, Cdr, Nil_Pointer);

				-- Set it as a frame operand
				Set_Frame_Opcode (Interp.Stack, Opcode_Procedure_Call);
				Set_Frame_Operand (Interp.Stack, Cdr);
				Clear_Frame_Result (Interp.Stack);

				-- Arrange to evaluate <operator> first.
				Push_Frame (Interp, Opcode_Evaluate_Object, Car);
			end if;

		when others =>
			-- normal literal object
			goto Literal;
	end case;
	goto Done;

<<Literal>>
	Pop_Frame (Interp); -- done
declare
w: object_word;
for w'address use operand'address;
begin
Ada.Text_IO.Put ("Return => (" & object_word'image(w) & ") =>" );
Print (Interp, Operand);
end;
	Put_Frame_Result (Interp, Interp.Stack, Operand);
	goto Done;

<<Done>>
	Pop_Tops (Interp, 3);
end Evaluate;
