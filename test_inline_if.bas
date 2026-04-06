' Testa todas as formas de IF inline

let test = 10

' 1) IF THEN com ELSE inline — condição falsa → deve executar ELSE (a = 20)
if test < 10 then let a = 10 else let a = 20
println "Test resulta = "; a    ' esperado: 20

' 2) IF THEN sem ELSE — condição verdadeira
if test = 10 then let b = 99
println "b = "; b               ' esperado: 99

' 3) IF THEN sem ELSE — condição falsa (b não muda)
if test > 100 then let b = 0
println "b ainda = "; b         ' esperado: 99

' 4) IF THEN ELSE — condição verdadeira → THEN (c = 1)
if test = 10 then let c = 1 else let c = 2
println "c = "; c               ' esperado: 1

' 5) IF THEN ELSE IF inline (cadeia)
if test < 5 then println "pequeno" else if test < 15 then println "medio" else println "grande"
' esperado: medio

println "And that's it."
end
