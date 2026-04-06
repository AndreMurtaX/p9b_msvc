' Test: ON ERROR catches file-not-found error
on error goto handler:
open "nonexistent_xyz_123.tmp" for input as #1
goto done:

handler:
  println "caught file error"
  if err = 53 then println "err ok"
  resume next

done:
println "done"
