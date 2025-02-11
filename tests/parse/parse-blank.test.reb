; %parse-blank.test.reb
;
; BLANK! matches blanks in arrays literally.  This is helpful in particular
; with breaking down paths with empty slots:
;
;    >> refinement-rule: [subparse path! [_ word!]]
;
;    >> parse [/a] [refinement-rule]
;    == a
;
; For strings and binaries, it is a synonym for SPACE.
;
; (At one time it was thought it might be how to say "match any value", as
; underscore is sometimes used in this wildcarding fashion in some languages:
;
;    >> did parse [x <y> "z"] [_ _ _]
;    == ~true~  ; isotope
;
; ...but the <any> tag combinator serves this purpose more literately.)
;

(
    refinement-rule: [subparse path! [_ word!]]
   'a = parse [/a] [refinement-rule]
)

(raised? parse [x] ['x blank])
('_ = parse [x _] ['x _])
([] == parse [x] [try blank 'x <end>])

(raised? parse [] [blank blank blank])

(raised? parse [x <y> "z"] ['_ '_ '_])
(_ == parse [_ _ _] ['_ '_ '_])
(_ == parse [_ _ _] [_ _ _])

[
    (raised? parse "" [_])
    (space = parse " " [_])
    (raised? parse "" [blank])
    (space = parse " " [blank])
]

; !!! Should matching in a binary against blank return 32 or SPACE?
[
    (raised? parse #{} [_])
    (space = parse #{20} [_])
    (raised? parse #{} [blank])
    (space = parse #{20} [blank])
]
