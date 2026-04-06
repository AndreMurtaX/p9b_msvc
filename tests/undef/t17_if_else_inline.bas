' Test 17: inline IF ELSE (regression)
x = 5
if x > 3 then println "big" else println "small"
if x > 10 then println "huge" else if x > 3 then println "medium" else println "tiny"
