#!/bin/sh

for i in $@; do :; done
script="$i"

run_partfile() {
	l_cmd="";
	l_nargs=$#

	while [ $# -gt 4 ]
	do
		l_cmd="$l_cmd $1"
		shift
	done

	l_script="$1"
	shift ## skip the original script.
	l_partno="$1"
	shift ## partno
	l_partstartlineno="$1"
	shift ## partstartlineno

	l_partfile="$1"
	l_cmd="$l_cmd $l_partfile"

	l_expected_errinfo=$(grep -n -o -E "##ERROR: .+" "$l_partfile" 2>/dev/null)
	[ -z "$l_expected_errinfo" ] && {
		echo "ERROR: INVALID TESTER - $l_script(part=$l_partno,line=$l_l_partstartlineno) contains no ERROR information"
		return 1
	}

	l_expected_errline=$(echo "$l_expected_errinfo" | cut -d: -f1)
	l_xlen=$(echo "$l_expected_errline" | wc -c)
	l_xlen=$(expr $l_xlen + 10)
	l_expected_errmsg=$(echo "$l_expected_errinfo" | cut -c${l_xlen}-)
	l_output=`$l_cmd 2>&1`

	l_errlineno=$(expr $l_partstartlineno + $l_expected_errline - 1)

	## the checks using grep may not be 100% accurate depending on message patterns.
	## but it's unlikely such a case really happens
	echo "$l_output" | grep -q -E "^ERROR:.+${l_partfile}\[${l_expected_errline},[[:digit:]]+\] .+" 2>&1 || {
		echo "ERROR: error not raised at line $l_expected_errline - $l_script(part=$l_partno,line=$l_errlineno) - $l_output"
		return 1
	}
	echo "$l_output" | grep -q -F " ${l_expected_errmsg}" 2>&1 || {
		echo "ERROR: error not raised at line $l_expected_errline - $l_script(part=$l_partno,line=$l_errlineno) - $l_output"
		return 1
	}

	echo "OK"
	return 0
}


ever_failed=0
partfile=`mktemp`
partno=0
partlines=0
lineno=0
partstartlineno=`expr $lineno + 1`
> "$partfile"

## dash behaves differently for read -r.
## while \n is read in literally by bash or other shells, dash converts it to a new-line
while IFS= read -r line
do
	lineno=`expr $lineno + 1`
	if [ "$line" = "---" ]
	then
		[ $partlines -gt 0 ] && {
			run_partfile "$@" "$partno" "$partstartlineno" "$partfile" || ever_failed=1
		}
		partno=`expr $partno + 1`
		partlines=0
		partstartlineno=`expr $lineno + 1`
		> "$partfile"
	else
		echo "$line" >> "$partfile"
		partlines=`expr $partlines + 1`
	fi
done < "$script"

[ $partlines -gt 0 ] && {
	run_partfile "$@" "$partno" "$partstartlineno" "$partfile" || ever_failed=1
}

rm -f "$partfile"
exit $ever_failed

