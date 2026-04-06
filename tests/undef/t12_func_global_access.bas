' Test 12: function accessing global variable -> OK (no false positive)
g = 100
function getg()
  getg = g
end function

println getg()
