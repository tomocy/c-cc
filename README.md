# c-cc

## Production rule

```
program = (func | gvar | tydef)*

func = decl "(" (decl (, decl)*)? ")" bloc_stmt
gvar = decl ("=" const_expr)? ("," declarator ("=" const_expr)?)* ";"
tydef = "typedef" decl ";"

bloc_stmt = "{" stmt* "}"
stmt = func |
    gvar |
    tydef |
    "if" "(" expr ")" stmt ("else" stmt)? |
    "switch" "(" expr ")" stmt |
    "case" ":" const_expr |
    "default" ":" |
    "for" "(" (expr | var_decl)? ";" expr? ";" expr? ";" ")" stmt |
    "while" "(" expr ")" stmt |
    "do" stmt "while" "(" expr ")" ";" |
    "return" expr? ";" |
    ident ":" stmt |
    "goto" ident ";" |
    "break" ";" |
    "continue" ";" |
    lvar |
    expr_stmt
expr_stmt = expr? ";"
expr = assign ("," expr)?
assign = conditional (("=" | "|=" | "^=" | "&=" | "<<=" | ">>=" | "+=" | "-=" | "*=" | "/=" | "%=") assign)*
conditional = orr ("?" expr ":" conditional)?
orr = andd ("||" addd)*
andd = bitorr ("&&" bitorr)*
bitorr = bitxorr ("|" bitxorr)*
bitxorr = bitandd ("^" bitandd)*
bitandd = equality ("&" equality)*
equality = realtional ("==" relational | "!=" relational)*
relational = shift ("<" shift | "<=" shift | ">" shift | ">=" shift)*
shift = add ("<<" add | ">>" add)*
add = mul ("+" mul | "-" mul)*
mul = cast ("*" cast | "/" cast | "%" cast)*
cast = "(" abstract_declarator ")" cast | unary
unary = ("+" | "-" | "&" | "*" | "!" | "~") cast |
    ("++" | "--") unary |
    "sizeof" cast |
    "sizeof" "(" abstract_declarator ")" |
    "_Alignof" "(" type-name ")" |
    postifx
postfix = primary ("[" expr "]" | "." ident | "->" ident | "++" | "--")*
compound_literal = "(" abstract_decl ")" initer
primary = "(" "{" stmt+ "}" ")" |
    "(" expr ")" |
    ident func_args? |
    num |
    str
num = ("0x" | "0X") hexadecimal | decimal | "0" octal | ("0b | "0B") binary
const_expr

lvar = var_decl ";"

initer = string_initer |
    struct_initer | direct_struct_initer |
    union_initer |
    array_initer | direct_array_initer |
    "{" assign "}" |
    assign
string_initer = str
struct_initer = "{" initer ("," initer)* ","? "}"
direct_struct_initer = initer ("," initer)*
union_initer = "{" initer ","? "}"
array_initer = "{" initer ("," initer)* ","? "}"
direct_array_initer = initer ("," initer)*
lvar_decl = decl_specifier (declarator ("=" assign)? ("," declarator ("=" initer)?)*)?

enum_specifier = "enum" ident? "{" ident ("=" const_expr)? ("," ident ("=" const_expr)?)* ","? "}"

struct_decl = "struct" ident? "{" member* "}"
union_decl = "union" ident? "{" member* "}"
member = decl_specifier (declarator ("," declarator)*)? ";"

decl_specifier = (
    "void" | "_Bool" | "char" |
    "short" | "int" | "long" |
    struct_decl | union_decl | defined_type |
    "extern" | "static" | "_Alignas"
)*
declarator = "*"* ("(" declarator ")" | ident) type_suffix
abstract_declarator = "*"* "(" abstract_declarator ")" type_suffix
decl = decl_specifier declarator
abstract_decl = decl_specifier abstract_declarator
type_suffix = array_dimensions | ε
array_dimensions = "[" const_expr? "]" type_suffix

func_args = "(" (assign (, assign)*)? ")"
```

## Build and Run docker image

```
make build-docker-container
make run-docker-container
```
