' Test 22: GOSUB / RETURN (regression)
x = 0
gosub counter:
gosub counter:
gosub counter:
println x
end

counter:
  x = x + 1
return
