; %expression.parse.test.reb
;
; Expression parser (found in %parse-test.red)

[
    (
        expr: [term ["+" | "-"] expr | term]
        term: [factor ["*" | "/"] term | factor]
        factor: [primary "**" factor | primary]
        primary: [some digit | "(" expr ")"]
        digit: charset "0123456789"
        true
    )

    (raised? parse "123a+2" expr)
    (raised? parse "a+b" expr)
    (")" == parse "4/5+3**2-(5*6+1)" expr)
    (#4 = parse "1+2*(3-2)/4" expr)
    (raised? parse "(+)" expr)
    (")" == parse "(1*9)" expr)
    (")" == parse "(1)" expr)
    (#4 == parse "1+2*(3-2)/4" expr)
    (raised? parse "1+2*(3-2)/" expr)
    (")" == parse "1+2*(3-2)" expr)
    (raised? parse "1+2*(3-2" expr)
    (raised? parse "1+2*(3-" expr)
    (raised? parse "1+2*(3" expr)
    (raised? parse "1+2*(" expr)
    (raised? parse "1+2*" expr)
    (#2 == parse "1+2" expr)
    (raised? parse "1+" expr)
    (#1 == parse "1" expr)
]
