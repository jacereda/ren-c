REBOL [
    System: "REBOL [R3] Language Interpreter and Run-time Environment"
    Title: "REBOL 3 Mezzanine: File Related"
    Rights: {
        Copyright 2012 REBOL Technologies
        REBOL is a trademark of REBOL Technologies
    }
    License: {
        Licensed under the Apache License, Version 2.0
        See: http://www.apache.org/licenses/LICENSE-2.0
    }
]

decode-url: :sys.util.decode-url


clean-path: func [
    {Returns new directory path with `.` and `..` processed.}

    return: [file! url! text!]
    path [file! url! text! tag! the-word!]
    /only "Do not prepend current directory"
    /dir "Add a trailing / if missing"
][
    ; TAG! is a shorthand for getting files relative to the path of the
    ; currently running script.
    ;
    ; !!! This has strange interactions if you have a function that gets
    ; called back after a script has finished, and it still wants to
    ; fetch resources relative to its original location.  These issues are
    ; parallel to that of using the current working directory, so one
    ; should be cautious.
    ;
    if tag? path [
        if #"/" = first path [
            fail ["TAG! import from SYSTEM.SCRIPT.PATH not relative:" path]
        ]
        if #"%" = first path [
            fail ["Likely mistake, % in TAG!-style import path:" path]
        ]
        if not find path "." [  ; !!! for compatibility, treat as index lookup
            path: to the-word! path
        ]
        else [
            path: join system.script.path (as text! path)
        ]
    ]

    ; This translates `@tool` into a URL!.  The list is itself loaded from
    ; the internet, URL is in `system.locale.library.utilities`.
    ;
    ; !!! Note the above compatibility hack that turns <foo> into @foo
    ;
    ; !!! As the project matures, this would have to come from a curated
    ; list, not just links on individuals' websites.  There should also be
    ; some kind of local caching facility.
    ;
    if the-word? path [
        path: switch as tag! path  ; !!! list actually used tags, should change
            (load system.locale.library.utilities)
        else [
            fail [{Module} path {not in system.locale.library.utilities}]
        ]
    ]

    let scheme: null

    let target
    case [
        url? path [
            scheme: decode-url path
            target: either scheme.path [
                to file! scheme.path
            ][
                copy %/
            ]
        ]

        any [
            only
            text? path
            #"/" = first path
        ][
            target: copy path
        ]

        file? path [
            if url? let current: what-dir [
                scheme: decode-url current
                current: any [
                    scheme.path
                    copy %/
                ]
            ]

            target: to file! unspaced [maybe current, path]  ; !!! why MAYBE?
        ]
    ]

    if all [dir, #"/" <> last target] [
        append target #"/"
    ]

    path: make kind of target length of target

    let count: 0
    let part
    parse3 reverse target [
        try some [not <end> [
            "../"
            (count: me + 1)
            |
            "./"
            |
            "/"
            (
                if any [
                    not file? target
                    #"/" <> last path
                ][
                    append path #"/"
                ]
            )
            |
            copy part: [to "/" | to <end>] (
                either count > 0 [
                    count: me - 1
                ][
                    if not find ["" "." ".."] as text! part [
                        append path part
                    ]
                ]
            )
        ]]
    ]

    if all [
        #"/" = last path
        #"/" <> last target
    ][
        remove back tail of path
    ]

    reverse path

    if not scheme [
        return path
    ]

    return to url! head insert path unspaced [
        form scheme.scheme "://"
        if scheme.user [
            unspaced [
                scheme.user
                if scheme.pass [
                    unspaced [":" scheme.pass]
                ]
                "@"
            ]
        ]
        scheme.host
        if scheme.port-id [
            unspaced [":" scheme.port-id]
        ]
    ]
]


ask: function [
    {Ask the user for input}

    return: "Null if the input was aborted (via ESCAPE, Ctrl-D, etc.)"
        [<opt> any-value!]
    question "Prompt to user, datatype to request, or dialect block"
        [block! text! type-word!]
    /hide "mask input with * (Rebol2 feature, not yet implemented)"
    ; !!! What about /MULTILINE ?
][
    if hide [
        fail [
            "ASK/HIDE not yet implemented:"
            https://github.com/rebol/rebol-issues/issues/476
        ]
    ]

    ; This is a limited implementation just to get the ball rolling; could
    ; do much more: https://forum.rebol.info/t/1124
    ;
    prompt: null
    type: text!
    switch/type question [
        text! [prompt: question]  ; `ask "Input:"` doesn't filter type
        type-word! [type: question]  ; `ask text!` has no prompt (like INPUT)
        block! [
            parse question [
                try prompt: text!
                try word: word! (type: ensure type-word! get word)
            ] except [
                fail "ASK currently only supports [{Prompt:} type-word!]"
            ]
        ]
        fail
    ]

    ; !!! Reading a single character is not something possible in buffered line
    ; I/O...you have to either be piping from a file or have a smart console.
    ; The PORT! model in R3-Alpha was less than half-baked, but this READ-CHAR
    ; has been added to try and help some piped I/O scenarios work (e.g. the
    ; Whitespace interpreter test scripts.)
    ;
    if type = issue! [return read-char]

    ; Loop indefinitely so long as the input can't be converted to the type
    ; requested (and there's no cancellation).  Print prompt each time.  Note
    ; that if TEXT! is requested, conversion cannot fail.
    ;
    cycle [
        if prompt [
            write-stdout prompt
            write-stdout space  ; space after prompt is implicit
        ]

        line: read-line else [
            ;
            ; NULL signals "end of file".  At present this only applies to
            ; redirected input--as there's no limit to how much you can type
            ; in the terminal.  But it might be useful to have a key sequence
            ; that will simulate end of file up until the current code finishes
            ; so you can test code interactively that expects to operate on
            ; files where the end would be reached.
            ;
            return null
        ]

        if '~escape~ = line [  ; escape key pressed.
            return null
        ]

        ; The original ASK would TRIM the output, so no leading or trailing
        ; space.  This assumes that is the caller's responsibility.
        ;
        if type = text! [return line]

        ; If not asking for text, currently we assume empty lines mean you
        ; want to ask again.  (This is questionable...should `ask tag!` allow
        ; you to give an empty string and return an empty tag?)
        ;
        if empty? line [continue]

        return (to type line except e -> [
            ;
            ; !!! The error trapped during the conversion may contain more
            ; useful information than just saying "** Invalid input".  But
            ; there's no API for a "light" printing of errors.  Scrub out all
            ; the extra information from the error so it isn't as verbose.
            ;
            e.file: null
            e.line: null
            e.where: null
            e.near: null
            print [e]

            continue  ; Keep cycling, bypasses the RETURN (...)
        ])
    ]
]


