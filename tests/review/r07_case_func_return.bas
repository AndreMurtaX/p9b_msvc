' Test: function return variable uses same slot regardless of case
function double(n)
  Double = n * 2    ' uppercase D — must be same var as function name
endfunction
println double(5)   ' should print 10
println double(21)  ' should print 42
