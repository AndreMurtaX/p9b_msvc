' Test: ERL reports line number of error
on error goto handler:
x = 1 / 0
goto done:

handler:
  println "err=" + str$(err)
  println "erl=" + str$(erl)
  resume next

done:
println "ok"