confirm: function [
    {Confirms a user choice}

    return: [logic?]
    question "Prompt to user"
        [any-series!]
    /with [text! block!]
][
    with: default [["y" "yes"] ["n" "no"]]

    all [
        block? with
        length of with > 2

        fail 'with [
            "maximum 2 arguments allowed for with [true false]"
            "got:" mold with
        ]
    ]

    response: ask question

    return case [
        empty? with [true]
        text? with [did find/match response with]
        length of with < 2 [did find/match response first with]
        find first with response [true]
        find second with response [false]
    ]
]


list-dir: function [
    "Print contents of a directory (ls)."

    return: <none>  ; don't want console to print evaluative result
    'path [<end> file! word! path! text!]
        "Accepts %file, :variables, and just words (as dirs)"
    /l "Line of info format"
    /f "Files only"
    /d "Dirs only"
;   /t "Time order"
    /r "Recursive"
    /i "Indent"
        [any-value!]
][
    i: default [""]

    save-dir: what-dir

    if not file? save-dir [
        fail ["No directory listing protocol registered for" save-dir]
    ]

    switch/type :path [
        null! []  ; Stay here
        file! [change-dir path]
        text! [change-dir local-to-file path]
        word! path! [change-dir to-file path]
    ]

    if r [l: true]
    if not l [l: make text! 62] ; approx width

    files: attempt [read %./] else [
        print ["Not found:" :path]
        change-dir save-dir
        return none
    ]

    for-each file files [
        any [
            all [f, dir? file]
            all [d, not dir? file]
        ] then [
            continue
        ]

        if text? l [
            append l file
            append/dup l #" " 15 - remainder length of l 15
            if greater? length of l 60 [print l clear l]
        ] else [
            info: get (words of query file)
            change info split-path info/1
            printf [i 16 -8 #" " 24 #" " 6] info
            if all [r, dir? file] [
                list-dir/l/r/i :file join i "    "
            ]
        ]
    ]

    all [text? l, not empty? l] then [print l]

    change-dir save-dir
]


undirize: function [
    {Returns a copy of the path with any trailing "/" removed.}

    return: [file! text! url!]
    path [file! text! url!]
][
    path: copy path
    if #"/" = last path [clear back tail of path]
    return path
]


in-dir: function [
    "Evaluate a block in a directory, and restore current directory when done"
    return: [<opt> any-value!]
    dir [file!]
        "Directory to change to (changed back after)"
    block [block!]
        "Block to evaluate"
][
    old-dir: what-dir
    change-dir dir

    ; You don't want the block to be done if the change-dir fails, for safety.

    return (
        do block  ; return result
        elide change-dir old-dir
    )
]


to-relative-file: function [
    {Returns relative portion of a file if in subdirectory, original if not.}

    return: [file! text!]
    file "File to check (local if text!)"
        [file! text!]
    /no-copy "Don't copy, just reference"
    /as-rebol "Convert to Rebol-style filename if not"
    /as-local "Convert to local-style filename if not"
][
    if text? file [ ; Local file
        comment [
            ; file-to-local drops trailing / in R2, not in R3
            if [@ tmp]: find/match file file-to-local what-dir [
                file: next tmp
            ]
        ]
        if [@ pos]: find/match file (file-to-local what-dir) [
            file: pos  ; !!! https://forum.rebol.info/t/1582/6
        ]
        if as-rebol [
            file: local-to-file file
            no-copy: true
        ]
    ] else [
        if [@ pos]: find/match file what-dir [
            file: pos  ; !!! https://forum.rebol.info/t/1582/6
        ]
        if as-local [
            file: file-to-local file
            no-copy: true
        ]
    ]

    return either no-copy [file] [copy file]
]


; !!! Probably should not be in the "core" mezzanine.  But to make it easier
; for people who seem to be unable to let go of the tabbing/CR past, this
; helps them turn their files into sane ones :-/
;
; http://www.rebol.com/r3/docs/concepts/scripts-style.html#section-4
;
detab-file: function [
    "detabs a disk file"

    return: <none>
    filename [file!]
][
    write filename detab to text! read filename
]

; temporary location
set-net: function [
    {sets the system/user/identity email smtp pop3 esmtp-usr esmtp-pass fqdn}

    return: <none>
    bl [block!]
][
    if 6 <> length of bl [fail "Needs all 6 parameters for set-net"]
    set (words of system/user/identity) bl
]
