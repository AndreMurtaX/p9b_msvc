' Test 13: LOCAL declaration initializes variable -> OK
function test()
  local x, s$
  x = 5
  s$ = "hi"
  test = x
end function

println test()
