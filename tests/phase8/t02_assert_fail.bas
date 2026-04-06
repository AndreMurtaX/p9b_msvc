on error goto handler:
assert 1 = 2
println "should not reach"
goto done:
handler:
  println "assertion caught"
done:
