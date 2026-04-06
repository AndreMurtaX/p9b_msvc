' Test: 3D array [p, r, c] access using compile-time row and column counts
dim cube[2, 3, 4]

' Fill with p*100 + r*10 + c
for p = 1 to 2
  for r = 1 to 3
    for c = 1 to 4
      cube[p, r, c] = p * 100 + r * 10 + c
    next c
  next r
next p

' Spot checks
println cube[1, 1, 1]   ' 111
println cube[1, 2, 3]   ' 123
println cube[1, 3, 4]   ' 134
println cube[2, 1, 1]   ' 211
println cube[2, 3, 4]   ' 234

' Verify boundary: last element = plane 2, row 3, col 4
' flat index = ((2-1)*3 + (3-1))*4 + 4 = (3+2)*4+4 = 24  (= total size)
println cube[2, 3, 4]   ' 234

' String 3D array
dim tags$[2, 2, 2]
tags$[1, 1, 1] = "AAA"
tags$[1, 1, 2] = "AAB"
tags$[1, 2, 1] = "ABA"
tags$[2, 2, 2] = "BBB"
println tags$[1, 1, 1]  ' AAA
println tags$[2, 2, 2]  ' BBB
