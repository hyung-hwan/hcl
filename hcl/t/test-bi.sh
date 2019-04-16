a=`openssl rand -hex 20 | tr '[a-z]' '[A-Z]'`
b=`openssl rand -hex 19 | tr '[a-z]' '[A-Z]'`

q1=$(echo -e "ibase=16\nprint ($a / $b)"  | bc -q)
r1=$(echo -e "ibase=16\nprint ($a % $b)"  | bc -q)

q2=$(echo -e "(printf \"%O\" (/ #x$a #x$b))"  | ~/xxx/bin/hcl /dev/stdin 2> /dev/null)
r2=$(echo -e "(printf \"%O\" (rem #x$a #x$b))"  | ~/xxx/bin/hcl /dev/stdin 2>/dev/null)

echo "$q2"
echo "$r2"
