' Test: INPUT$(n) reads exactly n characters from stdin
' (feed via pipe: echo "hello" | p9b t02...)
s$ = input$(5)
println s$
