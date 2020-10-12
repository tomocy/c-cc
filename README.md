# c-cc

## Production rule
```
expr = mul ("+" mul | "-" mul)*
mul = unary ("*" unary | "/" unary)*
unary = ("+" | "-")? primary
primary = num | "(" expr ")"
```