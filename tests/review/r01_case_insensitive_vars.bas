' Test: variables are truly case-insensitive (x, X, COUNTER, counter = same slot)
x = 42
println X           ' must print 42, not 0

Counter = 100
println counter     ' must print 100, not 0
println COUNTER     ' same slot

name$ = "hello"
println NAME$       ' must print hello
