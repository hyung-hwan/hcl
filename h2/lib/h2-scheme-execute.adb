
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
		Set_Frame_Operand (Interp.Stack, Get_Frame_Result(Interp.Stack));
		Clear_Frame_Result (Interp.Stack);
		Set_Frame_Opcode (Interp.Stack, Opcode_Evaluate_Object);
	end Evaluate_Result;

	-- ----------------------------------------------------------------
	generic
		with function Is_Good_Result (X: in Object_Pointer) return Standard.Boolean;
	procedure Evaluate_While;

	procedure Evaluate_While  is
		X: Object_Pointer;
		Y: Object_Pointer;
		Opcode: Opcode_Type;
	begin	
		X := Get_Frame_Operand(Interp.Stack);
		Y := Get_Frame_Result(Interp.Stack);

		-- Evaluate_And_Syntax/Evaluate_Or_Syntax has arranged to 
		-- evaluate <test1>. Y must be valid even at the first time 
		-- this procedure is called.

		if Is_Good_Result(Y) and then Is_Cons(X) then
			-- The result is not what I look for.
			-- Yet there are still more tests to evaluate.
			--Switch_Frame (Interp.Stack, Get_Frame_Opcode(Interp.Stack), Get_Cdr(X), Nil_Pointer);
			--Push_Frame (Interp, Opcode_Evaluate_Object, Get_Car(X));

			Opcode := Get_Frame_Opcode(Interp.Stack);
			Switch_Frame (Interp.Stack, Opcode_Evaluate_Object, Get_Car(X), Nil_Pointer);
			Push_SubFrame (Interp, Opcode, Get_Cdr(X));
		else
			-- Return the result of the last expression evaluated.
			Return_Frame (Interp, Y);
		end if;
	end Evaluate_While;

	function Is_False_Class (X: in Object_Pointer) return Standard.Boolean is
		pragma Inline (Is_False_Class);
	begin
		return X = False_Pointer;
	end Is_False_Class;

	function Is_True_Class (X: in Object_Pointer) return Standard.Boolean is
		pragma Inline (Is_True_Class);
	begin
		return X /= False_Pointer;
	end Is_True_Class;

	procedure Do_And_Finish is new Evaluate_While(Is_True_Class);
	procedure Do_Or_Finish is new Evaluate_While(Is_False_Class);

	-- ----------------------------------------------------------------
	procedure Do_Case_Finish is
		pragma Inline (Do_Case_Finish);
		
		R: Object_Pointer;
		O: Object_Pointer;
		C: Object_Pointer;
		D: Object_Pointer;
	begin
		R := Get_Frame_Result(Interp.Stack); -- <test> result
		O := Get_Frame_Operand(Interp.Stack); -- <clause> list
		
		while Is_Cons(O) loop
			C := Get_Car(O); -- <clause>
			D := Get_Car(C); -- <datum> list
			if D = Interp.Else_Symbol then
				Reload_Frame (Interp, Opcode_Grouped_Call, Get_Cdr(C));
				return;
			end if;

			while Is_Cons(D) loop
				if Equal_Values(R, Get_Car(D)) then -- <datum>
					Reload_Frame (Interp, Opcode_Grouped_Call, Get_Cdr(C));
					return;
				end if;
				D := Get_Cdr(D);
			end loop;

			O := Get_Cdr(O);
		end loop;

		-- no match found;
		Pop_Frame (Interp);
	end Do_Case_Finish;
	
	-- ----------------------------------------------------------------
	procedure Do_Cond_Finish is
		pragma Inline (Do_Cond_Finish);
		R: Object_Pointer;
		O: Object_Pointer;
	begin
		R := Get_Frame_Result(Interp.Stack); -- <test> result
		O := Get_Frame_Operand(Interp.Stack); -- <clause> list
		
		if Is_True_Class(R) then
			O := Get_Cdr(Get_Car(O)); -- <expression> list in <clause>
			if Is_Cons(O) then
				Reload_Frame (Interp, Opcode_Grouped_Call, O);
			else
				Pop_Frame (Interp); -- no <expression> to evaluate
			end if;
		else
			O := Get_Cdr(O); -- next <clause> list

			if not Is_Cons(O) then
				-- no more <clause>
				Pop_Frame (Interp);
			else
				R := Get_Car(O); -- next <clause>
				if Get_Car(R) = Interp.Else_Symbol then
					-- else <clause>
					O := Get_Cdr(R); -- <expression> list in else <clause>
					if Is_Cons(O) then
						Reload_Frame (Interp, Opcode_Grouped_Call, O);
					else
						Pop_Frame (Interp); -- no <expression> to evaluate
					end if;
				else
					Switch_Frame (Interp.Stack, Opcode_Evaluate_Object, Get_Car(R), Nil_Pointer);
					Push_Subframe (Interp, Opcode_Cond_Finish, O);
				end if;
			end if;
		end if;
	end Do_Cond_Finish;

	-- ----------------------------------------------------------------

	procedure Do_Define_Finish is
		pragma Inline (Do_Define_Finish);
		X: Object_Pointer;
		Y: aliased Object_Pointer;
	begin
		-- Manage Y as it's referenced after the gc point. 
		Push_Top (Interp, Y'Unchecked_Access); 

		X := Get_Frame_Operand(Interp.Stack); -- symbol
		pragma Assert (Is_Symbol(X));

		Y := Get_Frame_Result(Interp.Stack);  -- value list
		Set_Current_Environment (Interp, X, Y);  -- gc point
		Return_Frame (Interp, Y); -- Y is referenced here.
		Pop_Tops (Interp, 1); -- Unmanage Y
	end Do_Define_Finish;

	-- ----------------------------------------------------------------

	procedure Do_Do_Binding is
		pragma Inline (Do_Do_Binding);
		X: aliased Object_Pointer;
	begin
		Push_Top (Interp, X'Unchecked_Access);
		X := Get_Frame_Operand(Interp.StacK);
		Set_Parent_Environment (Interp, Get_Car(Get_Car(X)), Get_Frame_Result(Interp.Stack));
		
		X := Get_Cdr(X);
		if Is_Cons(X) then
			declare
				Envir: aliased Object_Pointer;
			begin
				pragma Assert (Get_Frame_Opcode(Get_Frame_Parent(Interp.Stack)) = Opcode_Do_Test);
				
				Push_top (Interp, Envir'Unchecked_Access);
				Envir := Get_Frame_Environment(Get_Frame_Parent(Get_Frame_Parent(Interp.Stack))); 
				Reload_Frame (Interp, Opcode_Do_Binding, X);
				Push_Frame_With_Environment (Interp, Opcode_Evaluate_Object, Get_Car(Get_Cdr(Get_Car(X))), Envir); -- <init>
				Pop_Tops (Interp, 1);
			end;
		else 
			Pop_Frame (Interp);
		end if;
		
		Pop_Tops (Interp, 1);
	end Do_Do_Binding;
	
	procedure Do_Do_Test is
		pragma Inline (Do_Do_Test);
		X: aliased Object_Pointer;
	begin
		Push_Top (Interp, X'Unchecked_Access);
		X := Get_Frame_Operand(Interp.Stack);
		Reload_Frame (Interp, Opcode_Do_Break, X);
		Push_Frame (Interp, Opcode_Evaluate_Object, Get_Car(Get_Car(Get_Cdr(X)))); -- <test>
		Pop_Tops (Interp, 1);
	end Do_Do_Test;
	
	procedure Do_Do_Break is
		X: aliased Object_Pointer;
	begin
		X := Get_Frame_Operand(Interp.Stack);
		if Is_True_Class(Get_Frame_Result(Interp.Stack)) then
			-- <test> is true. arrange to break out of 'do'.
			
			X := Get_Cdr(Get_Car(Get_Cdr(X)));
			if X = Nil_Pointer then
				-- no expression after <test>
				-- (do ((x 1)) (#t))
				Pop_Frame (Interp);
			else
				Reload_Frame (Interp, Opcode_Grouped_Call, X);
			end if;
		else
			-- <test> is false. 
			Push_Top (Interp, X'Unchecked_Access);
			Reload_Frame (Interp, Opcode_Do_Step, X);
			X := Get_Cdr(Get_Cdr(X));
			if X /= Nil_Pointer then
				Push_Frame (Interp, Opcode_Grouped_Call, X);
			end if;
			Pop_Tops (Interp, 1);
		end if;
	end Do_Do_Break;
	
	procedure Do_Do_Step is
		X: aliased Object_Pointer;
		Y: aliased Object_Pointer;
	begin
		-- arrange to evaluate <step> and update binding <variable>.
		Push_Top (Interp, X'Unchecked_Access);
		X := Get_Frame_Operand(Interp.Stack);

		Reload_Frame (Interp, Opcode_Do_Test, X);

		X := Get_Car(X);
		while Is_Cons(X) loop
			Y := Get_Cdr(Get_Cdr(Get_Car(X)));
			if Is_Cons(Y) then
				Push_Top (Interp, Y'Unchecked_Access);
				Push_Frame (Interp, Opcode_Do_Update, X);
				Push_Frame (Interp, Opcode_Evaluate_Object, Get_Car(Y)); -- first <step>
				Pop_Tops (Interp, 1);
				exit;
			else
				-- no <step>
				X := Get_Cdr(X);
			end if;
		end loop;
		
		Pop_Tops (Interp, 1);
	end Do_Do_Step;

	procedure Do_Do_Update is
		X: aliased Object_Pointer;
		Y: aliased Object_Pointer;
	begin
		Push_Top (Interp, X'Unchecked_Access);
		X := Get_Frame_Operand(Interp.StacK);
		Set_Parent_Environment (Interp, Get_Car(Get_Car(X)), Get_Frame_Result(Interp.Stack));
		
		loop
			X := Get_Cdr(X);
			if Is_Cons(X) then
				Y := Get_Cdr(Get_Cdr(Get_Car(X)));
				if Is_Cons(Y) then
					-- if <step> is specified
					Push_Top (Interp, Y'Unchecked_Access);
					Reload_Frame (Interp, Opcode_Do_Update, X);
					Push_Frame (Interp, Opcode_Evaluate_Object, Get_Car(Y)); -- <step>
					Pop_Tops (Interp, 1);
					exit;
				end if;
			else 
				-- no more <bindings>
				Pop_Frame (Interp);
				exit;
			end if;
		end loop;
		
		Pop_Tops (Interp, 1);
	end Do_Do_Update;
	-- ----------------------------------------------------------------
	
	procedure Do_If_Finish is
		pragma Inline (Do_If_Finish);
		X: Object_Pointer;
		Y: Object_Pointer;
	begin
		X := Get_Frame_Operand(Interp.Stack); -- cons cell containing <consequent>
		pragma Assert (Is_Cons(X)); 

		Y := Get_Frame_Result(Interp.Stack);  -- result list of <test>

		if Y = False_Pointer then
			-- <test> evaluated to #f.
			X := Get_Cdr(X); -- cons cell containing <alternate>
			if Is_Cons(X) then
				-- Switch the current current to evaluate <alternate> 
				-- keeping the environment untouched. Use Reload_Frame
				-- instead of Switch_Frame for continuation. If continuation
				-- has been created in <test>, continuation can be made to 
				-- this frame. 
				--
				-- For example,
				--   (if (define xx (call/cc call/cc)) 
				--       (+ 10 20) (* 1 2 3 4))
				--   (xx 99)
				-- When (xx 99) is evaluated, continuation is made to
				-- this frame. For this frame to evaluate <consequent> or 
				-- <alternate>, its opcode must remain as Opcode_If_Finish.

				--Switch_Frame (Interp.Stack, Opcode_Evaluate_Object, Get_Car(X), Nil_Pointer);
				Reload_Frame (Interp, Opcode_Evaluate_Object, Get_Car(X));
			else
				-- Return nil if no <alternate> is specified
				Return_Frame (Interp, Nil_Pointer);
			end if;
		else
			-- All values except #f are true values. evaluate <consequent>.
			-- Switch the current current to evaluate <consequent> keeping
			-- the environment untouched. Use Reload_Frame instead of
			-- Switch_Frame for continuation to work.
			--Switch_Frame (Interp.Stack, Opcode_Evaluate_Object, Get_Car(X), Nil_Pointer);
			Reload_Frame (Interp, Opcode_Evaluate_Object, Get_Car(X));
		end if;
	end Do_If_Finish;

	-- ----------------------------------------------------------------

	procedure Do_Procedure_Call is
		pragma Inline (Do_Procedure_Call);
		R: Object_Pointer;
		X: Object_Pointer;
	begin
		-- Note: if you change the assignment order of R and X, 
		--       Push_Top() and Pop_Tops() are needed.
		--Push_Top (Interp, X'Unchecked_Access);
		--Push_Top (Interp, R'Unchecked_Access);
		R := Make_Cons(Interp.Self, Get_Frame_Result(Interp.Stack), Get_Frame_Intermediate(Interp.Stack));
		X := Get_Frame_Operand(Interp.Stack);

		if Is_Cons(X) then
			Switch_Frame (Interp.Stack, Opcode_Evaluate_Object, Get_Car(X), Nil_Pointer);
			Push_Subframe_With_Intermediate (Interp, Opcode_Procedure_Call, Get_Cdr(X), R);
		else
			-- no more argument to evaluate. 
			-- apply the evaluated arguments to the evaluated operator.
			R := Reverse_Cons(R);

			-- This frame can be resumed. Switching the current frame to Opcode_Apply
			-- affects continuation objects that point to the current frame. However,
			-- keeping it unchanged causes this frame to repeat actions that has been 
			-- taken previously when it's resumed. So i change the frame to something 
			-- special designed for continuation only.
			Switch_Frame (Interp.Stack, Opcode_Procedure_Call_Finish, Get_Car(R), Nil_Pointer);
			Pop_Frame (Interp);

			-- Replace the current frame popped by a new applying frame.
			Push_Frame_With_Intermediate (Interp, Opcode_Apply, Get_Car(R), Get_Cdr(R));
		end if;

		--Pop_Tops (Interp, 2);
	end Do_Procedure_Call;

	procedure Do_Procedure_Call_Finish is
		pragma Inline (Do_Procedure_Call_Finish);
		R: Object_Pointer;
		X: Object_Pointer;
	begin
		-- TODO: is this really correct? verify this.

		-- Note: if you change the assignment order of R and X, 
		--       Push_Top() and Pop_Tops() are needed.
		--Push_Top (Interp, X'Unchecked_Access);
		--Push_Top (Interp, R'Unchecked_Access);
		R := Make_Cons(Interp.Self, Get_Frame_Result(Interp.Stack), Nil_Pointer);
		X := Get_Frame_Operand(Interp.Stack);

		Reload_Frame_With_Intermediate (Interp, Opcode_Apply, X, R);

		--Pop_Tops (Interp, 2);
	end Do_Procedure_Call_Finish;

	-- ----------------------------------------------------------------

	procedure Do_Grouped_Call is
		pragma Inline (Do_Grouped_Call);
		X: Object_Pointer;
	begin
		X := Get_Frame_Operand(Interp.Stack);

		pragma Assert (Is_Cons(X)); -- The caller must ensure this.
		-- Switch the current frame to evaluate the first 
		-- expression in the group.
		Switch_Frame (Interp.Stack, Opcode_Evaluate_Object, Get_Car(X), Nil_Pointer);

		X := Get_Cdr(X);
		if Is_Cons(X) then
			-- Add a new frame for handling the remaining expressions in 
			-- the group. Place it below the current frame so that it's 
			-- executed after the current frame switched is executed first.
			Push_Subframe (Interp, Opcode_Grouped_Call, X);
		end if;
	end Do_Grouped_Call;

	-- ----------------------------------------------------------------
	procedure Do_Let_Binding is
		pragma Inline (Do_Let_Binding);
		O: aliased Object_Pointer;
	begin
		-- Perform binding in the parent environment.
		Set_Parent_Environment (Interp, Get_Frame_Intermediate(Interp.Stack), Get_Frame_Result(Interp.Stack));

		O := Get_Frame_Operand(Interp.Stack);	

		-- Say, <bindings> is ((x 2) (y 2)).
		-- Get_Car(O) is (x 2).
		-- To get x, Get_Car(Get_Car(O))
		-- To get 2, Get_Car(Get_Cdr(Get_Car(O)))
		if Is_Cons(O) then
			Push_Top (Interp, O'Unchecked_Access);

			Reload_Frame (Interp, Opcode_Evaluate_Object, Get_Car(Get_Cdr(Get_Car(O))));
			Push_Subframe_With_Intermediate (Interp, Opcode_Let_Binding, Get_Cdr(O), Get_Car(Get_Car(O)));

			Pop_Tops (Interp, 1);
		else
			Pop_Frame (Interp); -- done. 
		end if;
	end Do_Let_Binding;

	procedure Do_Letast_Binding is
		pragma Inline (Do_Letast_Binding);
		O: aliased Object_Pointer;
		Envir: Object_Pointer;
	begin
		-- Perform binding in the parent environment.
		Set_Current_Environment (Interp, Get_Frame_Intermediate(Interp.Stack), Get_Frame_Result(Interp.Stack));

		O := Get_Frame_Operand(Interp.Stack);	

		-- Say, <bindings> is ((x 2) (y 2)).
		-- Get_Car(O) is (x 2).
		-- To get x, Get_Car(Get_Car(O))
		-- To get 2, Get_Car(Get_Cdr(Get_Car(O)))
		if Is_Cons(O) then
			Push_Top (Interp, O'Unchecked_Access);

         		Envir := Make_Environment(Interp.Self, Get_Frame_Environment(Interp.Stack));
         		Set_Frame_Environment (Interp.Stack, Envir); 

			Reload_Frame (Interp, Opcode_Evaluate_Object, Get_Car(Get_Cdr(Get_Car(O))));
			Push_Subframe_With_Intermediate (Interp, Opcode_Letast_Binding, Get_Cdr(O), Get_Car(Get_Car(O)));

			Pop_Tops (Interp, 1);
		else
--envir := get_frame_environment(interp.stack);
--declare
--w: object_word;
--for w'address use envir'address;
--begin
--ada.text_io.put_line ("i$$$$$$$$$$$$$$$$$$$$$$$$44 ENVIR => " & object_word'image(w));
--print (interp, envir);
--end;
			-- Get the final environment
			Envir := Get_Frame_Environment(Interp.Stack);

			-- Get <body> stored in the Opcode_Grouped_Call frame
			-- pushed in Evalute_Letast_Syntax().
			O := Get_Frame_Operand(Get_Frame_Parent(Interp.Stack));

			Pop_Frame (Interp); -- Current frame
			pragma Assert (Get_Frame_Opcode(Interp.Stack) = Opcode_Grouped_Call);

			-- Refresh the Opcode_Grouped_Call frame pushed in Evaluate_Letast_Syntax()
			-- with the final environment.
			Reload_Frame_With_Environment (Interp, Opcode_Grouped_Call, O, Envir);
		end if;
	end Do_Letast_Binding;

	procedure Do_Letrec_Binding is
		pragma Inline (Do_Letrec_Binding);
		O: aliased Object_Pointer;
	begin
		-- Perform binding in the parent environment.
		Set_Current_Environment (Interp, Get_Frame_Intermediate(Interp.Stack), Get_Frame_Result(Interp.Stack));

		O := Get_Frame_Operand(Interp.Stack);	

		-- Say, <bindings> is ((x 2) (y 2)).
		-- Get_Car(O) is (x 2).
		-- To get x, Get_Car(Get_Car(O))
		-- To get 2, Get_Car(Get_Cdr(Get_Car(O)))
		if Is_Cons(O) then
			Push_Top (Interp, O'Unchecked_Access);

			Reload_Frame (Interp, Opcode_Evaluate_Object, Get_Car(Get_Cdr(Get_Car(O))));
			Push_Subframe_With_Intermediate (Interp, Opcode_Letrec_Binding, Get_Cdr(O), Get_Car(Get_Car(O)));

			Pop_Tops (Interp, 1);
		else
			Pop_Frame (Interp);
		end if;
	end Do_Letrec_Binding;

	-- --------------------------------------------------------------------

	procedure Do_Set_Finish is
		pragma Inline (Do_Set_Finish);
		X: Object_Pointer;
		Y: aliased Object_Pointer;
	begin
		Push_Top (Interp, Y'Unchecked_Access);

		X := Get_Frame_Operand(Interp.Stack); -- symbol
		Y := Get_Frame_Result(Interp.Stack);  -- value

		pragma Assert (Is_Symbol(X));

		if Set_Environment(Interp.Self, X, Y) = null then
			Ada.Text_IO.Put_LINE ("ERROR: UNBOUND SYMBOL");
			raise Evaluation_Error;
		end if;

		Return_Frame (Interp, Y);

		Pop_Tops (Interp, 1);
	end Do_Set_Finish;

	procedure Evaluate is separate;
	procedure Apply is separate;

	-- --------------------------------------------------------------------

	procedure Unfetch_Character is
		pragma Inline (Unfetch_Character);
		pragma Assert (not Interp.LC_Unfetched);	
	begin
		Interp.LC_Unfetched := Standard.True;
	end Unfetch_Character;

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

	function Is_Delimiter (X: in Object_Character) return  Standard.Boolean is
	begin
		return X = Ch.Left_Parenthesis or else X = Ch.Right_Parenthesis or else
		       X = Ch.Quotation or else X = Ch.Semicolon or else 
	            Is_White_Space(X);
	end Is_Delimiter;

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

			when Ch.Pos.Number_Sign => 
				Fetch_Character;
				if LC.Kind /= Normal_Character then
					-- ended prematurely.
					-- TODO: Set Error code, Error Number.... Error location
					raise Syntax_Error;		
				end if;

				-- #t
				-- #f
				-- #\C -- character
				-- #\xHHHH -- unicode
				-- #\xHHHHHHHH -- unicode
				-- #( ) -- vector
				-- #[ ] -- list
				-- #{ } -- hash table
				-- #< > -- xxx

				case Object_Character'Pos(LC.Value) is
					when Ch.Pos.LC_T => -- #t
						Token.Set (Interp, True_Token, Ch.Number_Sign);
						Token.Append_Character (Interp, LC.Value);

					when Ch.Pos.LC_F => -- #f
						Token.Set (Interp, False_Token, Ch.Number_Sign);
						Token.Append_Character (Interp, LC.Value);

					when Ch.Pos.Backslash => -- #\C, #\space, #\newline
						Fetch_Character;
						if LC.Kind /= Normal_Character then
							ada.text_io.put_line ("ERROR: NO CHARACTER AFTER #\");
							raise Syntax_Error;
						end if;
						Token.Set (Interp, Character_Token, LC.Value);

						loop
							Fetch_Character;
							if LC.Kind /= Normal_Character or else 
							   Is_Delimiter(LC.Value) then
								Unfetch_Character;
								exit;
							end if;
							Token.Append_Character (Interp, LC.Value);
						end loop;

						if Interp.Token.Value.Last > 1 then
							-- TODO: case insensitive match. binary search for more diverse words
							-- TODO: #\xHHHH....
							if Interp.Token.Value.Ptr.all(1..Interp.Token.Value.Last) = Label_Newline then
								Token.Set (Interp, Character_Token, Ch.LF);  -- reset the token to LF
							elsif Interp.Token.Value.Ptr.all(1..Interp.Token.Value.Last) = Label_Space then
								Token.Set (Interp, Character_Token, Ch.Space); -- reset the token to Space
							else
								-- unknown character name.
								ada.text_io.put ("ERROR: UNKNOWN CHARACTER NAME ");
								for I in 1 .. interp.token.value.last loop
									ada.text_io.put (standard.character'val(object_character'pos(interp.token.value.ptr.all(i))));
								end loop;
								ada.text_io.new_line;

								raise Syntax_Error;
							end if;
						end if;

					--when Ch.Pos.Left_Parenthesis => -- #(
					--	Token.Set (Interp, Vector_Token, Ch.Number_Sign);
					--	Token.Append_Character (Interp, LC.Value);

					--when Ch.Pos.Left_Bracket => -- $[
					--	Token.Set (Interp, List_Token, Ch.Number_Sign);
					--	Token.Append_Character (Interp, LC.Value);

					--when Ch.Pos.Left_Bracket => -- ${
					--	Token.Set (Interp, Table_Token, Ch.Number_Sign);
					--	Token.Append_Character (Interp, LC.Value);

					when others =>
						-- unknown #letter
						-- TODO: Set Error code, Error Number.... Error location
						raise Syntax_Error;		
					
				end case;

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


			when Ch.Pos.Zero .. Ch.Pos.Nine =>
				-- TODO; negative number, floating-point number, bignum, hexdecimal, etc
				Token.Set (Interp, Integer_Token);
				loop
					Token.Append_Character (Interp, LC.Value);
					Fetch_Character;
					if LC.Kind /= Normal_Character or else
					   LC.Value not in Ch.Zero .. Ch.Nine  then
						-- Unfetch the last character
						Unfetch_Character;
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
							Unfetch_Character;
							exit;
						end if;
					end loop;
				else
					Token.Set (Interp, Identifier_Token, Tmp(1..1));
					loop
				-- TODO: more characters
						if LC.Kind /= Normal_Character or else 
						   Is_Delimiter(LC.Value) then
							Unfetch_Character;
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
					   Is_Delimiter(LC.Value) then
						Unfetch_Character;
						exit;
					end if;
				end loop;
		end case;
		
--Ada.Text_IO.Put (">>>>>>>>>>>>>>>>>>>>>>> Token: " & Interp.Token.Value.Ptr(1..Interp.Token.Value.Last));
	end Fetch_Token;

	procedure Read_List is
		pragma Inline (Read_List);
		V: Object_Pointer;
	begin
		-- This procedure reads each token in a list.
		-- If the list contains no period, this procedure reads up to the 
		-- closing right paranthesis; If a period is contained, it transfers
		-- the control over to Read_List_Cdr.
		
		Fetch_Token;
	
		case Interp.Token.Kind is
			when End_Token =>
Ada.Text_IO.Put_Line ("ERROR: PREMATURE LIST END");
				raise Syntax_Error;

			when Left_Parenthesis_Token =>
				Push_Frame (Interp, Opcode_Read_List, Nil_Pointer);

			when Right_Parenthesis_Token =>
				V := Get_Frame_Intermediate(Interp.Stack);
				if Is_Cons(V) then
					V := Reverse_Cons(V);
				end if;
				Pop_Frame (Interp); 
				Chain_Frame_Intermediate (Interp, Interp.Stack, V);

			when Period_Token =>
				V := Get_Frame_Intermediate(Interp.Stack);
				if V = Nil_Pointer then
					-- . immediately after (
					raise Syntax_Error;
				else
					Set_Frame_Opcode (Interp.Stack, Opcode_Read_List_Cdr);
				end if;

			when Single_Quote_Token =>
				Push_Frame (Interp, Opcode_Close_Quote_In_List, Nil_Pointer);
				Push_Frame (Interp, Opcode_Read_Object, Nil_Pointer);

			when others =>
				V := Token_To_Pointer (Interp.Self, Interp.Token);
				if V = null then
					-- TODO: set various error info
					raise Syntax_Error;
				else
					Chain_Frame_Intermediate (Interp, Interp.Stack, V);
				end if;

		end case;

	end Read_List;

	procedure Read_List_Cdr is
		pragma Inline (Read_List_Cdr);
		V: Object_Pointer;
	begin
		-- This procedure reads the first token after a period has been read.
		-- It transfers the control over to Read_List_End once it has read 
		-- and processed the token. It chains the value made of the token  
		-- to the front of the frame's return value list expecting Read_List_End
		-- to handle the head item specially.
		Fetch_Token;
	
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
				Push_Frame (Interp, Opcode_Close_Quote_In_List, Nil_Pointer);
				Push_Frame (Interp, Opcode_Read_Object, Nil_Pointer);

			when others =>
				V := Token_To_Pointer (Interp.Self, Interp.Token);
				if V = null then
					-- TODO: set various error info
					raise Syntax_Error;
				else
					Chain_Frame_Intermediate (Interp, Interp.Stack, V);
					Set_Frame_Opcode (Interp.Stack, Opcode_Read_List_End);
				end if;
	
		end case;

	end Read_List_Cdr;

	procedure Read_List_End is
		pragma Inline (Read_List_End);
		V: Object_Pointer;
	begin
		Fetch_Token;

		case Interp.Token.Kind is
			when Right_Parenthesis_Token =>
				V := Get_Frame_Intermediate(Interp.Stack);
				pragma Assert (Is_Cons(V));
				-- The first item in the chain is actually Cdr of the last cell.
				V := Reverse_Cons(Get_Cdr(V), Get_Car(V)); 
				Pop_Frame (Interp); 
				Chain_Frame_Intermediate (Interp, Interp.Stack, V);
			when others =>
Ada.Text_IO.Put_Line ("Right parenthesis expected");
				raise Syntax_Error;
		end case;

	end Read_List_End;

	procedure Close_List is
		pragma Inline (Close_List);
		V: Object_Pointer;
	begin
		V := Get_Frame_Intermediate(Interp.Stack);
		pragma Assert (Is_Cons(V));
		pragma Assert (Get_Cdr(V) = Nil_Pointer); -- only 1 item as it's used for the top-level list only
		Return_Frame (Interp, Get_Car(V));
	end Close_List;

	procedure Close_Quote_In_List is
		pragma Inline (Close_Quote_In_List);
		V: Object_Pointer;
	begin
		V := Get_Frame_Result(Interp.Stack);
		V := Make_Cons(Interp.Self, V, Nil_Pointer);
		V := Make_Cons(Interp.Self, Interp.Quote_Symbol, V);
		Pop_Frame (Interp); 
		Chain_Frame_Intermediate (Interp, Interp.Stack, V);
	end Close_Quote_In_List;

	procedure Close_Quote is
		pragma Inline (Close_Quote);
		V: Object_Pointer;
	begin
		V := Get_Frame_Result(Interp.Stack);
		V := Make_Cons(Interp.Self, V, Nil_Pointer);
		V := Make_Cons(Interp.Self, Interp.Quote_Symbol, V);
		Return_Frame (Interp, V);
	end Close_Quote;

	procedure Read_Object is
		pragma Inline (Read_Object);
		V: Object_Pointer;
	begin
		Fetch_Token;

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

			when others =>
				V := Token_To_Pointer (Interp.Self, Interp.Token);
				if V = null then
					-- TODO: set various error info
Ada.Text_IO.Put_Line ("INFO: UNKNOWN TOKEN " & Token_Kind'Image(Interp.Token.Kind));
					raise Syntax_Error;
				else
					Return_Frame (Interp, V);
				end if;
		end case;

	end Read_Object;

	-- --------------------------------------------------------------------

begin
	
	-- TODO: This comment is out-dated. Update it with Intermediate.
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

	loop
ada.text_io.put_line (Opcode_Type'Image(Get_Frame_Opcode(Interp.Stack)));
		case Get_Frame_Opcode(Interp.Stack) is
			when Opcode_Exit =>
				exit;

			when Opcode_Evaluate_Result =>
				Evaluate_Result;

			when Opcode_Evaluate_Object =>
				Evaluate;

			when Opcode_And_Finish => 
				Do_And_Finish;
 
			when Opcode_Case_Finish =>
				Do_Case_Finish;
				
			when Opcode_Cond_Finish => 
				Do_Cond_Finish;

			when Opcode_Define_Finish =>
				Do_Define_Finish;

			when Opcode_Do_Binding =>
				Do_Do_Binding;

			when Opcode_Do_Break =>
				Do_Do_Break;

			when Opcode_Do_Step =>
				Do_Do_Step;

			when Opcode_Do_Test =>
				Do_Do_Test;

			when Opcode_Do_Update =>
				Do_Do_Update;

			when Opcode_Grouped_Call =>
				Do_Grouped_Call;

			when Opcode_If_Finish =>
				Do_If_Finish; -- Conditional

			when Opcode_Let_Binding =>
				Do_Let_Binding; 

			when Opcode_Letast_Binding =>
				Do_Letast_Binding; 

			when Opcode_Letrec_Binding =>
				Do_Letrec_Binding; 

			when Opcode_Or_Finish => 
				Do_Or_Finish;

			when Opcode_Procedure_Call =>
				Do_Procedure_Call;

			when Opcode_Procedure_Call_Finish =>
				Do_Procedure_Call_Finish;

			when Opcode_Set_Finish =>
				Do_Set_Finish; -- Assignment


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

			when Opcode_Close_Quote_In_List =>
				Close_Quote_In_List;

		end case;
	end loop;

exception
	when Stream_End_Error =>
Ada.Text_IO.Put_Line ("INFO: NO MORE TOKEN .............");
		raise;

	when others =>
		Ada.Text_IO.Put_Line ("EXCEPTION OCCURRED");
		-- TODO: restore stack frame???
		-- TODO: restore envirronemtn frame???
		raise;
end Execute;
