# c-cc

## Production rule
```
program = (func | gvar)*

func = decl_specifier ident "(" (decl_specifier declarator (, decl_specifier declarator)*)? ")" bloc_stmt
gvar = decl_specifier (declarator ("=" num)? ("," declarator ("=" num)?)*)? ";"

bloc_stmt = "{" stmt* "}"
stmt = "if" "(" expr ")" stmt ("else" stmt)? | 
    "for" "(" expr? ";" expr? ";" expr? ";" ")" stmt |
    "while" "(" expr ")" stmt | 
    "return" expr ";" |
    lvar |
    expr ";"
expr = assign ("," expr)?
assign = equality ("=" equality)*
equality = realtional ("==" relational | "!=" relational)*
relational = add ("<" add | "<=" add | ">" add | ">=" add)*
add = mul ("+" mul | "-" mul)*
mul = unary ("*" unary | "/" unary)*
unary = ("+" | "-")? primary | ("&" | "*") unary | "sizeof" unary | postfix
postfix = primary ("[" expr "]" | "." ident)*
primary = "(" "{" stmt+ "}" ")" | "(" expr ")" | ident func_args? | num | str

lvar = decl_specifier (declarator ("=" assign)? ("," declarator ("=" assign)?)*)? ";"

decl_specifier = "int" | "char" | struct_decl
declarator = "*"* ident ("[" num "]")?

struct_decl = "struct" ident? "{" struct_member* "}"
struct_member = decl_specifier (declarator ("," declarator)*)? ";"

func_args = "(" (assign (, assign)*)? ")"
```

## Build and Run docker image
```
make build-docker-container
make run-docker-container
```