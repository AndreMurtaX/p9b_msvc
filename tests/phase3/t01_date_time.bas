' Test: DATE$() and TIME$() return non-empty strings in expected formats
d$ = date$()
t$ = time$()
' Check format: MM-DD-YYYY (10 chars) and HH:MM:SS (8 chars)
println len(d$)
println len(t$)
' Check separators
println mid$(d$, 3, 1)
println mid$(t$, 3, 1)
