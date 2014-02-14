separate (H2.Scheme.Execute)

procedure Evaluate is
	--pragma Inline (Evaluate);

	Operand: aliased Object_Pointer;

     -- ----------------------------------------------------------------

	generic
		Result: Object_Pointer; -- Result to return if no <test> expressions exist.
		Opcode: Opcode_Type; -- Switch to this opcode to evaluate the next <test>.
	procedure Generic_And_Or_Syntax;

	procedure Generic_And_Or_Syntax is
	begin
		-- (and <test1> <test2> ...)
		--   (and (= 2 2) (> 2 1))                  ==>  #t
		--   (and (= 2 2) (< 2 1))                  ==>  #f
		--   (and (= 2 2) (< 2 1) (= 3 3))          ==>  #f
		--   (and 1 2 'c '(f g))                    ==>  (f g)
		--   (and)                                  ==>  #t

		Operand := Get_Cdr(Operand);  -- Skip "And"
		if Operand = Nil_Pointer then
			-- (and)
			Return_Frame (Interp, Result);
		elsif not Is_Cons(Operand) or else Get_Last_Cdr(Operand) /= Nil_Pointer then
			-- (and . 10)
			-- (and 1 2 . 10)
			Ada.Text_IO.Put_LINE ("FUCKING CDR FOR DEFINE");
			raise Syntax_Error;	
		else
			--Switch_Frame (Interp.Stack, Opcode, Get_Cdr(Operand), Nil_Pointer); -- <test2> onwards
			--Push_Frame (Interp, Opcode_Evaluate_Object, Get_Car(Operand)); -- <test1>
			Switch_Frame (Interp.Stack, Opcode_Evaluate_Object, Get_Car(Operand), Nil_Pointer); -- <test2> onwards
			Push_Subframe (Interp, Opcode, Get_Cdr(Operand)); -- <test1> onwards
		end if;
	end Generic_And_Or_Syntax;

	procedure Evaluate_And_Syntax is new Generic_And_Or_Syntax(True_Pointer, Opcode_And_Finish);
	procedure Evaluate_Or_Syntax is new Generic_And_Or_Syntax(False_Pointer, Opcode_Or_Finish);

     -- ----------------------------------------------------------------

	procedure Evaluate_Begin_Syntax is
		pragma Inline (Evaluate_Begin_Syntax);
		Synlist: Object_Pointer;
	begin
		Synlist := Operand;
		Operand := Get_Cdr(Operand); -- Skip "begin"

		if (Interp.State and Force_Syntax_Check) /= 0 or else
		   (Synlist.Flags and Syntax_Checked) = 0 then

			if Operand /= Nil_Pointer and then
			   Get_Last_Cdr(Operand) /= Nil_Pointer then
				Ada.Text_IO.Put_LINE ("FUCKNING CDR FOR BEGIN");
				raise Syntax_Error;
			end if;

			Synlist.Flags := Synlist.Flags or Syntax_Checked;
		end if;

		if Operand = Nil_Pointer then
			-- (begin)
			-- Return nil to the upper frame for (begin).
			Return_Frame (Interp, Nil_Pointer);
		else
			Switch_Frame (Interp.Stack, Opcode_Grouped_Call, Operand, Nil_Pointer);
		end if;
	end Evaluate_Begin_Syntax;
     -- ----------------------------------------------------------------

	procedure Evaluate_Case_Syntax is
		pragma Inline (Evaluate_Case_Syntax);
		Synlist: Object_Pointer;
		Ptr1: Object_Pointer;
		Ptr2: Object_Pointer;
		Ptr3: Object_Pointer;
	begin
		-- (case <key> <clause 1> <clause 2> ...)
		-- <key> is an expression.
		-- <clause> should be of the form: 
		--    ((<datum 1> ...) <expression 1> <expression 2> ...)
		-- the last <clause> may be an else clause of the form:
		--    (else <expression 1> <expression 2> ...)
		--
		-- (case (* 2 3)
		--       ((2 3 5 7) 'prime)
		--       ((1 4 6 8 9) 'composite))
		--
		-- (case (car '(c d))
		--     ((a e i o u) 'vowel)
		--     ((w y) 'semivowel)
		--     (else 'consonant))
		--

		Synlist := Operand;
		Operand := Get_Cdr(Operand); -- Skip "case"

		if (Interp.State and Force_Syntax_Check) /= 0 or else
		   (Synlist.Flags and Syntax_Checked) = 0 then
		   
			if Not Is_Cons(Operand) then
				-- e.g) (case)
				--      (case . 10)
				Ada.Text_IO.Put_LINE ("NO KEY FOR CASE");
				raise Syntax_Error;
			end if;

			--Key := Get_Car(Operand); -- <key>

			Ptr1 := Get_Cdr(Operand); -- <clause> list.
			while Is_Cons(Ptr1) loop
				Ptr2 := Get_Car(Ptr1); -- <clause>
				if Get_Last_Cdr(Ptr2) /= Nil_Pointer then
					Ada.Text_IO.Put_Line ("FUCKING CDR FOR CASE CLAUSE");
					raise Syntax_Error;
				end if;
				
				Ptr3 := Get_Car(Ptr2); -- <datum> list or 'else'
				if Is_Cons(Ptr3) then
					if Get_Last_Cdr(Ptr3) /= Nil_Pointer then
						Ada.Text_IO.Put_LINE ("FUCKING CDR FOR CASE DATUM");
						raise Syntax_Error;
					end if;
				elsif Ptr3 = Interp.Else_Symbol then
					-- check <test>. if it's else, it should be in the last clause.
					if Is_Cons(Get_Cdr(Ptr1)) then
						Ada.Text_IO.Put_Line ("ELSE NOT IN THE LAST CASE CLAUSE");
						raise Syntax_Error;
					end if;
				else
					Ada.Text_IO.Put_LINE ("INVALID DATUM FOR CASE");
					raise Syntax_Error;
				end if;
				
				if Get_Cdr(Ptr2) = Nil_Pointer then
					Ada.Text_IO.Put_Line ("NO EXPRESSION IN CASE CLAUSE");
					raise Syntax_Error;
				end if;

				Ptr1 := Get_Cdr(Ptr1); -- next <clause> list
			end loop;

			if Ptr1 /= Nil_Pointer then
				Ada.Text_IO.Put_Line ("FUCKING LAST CLAUSE FOR CASE");
				raise Syntax_Error;
			end if;

			Synlist.Flags := Synlist.Flags or Syntax_Checked;
		end if;

		Switch_Frame (Interp.Stack, Opcode_Evaluate_Object, Get_Car(Operand), Nil_Pointer); -- <key>
		Push_Subframe (Interp, Opcode_Case_Finish, Get_Cdr(Operand)); -- <clause> list
	end Evaluate_Case_Syntax;

     -- ----------------------------------------------------------------

	procedure Evaluate_Cond_Syntax is
		pragma Inline (Evaluate_Cond_Syntax);
		Synlist: Object_Pointer;
	begin
		-- cond <clause 1> <clause 2> ...
		-- A clause should be of the form:
		--    (<test> <expression> ...)
		-- the last clause may be an else clause of the form:
		--    (else <expression> ...)
		-- 
		-- (cond ((> 3 2) 'greater)
		--       ((< 3 2) 'less))      =>  greater
		-- (cond ((> 3 3) 'greater)
		--       ((< 3 3) 'less)
		--       (else 'equal))        =>  equal

		Synlist := Operand;	
		Operand := Get_Cdr(Operand); -- Skip "cond". <clause> list
		
		if (Interp.State and Force_Syntax_Check) /= 0 or else
		   (Synlist.Flags and Syntax_Checked) = 0 then
			if Not Is_Cons(Operand) then
				-- e.g) (cond)
				--      (cond . 10)
				Ada.Text_IO.Put_LINE ("NO CLAUSE FOR COND");
				raise Syntax_Error;
			end if;

			declare 
				Ptr1: Object_Pointer := Operand;
				Ptr2: Object_Pointer;
			begin
				loop
					Ptr2 := Get_Car(Ptr1); -- <clause>
					if not Is_Cons(Ptr2) then
						Ada.Text_IO.Put_Line ("FUCKING CLAUSE FOR COND");
						raise Syntax_Error;
					end if;
					If Get_Last_Cdr(Ptr2) /= Nil_Pointer then
						Ada.Text_IO.Put_Line ("FUCKING CDR FOR COND CLAUSE");
						raise Syntax_Error;
					end if;
					
					if Get_Car(Ptr2) = Interp.Else_Symbol then
						-- check <test>. if it's else, it should be in the last clause.
						if Is_Cons(Get_Cdr(Ptr1)) then
							Ada.Text_IO.Put_Line ("ELSE NOT IN THE LAST COND CLAUSE");
							raise Syntax_Error;
						end if;
					end if;
					
					Ptr1 := Get_Cdr(Ptr1); -- next <clause> list
					exit when not Is_Cons(Ptr1);
				end loop;
				if Ptr1 /= Nil_Pointer then 
					Ada.Text_IO.Put_Line ("FUCKING LAST CLAUSE FOR COND");
					raise Syntax_Error;
				end if;
			end;
			
			Synlist.Flags := Synlist.Flags or Syntax_Checked;
		end if;
		
		Switch_Frame (Interp.Stack, Opcode_Evaluate_Object, Get_Car(Get_Car(Operand)), Nil_Pointer); -- <test> in first <clause>
		Push_Subframe (Interp, Opcode_Cond_Finish, Operand); -- <clause> list
	end Evaluate_Cond_Syntax;

     -- ----------------------------------------------------------------
	
	procedure Evaluate_Define_Syntax is
		pragma Inline (Evaluate_Define_Syntax);
		Synlist: Object_Pointer;
		Ptr1: Object_Pointer;
		Ptr2: Object_Pointer;
		Ptr3: Object_Pointer;
		Ptr4: Object_Pointer;
	begin
-- TODO: limit the context where define can be used.

		-- (define <variable> <expression>)
		-- (define (<variable> <formals>) <body>)
		-- (define (<variable> . <formal>) <body>)
		--
		-- e.g)
		--   (define x 10) 
		--   (define (add x y) (+ x y)) -> (define add (lambda (x y) (+ x y)))

		Synlist := Operand;
		Operand := Get_Cdr(Operand); -- Skip "define"

		if (Interp.State and Force_Syntax_Check) /= 0 or else 
		   (Synlist.Flags and Syntax_Checked) = 0 then

			if not Is_Cons(Operand) or else not Is_Cons(Get_Cdr(Operand)) then
				-- e.g) (define)
				--      (define . 10)
				--      (define x . 10)
				Ada.Text_IO.Put_LINE ("TOO FEW ARGUMENTS FOR DEFINE");
				raise Syntax_Error;
			end if;
			
			Ptr1 := Get_Car(Operand); 
			if Is_Cons(Ptr1) then
				-- (define (add x y) ...)

				Ptr2 := Get_Car(Ptr1); -- <variable> as a function name
				if not Is_Symbol(Ptr2) then
					Ada.Text_IO.Put_LINE ("WRONG NAME FOR DEFINE");
					raise Syntax_Error;
				end if;
				
				Ptr1 := Get_Cdr(Ptr1); -- <formals>
				Ptr2 := Ptr1;

				while Is_Cons(Ptr2) loop
					Ptr3 := Get_Car(Ptr2); -- <formal argument>
					if not Is_Symbol(Ptr3) then
						Ada.Text_IO.Put_Line ("WRONG FORMAL FOR LAMBDA");
						raise Syntax_Error;
					end if;

					-- Check for a duplication formal argument
					-- TODO: make duplication check optional or change the implementation more efficient so that this check is not repeated 
					Ptr4 := Ptr1;
					while Ptr4 /= Ptr2 loop
						if Get_Car(Ptr4) = Ptr3 then
							Ada.Text_IO.Put_Line ("DUPLICATE FORMAL FOR DEFINE");
							raise Syntax_Error;
						end if;
						Ptr4 := Get_Cdr(Ptr4);
					end loop;

					-- Move on to the next formal argument
					Ptr2 := Get_Cdr(Ptr2);
					exit when not Is_Cons(Ptr2);
				end loop;

				if Ptr2 /= Nil_Pointer and then not Is_Symbol(Ptr2) then
					Ada.Text_IO.Put_Line ("FUCKING CDR IN FORMALS FOR DEFINE");
					raise Syntax_Error;
				end if;

				Ptr1 := Get_Cdr(Operand); -- <body>
				if not Is_Cons(Ptr1) then
					Ada.Text_IO.Put_Line ("NO BODY");
					raise Syntax_Error;
				end if;

				if Get_Last_Cdr(Ptr1) /= Nil_Pointer then
					-- (lambda (x y) (+ x y) . 99)
					Ada.Text_IO.Put_Line ("FUCKING CDR IN BODY FOR LAMBDA");
					raise Syntax_Error;
				end if;

			elsif Is_Symbol(Ptr1) then
				if Get_Cdr(Get_Cdr(Operand)) /= Nil_Pointer then
					Ada.Text_IO.Put_LINE ("TOO MANY ARGUMENTS FOR DEFINE");
					raise Syntax_Error;
				end if;
			else
				Ada.Text_IO.Put_LINE ("NO SYMBOL NOR ARGUMENT LIST AFTER DEFINE");
				raise Syntax_Error;	
			end if;

			Synlist.Flags := Synlist.Flags or Syntax_Checked;
		end if;

		Ptr1 := Get_Car(Operand);
		if Is_Cons(Ptr1) then
			-- define a function:  (define (add x y) ...) 
			-- Get_Car(Ptr1) -- <variable>
			-- Get_Cdr(Ptr1) -- <formals>
			-- Get_Cdr(Operand) -- <body>

			-- It's ok to not reload but switch because no continuation
			-- can be created in this form of 'define'. 
			Switch_Frame (Interp.Stack, Opcode_Define_Finish, Get_Car(Ptr1), Nil_Pointer);

			-- Make closure and set it as a frame result. Note this is done
			-- after switching in order to avoid GC problems withoug using
			-- Push_Top/Pop_Tops.
			Ptr2 := Make_Cons(Interp.Self, Get_Cdr(Ptr1), Get_Cdr(Operand));
			Ptr2 := Make_Closure(Interp.Self, Ptr2, Get_Frame_Environment(Interp.Stack));
			Set_Frame_Result (Interp.Stack, Ptr2);
		else
			-- define a symbol: (define x ...)
			pragma Assert (Is_Symbol(Ptr1));

			-- Arrange to finish defining after value evaluation 
			-- and to evaluate the value part.
			Switch_Frame (Interp.Stack, Opcode_Evaluate_Object, Get_Car(Get_Cdr(Operand)), Nil_Pointer);
			Push_Subframe (Interp, Opcode_Define_Finish, Ptr1);
		end if;
	end Evaluate_Define_Syntax;

     -- ----------------------------------------------------------------

	procedure Evaluate_Do_Syntax is
		pragma Inline (Evaluate_Do_Syntax);
		Synlist: Object_Pointer;
	begin
		-- (do <bindings> <clause> <body>)
		-- <bindings> should be of the form: ((<variable 1> <init 1> <step 1>) ...)
		-- <clause> should  be of the form: (<test> <expression> ...)
		-- <body> is zero or more expressions.
		--
		-- (let ((x '(1 3 5 7 9)))
		--     (do ((x x (cdr x))
		--          (sum 0 (+ sum (car x)))) ; <bindings>
		--         ((null? x) sum) ; <clause>. if <test> is true, exit do with sum.
		--     )
		-- )
		--
		-- (do ((i 0 (+ i 1)))
		--     ((= i 5) i)     
		--     (display i) ; <body> downwards
		--     (newline)
		-- )
		
		Synlist := Operand;
		Operand := Get_Cdr(Operand); -- Skip "do"

		if (Interp.State and Force_Syntax_Check) /= 0 or else 
		   (Synlist.Flags and Syntax_Checked) = 0 then

			Ada.Text_IO.Put_LINE ("UNIMPLEMENTED");
			raise Evaluation_Error;

			Synlist.Flags := Synlist.Flags or Syntax_Checked;
		end if; 

		Ada.Text_IO.Put_LINE ("UNIMPLEMENTED");
		raise Evaluation_Error;
	end Evaluate_Do_Syntax;

     -- ----------------------------------------------------------------

	procedure Evaluate_If_Syntax is
		pragma Inline (Evaluate_If_Syntax);
		Synlist: Object_Pointer;
	begin
		-- (if <test> <consequent>)
		-- (if <test> <consequent> <alternate>)
		-- e.g) (if (> 3 2) 'yes)
		--      (if (> 3 2) 'yes 'no)
		--      (if (> 3 2) (- 3 2) (+ 3 2))

		Synlist := Operand;
		Operand := Get_Cdr(Operand); -- Skip "if".

		if (Interp.State and Force_Syntax_Check) /= 0 or else 
		   (Synlist.Flags and Syntax_Checked) = 0 then
			declare
				Ptr: Object_Pointer := Operand;
			begin
				if Not Is_Cons(Ptr) then
					-- e.g) (if)
					--      (if . 10)
					Ada.Text_IO.Put_LINE ("NO CONDITIONAL FOR IF");
					raise Syntax_Error;
				end if;

				Ptr := Get_Cdr(Ptr); -- cons cell containg <consequent>
				if not Is_Cons(Ptr) then
					Ada.Text_IO.Put_Line ("NO CONSEQUENT FOR IF");
					raise Syntax_Error;
				end if;

				Ptr := Get_Cdr(Ptr); -- cons cell containing <alternate>
				if Ptr = Nil_Pointer then
					-- no <alternate>. it's ok
					null;
				elsif not Is_Cons(Ptr) then
					-- no <alternate> but reduncant cdr.	
					-- (if (> 3 2) 3 . 99)
					Ada.Text_IO.Put_Line ("FUCKING CDR FOR IF");
					raise Syntax_Error;
					
				elsif Get_Cdr(Ptr) /= Nil_Pointer then
					-- (if (> 3 2) 3 2 . 99) 
					-- (if (> 3 2) 3 2 99) 
					Ada.Text_IO.Put_Line ("TOO MANY ARGUMENTS FOR IF");
					raise Syntax_Error;
				end if;
			end;

			Synlist.Flags := Synlist.Flags or Syntax_Checked;
		end if;
		
		-- Arrange to evaluate <consequent> or <alternate> after <test> 
		-- evaluation and to evaluate <test>. Use Switch_Frame/Push_Subframe
		-- instead of Switch_Frame/Push_Frame for continuation to work.
		Switch_Frame (Interp.Stack, Opcode_Evaluate_Object, Get_Car(Operand), Nil_Pointer); -- <test>
		Push_Subframe (Interp, Opcode_If_Finish, Get_Cdr(Operand)); -- <consequent> and later
	end Evaluate_If_Syntax;

     -- ----------------------------------------------------------------

	procedure Evaluate_Lambda_Syntax is
		pragma Inline (Evaluate_Lambda_Syntax);
		Synlist: Object_Pointer;
	begin
		-- (lambda <formals> <body>)
		-- e.g)  (lambda (x y) (+ x y))
		-- e.g)  (lambda (x y . z) z)
		-- e.g)  (lambda x (car x))
		Synlist := Operand;
		Operand := Get_Cdr(Operand); -- Skip "lambda". cons cell pointing to <formals>

		if (Interp.State and Force_Syntax_Check) /= 0 or else 
		   (Synlist.Flags and Syntax_Checked) = 0 then
			if not Is_Cons(Operand) then
				-- e.g) (lambda)
				--      (lambda . 10)
				Ada.Text_IO.Put_LINE ("FUCKNING CDR FOR LAMBDA");
				raise Syntax_Error;
			end if;

			declare
				Ptr1: Object_Pointer;
				Ptr2: Object_Pointer;
				Ptr3: Object_Pointer;
				Ptr4: Object_Pointer;
			begin

				Ptr1 := Get_Car(Operand);  -- <formals>
				if Ptr1 = Nil_Pointer or else Is_Symbol(Ptr1) then
					-- (lambda () ...) or (lambda x ...)
					-- nothing to do	
					null;
				elsif Is_Cons(Ptr1) then 
					Ptr2 := Ptr1;
					loop
						Ptr3 := Get_Car(Ptr2); -- <formal argument>
						if not Is_Symbol(Ptr3) then
							Ada.Text_IO.Put_Line ("WRONG FORMAL FOR LAMBDA");
							raise Syntax_Error;
						end if;

						-- Check for a duplication formal argument
						-- TODO: make duplication check optional or change the implementation more efficient so that this check is not repeated 
						Ptr4 := Ptr1;
						while Ptr4 /= Ptr2 loop
							if Get_Car(Ptr4) = Ptr3 then
								Ada.Text_IO.Put_Line ("DUPLICATE FORMAL FOR LAMBDA");
								raise Syntax_Error;
							end if;
							Ptr4 := Get_Cdr(Ptr4);
						end loop;

						-- Move on to the next formal argument
						Ptr2 := Get_Cdr(Ptr2);
						exit when not Is_Cons(Ptr2);
					end loop;

					if Ptr2 /= Nil_Pointer and then not Is_Symbol(Ptr2) then
						Ada.Text_IO.Put_Line ("FUCKING CDR IN FORMALS FOR LAMBDA");
						raise Syntax_Error;
					end if;
				else 
					Ada.Text_IO.Put_Line ("INVALID FORMALS FOR LAMBDA");
					raise Syntax_Error;
				end if;

				Ptr1 := Get_Cdr(Operand); -- cons cell containing <body>
				if not Is_Cons(Ptr1) then
					Ada.Text_IO.Put_Line ("NO BODY");
					raise Syntax_Error;
				end if;

				if Get_Last_Cdr(Ptr1) /= Nil_Pointer then
					-- (lambda (x y) (+ x y) . 99)
					Ada.Text_IO.Put_Line ("FUCKING CDR IN BODY FOR LAMBDA");
					raise Syntax_Error;
				end if;
			end;

			Synlist.Flags := Synlist.Flags or Syntax_Checked;
		end if;

		-- Create a closure object and return it the the upper frame.
		Return_Frame (Interp, Make_Closure(Interp.Self, Operand, Get_Frame_Environment(Interp.Stack)));
	end Evaluate_Lambda_Syntax;

     -- ----------------------------------------------------------------

	procedure Check_Let_Syntax is
		pragma Inline (Check_Let_Syntax);
		Bindings: Object_Pointer;
		LetBody: Object_Pointer;
	begin
		-- (let <bindings> <body>)
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
			declare
				Ptr1: Object_Pointer := Bindings;
				Ptr2: Object_Pointer;
				Ptr3: Object_Pointer;
			begin	
				loop
					Ptr2 := Get_Car(Ptr1); -- <binding>
					if not Is_Cons(Ptr2) or else not Is_Cons(Get_Cdr(Ptr2)) or else Get_Cdr(Get_Cdr(Ptr2)) /= Nil_Pointer then
						-- no binding name or no binding value or garbage after that
						Ada.Text_IO.Put_Line ("WRONG BINDING FOR LET");
						raise Syntax_Error;
					end if;

					Ptr2 := Get_Car(Ptr2); -- <binding> name
					if not Is_Symbol(Ptr2) then
						Ada.Text_IO.Put_Line ("WRONG BINDING NAME FOR LET");
						raise Syntax_Error;
					end if;

					-- Check for a duplicate binding name
					-- TODO: make duplication check optional or change the implementation more efficient so that this check is not repeated 
					Ptr3 := Bindings;
					while Ptr3 /= Ptr1 loop
						if Get_Car(Get_Car(Ptr3)) = Ptr2 then
							Ada.Text_IO.Put_Line ("DUPLICATE BINDING FOR LET");
							raise Syntax_Error;
						end if;
						Ptr3 := Get_Cdr(Ptr3);
					end loop;

					-- Move on to the next binding
					Ptr1 := Get_Cdr(Ptr1);
					exit when not Is_Cons(Ptr1);
				end loop;

				if Ptr1 /= Nil_Pointer then
					-- The last cdr is not nil.
					Ada.Text_IO.Put_Line ("FUCKING CDR FOR LET BINDING");
					raise Syntax_Error;
				end if;
			end;
		end  if;
	end Check_Let_Syntax;

	procedure Evaluate_Let_Syntax is
		pragma Inline (Evaluate_Let_Syntax);
		Synlist: Object_Pointer;
		Envir: aliased Object_Pointer;		
		Bindings: aliased Object_Pointer;
	begin
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

		Synlist := Operand;
		Operand := Get_Cdr(Operand); -- Skip "let".

		if (Interp.State and Force_Syntax_Check) /= 0 or else 
		   (Synlist.Flags and Syntax_Checked) = 0 then
			Check_Let_Syntax;
			Synlist.Flags := Synlist.Flags or Syntax_Checked;
		end if;

		-- Switch the frame to Opcode_Grouped_Call and let its environment
		-- be the new environment created. Use Reload_Frame() instead
		-- of Switch_Frame() for continuation. This frame is executed once
		-- the Opcode_Let_Binding frame pushed in the 'if' block is finished.
		Reload_Frame (Interp, Opcode_Grouped_Call, Get_Cdr(Operand)); -- <body>

		-- Create a new environment over the current environment.
		Envir := Make_Environment(Interp.Self, Get_Frame_Environment(Interp.Stack));
		Set_Frame_Environment (Interp.Stack, Envir); -- update the environment

		Bindings := Get_Car(Operand); -- <bindings>
		if Bindings /= Nil_Pointer then
			-- <bindings> is not empty

			Push_Top (Interp, Envir'Unchecked_Access);
			Push_Top (Interp, Bindings'Unchecked_Access);

			Envir := Get_Frame_Environment(Get_Frame_Parent(Interp.Stack));

			-- Say, <bindings> is ((x 2) (y 2)).
			-- Get_Car(Bindings) is (x 2).
			-- To get x, Get_Car(Get_Car(Bindings))
			-- To get 2, Get_Car(Get_Cdr(Get_Car(Bindings)))

			-- Arrange to evaluate the first <binding> expression in the parent environment.
			Push_Frame_With_Environment (Interp, Opcode_Evaluate_Object, Get_Car(Get_Cdr(Get_Car(Bindings))), Envir);

			-- Arrange to perform actual binding. Pass the <binding> name as an intermediate 
			-- and the next remaing <binding> list as an operand.
			Push_Subframe_With_Environment_And_Intermediate (Interp, Opcode_Let_Binding, Get_Cdr(Bindings), Envir, Get_Car(Get_Car(Bindings)));

			Pop_Tops (Interp, 2);
		end if;
	end Evaluate_Let_Syntax;

	procedure Evaluate_Letast_Syntax is
		pragma Inline (Evaluate_Letast_Syntax);
		Synlist: Object_Pointer;
		Envir: aliased Object_Pointer;
		Bindings: aliased Object_Pointer;
	begin
		Synlist := Operand;
		Operand := Get_Cdr(Operand); -- Skip "let".
		
		if (Interp.State and Force_Syntax_Check) /= 0 or else 
		   (Synlist.Flags and Syntax_Checked) = 0 then
			Check_Let_Syntax;
			Synlist.Flags := Synlist.Flags or Syntax_Checked;
		end if;

		Reload_Frame (Interp, Opcode_Grouped_Call, Get_Cdr(Operand)); -- <body>

		-- Create a new environment over the current environment.
		Envir := Make_Environment(Interp.Self, Get_Frame_Environment(Interp.Stack));
		Set_Frame_Environment (Interp.Stack, Envir); -- update the environment

		Bindings := Get_Car(Operand); -- <bindings>
		if Bindings /= Nil_Pointer then
			-- <bindings> is not empty

			Push_Top (Interp, Envir'Unchecked_Access);
			Push_Top (Interp, Bindings'Unchecked_Access);

			-- Say, <bindings> is ((x 2) (y 2)).
			-- Get_Car(Bindings) is (x 2).
			-- To get x, Get_Car(Get_Car(Bindings))
			-- To get 2, Get_Car(Get_Cdr(Get_Car(Bindings)))

			-- Arrange to evaluate the first <binding> expression in the parent environment.
			Push_Frame_With_Environment (Interp, Opcode_Evaluate_Object, Get_Car(Get_Cdr(Get_Car(Bindings))), Envir);

			-- Arrange to perform actual binding. Pass the <binding> name as an intermediate 
			-- and the next remaing <binding> list as an operand.
			Push_Subframe_With_Environment_And_Intermediate (Interp, Opcode_Letast_Binding, Get_Cdr(Bindings), Envir, Get_Car(Get_Car(Bindings)));

			Pop_Tops (Interp, 2);
		end if;
	end Evaluate_Letast_Syntax;

	procedure Evaluate_Letrec_Syntax is
		pragma Inline (Evaluate_Letrec_Syntax);
		Synlist: Object_Pointer;
		Envir: Object_Pointer;
		Bindings: aliased Object_Pointer;
	begin
		Synlist := Operand;
		Operand := Get_Cdr(Operand); -- Skip "let".
		
		if (Interp.State and Force_Syntax_Check) /= 0 or else 
		   (Synlist.Flags and Syntax_Checked) = 0 then
			Check_Let_Syntax;
			Synlist.Flags := Synlist.Flags or Syntax_Checked;
		end if;

		-- Switch the frame to Opcode_Grouped_Call and let its environment
		-- be the new environment created. Use Reload_Frame() instead
		-- of Switch_Frame() for continuation. This frame is executed once
		-- the Opcode_Letrec_Binding frame pushed in the 'if' block is finished.
		Reload_Frame (Interp, Opcode_Grouped_Call, Get_Cdr(Operand)); -- <test>

		-- Create a new environment over the current environment.
		Envir := Make_Environment(Interp.Self, Get_Frame_Environment(Interp.Stack));
		Set_Frame_Environment (Interp.Stack, Envir); -- update the environment

		Bindings := Get_Car(Operand); -- <bindings>
		if Bindings /= Nil_Pointer then
			-- <bindings> is not empty

			Push_Top (Interp, Bindings'Unchecked_Access);
			-- Say, <bindings> is ((x 2) (y 2)).
			-- Get_Car(Bindings) is (x 2).
			-- To get x, Get_Car(Get_Car(Bindings))
			-- To get 2, Get_Car(Get_Cdr(Get_Car(Bindings)))

			-- Arrange to evaluate the first <binding> expression in the parent environment.
			Push_Frame (Interp, Opcode_Evaluate_Object, Get_Car(Get_Cdr(Get_Car(Bindings))));

			-- Arrange to perform actual binding. Pass the <binding> name as an intermediate 
			-- and the next remaing <binding> list as an operand.
			Push_Subframe_With_Intermediate (Interp, Opcode_Letrec_Binding, Get_Cdr(Bindings), Get_Car(Get_Car(Bindings)));

			Pop_Tops (Interp, 1);
		end if;
	end Evaluate_Letrec_Syntax;

     -- ----------------------------------------------------------------
	procedure Evaluate_Quasiquote_Syntax is
		pragma Inline (Evaluate_Quasiquote_Syntax);
	begin
		Ada.Text_IO.Put_LINE ("UNIMPLEMENTED");
		raise Evaluation_Error;
	end Evaluate_Quasiquote_Syntax;

     -- ----------------------------------------------------------------

	procedure Evaluate_Quote_Syntax is
		pragma Inline (Evaluate_Quote_Syntax);
		Synlist: Object_Pointer;
	begin
		Synlist := Operand;
		Operand := Get_Cdr(Operand); -- Skip "quote". Get the first argument.

		if (Interp.State and Force_Syntax_Check) /= 0 or else 
		   (Synlist.Flags and Syntax_Checked) = 0 then

			if not Is_Cons(Operand) then
				-- e.g) (quote)
				--      (quote . 10)
				Ada.Text_IO.Put_LINE ("TOO FEW ARGUMETNS FOR QUOTE");
				raise Syntax_Error;
			elsif Get_Cdr(Operand) /= Nil_Pointer then
				Ada.Text_IO.Put_LINE ("TOO MANY ARGUMENTS FOR QUOTE");
				raise Syntax_Error;
			end if;

			Synlist.Flags := Synlist.Flags or Syntax_Checked;
		end if;

		Return_Frame (Interp, Get_Car(Operand));
	end Evaluate_Quote_Syntax;

     -- ----------------------------------------------------------------

	procedure Evaluate_Set_Syntax is
		pragma Inline (Evaluate_Set_Syntax);
		Synlist: Object_Pointer;
	begin
		-- (set! <variable> <expression>)
		-- e.g) (set! x 10) 

		Synlist := Operand;
		Operand := Get_Cdr(Operand); -- Skip "set!"

		if (Interp.State and Force_Syntax_Check) /= 0 or else 
		   (Synlist.Flags and Syntax_Checked) = 0 then
			if not Is_Cons(Operand) or else not Is_Cons(Get_Cdr(Operand)) then
				-- e.g) (set!)
				--      (set . 10)
				--      (set x . 10)
				Ada.Text_IO.Put_LINE ("TOO FEW ARGUMENTS FOR SET!");
				raise Syntax_Error;
			end if;

			if not Is_Symbol(Get_Car(Operand)) then -- <variable>
				Ada.Text_IO.Put_LINE ("INVALID SYMBOL AFTER SET!");
				raise Syntax_Error;	
			end if;

			if Get_Cdr(Get_Cdr(Operand)) /= Nil_Pointer then
				-- (set x 10 20)
				-- (set x 10 . 20)
				Ada.Text_IO.Put_LINE ("TOO MANY ARGUMENTS FOR SET!");
				raise Syntax_Error;
			end if;

			Synlist.Flags := Synlist.Flags or Syntax_Checked;
		end if;

		Switch_Frame (Interp.Stack, Opcode_Evaluate_Object, Get_Car(Get_Cdr(Operand)), Nil_Pointer); -- <expression>
		Push_Subframe (Interp, Opcode_Set_Finish, Get_Car(Operand));  -- <variable>
	end Evaluate_Set_Syntax;

     -- ----------------------------------------------------------------

	procedure Evaluate_List is
		Ptr: Object_Pointer;
	begin
		Ptr := Get_Car(Operand);
		if Is_Syntax(Ptr) then
			-- special syntax symbol. normal evaluation rule doesn't 
			-- apply for special syntax objects.

			case Ptr.Scode is
				when And_Syntax =>
					Evaluate_And_Syntax;
					
				when Begin_Syntax =>
					Evaluate_Begin_Syntax;

				when Case_Syntax =>
					Evaluate_Case_Syntax;

				when Cond_Syntax =>
					Evaluate_Cond_Syntax;

				when Define_Syntax =>
					Evaluate_Define_Syntax;

				when Do_Syntax =>
					Evaluate_Do_Syntax;

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

				when Quasiquote_Syntax =>
					Evaluate_Quasiquote_Syntax;

				when Quote_Syntax =>
					Evaluate_Quote_Syntax;

				when Set_Syntax => -- set!
					Evaluate_Set_Syntax;

			end case;
		else
			-- procedure call
			-- (<operator> <operand1> ...)
			if (Interp.State and Force_Syntax_Check) /= 0 or else 
			   (Operand.Flags and Syntax_Checked) = 0 then

				if Get_Last_Cdr(Operand) /= Nil_Pointer then
					Ada.Text_IO.Put_Line ("ERROR: FUCKING CDR FOR PROCEDURE CALL.$$$$");
					raise Syntax_Error;
				end if;

				Operand.Flags := Operand.Flags or Syntax_Checked;
			end if;

			-- Switch the current frame to evaluate <operator>
			Switch_Frame (Interp.Stack, Opcode_Evaluate_Object, Ptr, Nil_Pointer);
			-- Push a new frame to evaluate arguments.
			Push_Subframe (Interp, Opcode_Procedure_Call, Get_Cdr(Operand));
		end if;
	end Evaluate_List;

begin
	Push_Top (Interp, Operand'Unchecked_Access);

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
			declare
				Ptr: Object_Pointer;
			begin
				Ptr := Get_Environment (Interp.Self, Operand);
				if Ptr = null then
					-- unbound
					Ada.Text_IO.Put_Line ("Unbound symbol....");
					Print (Interp, Operand);		
					raise Evaluation_Error;
				else
					-- symbol found in the environment
					Operand := Ptr; 
					goto Literal;  -- In fact, this is not a literal, but can be handled in the same way
				end if;
			end;

		when Cons_Object => -- Is_Cons(Operand)
			-- ( ... )
			Evaluate_List;

		when others =>
			-- normal literal object
			goto Literal;
	end case;
	goto Done;

<<Literal>>
	Return_Frame (Interp, Operand);
	goto Done;

<<Done>>
	Pop_Tops (Interp, 1);
end Evaluate;
