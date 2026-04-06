' Test: negative base ^ fractional exponent gives ERR=5
on error goto handler:
x = (-4) ^ 0.5
goto done:

handler:
  println "caught pow error"
  println err = 5
  resume next

done:
println "done"
