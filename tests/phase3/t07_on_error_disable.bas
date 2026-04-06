' Test: ON ERROR GOTO 0 disables error handler (error propagates normally)
on error goto handler:
on error goto 0
println "no handler active"
goto done:

handler:
  println "should not reach here"

done:
