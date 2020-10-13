# c-cc

## Production rule
```
program = stmt*
stmt = expr ";" | 
    "if" "(" expr ")" stmt ("else" stmt)? | 
    while" "(" expr ")" stmt | 
    "for" "(" expr? ";" expr? ";" expr? ";" ")" stmt |
    "return" expr ";"
expr = assign
assign = equality ("=" equality)*
equality = realtional ("==" relational | "!=" relational)*
relational = add ("<" add | "<=" add | ">" add | ">=" add)*
add = mul ("+" mul | "-" mul)*
mul = unary ("*" unary | "/" unary)*
unary = ("+" | "-")? primary
primary = num | ident | "(" expr ")"
```