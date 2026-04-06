' Test: bare CLOSE closes all files; #1 can be re-opened
open "t07.tmp" for output as #1
print #1, "first"
close
open "t07.tmp" for output as #1
print #1, "second"
close #1
open "t07.tmp" for input as #1
line input #1, s$
close #1
println s$
