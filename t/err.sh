#!/bin/sh

for i in $@; do :; done
script="$i"

expected_errinfo=$(grep -n -o -E "##ERROR: .+" "$script" 2>/dev/null)
[ -z "$expected_errinfo" ] && {
	echo "INVALID TESTER - $script contains no ERROR information"
	exit 1
}

expected_errline=$(echo $expected_errinfo | cut -d: -f1)
xlen=$(echo $expected_errline | wc -c)
xlen=$(expr $xlen + 10)
expected_errmsg=$(echo $expected_errinfo | cut -c${xlen}-)

output=$($@ 2>&1) 
## the regular expression is not escaped properly. the error information must not
## include specifial regex characters to avoid problems.
echo "$output" | grep -E "ERROR:.+${script}\[${expected_errline},[[:digit:]]+\] ${expected_errmsg}" || {
	echo "$script - $output"
	exit 1
}
exit 0
