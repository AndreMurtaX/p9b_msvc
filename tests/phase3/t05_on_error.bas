' Test: ON ERROR GOTO catches runtime error
on error goto handler:
x = 10 / 0
println "after error"
goto done:

handler:
  println "caught error " + str$(err)
  resume next

done:
println "done"
