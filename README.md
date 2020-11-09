# c-cc

## Production rule
```
program = (func | gvar)*
func = type_head ident "(" (type_head ident (, type_head ident)*)? ")" bloc_stmt
gvar = var ("=" num)? ";"
var = type_head ident type_tail?
type_head = type_name "*"*
type_name = "int" | "char"
type_tail = ("[" num "]")
bloc_stmt = "{" stmt* "}"
stmt = expr ";" | 
    "if" "(" expr ")" stmt ("else" stmt)? | 
    "for" "(" expr? ";" expr? ";" expr? ";" ")" stmt |
    "while" "(" expr ")" stmt | 
    "return" expr ";" |
    var ("=" expr)? ";"
expr = assign ("," expr)?
assign = equality ("=" equality)*
equality = realtional ("==" relational | "!=" relational)*
relational = add ("<" add | "<=" add | ">" add | ">=" add)*
add = mul ("+" mul | "-" mul)*
mul = unary ("*" unary | "/" unary)*
unary = ("+" | "-")? primary | ("&" | "*") unary | "sizeof" unary | postfix
postfix = primary ("[" expr "]")?
primary = "(" "{" stmt+ "}" ")" | "(" expr ")" | ident func_args? | num | str
func_args = "(" (expr (, expr)*)? ")"
```

## Build and Run docker image
```
make build-docker-container
make run-docker-container
```