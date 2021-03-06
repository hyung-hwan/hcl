-- Generated with ascii.txt and ascii.awk
-- Run qseawk -f ascii.awk ascii.txt > h2-ascii.ads for regeneration

generic
	type Slim_Character is (<>);
	type Wide_Character is (<>);
package H2.Ascii is

	package Code is
		NUL                 : constant := 0;
		SOH                 : constant := 1;
		STX                 : constant := 2;
		ETX                 : constant := 3;
		EOT                 : constant := 4;
		ENQ                 : constant := 5;
		ACK                 : constant := 6;
		BEL                 : constant := 7;
		BS                  : constant := 8;
		HT                  : constant := 9;
		LF                  : constant := 10;
		VT                  : constant := 11;
		FF                  : constant := 12;
		CR                  : constant := 13;
		SO                  : constant := 14;
		SI                  : constant := 15;
		DLE                 : constant := 16;
		DC1                 : constant := 17;
		DC2                 : constant := 18;
		DC3                 : constant := 19;
		DC4                 : constant := 20;
		NAK                 : constant := 21;
		SYN                 : constant := 22;
		ETB                 : constant := 23;
		CAN                 : constant := 24;
		EM                  : constant := 25;
		SUB                 : constant := 26;
		ESC                 : constant := 27;
		FS                  : constant := 28;
		GS                  : constant := 29;
		RS                  : constant := 30;
		US                  : constant := 31;
		Space               : constant := 32; --  
		Exclamation         : constant := 33; -- !
		Quotation           : constant := 34; -- "
		Number_Sign         : constant := 35; -- #
		Dollar_Sign         : constant := 36; -- $
		Percent_Sign        : constant := 37; -- %
		Ampersand           : constant := 38; -- &
		Apostrophe          : constant := 39; -- '
		Left_Parenthesis    : constant := 40; -- (
		Right_Parenthesis   : constant := 41; -- )
		Asterisk            : constant := 42; -- *
		Plus_Sign           : constant := 43; -- +
		Comma               : constant := 44; -- ,
		Minus_Sign          : constant := 45; -- -
		Period              : constant := 46; -- .
		Slash               : constant := 47; -- /
		Zero                : constant := 48; -- 0
		One                 : constant := 49; -- 1
		Two                 : constant := 50; -- 2
		Three               : constant := 51; -- 3
		Four                : constant := 52; -- 4
		Five                : constant := 53; -- 5
		Six                 : constant := 54; -- 6
		Seven               : constant := 55; -- 7
		Eight               : constant := 56; -- 8
		Nine                : constant := 57; -- 9
		Colon               : constant := 58; -- :
		Semicolon           : constant := 59; -- ;
		Less_Than_Sign      : constant := 60; -- <
		Equal_Sign          : constant := 61; -- =
		Greater_Than_Sign   : constant := 62; -- >
		Question            : constant := 63; -- ?
		Commercial_At       : constant := 64; -- @
		UC_A                : constant := 65; -- A
		UC_B                : constant := 66; -- B
		UC_C                : constant := 67; -- C
		UC_D                : constant := 68; -- D
		UC_E                : constant := 69; -- E
		UC_F                : constant := 70; -- F
		UC_G                : constant := 71; -- G
		UC_H                : constant := 72; -- H
		UC_I                : constant := 73; -- I
		UC_J                : constant := 74; -- J
		UC_K                : constant := 75; -- K
		UC_L                : constant := 76; -- L
		UC_M                : constant := 77; -- M
		UC_N                : constant := 78; -- N
		UC_O                : constant := 79; -- O
		UC_P                : constant := 80; -- P
		UC_Q                : constant := 81; -- Q
		UC_R                : constant := 82; -- R
		UC_S                : constant := 83; -- S
		UC_T                : constant := 84; -- T
		UC_U                : constant := 85; -- U
		UC_V                : constant := 86; -- V
		UC_W                : constant := 87; -- W
		UC_X                : constant := 88; -- X
		UC_Y                : constant := 89; -- Y
		UC_Z                : constant := 90; -- Z
		Left_Square_Bracket : constant := 91; -- [
		Backslash           : constant := 92; -- \
		Right_Square_Bracket: constant := 93; -- ]
		Circumflex          : constant := 94; -- ^
		Low_Line            : constant := 95; -- _
		Grave               : constant := 96; -- `
		LC_A                : constant := 97; -- a
		LC_B                : constant := 98; -- b
		LC_C                : constant := 99; -- c
		LC_D                : constant := 100; -- d
		LC_E                : constant := 101; -- e
		LC_F                : constant := 102; -- f
		LC_G                : constant := 103; -- g
		LC_H                : constant := 104; -- h
		LC_I                : constant := 105; -- i
		LC_J                : constant := 106; -- j
		LC_K                : constant := 107; -- k
		LC_L                : constant := 108; -- l
		LC_M                : constant := 109; -- m
		LC_N                : constant := 110; -- n
		LC_O                : constant := 111; -- o
		LC_P                : constant := 112; -- p
		LC_Q                : constant := 113; -- q
		LC_R                : constant := 114; -- r
		LC_S                : constant := 115; -- s
		LC_T                : constant := 116; -- t
		LC_U                : constant := 117; -- u
		LC_V                : constant := 118; -- v
		LC_W                : constant := 119; -- w
		LC_X                : constant := 120; -- x
		LC_Y                : constant := 121; -- y
		LC_Z                : constant := 122; -- z
		Left_Curly_Bracket  : constant := 123; -- {
		Vertical_Line       : constant := 124; -- |
		Right_Curly_Bracket : constant := 125; -- }
		Tilde               : constant := 126; -- ~
		DEL                 : constant := 127;
	end Code;

	package Slim is
		NUL                 : constant Slim_Character := Slim_Character'Val(Code.NUL);
		SOH                 : constant Slim_Character := Slim_Character'Val(Code.SOH);
		STX                 : constant Slim_Character := Slim_Character'Val(Code.STX);
		ETX                 : constant Slim_Character := Slim_Character'Val(Code.ETX);
		EOT                 : constant Slim_Character := Slim_Character'Val(Code.EOT);
		ENQ                 : constant Slim_Character := Slim_Character'Val(Code.ENQ);
		ACK                 : constant Slim_Character := Slim_Character'Val(Code.ACK);
		BEL                 : constant Slim_Character := Slim_Character'Val(Code.BEL);
		BS                  : constant Slim_Character := Slim_Character'Val(Code.BS);
		HT                  : constant Slim_Character := Slim_Character'Val(Code.HT);
		LF                  : constant Slim_Character := Slim_Character'Val(Code.LF);
		VT                  : constant Slim_Character := Slim_Character'Val(Code.VT);
		FF                  : constant Slim_Character := Slim_Character'Val(Code.FF);
		CR                  : constant Slim_Character := Slim_Character'Val(Code.CR);
		SO                  : constant Slim_Character := Slim_Character'Val(Code.SO);
		SI                  : constant Slim_Character := Slim_Character'Val(Code.SI);
		DLE                 : constant Slim_Character := Slim_Character'Val(Code.DLE);
		DC1                 : constant Slim_Character := Slim_Character'Val(Code.DC1);
		DC2                 : constant Slim_Character := Slim_Character'Val(Code.DC2);
		DC3                 : constant Slim_Character := Slim_Character'Val(Code.DC3);
		DC4                 : constant Slim_Character := Slim_Character'Val(Code.DC4);
		NAK                 : constant Slim_Character := Slim_Character'Val(Code.NAK);
		SYN                 : constant Slim_Character := Slim_Character'Val(Code.SYN);
		ETB                 : constant Slim_Character := Slim_Character'Val(Code.ETB);
		CAN                 : constant Slim_Character := Slim_Character'Val(Code.CAN);
		EM                  : constant Slim_Character := Slim_Character'Val(Code.EM);
		SUB                 : constant Slim_Character := Slim_Character'Val(Code.SUB);
		ESC                 : constant Slim_Character := Slim_Character'Val(Code.ESC);
		FS                  : constant Slim_Character := Slim_Character'Val(Code.FS);
		GS                  : constant Slim_Character := Slim_Character'Val(Code.GS);
		RS                  : constant Slim_Character := Slim_Character'Val(Code.RS);
		US                  : constant Slim_Character := Slim_Character'Val(Code.US);
		Space               : constant Slim_Character := Slim_Character'Val(Code.Space);
		Exclamation         : constant Slim_Character := Slim_Character'Val(Code.Exclamation);
		Quotation           : constant Slim_Character := Slim_Character'Val(Code.Quotation);
		Number_Sign         : constant Slim_Character := Slim_Character'Val(Code.Number_Sign);
		Dollar_Sign         : constant Slim_Character := Slim_Character'Val(Code.Dollar_Sign);
		Percent_Sign        : constant Slim_Character := Slim_Character'Val(Code.Percent_Sign);
		Ampersand           : constant Slim_Character := Slim_Character'Val(Code.Ampersand);
		Apostrophe          : constant Slim_Character := Slim_Character'Val(Code.Apostrophe);
		Left_Parenthesis    : constant Slim_Character := Slim_Character'Val(Code.Left_Parenthesis);
		Right_Parenthesis   : constant Slim_Character := Slim_Character'Val(Code.Right_Parenthesis);
		Asterisk            : constant Slim_Character := Slim_Character'Val(Code.Asterisk);
		Plus_Sign           : constant Slim_Character := Slim_Character'Val(Code.Plus_Sign);
		Comma               : constant Slim_Character := Slim_Character'Val(Code.Comma);
		Minus_Sign          : constant Slim_Character := Slim_Character'Val(Code.Minus_Sign);
		Period              : constant Slim_Character := Slim_Character'Val(Code.Period);
		Slash               : constant Slim_Character := Slim_Character'Val(Code.Slash);
		Zero                : constant Slim_Character := Slim_Character'Val(Code.Zero);
		One                 : constant Slim_Character := Slim_Character'Val(Code.One);
		Two                 : constant Slim_Character := Slim_Character'Val(Code.Two);
		Three               : constant Slim_Character := Slim_Character'Val(Code.Three);
		Four                : constant Slim_Character := Slim_Character'Val(Code.Four);
		Five                : constant Slim_Character := Slim_Character'Val(Code.Five);
		Six                 : constant Slim_Character := Slim_Character'Val(Code.Six);
		Seven               : constant Slim_Character := Slim_Character'Val(Code.Seven);
		Eight               : constant Slim_Character := Slim_Character'Val(Code.Eight);
		Nine                : constant Slim_Character := Slim_Character'Val(Code.Nine);
		Colon               : constant Slim_Character := Slim_Character'Val(Code.Colon);
		Semicolon           : constant Slim_Character := Slim_Character'Val(Code.Semicolon);
		Less_Than_Sign      : constant Slim_Character := Slim_Character'Val(Code.Less_Than_Sign);
		Equal_Sign          : constant Slim_Character := Slim_Character'Val(Code.Equal_Sign);
		Greater_Than_Sign   : constant Slim_Character := Slim_Character'Val(Code.Greater_Than_Sign);
		Question            : constant Slim_Character := Slim_Character'Val(Code.Question);
		Commercial_At       : constant Slim_Character := Slim_Character'Val(Code.Commercial_At);
		UC_A                : constant Slim_Character := Slim_Character'Val(Code.UC_A);
		UC_B                : constant Slim_Character := Slim_Character'Val(Code.UC_B);
		UC_C                : constant Slim_Character := Slim_Character'Val(Code.UC_C);
		UC_D                : constant Slim_Character := Slim_Character'Val(Code.UC_D);
		UC_E                : constant Slim_Character := Slim_Character'Val(Code.UC_E);
		UC_F                : constant Slim_Character := Slim_Character'Val(Code.UC_F);
		UC_G                : constant Slim_Character := Slim_Character'Val(Code.UC_G);
		UC_H                : constant Slim_Character := Slim_Character'Val(Code.UC_H);
		UC_I                : constant Slim_Character := Slim_Character'Val(Code.UC_I);
		UC_J                : constant Slim_Character := Slim_Character'Val(Code.UC_J);
		UC_K                : constant Slim_Character := Slim_Character'Val(Code.UC_K);
		UC_L                : constant Slim_Character := Slim_Character'Val(Code.UC_L);
		UC_M                : constant Slim_Character := Slim_Character'Val(Code.UC_M);
		UC_N                : constant Slim_Character := Slim_Character'Val(Code.UC_N);
		UC_O                : constant Slim_Character := Slim_Character'Val(Code.UC_O);
		UC_P                : constant Slim_Character := Slim_Character'Val(Code.UC_P);
		UC_Q                : constant Slim_Character := Slim_Character'Val(Code.UC_Q);
		UC_R                : constant Slim_Character := Slim_Character'Val(Code.UC_R);
		UC_S                : constant Slim_Character := Slim_Character'Val(Code.UC_S);
		UC_T                : constant Slim_Character := Slim_Character'Val(Code.UC_T);
		UC_U                : constant Slim_Character := Slim_Character'Val(Code.UC_U);
		UC_V                : constant Slim_Character := Slim_Character'Val(Code.UC_V);
		UC_W                : constant Slim_Character := Slim_Character'Val(Code.UC_W);
		UC_X                : constant Slim_Character := Slim_Character'Val(Code.UC_X);
		UC_Y                : constant Slim_Character := Slim_Character'Val(Code.UC_Y);
		UC_Z                : constant Slim_Character := Slim_Character'Val(Code.UC_Z);
		Left_Square_Bracket : constant Slim_Character := Slim_Character'Val(Code.Left_Square_Bracket);
		Backslash           : constant Slim_Character := Slim_Character'Val(Code.Backslash);
		Right_Square_Bracket: constant Slim_Character := Slim_Character'Val(Code.Right_Square_Bracket);
		Circumflex          : constant Slim_Character := Slim_Character'Val(Code.Circumflex);
		Low_Line            : constant Slim_Character := Slim_Character'Val(Code.Low_Line);
		Grave               : constant Slim_Character := Slim_Character'Val(Code.Grave);
		LC_A                : constant Slim_Character := Slim_Character'Val(Code.LC_A);
		LC_B                : constant Slim_Character := Slim_Character'Val(Code.LC_B);
		LC_C                : constant Slim_Character := Slim_Character'Val(Code.LC_C);
		LC_D                : constant Slim_Character := Slim_Character'Val(Code.LC_D);
		LC_E                : constant Slim_Character := Slim_Character'Val(Code.LC_E);
		LC_F                : constant Slim_Character := Slim_Character'Val(Code.LC_F);
		LC_G                : constant Slim_Character := Slim_Character'Val(Code.LC_G);
		LC_H                : constant Slim_Character := Slim_Character'Val(Code.LC_H);
		LC_I                : constant Slim_Character := Slim_Character'Val(Code.LC_I);
		LC_J                : constant Slim_Character := Slim_Character'Val(Code.LC_J);
		LC_K                : constant Slim_Character := Slim_Character'Val(Code.LC_K);
		LC_L                : constant Slim_Character := Slim_Character'Val(Code.LC_L);
		LC_M                : constant Slim_Character := Slim_Character'Val(Code.LC_M);
		LC_N                : constant Slim_Character := Slim_Character'Val(Code.LC_N);
		LC_O                : constant Slim_Character := Slim_Character'Val(Code.LC_O);
		LC_P                : constant Slim_Character := Slim_Character'Val(Code.LC_P);
		LC_Q                : constant Slim_Character := Slim_Character'Val(Code.LC_Q);
		LC_R                : constant Slim_Character := Slim_Character'Val(Code.LC_R);
		LC_S                : constant Slim_Character := Slim_Character'Val(Code.LC_S);
		LC_T                : constant Slim_Character := Slim_Character'Val(Code.LC_T);
		LC_U                : constant Slim_Character := Slim_Character'Val(Code.LC_U);
		LC_V                : constant Slim_Character := Slim_Character'Val(Code.LC_V);
		LC_W                : constant Slim_Character := Slim_Character'Val(Code.LC_W);
		LC_X                : constant Slim_Character := Slim_Character'Val(Code.LC_X);
		LC_Y                : constant Slim_Character := Slim_Character'Val(Code.LC_Y);
		LC_Z                : constant Slim_Character := Slim_Character'Val(Code.LC_Z);
		Left_Curly_Bracket  : constant Slim_Character := Slim_Character'Val(Code.Left_Curly_Bracket);
		Vertical_Line       : constant Slim_Character := Slim_Character'Val(Code.Vertical_Line);
		Right_Curly_Bracket : constant Slim_Character := Slim_Character'Val(Code.Right_Curly_Bracket);
		Tilde               : constant Slim_Character := Slim_Character'Val(Code.Tilde);
		DEL                 : constant Slim_Character := Slim_Character'Val(Code.DEL);
	end Slim;

	package Wide is
		NUL                 : constant Wide_Character := Wide_Character'Val(Code.NUL);
		SOH                 : constant Wide_Character := Wide_Character'Val(Code.SOH);
		STX                 : constant Wide_Character := Wide_Character'Val(Code.STX);
		ETX                 : constant Wide_Character := Wide_Character'Val(Code.ETX);
		EOT                 : constant Wide_Character := Wide_Character'Val(Code.EOT);
		ENQ                 : constant Wide_Character := Wide_Character'Val(Code.ENQ);
		ACK                 : constant Wide_Character := Wide_Character'Val(Code.ACK);
		BEL                 : constant Wide_Character := Wide_Character'Val(Code.BEL);
		BS                  : constant Wide_Character := Wide_Character'Val(Code.BS);
		HT                  : constant Wide_Character := Wide_Character'Val(Code.HT);
		LF                  : constant Wide_Character := Wide_Character'Val(Code.LF);
		VT                  : constant Wide_Character := Wide_Character'Val(Code.VT);
		FF                  : constant Wide_Character := Wide_Character'Val(Code.FF);
		CR                  : constant Wide_Character := Wide_Character'Val(Code.CR);
		SO                  : constant Wide_Character := Wide_Character'Val(Code.SO);
		SI                  : constant Wide_Character := Wide_Character'Val(Code.SI);
		DLE                 : constant Wide_Character := Wide_Character'Val(Code.DLE);
		DC1                 : constant Wide_Character := Wide_Character'Val(Code.DC1);
		DC2                 : constant Wide_Character := Wide_Character'Val(Code.DC2);
		DC3                 : constant Wide_Character := Wide_Character'Val(Code.DC3);
		DC4                 : constant Wide_Character := Wide_Character'Val(Code.DC4);
		NAK                 : constant Wide_Character := Wide_Character'Val(Code.NAK);
		SYN                 : constant Wide_Character := Wide_Character'Val(Code.SYN);
		ETB                 : constant Wide_Character := Wide_Character'Val(Code.ETB);
		CAN                 : constant Wide_Character := Wide_Character'Val(Code.CAN);
		EM                  : constant Wide_Character := Wide_Character'Val(Code.EM);
		SUB                 : constant Wide_Character := Wide_Character'Val(Code.SUB);
		ESC                 : constant Wide_Character := Wide_Character'Val(Code.ESC);
		FS                  : constant Wide_Character := Wide_Character'Val(Code.FS);
		GS                  : constant Wide_Character := Wide_Character'Val(Code.GS);
		RS                  : constant Wide_Character := Wide_Character'Val(Code.RS);
		US                  : constant Wide_Character := Wide_Character'Val(Code.US);
		Space               : constant Wide_Character := Wide_Character'Val(Code.Space);
		Exclamation         : constant Wide_Character := Wide_Character'Val(Code.Exclamation);
		Quotation           : constant Wide_Character := Wide_Character'Val(Code.Quotation);
		Number_Sign         : constant Wide_Character := Wide_Character'Val(Code.Number_Sign);
		Dollar_Sign         : constant Wide_Character := Wide_Character'Val(Code.Dollar_Sign);
		Percent_Sign        : constant Wide_Character := Wide_Character'Val(Code.Percent_Sign);
		Ampersand           : constant Wide_Character := Wide_Character'Val(Code.Ampersand);
		Apostrophe          : constant Wide_Character := Wide_Character'Val(Code.Apostrophe);
		Left_Parenthesis    : constant Wide_Character := Wide_Character'Val(Code.Left_Parenthesis);
		Right_Parenthesis   : constant Wide_Character := Wide_Character'Val(Code.Right_Parenthesis);
		Asterisk            : constant Wide_Character := Wide_Character'Val(Code.Asterisk);
		Plus_Sign           : constant Wide_Character := Wide_Character'Val(Code.Plus_Sign);
		Comma               : constant Wide_Character := Wide_Character'Val(Code.Comma);
		Minus_Sign          : constant Wide_Character := Wide_Character'Val(Code.Minus_Sign);
		Period              : constant Wide_Character := Wide_Character'Val(Code.Period);
		Slash               : constant Wide_Character := Wide_Character'Val(Code.Slash);
		Zero                : constant Wide_Character := Wide_Character'Val(Code.Zero);
		One                 : constant Wide_Character := Wide_Character'Val(Code.One);
		Two                 : constant Wide_Character := Wide_Character'Val(Code.Two);
		Three               : constant Wide_Character := Wide_Character'Val(Code.Three);
		Four                : constant Wide_Character := Wide_Character'Val(Code.Four);
		Five                : constant Wide_Character := Wide_Character'Val(Code.Five);
		Six                 : constant Wide_Character := Wide_Character'Val(Code.Six);
		Seven               : constant Wide_Character := Wide_Character'Val(Code.Seven);
		Eight               : constant Wide_Character := Wide_Character'Val(Code.Eight);
		Nine                : constant Wide_Character := Wide_Character'Val(Code.Nine);
		Colon               : constant Wide_Character := Wide_Character'Val(Code.Colon);
		Semicolon           : constant Wide_Character := Wide_Character'Val(Code.Semicolon);
		Less_Than_Sign      : constant Wide_Character := Wide_Character'Val(Code.Less_Than_Sign);
		Equal_Sign          : constant Wide_Character := Wide_Character'Val(Code.Equal_Sign);
		Greater_Than_Sign   : constant Wide_Character := Wide_Character'Val(Code.Greater_Than_Sign);
		Question            : constant Wide_Character := Wide_Character'Val(Code.Question);
		Commercial_At       : constant Wide_Character := Wide_Character'Val(Code.Commercial_At);
		UC_A                : constant Wide_Character := Wide_Character'Val(Code.UC_A);
		UC_B                : constant Wide_Character := Wide_Character'Val(Code.UC_B);
		UC_C                : constant Wide_Character := Wide_Character'Val(Code.UC_C);
		UC_D                : constant Wide_Character := Wide_Character'Val(Code.UC_D);
		UC_E                : constant Wide_Character := Wide_Character'Val(Code.UC_E);
		UC_F                : constant Wide_Character := Wide_Character'Val(Code.UC_F);
		UC_G                : constant Wide_Character := Wide_Character'Val(Code.UC_G);
		UC_H                : constant Wide_Character := Wide_Character'Val(Code.UC_H);
		UC_I                : constant Wide_Character := Wide_Character'Val(Code.UC_I);
		UC_J                : constant Wide_Character := Wide_Character'Val(Code.UC_J);
		UC_K                : constant Wide_Character := Wide_Character'Val(Code.UC_K);
		UC_L                : constant Wide_Character := Wide_Character'Val(Code.UC_L);
		UC_M                : constant Wide_Character := Wide_Character'Val(Code.UC_M);
		UC_N                : constant Wide_Character := Wide_Character'Val(Code.UC_N);
		UC_O                : constant Wide_Character := Wide_Character'Val(Code.UC_O);
		UC_P                : constant Wide_Character := Wide_Character'Val(Code.UC_P);
		UC_Q                : constant Wide_Character := Wide_Character'Val(Code.UC_Q);
		UC_R                : constant Wide_Character := Wide_Character'Val(Code.UC_R);
		UC_S                : constant Wide_Character := Wide_Character'Val(Code.UC_S);
		UC_T                : constant Wide_Character := Wide_Character'Val(Code.UC_T);
		UC_U                : constant Wide_Character := Wide_Character'Val(Code.UC_U);
		UC_V                : constant Wide_Character := Wide_Character'Val(Code.UC_V);
		UC_W                : constant Wide_Character := Wide_Character'Val(Code.UC_W);
		UC_X                : constant Wide_Character := Wide_Character'Val(Code.UC_X);
		UC_Y                : constant Wide_Character := Wide_Character'Val(Code.UC_Y);
		UC_Z                : constant Wide_Character := Wide_Character'Val(Code.UC_Z);
		Left_Square_Bracket : constant Wide_Character := Wide_Character'Val(Code.Left_Square_Bracket);
		Backslash           : constant Wide_Character := Wide_Character'Val(Code.Backslash);
		Right_Square_Bracket: constant Wide_Character := Wide_Character'Val(Code.Right_Square_Bracket);
		Circumflex          : constant Wide_Character := Wide_Character'Val(Code.Circumflex);
		Low_Line            : constant Wide_Character := Wide_Character'Val(Code.Low_Line);
		Grave               : constant Wide_Character := Wide_Character'Val(Code.Grave);
		LC_A                : constant Wide_Character := Wide_Character'Val(Code.LC_A);
		LC_B                : constant Wide_Character := Wide_Character'Val(Code.LC_B);
		LC_C                : constant Wide_Character := Wide_Character'Val(Code.LC_C);
		LC_D                : constant Wide_Character := Wide_Character'Val(Code.LC_D);
		LC_E                : constant Wide_Character := Wide_Character'Val(Code.LC_E);
		LC_F                : constant Wide_Character := Wide_Character'Val(Code.LC_F);
		LC_G                : constant Wide_Character := Wide_Character'Val(Code.LC_G);
		LC_H                : constant Wide_Character := Wide_Character'Val(Code.LC_H);
		LC_I                : constant Wide_Character := Wide_Character'Val(Code.LC_I);
		LC_J                : constant Wide_Character := Wide_Character'Val(Code.LC_J);
		LC_K                : constant Wide_Character := Wide_Character'Val(Code.LC_K);
		LC_L                : constant Wide_Character := Wide_Character'Val(Code.LC_L);
		LC_M                : constant Wide_Character := Wide_Character'Val(Code.LC_M);
		LC_N                : constant Wide_Character := Wide_Character'Val(Code.LC_N);
		LC_O                : constant Wide_Character := Wide_Character'Val(Code.LC_O);
		LC_P                : constant Wide_Character := Wide_Character'Val(Code.LC_P);
		LC_Q                : constant Wide_Character := Wide_Character'Val(Code.LC_Q);
		LC_R                : constant Wide_Character := Wide_Character'Val(Code.LC_R);
		LC_S                : constant Wide_Character := Wide_Character'Val(Code.LC_S);
		LC_T                : constant Wide_Character := Wide_Character'Val(Code.LC_T);
		LC_U                : constant Wide_Character := Wide_Character'Val(Code.LC_U);
		LC_V                : constant Wide_Character := Wide_Character'Val(Code.LC_V);
		LC_W                : constant Wide_Character := Wide_Character'Val(Code.LC_W);
		LC_X                : constant Wide_Character := Wide_Character'Val(Code.LC_X);
		LC_Y                : constant Wide_Character := Wide_Character'Val(Code.LC_Y);
		LC_Z                : constant Wide_Character := Wide_Character'Val(Code.LC_Z);
		Left_Curly_Bracket  : constant Wide_Character := Wide_Character'Val(Code.Left_Curly_Bracket);
		Vertical_Line       : constant Wide_Character := Wide_Character'Val(Code.Vertical_Line);
		Right_Curly_Bracket : constant Wide_Character := Wide_Character'Val(Code.Right_Curly_Bracket);
		Tilde               : constant Wide_Character := Wide_Character'Val(Code.Tilde);
		DEL                 : constant Wide_Character := Wide_Character'Val(Code.DEL);
	end Wide;

end H2.Ascii;
