on error goto handler:
assert 0, "custom message"
println "should not reach"
goto done:
handler:
  println "caught assert"
done:
