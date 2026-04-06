' Test: write a string to file, read it back
open "t01.tmp" for output as #1
print #1, "hello world"
close #1
open "t01.tmp" for input as #1
line input #1, s$
close #1
println s$
