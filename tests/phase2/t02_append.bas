' Test: append to an existing file
open "t02.tmp" for output as #1
print #1, "line1"
close #1
open "t02.tmp" for append as #1
print #1, "line2"
close #1
open "t02.tmp" for input as #1
line input #1, a$
line input #1, b$
close #1
println a$
println b$
