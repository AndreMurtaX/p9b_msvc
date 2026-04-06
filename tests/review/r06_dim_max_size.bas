' Test: DIM with size > 10 000 000 is caught as runtime error
on error goto handler:
dim arr[20000000]
goto done:
handler:
  println "caught dim error"
  resume next
done:
println "done"
