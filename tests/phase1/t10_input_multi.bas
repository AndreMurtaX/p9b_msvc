' Test: INPUT with multiple variables
' This test uses a simulated stdin via file redirect
' We verify the syntax compiles correctly and reads values
input "Enter a and b: "; a, b
println a + b
