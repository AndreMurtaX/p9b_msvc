' Test: EOF() detection loop
open "t03.tmp" for output as #1
print #1, "10"
print #1, "20"
print #1, "30"
close #1
open "t03.tmp" for input as #1
while eof(1) = 0
  input #1, n
  println n
wend
close #1
println "done"
