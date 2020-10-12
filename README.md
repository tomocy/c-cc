# c-cc

## Production rule
```
expr = equality
equality = add ("==" add | "!=" add)*
add = mul ("+" mul | "-" mul)*
mul = unary ("*" unary | "/" unary)*
unary = ("+" | "-")? primary
primary = num | "(" expr ")"
```