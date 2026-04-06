' Test: SELECT CASE falling to CASE ELSE
x = 99
select case x
  case 1
    println "one"
  case 2
    println "two"
  case else
    println "none matched: " + str$(x)
end select
