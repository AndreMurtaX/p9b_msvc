' Test 19: string function returning by name assignment
function repeat$(s$, n)
  result$ = ""
  for i = 1 to n
    result$ = result$ + s$
  next i
  repeat$ = result$
end function

println repeat$("ab", 3)
println repeat$("x", 5)
