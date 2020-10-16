# c-cc

## Production rule
```
program = func_def*
func_def = type ident "(" (type ident (, "int" ident)*)? ")" bloc_stmt
bloc_stmt = "{" stmt* "}"
stmt = expr ";" | 
    "if" "(" expr ")" stmt ("else" stmt)? | 
    "for" "(" expr? ";" expr? ";" expr? ";" ")" stmt |
    "while" "(" expr ")" stmt | 
    "return" expr ";" |
    type ident ("[" num "]")? ";"
expr = assign
assign = equality ("=" equality)*
equality = realtional ("==" relational | "!=" relational)*
relational = add ("<" add | "<=" add | ">" add | ">=" add)*
add = mul ("+" mul | "-" mul)*
mul = unary ("*" unary | "/" unary)*
unary = ("+" | "-")? primary | ("&" | "*") unary | "sizeof" unary | postfix
postfix = primary ("[" expr "]")?
primary = num | ident func_args? | "(" expr ")"
func_args = "(" (expr (, expr)*)? ")"
type = "int" "*"*
```