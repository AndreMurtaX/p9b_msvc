' Test: LINE INPUT reads whole line including commas
open "t06.tmp" for output as #1
print #1, "hello, world, 42"
close #1
open "t06.tmp" for input as #1
line input #1, s$
close #1
println s$
