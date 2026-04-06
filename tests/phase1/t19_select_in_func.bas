' Test: SELECT CASE inside a function
function grade$(score)
  select case score
    case is >= 90
      grade$ = "A"
    case is >= 80
      grade$ = "B"
    case is >= 70
      grade$ = "C"
    case is >= 60
      grade$ = "D"
    case else
      grade$ = "F"
  end select
end function
println grade$(95)
println grade$(83)
println grade$(71)
println grade$(62)
println grade$(45)
