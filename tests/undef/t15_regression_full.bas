' Test 15: regression - complex program still works
dim names$[5]
dim scores[5]

for i = 1 to 5
  names$[i] = "Player" + str$(i)
  scores[i] = i * 10
next i

total = 0
for i = 1 to 5
  println names$[i] + ": " + str$(scores[i])
  total = total + scores[i]
next i
println "Total: " + str$(total)
