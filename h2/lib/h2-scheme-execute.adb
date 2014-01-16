
separate (H2.Scheme)

procedure Execute (Interp: in out Interpreter_Record) is

	LC: IO_Character_Record renames Interp.Input.Iochar;

	procedure Evaluate_Result is
		pragma Inline (Evaluate_Result);
	begin
		-- The result from the previous frame is stored in the current frame.
		-- This procedure takes the result and switch it to an operand and clears it.
		-- It is used to evaluate the result of Read_Object in principle.
		-- It takes only the head(car) element of the result chain. 
		-- Calling this function to evaluate the result of any arbitrary frame 
		-- other than 'Read_Object' is not recommended.
		Set_Frame_Operand (Interp.Stack, Get_Car(Get_Frame_Result(Interp.Stack)));
		Clear_Frame_Result (Interp.Stack);
		Set_Frame_Opcode (Interp.Stack, Opcode_Evaluate_Object);
	end Evaluate_Result;

	procedure Evaluate_Group is
		pragma Inline (Evaluate_Group);

		Operand: aliased Object_Pointer;
		Car: aliased Object_Pointer;
		Cdr: aliased Object_Pointer;
	begin
		Push_Top (Interp, Operand'Unchecked_Access);
		Push_Top (Interp, Car'Unchecked_Access);
		Push_Top (Interp, Cdr'Unchecked_Access);

		Operand := Get_Frame_Operand(Interp.Stack);
		pragma Assert (Is_Normal_Pointer(Operand));

		case Operand.Tag is
			when Cons_Object =>
				Car := Get_Car(Operand);
				Cdr := Get_Cdr(Operand);

				if Is_Cons(Cdr) then
					-- Let the current frame remember the next expression list
					Set_Frame_Operand (Interp.Stack, Cdr);
				else
					if Cdr /= Nil_Pointer then
						-- The last CDR is not Nil.
						Ada.Text_IO.Put_Line ("$$$$..................FUCKING CDR. FOR GROUP....................$$$$");
						-- raise Syntax_Error;
					end if;

					-- Change the operand to a mark object so that the call to this 
					-- procedure after the evaluation of the last car goes to the 
					-- Mark_Object case.
					Set_Frame_Operand (Interp.Stack, Interp.Mark); 
				end if;

				-- Clear the return value from the previous expression.
				Clear_Frame_Result (Interp.Stack);

				-- Arrange to evaluate the current expression
				Push_Frame (Interp, Opcode_Evaluate_Object, Car);

			when Mark_Object =>
				Operand := Get_Frame_Result (Interp.Stack);
				Pop_Frame (Interp); -- Done

				-- There must be only 1 return value chained in the Group frame.
				pragma Assert (Get_Cdr(Operand) = Nil_Pointer);

				-- Transfer the only return value to the upper chain
				Chain_Frame_Result (Interp, Interp.Stack, Get_Car(Operand));

			when others =>
				raise Internal_Error;
		end case;

		Pop_Tops (Interp, 3);
	end Evaluate_Group;

	procedure Evaluate_Object is
		pragma Inline (Evaluate_Object);

		Operand: aliased Object_Pointer;
		Car: aliased Object_Pointer;
		Cdr: aliased Object_Pointer;
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
						when Begin_Syntax =>

							Operand := Cdr; -- Skip "begin"

							if not Is_Cons(Operand) then
								-- e.g) (begin)
								--      (begin . 10)
								Ada.Text_IO.Put_LINE ("FUCKNING CDR FOR BEGIN");
								raise Syntax_Error;
								--Pop_Frame (Interp);  -- Done

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
							-- (define x 10) 
							-- (define (add x y) (+ x y)) -> (define add (lambda (x y) (+ x y)))
							Operand := Cdr; -- Skip "define"

							if not Is_Cons(Operand) then
								-- e.g) (define)
								--      (define . 10)
								Ada.Text_IO.Put_LINE ("FUCKNING CDR FOR DEFINE");
								raise Syntax_Error;
							elsif Get_Cdr(Operand) /= Nil_Pointer then
								-- TODO: IMPLEMENT OTHER CHECK
								null;
							end if;
				
							--Pop_Frame (Interp);	 -- Done
							--Chain_Frame_Result (Interp, Interp.Stack,  Get_Car(Operand));
							-- TODO: IMPLEMENT DEFINE.

						when Lambda_Syntax =>
							-- (lambda (x y) (+ x y));
							Operand := Cdr; -- Skip "lambda"
							if not Is_Cons(Operand) then
								-- e.g) (lambda)
								--      (lambda . 10)
								Ada.Text_IO.Put_LINE ("FUCKNING CDR FOR BEGIN");
								raise Syntax_Error;
								--Pop_Frame (Interp);  -- Done
							else
								if not Is_Cons(Get_Car(Operand)) then
									Ada.Text_IO.Put_Line ("INVALID PARRAMETER LIST");
									raise Syntax_Error;
									--Pop_Frame (Interp);  -- Done
								end if;

