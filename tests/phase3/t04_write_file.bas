' Test: WRITE #n generates CSV-quoted output
open "t04.tmp" for output as #1
write #1, "hello", 42, "world"
close #1
open "t04.tmp" for input as #1
line input #1, s$
close #1
println s$
