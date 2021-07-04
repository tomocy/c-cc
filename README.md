# c-cc

## Production rule

```
program = (func | gvar | tydef)*

func = decl "(" (decl ("," decl)* ("," "...")?)? ")" bloc_stmt
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
conditional = log_or ("?" expr ":" conditional)?
log_or = log_and ("||" log_and)*
log_and = bit_or ("&&" bit_or)*
bit_or = bit_xor ("|" bit_xor)*
bit_xor = bit_and ("^" bit_and)*
bit_and = equality ("&" equality)*
equality = realtional ("==" relational | "!=" relational)*
relational = shift ("<" shift | "<=" shift | ">" shift | ">=" shift)*
shift = add ("<<" add | ">>" add)*
add = mul ("+" mul | "-" mul)*
mul = cast ("*" cast | "/" cast | "%" cast)*
cast = "(" abstract_declarator ")" cast | unary
unary = ("+" | "-" | "&" | "*" | "!" | "~") cast |
    ("++" | "--") unary |
    "sizeof" "(" abstract_declarator ")" |
    "sizeof" unary |
    "_Alignof" "(" abstract_declarator ")" |
    "_Alignof" unary |
    "__builtin_reg_class" "(" abstract_declarator ")" |
    postifx
postfix = primary (
    func_args |
    "[" expr "]" |
    "." ident |
    "->" ident |
    "++" |
    "--"
)*
compound_literal = "(" abstract_decl ")" initer
primary = "(" "{" stmt+ "}" ")" |
    "(" expr ")" |
    ident |
    num |
    str
num = ("0x" | "0X") hexadecimal | decimal | "0" octal | ("0b | "0B") binary | float
const_expr

lvar = var_decl ";"

func_args = "(" (assign (, assign)*)? ")"

initer = string_initer |
    struct_initer | direct_struct_initer |
    union_initer |
    array_initer | direct_array_initer |
    designated_initer |
    "{" assign "}" |
    assign
designated_initer = (("[" const_expr "]") | ("." ident))+ "="? initer
string_initer = str
struct_initer = "{" initer ("," initer)* ","? "}"
direct_struct_initer = initer ("," initer)*
union_initer = "{" initer ","? "}"
array_initer = "{" initer ("," initer)* ","? "}"
direct_array_initer = initer ("," initer)*
lvar_decl = decl_specifier (declarator ("=" assign)? ("," declarator ("=" initer)?)*)?

struct_decl = "struct" ident? "{" member* "}"
union_decl = "union" ident? "{" member* "}"
member = decl_specifier (declarator ("," declarator)*)? ";"
enum_specifier = "enum" ident? "{" ident ("=" const_expr)? ("," ident ("=" const_expr)?)* ","? "}"
typeof_specifier = "typeof" "(" (decl_specifier | expr) ")"

decl_specifier = (
    "void" | "_Bool" | "char" |
    "short" | "int" | "long" |
    "float" | "double" |
    struct_decl | union_decl |
    enum_specifier | typeof_specifier |
    defined_type |
    "extern" | "static" | "_Alignas" | "signed" |
    "const" | "volatile" | "auto" | "register" |
    "restrict" | "__restrict" | "__restrict__" | "_Noreturn"
)*
pointers = ("*" ("const" | "volatile" | "restcit" | "__restrict" | "__restrict__")*)*
declarator = pointers ("(" declarator ")" | ident?) type_suffix
abstract_declarator = pointers "(" abstract_declarator ")" type_suffix
decl = decl_specifier declarator
abstract_decl = decl_specifier abstract_declarator
type_suffix = func_params | array_dimensions | ε
func_params = "(" ("void" | decl ("," decl)* ("," "...")?)? ")"
array_dimensions = "[" ("static" | "restrict")* const_expr? "]" type_suffix
```