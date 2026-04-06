' Test 21: explicit RETURN inside function
function max(a, b)
  if a > b then
    return a
  end if
  return b
end function

println max(3, 7)
println max(10, 4)
