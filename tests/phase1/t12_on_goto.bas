' Test: ON expr GOTO
for x = 1 to 4
  on x goto case1:, case2:, case3:, case4:
  goto done:
  case1:
    print "A ";
    goto done:
  case2:
    print "B ";
    goto done:
  case3:
    print "C ";
    goto done:
  case4:
    print "D ";
  done:
next x
println ""
