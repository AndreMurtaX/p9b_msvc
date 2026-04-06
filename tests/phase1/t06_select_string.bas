' Test: SELECT CASE with string expression
dim words$[4]
words$[1] = "apple"
words$[2] = "banana"
words$[3] = "cherry"
words$[4] = "durian"
for i = 1 to 4
  select case words$[i]
    case "apple", "cherry"
      println words$[i] + " -> fruit A or C"
    case "banana"
      println words$[i] + " -> banana"
    case else
      println words$[i] + " -> other"
  end select
next i
