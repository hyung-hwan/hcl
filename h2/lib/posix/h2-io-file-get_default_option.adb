separate (H2.IO.File)

function Get_Default_Option return Option_Record is

	Default_Option: constant Option_Record := (
		Bits => OPTION_CRLF_IN,
		CR => Ascii.Code.CR,
		LF => Ascii.Code.LF
	);

begin
	return Default_Option;
end Get_Default_Option;
