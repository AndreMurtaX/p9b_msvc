' Test: 2D array [r,c] access using compile-time column count
dim m[3, 4]

' Fill with r*10 + c
for r = 1 to 3
  for c = 1 to 4
    m[r, c] = r * 10 + c
  next c
next r

' Read back and verify a few cells
println m[1, 1]    ' 11
println m[1, 4]    ' 14
println m[2, 3]    ' 23
println m[3, 4]    ' 34

' String 2D array
dim words$[2, 3]
words$[1, 1] = "AA"
words$[1, 2] = "AB"
words$[2, 1] = "BA"
words$[2, 3] = "BC"
println words$[1, 1]   ' AA
println words$[2, 3]   ' BC
