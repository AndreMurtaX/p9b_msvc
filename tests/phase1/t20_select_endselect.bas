' Test: ENDSELECT (one-token form) works too
x = 2
select case x
  case 1
    println "one"
  case 2
    println "two"
endselect
println "done"
