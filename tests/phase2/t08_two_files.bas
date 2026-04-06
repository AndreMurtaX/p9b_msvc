' Test: two files open simultaneously
open "t08a.tmp" for output as #1
open "t08b.tmp" for output as #2
print #1, "from1"
print #2, "from2"
close #1
close #2
open "t08a.tmp" for input as #1
open "t08b.tmp" for input as #2
line input #1, a$
line input #2, b$
close #1
close #2
println a$
println b$
