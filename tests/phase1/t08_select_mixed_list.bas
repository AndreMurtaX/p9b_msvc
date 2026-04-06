' Test: CASE with list and range mixed
for x = 1 to 12
  select case x
    case 1, 3, 5, 7, 9, 11
      print "O";
    case 2, 4, 6, 8, 10, 12
      print "E";
  end select
next x
println ""
