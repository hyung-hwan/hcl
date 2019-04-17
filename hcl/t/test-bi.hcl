## test big integer operations
## hcl --log /dev/null,warn+ test-bi.hcl

(sys.srandom (sys.time))
(set count 0)
(while true
	(printf "%O\r" count)
	(set count (+ count 1))

	(set limit (rem (sys.random) 30))
	(set dividend (sys.random))
	(set i 1)
	(while (< i limit)
		(set dividend (bit-or (bit-shift dividend 30) (sys.random)))
		(set i (+ i 1))
	)

	(set limit (rem (sys.random) 30))
	(set divisor (sys.random))
	(set i 1)
	(while (< i limit)
		(set divisor (bit-or (bit-shift divisor 30) (sys.random)))
		(set i (+ i 1))
	)

	(set quotient (/ dividend divisor))
	(set remainder (rem dividend divisor))
	(set derived_dividend (+ (* quotient divisor) remainder))

	(if (/= dividend derived_dividend)
		(printf ">> dividend %O\n>> divisor %O\n>> quotient %O\n>> remainder %O\n>> derived_dividend %O\n"
			dividend divisor quotient remainder derived_dividend)
		(break)
	)
)
(printf "\n")
