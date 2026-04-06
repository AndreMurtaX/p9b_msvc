' Test: FOR STEP 0 is caught as a runtime error; handler can end cleanly
on error goto handler:
for i = 1 to 5 step 0
  println i
next i
println "loop completed"
goto done:

handler:
  println "caught: step=0"
  end

done:
println "done"
