count=0
while true
do
	/bin/echo -n -e "$count\r"
	count=$(($count + 1))

	a=`openssl rand -hex 1 | tr '[a-z]' '[A-Z]'`
	a=$(echo -e "(printf \"%O\" #x$a)"  | ~/xxx/bin/hcl --log /dev/null /dev/stdin)
	[ "$a" = "0" ] && a=1
	a=`openssl rand -hex $a | tr '[a-z]' '[A-Z]'`
	
	
	b=`openssl rand -hex 1 | tr '[a-z]' '[A-Z]'`
	b=$(echo -e "(printf \"%O\" #x$b)"  | ~/xxx/bin/hcl --log /dev/null /dev/stdin)
	[ "$b" = "0" ]  && b=1
	b=`openssl rand -hex $b | tr '[a-z]' '[A-Z]'`
	
	a=$(echo -e "(printf \"%O\" #x$a)"  | ~/xxx/bin/hcl --log /dev/null /dev/stdin)
	b=$(echo -e "(printf \"%O\" #x$b)"  | ~/xxx/bin/hcl --log /dev/null /dev/stdin)
	[ "$b" = "0" ] && b=1

	q=$(echo -e "(printf \"%O\" (/ $a $b))"  | ~/xxx/bin/hcl --log /dev/null /dev/stdin)
	r=$(echo -e "(printf \"%O\" (rem $a $b))"  | ~/xxx/bin/hcl --log /dev/null /dev/stdin)
	a1=$(echo -e "(printf \"%O\" (+ (* $q $b) $r))"  | ~/xxx/bin/hcl --log /dev/null /dev/stdin)
	
	if [ "$a" != "$a1" ]
	then
		echo "a=>$a"
		echo "b=>$b"
		echo "q=>$q"
		echo "r=>$r"
		echo "a1=>$a1"
		break
	fi
done