--Print (Interp, Get_Cdr(Operand));
								if not Is_Cons(Get_Cdr(Operand)) then
									Ada.Text_IO.Put_Line ("NO BODY");
									raise Syntax_Error;
									--Pop_Frame (Interp);  -- Done
								end if;

								declare
									Closure: aliased Object_Pointer;
								begin
Push_Top (Interp, Closure'Unchecked_Access); -- not necessary
									Closure := Make_Closure (Interp.Self, Operand, Interp.Environment);
									Pop_Frame (Interp);  -- Done
									Chain_Frame_Result (Interp, Interp.Stack, Closure);
Pop_Tops (Interp, 1); -- not necessary 
								end;
							end if;

						when Quote_Syntax =>
							Operand := Cdr; -- Skip "quote"
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
							Chain_Frame_Result (Interp, Interp.Stack,  Get_Car(Operand));

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
							Chain_Frame_Result (Interp, Interp.Stack, Car);
							if Is_Cons(Cdr) then
								Operand := Cdr;
								Car := Get_Car(Operand);
								Cdr := Get_Cdr(Operand);
							else
								-- last cons 
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
							Ada.Text_IO.Put_Line ("$$$$..................FUCKING CDR.....................$$$$");
							-- raise Syntax_Error;
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
	end Evaluate_Object;

	procedure Evaluate_Procedure is
		pragma Inline (Evaluate_Procedure);
	begin
		null;
	end Evaluate_Procedure;

	procedure Apply is
		pragma Inline (Apply);

		Operand: aliased Object_Pointer;
		Func: aliased Object_Pointer;
		Args: aliased Object_Pointer;

		procedure Apply_Car_Procedure is
		begin
			Pop_Frame (Interp); -- Done with the current frame
			Chain_Frame_Result (Interp, Interp.Stack, Get_Car(Args));
		end Apply_Car_Procedure;

		procedure Apply_Cdr_Procedure is
		begin
			Pop_Frame (Interp); -- Done with the current frame
			Chain_Frame_Result (Interp, Interp.Stack, Get_Cdr(Args));
		end Apply_Cdr_Procedure;

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
-- TODO: GC. Func may be invalid if GC has been invoked.

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
				
-- TODO: GC. the environment construction can cause GC. so Fbody here may be invalid.
-- TODO: is it correct to keep the environement  in the frame?
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

Print (Interp, Operand);
		Func := Get_Car(Operand);
		if not Is_Normal_Pointer(Func) then
			Ada.Text_IO.Put_Line ("INVALID FUNCTION TYPE");
			raise Evaluation_Error;
		end if;

		Args := Get_Cdr(Operand);

		-- No GC must be performed here.
		-- Otherwise, Operand, Func, Args get invalidated
		-- since GC doesn't update local variables.

		case Func.Tag is
			when Procedure_Object => 
				case Get_Procedure_Opcode(Func) is
					when Car_Procedure =>
						Apply_Car_Procedure;
					when Cdr_Procedure =>
						Apply_Cdr_Procedure;

					when Add_Procedure =>
						Apply_Add_Procedure;
					when Subtract_Procedure =>
						Apply_Subtract_Procedure;

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

	procedure Fetch_Character is
	begin
	-- TODO: calculate Interp.Input.Row, Interp.Input.Column
		if Interp.Input.Pos >= Interp.Input.Last then
			if Interp.Input.Flags /= 0 then
				-- An error has occurred or EOF has been reached previously.
				-- Note calling this procedure after EOF results in an error.
				Interp.Input.Iochar := (Error_Character, Object_Character'First);
				--return;
				raise IO_Error;
			end if;

			Interp.Input.Pos := Interp.Input.Data'First - 1;
			begin
				Read (Interp.Input.Stream.all, Interp.Input.Data, Interp.Input.Last);
			exception
				when others =>
					-- The callee can raise an exception upon errors.
					-- If an exception is raised, data read into the buffer 
					-- is also ignored.
					Interp.Input.Flags := Interp.Input.Flags and IO_Error_Occurred;
					Interp.Input.Iochar := (Error_Character, Object_Character'First);
					--return;
					raise IO_Error;
			end;
			if Interp.Input.Last < Interp.Input.Data'First then	
				-- The callee must read 0 bytes on EOF 
				Interp.Input.Flags := Interp.Input.Flags and IO_End_Reached;
				Interp.Input.Iochar := (End_Character, Object_Character'First);
				return;
			end if;
		end if;
		Interp.Input.Pos := Interp.Input.Pos + 1;
		Interp.Input.Iochar := (Normal_Character, Interp.Input.Data(Interp.Input.Pos));
	end Fetch_Character;

	function Is_White_Space (X: in Object_Character) return Standard.Boolean is
	begin
		return X = Ch.Space or else X = Ch.HT or else X = Ch.VT or else 
		       X = Ch.CR or else X = Ch.LF or else X = Ch.FF;
	end Is_White_Space;

	function Is_Identifier_Stopper (X: in Object_Character) return  Standard.Boolean is
	begin
		return X = Ch.Left_Parenthesis or else X = Ch.Right_Parenthesis or else
		       X = Ch.Apostrophe or else LC.Value = Ch.Quotation or else
		       X = Ch.Number_Sign or else LC.Value = Ch.Semicolon or else 
	            Is_White_Space(X);
	end Is_Identifier_Stopper;

	procedure Skip_Spaces_And_Comments is
	begin
		loop
			exit when LC.Kind /= Normal_Character;

			-- Normal character
			if Is_White_Space(LC.Value) then
				Fetch_Character;
			elsif LC.Value = Ch.Semicolon then
				-- Comment.
				loop
					Fetch_Character;
					exit when LC.Kind = End_Character; -- EOF before LF

					if LC.Kind = Normal_Character and then LC.Value = Ch.LF then -- TODO: handle different line ending convention
						Fetch_Character; -- Read the next character after LF
						exit;
					end if;
				end loop;
			else
				exit;
			end if;
		end loop;
	end Skip_Spaces_And_Comments;

	procedure Fetch_Token is
		Tmp: Object_Character_Array(1..10); -- large enough???
	begin
		if not Interp.LC_Unfetched then
			Fetch_Character;
		else
			-- Reuse the last character unfetched
			Interp.LC_Unfetched := Standard.False;
		end if;
		Skip_Spaces_And_Comments;
		if LC.Kind /= Normal_Character then
			Token.Set (Interp, End_Token);
			return;
		end if;

		-- TODO: Pass Token Location when calling Token.Set

		-- Use Ch.Pos.XXX values instead of Ch.XXX values as gnat complained that 
		-- Ch.XXX values are not static. For this reason, "case LC.Value is ..."
		-- changed to use Object_Character'Pos(LC.Value).
		case Object_Character'Pos(LC.Value) is

			when Ch.Pos.Left_Parenthesis =>
				Token.Set (Interp, Left_Parenthesis_Token, LC.Value);

			when Ch.Pos.Right_Parenthesis =>
				Token.Set (Interp, Right_Parenthesis_Token, LC.Value);

			when Ch.Pos.Period =>
				Token.Set (Interp, Period_Token, LC.Value);

			when Ch.Pos.Apostrophe =>
				Token.Set (Interp, Single_Quote_Token, LC.Value);

			when Ch.Pos.Quotation =>
				Fetch_Character;
				Token.Set (Interp, String_Token);
				loop
					if LC.Kind /= Normal_Character then
						-- String ended prematurely.
						-- TODO: Set Error code, Error Number.... Error location
						raise Syntax_Error;		
					end if;

					if LC.Value = Ch.Backslash then
						Fetch_Character;
						if LC.Kind /= Normal_Character then
							-- String ended prematurely.
							-- TODO: Set Error code, Error Number.... Error location
							raise Syntax_Error;		
						end if;
						-- TODO: escape letters??? \n \r \\ etc....
						Token.Append_Character (Interp, LC.Value);
					elsif LC.Value = Ch.Quotation then
						exit;
					else
						Token.Append_Character (Interp, LC.Value);
						Fetch_Character;
					end if;
				end loop;

			when Ch.Pos.Number_Sign =>
				Fetch_Character;
				-- TODO: t, false, etc

			when Ch.Pos.Zero .. Ch.Pos.Nine =>
				-- TODO; negative number, floating-point number, bignum, hexdecimal, etc
				Token.Set (Interp, Integer_Token);
				loop
					Token.Append_Character (Interp, LC.Value);
					Fetch_Character;
					if LC.Kind /= Normal_Character or else
					   LC.Value not in Ch.Zero .. Ch.Nine  then
						-- Unfetch the last character
						Interp.LC_Unfetched := Standard.True;
						exit;
					end if;
				end loop;

			when Ch.Pos.Plus_Sign | Ch.Pos.Minus_Sign =>

				Tmp(1) := LC.Value;

				Fetch_Character;
				if LC.Kind = Normal_Character and then
				   LC.Value in Ch.Zero .. Ch.Nine then
					Token.Set (Interp, Integer_Token, Tmp(1..1));
					loop
						Token.Append_Character (Interp, LC.Value);
						Fetch_Character;
						if LC.Kind /= Normal_Character or else
						   LC.Value not in Ch.Zero .. Ch.Nine  then
							-- Unfetch the last character
							Interp.LC_Unfetched := Standard.True;
							exit;
						end if;
					end loop;
				else
					Token.Set (Interp, Identifier_Token, Tmp(1..1));
					loop
				-- TODO: more characters
						if LC.Kind /= Normal_Character or else
						   Is_Identifier_Stopper(LC.Value) then
							-- Unfetch the last character
							Interp.LC_Unfetched := Standard.True;
							exit;
						end if;

						Token.Append_Character (Interp, LC.Value);
						Fetch_Character;
					end loop;
				end if;

			when others =>
				Token.Set (Interp, Identifier_Token);
				loop
					Token.Append_Character (Interp, LC.Value);
					Fetch_Character;
					--exit when not Is_Ident_Char(C.Value);
				-- TODO: more characters
					if LC.Kind /= Normal_Character or else
					   Is_Identifier_Stopper(LC.Value) then
						-- Unfetch the last character
						Interp.LC_Unfetched := Standard.True;
						exit;
					end if;
				end loop;
		end case;
		
--Ada.Text_IO.Put (">>>>>>>>>>>>>>>>>>>>>>> Token: " & Interp.Token.Value.Ptr(1..Interp.Token.Value.Last));
	end Fetch_Token;

	procedure Read_List is
		--pragma Inline (Read_List);
		V: aliased Object_Pointer;
	begin
		-- This procedure reads each token in a list.
		-- If the list contains no period, this procedure reads up to the 
		-- closing right paranthesis; If a period is contained, it transfers
		-- the control over to Read_List_Cdr.
		
		Fetch_Token;
	
		Push_Top (Interp, V'Unchecked_Access);

		case Interp.Token.Kind is
			when End_Token =>
Ada.Text_IO.Put_Line ("ERROR: PREMATURE LIST END");
				raise Syntax_Error;

			when Left_Parenthesis_Token =>
				Push_Frame (Interp, Opcode_Read_List, Nil_Pointer);

			when Right_Parenthesis_Token =>
				V := Get_Frame_Result(Interp.Stack);
				if V /= Nil_Pointer then
					V := Reverse_Cons(V);
				end if;
				Pop_Frame (Interp); 
				Chain_Frame_Result (Interp, Interp.Stack, V);

			when Period_Token =>
				V := Get_Frame_Result(Interp.Stack);
				if V = Nil_Pointer then
					-- . immediately after (
					raise Syntax_Error;
				else
					Set_Frame_Opcode (Interp.Stack, Opcode_Read_List_Cdr);
				end if;

			when Single_Quote_Token =>
				Push_Frame (Interp, Opcode_Close_Quote, Nil_Pointer);
				Push_Frame (Interp, Opcode_Read_Object, Nil_Pointer);

			when Integer_Token =>
				-- TODO: bignum
				V := String_To_Integer_Pointer(Interp.Token.Value.Ptr.all(1..Interp.Token.Value.Last));
				Chain_Frame_Result (Interp, Interp.Stack, V);

			when String_Token =>
				V := Make_String (Interp.Self, Interp.Token.Value.Ptr.all(1..Interp.Token.Value.Last));
				Chain_Frame_Result (Interp, Interp.Stack, V);

			when Identifier_Token =>	
Print_Object_Pointer ("000 Identifier => Stack => ", Interp.Stack);
				V := Make_Symbol (Interp.Self, Interp.Token.Value.Ptr.all(1..Interp.Token.Value.Last));
Print_Object_Pointer ("111 Identifier => Stack => ", Interp.Stack);
				Chain_Frame_Result (Interp, Interp.Stack, V);
Print_Object_Pointer ("222 Identifier => Stack => ", Interp.Stack);

			when others =>
				-- TODO: set various error info
				raise Syntax_Error;
		end case;

		Pop_Tops (Interp, 1);
	end Read_List;

	procedure Read_List_Cdr is
		pragma Inline (Read_List_Cdr);
		V: aliased Object_Pointer;
	begin
		-- This procedure reads the first token after a period has been read.
		-- It transfers the control over to Read_List_End once it has read 
		-- and processed the token. It chains the value made of the token  
		-- to the front of the frame's return value list expecting Read_List_End
		-- to handle the head item specially.
		Fetch_Token;
	
		Push_Top (Interp, V'Unchecked_Access);

		case Interp.Token.Kind is
			when End_Token =>
Ada.Text_IO.Put_Line ("ERROR: PREMATURE LIST END");
				raise Syntax_Error;

			when Left_Parenthesis_Token =>
				Set_Frame_Opcode (Interp.Stack, Opcode_Read_List_End);
				Push_Frame (Interp, Opcode_Read_List, Nil_Pointer);

			when Single_Quote_Token =>
Ada.Text_IO.Put_Line ("ERROR: CDR QUOT LIST END");
				Set_Frame_Opcode (Interp.Stack, Opcode_Read_List_End);
				Push_Frame (Interp, Opcode_Close_Quote, Nil_Pointer);
				Push_Frame (Interp, Opcode_Read_Object, Nil_Pointer);

			when Integer_Token =>
				-- TODO: bignum
				V := String_To_Integer_Pointer(Interp.Token.Value.Ptr.all(1..Interp.Token.Value.Last));
				Set_Frame_Opcode (Interp.Stack, Opcode_Read_List_End);
				Chain_Frame_Result (Interp, Interp.Stack, V);
		
			when String_Token =>
				V := Make_String (Interp.Self, Interp.Token.Value.Ptr.all(1..Interp.Token.Value.Last));
				-- TODO: make V gc-aware
				Set_Frame_Opcode (Interp.Stack, Opcode_Read_List_End);
				Chain_Frame_Result (Interp, Interp.Stack, V);

			when Identifier_Token =>	
				V := Make_Symbol (Interp.Self, Interp.Token.Value.Ptr.all(1..Interp.Token.Value.Last));
				-- TODO: make V gc-aware
				Set_Frame_Opcode (Interp.Stack, Opcode_Read_List_End);
				Chain_Frame_Result (Interp, Interp.Stack, V);

			when others =>
				-- TODO: set various error info
				raise Syntax_Error;
		end case;

		Pop_Tops (Interp, 1);
	end Read_List_Cdr;

	procedure Read_List_End is
		pragma Inline (Read_List_End);
		V: aliased Object_Pointer;
	begin
		Fetch_Token;

		Push_Top (Interp, V'Unchecked_Access);

		case Interp.Token.Kind is
			when Right_Parenthesis_Token =>
				V := Get_Frame_Result(Interp.Stack);
				pragma Assert (V /= Nil_Pointer);
				-- The first item in the chain is actually Cdr of the last cell.
				V := Reverse_Cons(Get_Cdr(V), Get_Car(V)); -- TODO: GC
				Pop_Frame (Interp); 
				Chain_Frame_Result (Interp, Interp.Stack, V);
			when others =>
				raise Syntax_Error;
		end case;

		Pop_Tops (Interp, 1);
	end Read_List_End;

	procedure Close_List is
		pragma Inline (Close_List);
		V: aliased Object_Pointer;
	begin
		Push_Top (Interp, V'Unchecked_Access);

		V := Get_Frame_Result(Interp.Stack);
		pragma Assert (Get_Cdr(V) = Nil_Pointer);
		Pop_Frame (Interp); -- Done with the current frame
		Chain_Frame_Result (Interp, Interp.Stack, Get_Car(V));

		Pop_Tops (Interp, 1);
	end Close_List;

	procedure Close_Quote is
		pragma Inline (Close_Quote);
		V: aliased Object_Pointer;
	begin
		Push_Top (Interp, V'Unchecked_Access);

-- TODO: use Interp.Quote_Syntax instead of Make_Symbol("quote")
		Chain_Frame_Result (Interp, Interp.Stack, Make_Symbol(Interp.Self, Label_Quote));
		V := Get_Frame_Result(Interp.Stack);
		Pop_Frame (Interp); -- Done with the current frame
		Chain_Frame_Result (Interp, Interp.Stack, V);

		Pop_Tops (Interp, 1);
	end Close_Quote;

	procedure Read_Object is
		pragma Inline (Read_Object);
		V: aliased Object_Pointer;
	begin
		Fetch_Token;

		Push_Top (Interp, V'Unchecked_Access);

		case Interp.Token.Kind is
			when End_Token =>
Ada.Text_IO.Put_Line ("INFO: NO MORE TOKEN ");
				raise Stream_End_Error;

			when Left_Parenthesis_Token =>
				Set_Frame_Opcode (Interp.Stack, Opcode_Close_List);
				Push_Frame (Interp, Opcode_Read_List, Nil_Pointer);

			when Single_Quote_Token =>
				Set_Frame_Opcode (Interp.Stack, Opcode_Close_Quote);
				Push_Frame (Interp, Opcode_Read_Object, Nil_Pointer);

			when Integer_Token =>
				-- TODO: bignum
				V := String_To_Integer_Pointer(Interp.Token.Value.Ptr.all(1..Interp.Token.Value.Last));
				Pop_Frame (Interp); -- Done with the current frame
				Chain_Frame_Result (Interp, Interp.Stack, V);

			when String_Token =>
				V := Make_String (Interp.Self, Interp.Token.Value.Ptr.all(1..Interp.Token.Value.Last));
				-- TODO: make V gc-aware
				Pop_Frame (Interp); -- Done with the current frame
				Chain_Frame_Result (Interp, Interp.Stack, V);

			when Identifier_Token =>	
				V := Make_Symbol (Interp.Self, Interp.Token.Value.Ptr.all(1..Interp.Token.Value.Last));
				-- TODO: make V gc-aware
				Pop_Frame (Interp); -- Done with the current frame
				Chain_Frame_Result (Interp, Interp.Stack, V);

			when others =>
				-- TODO: set various error info
				raise Syntax_Error;
		end case;

		Pop_Tops (Interp, 1);
	end Read_Object;

begin
	
	-- Stack frames looks like this upon initialization
	-- 
	--               | Opcode                 | Operand    | Result
	--     -----------------------------------------------------------------
	--     top       | Opcode_Evaluate_Object | Source     | Nil
	--     bottom    | Opcode_Exit            | Nil        | Nil
	--
	-- For a source (+ 1 2), it should look like this.
	--     -----------------------------------------------------------------
	--     top       | Opcode_Evaluate_Object | Source       | Nil
	--     bottom    | Opcode_Exit            | Nil          | Nil
	-- 
	-- The operand changes to the cdr of the source.
	-- The symbol '+' is pushed to the stack with Opcode_Evaluate_Object.
	--     -----------------------------------------------------------------
	--     top       | Opcode_Evaluate_Object | +            | Nil
	--               | Opcode_Evaluate_Object | (1 2)        | Nil
	--     bottom    | Opcode_Exit            | Nil          | Nil
	--
	-- After the evaluation of the symbol, the pushed frame is removed
	-- and the result is set to the return field.
	--     -----------------------------------------------------------------
	--     top       | Opcode_Evaluate_Object | (1 2)        | (#Proc+)
	--     bottom    | Opcode_Exit            | Nil          | Nil
	--
	-- The same action is taken to evaluate the literal 1.
	--     -----------------------------------------------------------------
	--     top       | Opcode_Evaluate_Object | 1            | Nil
	--               | Opcode_Evaluate_Object | (2)          | (#Proc+)
	--     bottom    | Opcode_Exit            | Nil          | Nil
	--
	-- The result of the valuation is reverse-chained to the return field. 
	--     -----------------------------------------------------------------
	--     top       | Opcode_Evaluate_Object | (2)          | (1 #Proc+)
	--     bottom    | Opcode_Exit            | Nil          | Nil
	--
	-- The same action is taken to evaluate the literal 2.
	--     -----------------------------------------------------------------
	--     top       | Opcode_Evaluate_Object | 2            | Nil
	--               | Opcode_Evaluate_Object | Mark         | (1 #Proc+)
	--     bottom    | Opcode_Exit            | Nil          | Nil
	--
	-- The result of the valuation is reverse-chained to the return field. 
	--     -----------------------------------------------------------------
	--     top       | Opcode_Evaluate_Object | Mark         | (2 1 #Proc+)
	--     bottom    | Opcode_Exit            | Nil          | Nil
	--
	-- Once evluation of each cons cell is complete, switch the top frame
	-- to 'Apply' reversing the result field into the operand field and
	-- nullifying the result field afterwards.
	--     -----------------------------------------------------------------
	--     top       | Apply                  | (#Proc+ 1 2) | Nil
	--     bottom    | Opcode_Exit            | Nil          | Nil
	--
	-- The apply operation produces the final result and sets it to the 
	-- parent frame while removing the apply frame.
	--     -----------------------------------------------------------------
	--     top/bottom| Opcode_Exit            | Nil          | (3)
	
	-- The caller must push some frames before calling this procedure
	pragma Assert (Interp.Stack /= Nil_Pointer);

	-- The caller must ensure there are no temporary object pointers.
	pragma Assert (Interp.Top.Last < Interp.Top.Data'First);

	loop

--if Is_Normal_Pointer(Interp.Stack) then
--declare
--     X: Heap_Number := Get_Heap_Number(Interp.Self, Interp.Stack);
--	type XX is access all object_pointer;
--	t: xx := Interp.Stack'Unchecked_access;
--	w: object_word;
--	for w'address use t'address;
--
--	ww: object_word;
--	for ww'address use interp.stack'address;
--
--	www: object_word;
--	for www'address use interp.stack'address;
--begin
--    Ada.Text_IO.Put_Line ("$$$$$ [XXXXX] Stack in HEAP: " & Heap_Number'Image(X) & " FROM: " &object_word'image(w) & " VALUE: " & object_word'image(ww) & " VALUE2: " & object_word'image(www));
--	Print_Object_Pointer ("   ====> t", t.all);
--end;
--Print_Object_Pointer ("   ====> Stack", Interp.Stack);
--end if;

		case Get_Frame_Opcode(Interp.Stack) is
			when Opcode_Exit =>
				exit;

			when Opcode_Evaluate_Result =>
				Evaluate_Result;

			when Opcode_Evaluate_Object =>
				Evaluate_Object;

			when Opcode_Evaluate_Group =>
				Evaluate_Group;
				
			when Opcode_Evaluate_Procedure =>
				Evaluate_Procedure;

			when Opcode_Apply =>
				Apply;

			when Opcode_Read_Object =>
				Read_Object;

			when Opcode_Read_List =>
				Read_List;

			when Opcode_Read_List_Cdr =>
				Read_List_Cdr;

			when Opcode_Read_List_End =>
				Read_List_End;

			when Opcode_Close_List =>
				Close_List;

			when Opcode_Close_Quote =>
				Close_Quote;

		end case;
	end loop;

	-- the stack must be empty when the loop is terminated
	--pragma Assert (Interp.Stack = Nil_Pointer);

exception
	when Stream_End_Error =>
		raise;

	when others =>
		Ada.Text_IO.Put_Line ("EXCEPTION OCCURRED");
		-- TODO: restore stack frame???
		-- TODO: restore envirronemtn frame???
		raise;
end Execute;
