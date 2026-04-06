' Test: SHR uses logical (unsigned) right shift, not arithmetic
' SHR(-1, 1)  → 0x7FFFFFFFFFFFFFFF = 9223372036854775807
' SHR(-2, 1)  → 0x7FFFFFFFFFFFFFFF = 9223372036854775807  (wrong if arithmetic: -1)
' SHR(8, 2)   → 2   (positive, same either way)
' SHR(-8, 2)  → 0x3FFFFFFFFFFFFFFF... but let's use a simpler case:
' SHR(16, 1)  → 8

println SHR(16, 1)      ' 8
println SHR(8, 2)       ' 2
println SHR(0, 5)       ' 0
' Negative: logical shift fills with 0, not sign bit
' -1 in 64-bit = 0xFFFFFFFFFFFFFFFF; >>1 = 0x7FFFFFFFFFFFFFFF = 9223372036854775807
' Stored as double and printed by PRINTLN → 9.22337e+18 (nearest representable)
println SHR(-1, 1)      ' 9.22337e+18
