' Test: SELECT CASE with no match and no ELSE — just falls through
x = 42
select case x
  case 1
    println "one"
  case 2
    println "two"
end select
println "after select"
