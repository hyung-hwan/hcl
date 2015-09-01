; what the
;(+ x (+ c z) (+ y (+ a b)))
;(+ x y z)
;hello-world
;()
;(x . y)
;(x . (y))
;(x . ((a b c)))
;(x y  . ())
;(x .  y z)
;(x . )
;(() x y z)
;(() . (x y z))
;(() . (x (y a b c d e . j) z))
;(() . (x (y a b "hello world" d "so good" . j) z))

;'10
;'(+ x (+ c z) '(+ y (+ a b)))
;(x 'y z)

;(x . 'y)


(+ -10 +20 30)

( 
	(lambda (x y) (+ x y))
	100 200
)


;'(10 20 30)

'(+ x (+ c z) '(+ y (+ a b)))

;(define makeOperator
;  (lambda (operator)
;    (lambda (num1 num2)
;     (operator num1 num2))))
;example useage - equivalent to (* 3 4):
;((makeOperator *) 3 4)

;(define (makeOperator operator)
;  (lambda (num1 num2)
;    (operator num1 num2)))

;(define (makeOperator operator)
;  (define (foo num1 num2)
;    (operator num1 num2))
;  foo)

;(define ((foo x) y) (+ x y))
;(foo 5)
;; => procedure
;((foo 5) 3)
;; => 8
