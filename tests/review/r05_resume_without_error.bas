' Test: RESUME outside error handler triggers error code 20
on error goto handler:
x = 1 / 0
println "after div0"
goto done:

handler:
  println "in handler"
  resume next

done:
println "done"
