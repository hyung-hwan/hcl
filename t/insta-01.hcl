## test class instantiation methods

fun UndefinedObject: ~= (oprnd) { return (nqv? self oprnd) } ## for if (a ~= nil) ...
fun Number: + (oprnd) { return (+ self oprnd) }
fun Number: - (oprnd) { return (- self oprnd) }
fun Number: * (oprnd) { return (* self oprnd) }
fun Number: / (oprnd) { return (/ self oprnd) }
fun Number: > (oprnd) { return (> self oprnd) }
fun Number: < (oprnd) { return (< self oprnd) }
fun Number: >= (oprnd) { return (>= self oprnd) }
fun Number: <= (oprnd) { return (<= self oprnd) }
fun Number: == (oprnd) { return (== self oprnd) }
fun Number: ~= (oprnd) { return (~= self oprnd) }

class A {
	var a b c

	fun(#ci) newInstance(x y z) {
		set a x;
		set b y;
		set c z;
		return self;
	};

	fun get-a() { return self.a; };
	fun get-b() { return self.b; };
	fun get-c() { return self.c; };
};

class B: A {
	var d e f

	fun(#ci) newInstance(x y z) {
		super:newInstance (* x 2) (* y 2) (* z 2);
		set d x;
		set e y;
		set f z;
		return self;
	};

	fun(#c) getSuper() { return super; };
	###fun(#c) getSuperclass() { return (self:superclass); };
	fun(#c) getSelf() { return self; };

	fun sum() {
		return (+ (super:get-a) (super:get-b) (super:get-c) self.d self.e self.f);
	};
};

a := ((B:newInstance 1 2 3):sum)
if (a ~= 18) { printf "ERROR: a must be 18\n"; } \
else { printf "OK %d\n" a; }

b := (B:newInstance 2 3 4)
a := (b:get-a)
if (a ~= 4) {printf "ERROR: a must be 4\n" } \
else { printf "OK %d\n" a }

c := (object-new A)
a := (c:get-a)
if (a ~= nil) {printf "ERROR: a must be nil\n" } \
else { printf "OK %O\n" a }


a := (b:get-b);
if (a ~= 6) { printf "ERROR: a must be 6\n" } \
else { printf "OK %d\n" a };

a := (b:get-c);
if (a ~= 8) { printf "ERROR: a must be 8\n" } \
else {printf "OK %d\n" a };

a := (b:sum);
if (a ~= 27) { printf "ERROR: a must be 27\n" } \
else { printf "OK %d\n" a };

## super is equivalent to self unless a message is sent to it.
## if super itself is returned without a message, it just return
## the receiver just like self. To get the superclass, it must use
## the superclass method inherited from the Class class.

b := (B:getSelf)
a := (B:getSuper)
##c := (B:getSuperlcass)

if (nqv? a b) { printf "ERROR: a is not equivalent to b\n" } \
else { printf "OK a is equivalent to b\n" };

##if (eqv? a c) { printf "ERROR: a is equivalent to b\n" } \
##else { printf "OK a is not equivalent to b\n" };

if (nqv? a B) { printf "ERROR: a is not equivalent to B\n" } \
else { printf "OK a is equivalent to A\n" };

if (nqv? b B) { printf "ERROR: b is not equivalent to B\n" } \
else { printf "OK b is equivalent to B\n" };

##if (nqv? c A) { printf "ERROR: c is not equivalent to A\n" } \
##else { printf "OK c is equivalent to A\n" };
