' Test: SELECT CASE with numeric value
x = 2
select case x
  case 1
    println "one"
  case 2
    println "two"
  case 3
    println "three"
  case else
    println "other"
end select
