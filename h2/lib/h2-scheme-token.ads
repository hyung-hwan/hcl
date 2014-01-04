
private package H2.Scheme.Token is

	procedure Purge (Interp: in out Interpreter_Record);
	pragma Inline (Purge);

	procedure Set (Interp: in out Interpreter_Record;
	                     Kind:   in     Token_Kind;
	                     Value:  in     Object_String);

	procedure Append_String (Interp: in out Interpreter_Record;
	                               Value:  in     Object_String);
	pragma Inline (Append_String);

	procedure Append_Character (Interp: in out Interpreter_Record;
	                                  Value:  in     Object_Character);
	pragma Inline (Append_Character);


end H2.Scheme.Token;

