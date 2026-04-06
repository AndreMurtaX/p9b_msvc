' Test: CASE val TO val (range)
for x = 1 to 10
  select case x
    case 1 to 3
      print "low ";
    case 4 to 6
      print "mid ";
    case 7 to 10
      print "high ";
  end select
next x
println ""
