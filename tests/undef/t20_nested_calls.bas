' Test 20: nested function calls + global modification from function
total = 0

function addToTotal(n)
  total = total + n   ' modifies global
  addToTotal = total
end function

println addToTotal(10)
println addToTotal(20)
println addToTotal(30)
