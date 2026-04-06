' Test: ON out-of-range falls through silently
x = 5
on x goto lbl1:, lbl2:, lbl3:
println "fell through"
end
lbl1: println "1" : end
lbl2: println "2" : end
lbl3: println "3" : end
