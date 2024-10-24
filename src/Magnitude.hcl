class Magnitude: Object {
	##fun <  (oprnd) { self subclassResponsibility: #< }
	fun <  (oprnd) { return (self:subclassResponsibility #<) }
	fun >  (oprnd) { return (oprnd < self) }
	fun <= (oprnd) { return ((oprnd < self):not) }
	fun >= (oprnd) { return ((self < oprnd):not) }
}

class(#limited) Number: Magnitude {
	fun + (oprnd) { return (+ self oprnd) }
	fun - (oprnd) { return (- self oprnd) }
	fun * (oprnd) { return (* self oprnd) }
	fun / (oprnd) { return (/ self oprnd) }

	##fun > (oprnd) { return (> self oprnd) }
	##fun < (oprnd) { return (< self oprnd) }
	##fun >= (oprnd) { return (>= self oprnd) }
	##fun <= (oprnd) { return (<= self oprnd) }
}

