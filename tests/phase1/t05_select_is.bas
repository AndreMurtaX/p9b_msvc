' Test: CASE IS relop
for x = 1 to 5
  select case x
    case is < 2
      print "small ";
    case is >= 4
      print "large ";
    case else
      print "medium ";
  end select
next x
println ""
