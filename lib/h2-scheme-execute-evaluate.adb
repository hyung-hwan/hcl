separate (H2.Scheme.Execute)

procedure Evaluate is
	pragma Inline (Evaluate);

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
			Chain_Frame_Result (Interp, Interp.Stack, V);
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
			Closure := Make_Closure(Interp.Self, Operand, Interp.Environment);
			Pop_Frame (Interp);  -- Done
			Chain_Frame_Result (Interp, Interp.Stack, Closure);
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
		if not Is_Cons(Letbody) then
			-- (let ((x 2)) )
			-- (let ((x 2)) . 99)
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
	begin
		Check_Let_Syntax;
		-- Car: <bindings>, Cdr: <body>
		Set_Frame_Opcode (Interp.Stack, Opcode_Let_Finish);
		Set_Frame_Operand (Interp.Stack, Cdr); 

		Interp.Environment := Make_Environment(Interp.Self, Interp.Environment);
		Set_Frame_Environment (Interp.Stack, Interp.Environment); 

		-- Some let samples:
		-- #1.
		--    (define x 99)
		--    (let () (define x 100)) ; no actual bindings
		--    x ; this must be 99
		--
		-- #2.
		--    ...

		if Car /= Nil_Pointer then
			-- <bindings> is not empty
			Push_Frame (Interp, Opcode_Let_Binding, Car);
			Push_Frame (Interp, Opcode_Let_Evaluation, Car);
		end if;
	end Evaluate_Let_Syntax;

	procedure Evaluate_Letast_Syntax is
		pragma Inline (Evaluate_Letast_Syntax);
	begin
		Check_Let_Syntax;
		-- Car: <bindings>, Cdr: <body>

		-- Letast_Binding must see this new environment 
		-- and must make the binding in this environment.
		Interp.Environment := Make_Environment(Interp.Self, Interp.Environment);

		-- Body evaluation can be done the same way as normal let.
		Set_Frame_Opcode (Interp.Stack, Opcode_Let_Finish);
		Set_Frame_Operand (Interp.Stack, Cdr); 
		-- but in the environment pushed above.
		Set_Frame_Environment (Interp.Stack, Interp.Environment); 

		if Car /= Nil_Pointer then
			-- <bindings> is not empty
			Push_Frame (Interp, Opcode_Letast_Binding, Car);
		end if;
	end Evaluate_Letast_Syntax;

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
		Chain_Frame_Result (Interp, Interp.Stack, Get_Car(Operand));
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
			Ada.Text_IO.Put_LINE ("TOO FEW ARGUMENTS FOR SET");
			raise Syntax_Error;
		end if;

		Car := Get_Car(Operand); -- <variable>
		Cdr := Get_Cdr(Operand); -- cons cell to <expression>
		if Is_Symbol(Car) then
			if Get_Cdr(Cdr) /= Nil_Pointer then
				Ada.Text_IO.Put_LINE ("TOO MANY ARGUMENTS FOR set!");
				raise Syntax_Error;
			end if;
			Cdr := Get_Car(Cdr); -- <expression>

			-- Arrange to finish setting a variable after <expression> evaluation.
			Set_Frame_Opcode (Interp.Stack, Opcode_Finish_Set_Syntax);
			Set_Frame_Operand (Interp.Stack, Car); 

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

						if not Is_Cons(Operand) then
							-- e.g) (begin)
							--      (begin . 10)
							Ada.Text_IO.Put_LINE ("FUCKNING CDR FOR BEGIN");
							raise Syntax_Error;

						else
							Set_Frame_Opcode (Interp.Stack, Opcode_Evaluate_Group);
							Set_Frame_Operand (Interp.Stack, Operand);

							if (Interp.Trait.Trait_Bits and No_Optimization) = 0 then
								-- I call Evaluate_Group for optimization here.
								Evaluate_Group; -- for optimization only. not really needed.
								-- I can jump to Start_Over because Evaluate_Group called 
								-- above pushes an Opcode_Evaluate_Object frame.
								pragma Assert (Get_Frame_Opcode(Interp.Stack) = Opcode_Evaluate_Object);
								goto Start_Over; -- for optimization only. not really needed.
							end if;
						end if;

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
				if (Interp.Trait.Trait_Bits and No_Optimization) = 0 then
					while not Is_Normal_Pointer(Car) loop
						-- This while block is for optimization only. It's not really needed.
						-- If I know that the next object to evaluate is a literal object,
						-- I can simply reverse-chain it to the return field of the current 
						-- frame without pushing another frame dedicated for it.

						-- TODO: some normal pointers may point to a literal object. e.g.) bignum 
						--       then it can goto <<Literal>>.
						Chain_Frame_Result (Interp, Interp.Stack, Car);
						if Is_Cons(Cdr) then
							Operand := Cdr;
							Car := Get_Car(Operand);
							Cdr := Get_Cdr(Operand);
						else
							-- last cons 
							if Cdr /= Nil_Pointer then
								-- The last CDR is not Nil.
								Ada.Text_IO.Put_Line ("WARNING: $$$$$$$$$$$$$$$$$$$$$..FUCKING CDR.....................OPTIMIZATIN $$$$");
								raise Syntax_Error;
							end if;

							Operand := Reverse_Cons(Get_Frame_Result(Interp.Stack));
							Clear_Frame_Result (Interp.Stack);
							Set_Frame_Opcode (Interp.Stack, Opcode_Apply); 
							Set_Frame_Operand (Interp.Stack, Operand);
							goto Done;
						end if;
					end loop;
				end if;

				if Is_Cons(Cdr) then
					-- Not the last cons cell yet
					Set_Frame_Operand (Interp.Stack, Cdr); -- change the operand for the next call 
				else
					-- Reached the last cons cell
					if Cdr /= Nil_Pointer then
						-- The last CDR is not Nil.
						Ada.Text_IO.Put_Line ("WARNING: $$$$$$$$$$$$$$$$$$$$$..FUCKING CDR.....................$$$$");
						raise Syntax_Error;
					end if;

					-- Change the operand to a mark object so that the call to this 
					-- procedure after the evaluation of the last car goes to the 
					-- Mark_Object case.
					Set_Frame_Operand (Interp.Stack, Interp.Mark); 
				end if;

				-- Arrange to evaluate the car object
				if (Interp.Trait.Trait_Bits and No_Optimization) = 0 then
					Push_Frame (Interp, Opcode_Evaluate_Object, Car);
					goto Start_Over; -- for optimization only. not really needed.
				end if;
			end if;

		when Mark_Object =>
			-- TODO: you can use the mark context to differentiate context

			-- Get the evaluation result stored in the current stack frame by
			-- various sub-Opcode_Evaluate_Object frames. the return value 
			-- chain must be reversed Chain_Frame_Result reverse-chains values.
			Operand := Reverse_Cons(Get_Frame_Result(Interp.Stack));

			-- Refresh the current stack frame to Opcode_Apply.
			-- This should be faster than Popping the current frame and pushing
			-- a new frame.
			--   Envir := Get_Frame_Environment(Interp.Stack);
			--   Pop_Frame (Interp); -- done
			--   Push_Frame (Interp, Opcode_Apply, Operand, Envir); 
			Clear_Frame_Result (Interp.Stack);
			Set_Frame_Opcode (Interp.Stack, Opcode_Apply); 
			Set_Frame_Operand (Interp.Stack, Operand);

		when others =>
			-- normal literal object
			goto Literal;
	end case;
	goto Done;

<<Literal>>
	Pop_Frame (Interp); -- done
Ada.Text_IO.Put ("Return => ");
Print (Interp, Operand);
	Chain_Frame_Result (Interp, Interp.Stack, Operand);
	goto Done;

<<Done>>
	Pop_Tops (Interp, 3);
end Evaluate;
