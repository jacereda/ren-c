(
    foo: func [x [integer! <...>]] [
        sum: 0
        while-not [tail? x] [
            sum: sum + take x
        ]
    ]
    y: (z: foo 1 2 3 | 4 5)
    all [y = 5 | z = 6]
)
(
    foo: func [x [integer! <...>]] [make block! x]
    [1 2 3 4] = foo 1 2 3 4
)

(
    ;-- leaked VARARGS! cannot be accessed after call is over
    error? trap [take eval (foo: func [x [integer! <...>]] [x])]
)

(
    f: func [args [any-value! <opt> <...>]] [
       b: take args
       either tail? args [b] ["not at end"]
    ]
    x: make varargs! [_]
    blank? apply :f [args: x]
)

(
    f: func [:look [<...>]] [to-value first look]
    blank? apply 'f [look: make varargs! []]
)

; !!! Experimental behavior of enfixed variadics, is to act as either 0 or 1
; items.  0 is parallel to <end>, and 1 is parallel to a single parameter.
; It's a little wonky because the evaluation of the parameter happens *before*
; the TAKE is called, but theorized that's still more useful than erroring.
[
    (
        normal: enfix function [v [integer! <...>]] [
            sum: 0
            while [not tail? v] [
                sum: sum + take v
            ]
            return sum + 1
        ]
        true
    )

    (1 = do [normal])
    (11 = do [10 normal])
    (21 = do [10 20 normal])
    (31 = do [x: 30 | y: 'x | 1 2 x normal])
    (30 = do [multiply 3 9 normal]) ;-- seen as ((multiply 3 (9 normal))
][
    (
        defers: enfix function [v [<defer> integer! <...>]] [
            sum: 0
            while [not tail? v] [
                sum: sum + take v
            ]
            return sum + 1
        ]
        true
    )

    (1 = do [defers])
    (11 = do [10 defers])
    (21 = do [10 20 defers])
    (31 = do [x: 30 | y: 'x | 1 2 x defers])
    (28 = do [multiply 3 9 defers]) ;-- seen as (multiply 3 9) defers))
][
    (
        soft: enfix function ['v [any-value! <...>]] [
            stuff: copy []
            while [not tail? v] [
                append/only stuff take v
            ]
            return stuff
        ]
        true
    )

    ([] = do [soft])
    ([a] = do [a soft])
    ([7] = do [(1 + 2) (3 + 4) soft])
][
    (
        hard: enfix function [:v [any-value! <...>]] [
            stuff: copy []
            while [not tail? v] [
                append/only stuff take v
            ]
            return stuff
        ]
        true
    )

    ([] = do [hard])
    ([a] = do [a hard])
    ([(3 + 4)] = do [(1 + 2) (3 + 4) hard])
]


; Testing the variadic behavior of |> and <| is easier than rewriting tests
; here to do the same thing.

; <| and |> were originally enfix, so the following tests would have meant x
; would be unset
(
    unset 'value
    unset 'x

    3 = (value: 1 + 2 <| 30 + 40 x: value  () ())

    did all [value = 3 | x = 3]
)
(
    unset 'value
    unset 'x

    70 = (value: 1 + 2 |> 30 + 40 x: value () () ())

    did all [value = 3 | x = 3]
)

(
    is-barrier?: func [x [<end> integer!]] [unset? 'x]
    is-barrier? (<| 10)
)
(
    void? (10 |>)
)

(
    2 = (1 |> 2 | 3 + 4 | 5 + 6)
)
(
    1 = (1 <| 2 | 3 + 4 | 5 + 6)
)

; WATERSHED TEST: This involves the parity of variadics with normal actions,
; showing that simply taking arguments in order gives compatible results.
;
; https://github.com/metaeducation/ren-c/issues/912

(
    vblock: collect [
        log: adapt 'keep [value: reduce value]
        variadic2: func [v [any-value! <...>]] [
           log [<1> take v]
           log [<2> take v]
           if not tail? v [fail "THEN SHOULD APPEAR AS IF IT IS VARARGS END"]
           return "returned"
       ]
       result: variadic2 "a" "b" then (t => [log [<t> t] "then"])
       log [<result> result]
    ]

    nblock: collect [
        log: adapt 'keep [value: reduce value]
        normal2: func [n1 n2] [
            log [<1> n1 <2> n2]
            return "returned"
        ]
        result: normal2 "a" "b" then (t => [log [<t> t] "then"])
        log [<result> result]
    ]

    vblock == nblock
)
