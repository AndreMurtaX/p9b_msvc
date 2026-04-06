' Test: RESUME NEXT skips past the failing statement
on error goto handler:
x = 1 / 0
println "after"
goto done:

handler:
  resume next

done:
println "done"
