# HCL - Hybrid Command Language

## Language Syntax

A HCL program is composed of expressions.

## Keywords
* nil
* true
* false

## Special Form Expression
* and
* break
* defun
* do
* elif
* else
* if
* lambda
* or
* return
* set
* until
* while


## literals
* integer
* character
* small pointer
* error
* string
* dictionary
* array
* byte array

## Builtin functions

* not
* _and
* _or
* eqv?
* eql?
* eqk?
* nqv?
* nql?
* nqk?
* sprintf
* printf
* _+_
* _-_
* _*_
* mlt
* /
* quo
* mod
* sqrt
* bit-and
* bit-or
* bit-xor
* bit-not
* bit-shift

## Defining a function

```
(defun function-name (arguments)
	| local variables |
	function body
)
```

```
(set function-name (lambda (arguments)
	| local variables |
	function body
)
```
