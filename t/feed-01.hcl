## this file is to test the reader/feeder against weirdly formatted input text.

{  ## START

| J |

fun xxx (x y z
	:: r ) {

	| k
	b
	s |

	k := (+ x y z)
	b := (* k k)
	s := (* b b)

	printf "%d %d %d\n" k b s

	r := s
	J := r
}


[
j
] \
	:= (xxx
	10
	20
	30)

if (eqv? j 12960000) \
{
	printf "OK: j is 12960000\n"
} else {
	printf "BAD: j is not 12960000\n"
}

xxx \
	1 \
	2 \
	3

if (eqv? J 1296) {
	printf "OK: J is 1296\n"
} else {
	printf "BAD: J is not 1296\n"
}


k := 5
if { q := 10; < k q } {  ## a block expression is a normal expression. so it can be used as a conditional expression for if
	printf "OK: k is less than q\n"
} else (printf "BAD: k is not less than q\n")


fun ByteArray:at(pos) {
	return (core.basicAt self pos)
}

fun CharacterArray:at(pos) {
	return (core.basicAt self pos)
}

fun SmallInteger:toCharacter() {
	return (core.smooiToChar self)
}

fun Character:toCode() {
	return (core.charToSmooi self)
}

ba := #B[1 2 3 4 5 6 7 8 9 10 11 12 13 14 15]
i := 0
while (< i 15) {
	x := (+ i 1)
	if (== (ba:at i) x) { printf "OK: (ba:at %d) is %O\n" i (ba:at i) } \
	else { printf "ERROR: (ba:at %d) is not %d\n" i x }
	i := x
}

ca := #C[
	'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm',
	'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z',
	'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M',
	'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z'
]

i := 0
while (< i 26) {
	x := ((+ i ('a':toCode)):toCharacter)
	if (eqv? (ca:at i) x) { printf "OK: (ca:at %d) is %O\n" i x } \
	else { printf "ERROR: (ca:at %d) is not %O\n" i x}
	i := (+ i 1)
}

i := 26
while (< i 52) {
	x := ((+ (- i 26) ('A':toCode)):toCharacter)
	if (eqv? (ca:at i) x) { printf "OK: (ca:at %d) is %O\n" i x } \
	else { printf "ERROR: (ca:at %d) is not %O\n" i x}
	i := (+ i 1)
}


i := 0xAbCd93481FFAABBCCDDEeFa12837281
j := 0o125715446440377652567463357357502240671201
k := 0b1010101111001101100100110100100000011111111110101010101110111100110011011101111011101111101000010010100000110111001010000001
l := 14272837210234798094990047170340811393
m := 16rAbCd93481FFAABBCCDDEeFa12837281
n := 36rMVV36DNKTQ0Z2Q3L027T3NGH
o := +35r18Q4OPTU2KRS7FVR09B5MORY3

if (== i j) { printf "OK: i is equal to j\n" } \
else { printf "ERROR: i is not equal to j\n" }
if (== i k) { printf "OK: i is equal to k\n" } \
else { printf "ERROR: i is not equal to k\n" }
if (== i l) { printf "OK: i is equal to l\n" } \
else { printf "ERROR: i is not equal to l\n" }
if (== i m) { printf "OK: i is equal to m\n" } \
else { printf "ERROR: i is not equal to m\n" }
if (== i n) { printf "OK: i is equal to n\n" } \
else { printf "ERROR: i is not equal to n\n" }
if (== i o) { printf "OK: i is equal to o\n" } \
else { printf "ERROR: i is not equal to o\n" }


i := (- -16r123abcd128738279878172387123aabbea19849d8282882822332332 123)
k := -1919784483373631008405784517212288102153752573650638404319957951405
if (== i k) { printf "OK: i is %d\n" i k } \
else { printf "ERROR: i is not equal to %d\n" k }


a := #{
  (if (> 10 20) true else false ): 10,
  (if (< 10 20) true else false ): 20
}
b := (dic.get a true)
c := (dic.get a false)
if (== b 20) { printf "OK: b is %d\n" b } \
else { printf "ERROR: b is %d, not 20\n" b }
if (== c 10) { printf "OK: b is %d\n" b } \
else { printf "ERROR: b is %d, not 10\n" b }

}  ## END
