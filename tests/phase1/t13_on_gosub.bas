' Test: ON expr GOSUB
result$ = ""
for x = 1 to 3
  on x gosub sub1:, sub2:, sub3:
next x
println result$
end
sub1:
  result$ = result$ + "A"
return
sub2:
  result$ = result$ + "B"
return
sub3:
  result$ = result$ + "C"
return
