
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
						raise Syntax_Error;
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

	procedure Finish_Define_Symbol is
		pragma Inline (Finish_Define_Symbol);
		X: aliased Object_Pointer;
		Y: aliased Object_Pointer;
	begin
Ada.Text_IO.PUt_Line ("FINISH DEFINE SYMBOL");
		Push_Top (Interp, X'Unchecked_Access);
		Push_Top (Interp, Y'Unchecked_Access);

		X := Get_Frame_Operand(Interp.Stack); -- symbol
		Y := Get_Car(Get_Frame_Result(Interp.Stack));  -- value
		pragma Assert (Is_Symbol(X));
		pragma Assert (Get_Cdr(Get_Frame_Result(Interp.Stack)) = Nil_Pointer);

		Put_Environment (Interp, X, Y);

		Pop_Frame (Interp);     -- Done
		Chain_Frame_Result (Interp, Interp.Stack, Y);

		Pop_Tops (Interp, 2);
	end Finish_Define_Symbol;

	procedure Finish_If is
		pragma Inline (Finish_If);
		X: aliased Object_Pointer;
		Y: aliased Object_Pointer;
		Z: aliased Object_Pointer;
	begin
Ada.Text_IO.PUt_Line ("FINISH IF");

		Push_Top (Interp, X'Unchecked_Access);
		Push_Top (Interp, Y'Unchecked_Access);

		X := Get_Frame_Operand(Interp.Stack); -- cons cell containing <consequent>
		Y := Get_Car(Get_Frame_Result(Interp.Stack)); -- result of conditional
		pragma Assert (Is_Cons(X)); 
		pragma Assert (Get_Cdr(Get_Frame_Result(Interp.Stack)) = Nil_Pointer);

		Pop_Frame (Interp);
		if Y = False_Pointer then
			-- <test> evaluated to #f.
			X := Get_Cdr(X); -- cons cell containing <alternate>
			if Is_Cons(X) then
				-- evaluate <alternate>
				Push_Frame (Interp, Opcode_Evaluate_Object, Get_Car(X));
			else
				-- return nil if no <alternate> is specified
				Chain_Frame_Result (Interp, Interp.Stack, Nil_Pointer);
			end if;
		else
			-- all values except #f are true values. evaluate <consequent>
			Push_Frame (Interp, Opcode_Evaluate_Object, Get_Car(X));
		end if;

		Pop_Tops (Interp, 2);
	end Finish_If;

	procedure Finish_Set is
		pragma Inline (Finish_Set);
		X: aliased Object_Pointer;
		Y: aliased Object_Pointer;
	begin
Ada.Text_IO.PUt_Line ("FINISH Set");
		Push_Top (Interp, X'Unchecked_Access);
		Push_Top (Interp, Y'Unchecked_Access);

		X := Get_Frame_Operand(Interp.Stack); -- symbol
		Y := Get_Car(Get_Frame_Result(Interp.Stack));  -- value
		pragma Assert (Is_Symbol(X));
		pragma Assert (Get_Cdr(Get_Frame_Result(Interp.Stack)) = Nil_Pointer);

		if Set_Environment(Interp.Self, X, Y) = null then
			Ada.Text_IO.PUt_LINE ("ERROR: UNBOUND SYMBOL");
			raise Evaluation_Error;
		end if;

		Pop_Frame (Interp);     -- Done
		Chain_Frame_Result (Interp, Interp.Stack, Y);

		Pop_Tops (Interp, 2);
	end Finish_Set;


	procedure Evaluate is separate;
	procedure Apply is separate;

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
		V: aliased Object_Pointer;
	begin
		-- This procedure reads each token in a list.
		-- If the list contains no period, this procedure reads up to the 
		-- closing right paranthesis; If a period is contained, it transfers
		-- the control over to Read_List_Cdr.
		
		Fetch_Token;
	
		--Push_Top (Interp, V'Unchecked_Access);

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

			when Character_Token =>
				pragma Assert (Interp.Token.Value.Last = 1);
				V := Character_To_Pointer(Interp.Token.Value.Ptr.all(1));
				Chain_Frame_Result (Interp, Interp.Stack, V);

			when String_Token =>
				V := Make_String(Interp.Self, Interp.Token.Value.Ptr.all(1..Interp.Token.Value.Last));
				Chain_Frame_Result (Interp, Interp.Stack, V);

			when Identifier_Token =>	
				V := Make_Symbol(Interp.Self, Interp.Token.Value.Ptr.all(1..Interp.Token.Value.Last));
				Chain_Frame_Result (Interp, Interp.Stack, V);

			when True_Token =>
				Chain_Frame_Result (Interp, Interp.Stack, True_Pointer);

			when False_Token =>
				Chain_Frame_Result (Interp, Interp.Stack, False_Pointer);

			when others =>
				-- TODO: set various error info
				raise Syntax_Error;
		end case;

		--Pop_Tops (Interp, 1);
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
	
		--Push_Top (Interp, V'Unchecked_Access);

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

			when Character_Token =>
				pragma Assert (Interp.Token.Value.Last = 1);
				V := Character_To_Pointer(Interp.Token.Value.Ptr.all(1));
				Set_Frame_Opcode (Interp.Stack, Opcode_Read_List_End);
				Chain_Frame_Result (Interp, Interp.Stack, V);
		
			when String_Token =>
				V := Make_String (Interp.Self, Interp.Token.Value.Ptr.all(1..Interp.Token.Value.Last));
				Set_Frame_Opcode (Interp.Stack, Opcode_Read_List_End);
				Chain_Frame_Result (Interp, Interp.Stack, V);

			when Identifier_Token =>	
				V := Make_Symbol (Interp.Self, Interp.Token.Value.Ptr.all(1..Interp.Token.Value.Last));
				Set_Frame_Opcode (Interp.Stack, Opcode_Read_List_End);
				Chain_Frame_Result (Interp, Interp.Stack, V);

			when True_Token =>
				Set_Frame_Opcode (Interp.Stack, Opcode_Read_List_End);
				Chain_Frame_Result (Interp, Interp.Stack, True_Pointer);

			when False_Token =>
				Set_Frame_Opcode (Interp.Stack, Opcode_Read_List_End);
				Chain_Frame_Result (Interp, Interp.Stack, False_Pointer);

			when others =>
				-- TODO: set various error info
				raise Syntax_Error;
		end case;

		--Pop_Tops (Interp, 1);
	end Read_List_Cdr;

	procedure Read_List_End is
		pragma Inline (Read_List_End);
		V: aliased Object_Pointer;
	begin
		Fetch_Token;

		--Push_Top (Interp, V'Unchecked_Access);

		case Interp.Token.Kind is
			when Right_Parenthesis_Token =>
				V := Get_Frame_Result(Interp.Stack);
				pragma Assert (V /= Nil_Pointer);
				-- The first item in the chain is actually Cdr of the last cell.
				V := Reverse_Cons(Get_Cdr(V), Get_Car(V)); 
				Pop_Frame (Interp); 
				Chain_Frame_Result (Interp, Interp.Stack, V);
			when others =>
				raise Syntax_Error;
		end case;

		--Pop_Tops (Interp, 1);
	end Read_List_End;

	procedure Close_List is
		pragma Inline (Close_List);
		V: aliased Object_Pointer;
	begin
		--Push_Top (Interp, V'Unchecked_Access);

		V := Get_Frame_Result(Interp.Stack);
		pragma Assert (Get_Cdr(V) = Nil_Pointer);
		Pop_Frame (Interp); -- Done with the current frame
		Chain_Frame_Result (Interp, Interp.Stack, Get_Car(V));

		--Pop_Tops (Interp, 1);
	end Close_List;

	procedure Close_Quote is
		pragma Inline (Close_Quote);
		V: aliased Object_Pointer;
	begin
		--Push_Top (Interp, V'Unchecked_Access);

		Chain_Frame_Result (Interp, Interp.Stack, Interp.Symbol.Quote);
		V := Get_Frame_Result(Interp.Stack);
		Pop_Frame (Interp); -- Done with the current frame
		Chain_Frame_Result (Interp, Interp.Stack, V);

		--Pop_Tops (Interp, 1);
	end Close_Quote;

	procedure Read_Object is
		pragma Inline (Read_Object);
		V: aliased Object_Pointer;
	begin
		Fetch_Token;

		--Push_Top (Interp, V'Unchecked_Access);

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

			when Character_Token =>
				pragma Assert (Interp.Token.Value.Last = 1);
				V := Character_To_Pointer(Interp.Token.Value.Ptr.all(1));
				Pop_Frame (Interp); -- Done with the current frame
				Chain_Frame_Result (Interp, Interp.Stack, V);

			when String_Token =>
				V := Make_String (Interp.Self, Interp.Token.Value.Ptr.all(1..Interp.Token.Value.Last));
				Pop_Frame (Interp); -- Done with the current frame
				Chain_Frame_Result (Interp, Interp.Stack, V);

			when Identifier_Token =>	
				V := Make_Symbol (Interp.Self, Interp.Token.Value.Ptr.all(1..Interp.Token.Value.Last));
				Pop_Frame (Interp); -- Done with the current frame
				Chain_Frame_Result (Interp, Interp.Stack, V);

			when True_Token =>	
				Pop_Frame (Interp); -- Done with the current frame
				Chain_Frame_Result (Interp, Interp.Stack, True_Pointer);

			when False_Token =>	
				Pop_Frame (Interp); -- Done with the current frame
				Chain_Frame_Result (Interp, Interp.Stack, False_Pointer);


			when others =>
				-- TODO: set various error info
				raise Syntax_Error;
		end case;

		--Pop_Tops (Interp, 1);
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
		case Get_Frame_Opcode(Interp.Stack) is
			when Opcode_Exit =>
				exit;

			when Opcode_Evaluate_Result =>
				Evaluate_Result;

			when Opcode_Evaluate_Object =>
				Evaluate;

			when Opcode_Evaluate_Group =>
				Evaluate_Group;

			when Opcode_Finish_Define_Symbol =>
				Finish_Define_Symbol;

			when Opcode_Finish_If =>
				Finish_If;

			when Opcode_Finish_Set =>
				Finish_Set;
				
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
