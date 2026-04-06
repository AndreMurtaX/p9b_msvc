' Test: INPUT #n with string variables
open "t05.tmp" for output as #1
print #1, "foo"
print #1, "bar"
close #1
open "t05.tmp" for input as #1
input #1, x$
input #1, y$
close #1
println x$ + y$
