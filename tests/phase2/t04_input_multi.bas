' Test: INPUT #n with multiple numeric variables
open "t04.tmp" for output as #1
print #1, "3"
print #1, "7"
print #1, "11"
close #1
open "t04.tmp" for input as #1
input #1, a
input #1, b
input #1, c
close #1
println a + b + c
