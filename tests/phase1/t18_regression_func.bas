' Regression: functions still work after emit() change
function fib(n)
  if n <= 1 then
    fib = n
  else
    fib = fib(n - 1) + fib(n - 2)
  end if
end function
for i = 0 to 9
  print str$(fib(i)) + " ";
next i
println ""
