' Test: CASE with comma list
for x = 1 to 7
  select case x
    case 1, 3, 5, 7
      print "odd ";
    case 2, 4, 6
      print "even ";
  end select
next x
println ""
