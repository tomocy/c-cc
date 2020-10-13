# c-cc

## Production rule
```
program = func-decl*
func-decl = ident ident "(" (ident (, ident)*)? ")" bloc-stmt
bloc-stmt = "{" stmt* "}"
stmt = expr ";" | 
    "if" "(" expr ")" stmt ("else" stmt)? | 
    "while" "(" expr ")" stmt | 
    "for" "(" expr? ";" expr? ";" expr? ";" ")" stmt |
    "return" expr ";"
expr = assign
assign = equality ("=" equality)*
equality = realtional ("==" relational | "!=" relational)*
relational = add ("<" add | "<=" add | ">" add | ">=" add)*
add = mul ("+" mul | "-" mul)*
mul = unary ("*" unary | "/" unary)*
unary = ("+" | "-")? primary
primary = num | ident func-args? | "(" expr ")"
func-args = "(" (expr (, expr)*)? ")"
```