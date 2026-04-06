' Test: READ type mismatch triggers runtime error (ERR=13)
on error goto handler:
data "hello"
read x
goto done:
handler:
  println "mismatch err=" + str$(err)
  resume next
done:
println "done"
