; functions/context/set.r
[#1763
    (
        a: <before>
        '_ = [a]: unpack inert reduce/predicate [null] :reify
        '_ = a
    )
]
(
    a: <a-before>
    b: <b-before>
    2 = [a b]: unpack inert reduce/predicate [2 null] :reify
    a = 2
    '_ = b
)
(
    x: make object! [a: 1]
    all [
        error? sys.util.rescue [set x reduce [()]]
        x.a = 1
    ]
)
(
    x: make object! [a: 1 b: 2]
    all [
        error? sys.util.rescue [set x reduce [3 ()]]
        x.a = 1
    ]
)

; set [:get-word] [word]
(a: 1 b: _ [b]: unpack inert [a] b = 'a)

(
    a: 10
    b: 20
    did all [blank = [a b]: unpack @[_ _], blank? a, blank? b]
)
(
    a: 10
    b: 20
    c: 30
    [a b c]: unpack [_ 99]
    did all [null? a, b = 99, ^c = '~]
)

; #1745  !!! UNPACK supports narrower dialecting, review potential meanings
;(
;    [1 2 3 4 5 6] = set [a 'b :c d: /e ^f] [1 2 3 4 5 6]
;)

(10 = set void 10)
(null = get void)
