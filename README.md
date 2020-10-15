# c-cc

## Production rule
```
program = func-def*
func-def = "int" ident "(" ("int" ident (, "int" ident)*)? ")" bloc-stmt
bloc-stmt = "{" stmt* "}"
stmt = expr ";" | 
    "if" "(" expr ")" stmt ("else" stmt)? | 
    "for" "(" expr? ";" expr? ";" expr? ";" ")" stmt |
    "while" "(" expr ")" stmt | 
    "return" expr ";" |
    "int" ident ";"
expr = assign
assign = equality ("=" equality)*
equality = realtional ("==" relational | "!=" relational)*
relational = add ("<" add | "<=" add | ">" add | ">=" add)*
add = mul ("+" mul | "-" mul)*
mul = unary ("*" unary | "/" unary)*
unary = ("+" | "-")? primary | ("&" | "*") unary
primary = num | ident func-args? | "(" expr ")"
func-args = "(" (expr (, expr)*)? ")"
```