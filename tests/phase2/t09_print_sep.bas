' Test: PRINT #n with semicolon suppresses extra newlines
open "t09.tmp" for output as #1
print #1, "a"; "b"; "c"
close #1
open "t09.tmp" for input as #1
line input #1, s$
close #1
println s$
