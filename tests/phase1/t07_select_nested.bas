' Test: nested SELECT CASE
for x = 1 to 3
  for y = 1 to 3
    select case x
      case 1
        select case y
          case 1
            print "1,1 ";
          case else
            print "1,? ";
        end select
      case else
        print "?,? ";
    end select
  next y
next x
println ""
