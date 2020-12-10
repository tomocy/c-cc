# c-cc

## Production rule
```
program = (func | gvar)*

func = decl_specifier ident "(" (decl_specifier declarator (, decl_specifier declarator)*)? ")" bloc_stmt
gvar = decl_specifier (declarator ("=" num)? ("," declarator ("=" num)?)*)? ";"
tydef = "typedef" decl_specifier declarator ";"

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
unary = ("+" | "-")? primary | ("&" | "*") unary | "sizeof" "(" abstract_declarator ")" | "sizeof" unary | postfix
postfix = primary ("[" expr "]" | "." ident | "->" ident)*
primary = "(" "{" stmt+ "}" ")" | "(" expr ")" | ident func_args? | num | str

lvar = decl_specifier (declarator ("=" assign)? ("," declarator ("=" assign)?)*)? ";"

decl_specifier = ("void" | "char" | "short" | "int" | "long" | struct_decl | union_decl | defined_type )*
declarator = "*"* ("(" declarator ")" | ident) type_suffix
abstract_declarator = "*"* "(" abstract_declarator ")" type_suffix

type_suffix = "[" num "]" type_suffix | ε

struct_decl = "struct" ident? "{" member* "}"

union_decl = "union" ident? "{" member* "}"

member = decl_specifier (declarator ("," declarator)*)? ";"

func_args = "(" (assign (, assign)*)? ")"
```

## Build and Run docker image
```
make build-docker-container
make run-docker-container
```