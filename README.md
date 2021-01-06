# c-cc

## Production rule
```
program = (func | gvar | tydef)*

func = decl_specifier ident "(" (decl_specifier declarator (, decl_specifier declarator)*)? ")" bloc_stmt
gvar = decl_specifier (declarator ("=" num)? ("," declarator ("=" num)?)*)? ";"
tydef = "typedef" decl_specifier declarator ";"

bloc_stmt = "{" stmt* "}"
stmt = "if" "(" expr ")" stmt ("else" stmt)? |
    "for" "(" (expr | var_decl)? ";" expr? ";" expr? ";" ")" stmt |
    "while" "(" expr ")" stmt |
    "return" expr ";" |
    ident ":" stmt |
    "goto" ident ";" |
    lvar |
    expr ";"
expr = assign ("," expr)?
assign = equality (("=" | "|=" | "^=" | "&=" | "+=" | "-=" | "*=" | "/=" | "%=") bitorr)*
orr = andd ("||" addd)*
andd = bitorr ("&&" bitorr)*
bitorr = bitxorr ("|" bitxorr)*
bitxorr = bitandd ("^" bitandd)*
bitandd = equality ("&" equality)*
equality = realtional ("==" relational | "!=" relational)*
relational = add ("<" add | "<=" add | ">" add | ">=" add)*
add = mul ("+" mul | "-" mul)*
mul = cast ("*" cast | "/" cast | "%" cast)*
cast = "(" abstract_declarator ")" cast | unary
unary = ("+" | "-" | "&" | "*" | "!" | "~") cast |
    ("++" | "--") unary |
    "sizeof" cast |
    "sizeof" "(" abstract_declarator ")" |
    postifx
postfix = primary ("[" expr "]" | "." ident | "->" ident | "++" | "--")*
primary = "(" "{" stmt+ "}" ")" | "(" expr ")" | ident func_args? | num | str
num = ("0x" | "0X") hexadecimal | decimal | "0" octal | ("0b | "0B") binary

lvar = var_decl ";"

var_decl = decl_specifier (declarator ("=" assign)? ("," declarator ("=" assign)?)*)?

enum_specifier = "enum" ident? "{" ident ("=" num)? ("," ident ("=" num)?)* "}"

struct_decl = "struct" ident? "{" member* "}"
union_decl = "union" ident? "{" member* "}"
member = decl_specifier (declarator ("," declarator)*)? ";"

decl_specifier = (
    "void" | "_Bool" | "char" |
    "short" | "int" | "long" |
    struct_decl | union_decl | defined_type
)*
declarator = "*"* ("(" declarator ")" | ident) type_suffix
abstract_declarator = "*"* "(" abstract_declarator ")" type_suffix
type_suffix = array_dimensions | ε
array_dimensions = "[" num? "]" type_suffix

func_args = "(" (assign (, assign)*)? ")"
```

## Build and Run docker image
```
make build-docker-container
make run-docker-container
```