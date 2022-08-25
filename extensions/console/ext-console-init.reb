REBOL [
    Title: "Console Extension (Rebol's Read-Eval-Print-Loop, ie. REPL)"

    Name: console
    Type: Module

    Rights: {
        Copyright 2016-2021 Ren-C Open Source Contributors
        REBOL is a trademark of REBOL Technologies
    }
    License: {
        Licensed under the Apache License, Version 2.0
        See: http://www.apache.org/licenses/LICENSE-2.0
    }
    Description: {
        This is a rich, skinnable console for Rebol--where basically all the
        implementation is itself userspace Rebol code.  Documentation for the
        skinning hooks exist here:

        https://github.com/r3n/reboldocs/wiki/User-and-Console

        The HOST-CONSOLE Rebol function is invoked in a loop by a small C
        main function (see %main/main.c).  HOST-CONSOLE does not itself run
        arbitrary user code with DO.  That would be risky, because it actually
        is not allowed to fail or be canceled with Ctrl-C.  Instead, it just
        gathers input...and produces a block which is returned to C to
        actually execute.

        This design allows the console to sandbox potentially misbehaving
        skin code, and fall back on a default skin if there is a problem.
        It also makes sure that that user code doesn't see the console's
        implementation in its backtrace.

        !!! While not implemented in C as the R3-Alpha console was, this
        code relies upon the READ-LINE function to communicate with the user.
        READ-LINE is a black box that reads lines from the "console port",
        which is implemented via termios on POSIX, the Win32 Console API
        on Windows, and by JavaScript code in the Web build:

        https://blog.nelhage.com/2009/12/a-brief-introduction-to-termios/
        https://docs.microsoft.com/en-us/windows/console/console-functions

        Someday in the future, the console port itself should offer keystroke
        events and allow the line history (e.g. Cursor Up, Cursor Down) to be
        implemented in Rebol as well.
     }
]


boot-print: redescribe [
    "Prints during boot when not quiet."
](
    ; !!! Duplicates code in %main-startup.reb, where this isn't exported.
    enclose :print f -> [if not system.options.quiet [do f]]
)

loud-print: redescribe [
    "Prints during boot when verbose."
](
    ; !!! Duplicates code in %main-startup.reb, where this isn't exported.
    enclose :print f -> [if system.options.verbose [do f]]
)


; Define base console! behaviors.  Custom console skins derive from this.
;
; If a console skin has an error while running, the error will be trapped and
; the system will revert to using a copy of this base object.
;
; !!! We EXPORT the CONSOLE! object, because the concept was that users should
; be able to create new instances of the console object.  What they'd do with
; it hasn't really been worked through yet (nested REPLs?)  Review.
;
export console!: make object! [
    name: _
    repl: true  ; used to identify this as a console! object
    is-loaded: false  ; if true then this is a loaded (external) skin
    was-updated: false  ; if true then console! object found in loaded skin
    last-result: ~startup~  ; last evaluated result (sent by HOST-CONSOLE)

    === APPEARANCE (can be overridden) ===

    prompt: {>>}
    result: {==}
    warning: {!!}
    error: {**}  ; errors FORM themselves, so this is not used yet
    info: {(i)}  ; was `to-text #{e29398}` for "(i)" symbol, caused problems
    greeting:
{Welcome to Rebol.  For more information please type in the commands below:

  HELP    - For starting information
  ABOUT   - Information about your Rebol}

    print-greeting: meth [
        return: <none>
        {Adds live elements to static greeting content (build #, version)}
    ][
        boot-print [
            "Rebol 3 (Ren-C branch)"
            mold compose [version: (system.version) build: (system.build)]
            newline
        ]

        boot-print greeting
    ]

    print-prompt: meth [return: <none>] [
        ;
        ; Note: See example override in skin in the Debugger extension, which
        ; adds the stack "level" number and "current" function name.

        ; We don't want to use PRINT here because it would put the cursor on
        ; a new line.
        ;
        write-stdout unspaced prompt
        write-stdout space
    ]

    print-result: meth [
        return: <none>
        ^v "Value (done with meta parameter to discern isotope status)"
            [<opt> <void> any-value!]
    ][
        ; We use SET instead of a SET-WORD! here to avoid caching the action
        ; name as "last-result", so it should keep the name it had before.
        ; (also it gives us the /ANY option to save isotopes!)
        ;
        set/any 'last-result unmeta v

        === DISPLAY NULL AS IF IT WERE A COMMENT, AS IT HAS NO VALUE ===

        if v = null' [
            ;
            ; Key to NULL's purpose is that it lacks any value representation,
            ; and only exists as an evaluation product you can store in a
            ; variable.  So there's nothing we can print like `== null` (which
            ; would look like the WORD! null), and no special syntax exists for
            ; them...that's by design.
            ;
            ; It might seem that giving *no* output would be the most natural
            ; case for such a situation.  But it provides more grounding to
            ; show *something*, so we are tricky here in the text medium and
            ; display a line in comment form, without the ==.  It has settled
            ; into being a good compromise for the situation...helping to
            ; ground users in what is going on.
            ;
            print "; null"
            return none
        ]

        === DISPLAY VOID AS IF IT WERE A COMMENT, ALSO ===

        if v = void' [
            ;
            ; True void is not to be confused with the isotope of the WORD!
            ; void (e.g. ~void~).  The word isotopes are made by conditionals
            ; when they take a branch but the branch produces void, e.g.
            ; `if true [comment "this makes a ~void~ isotope"]`.
            ;
            print "; void"
            return none
        ]

        === ISOTOPE BAD WORDS (^META v parameter means they look plain) ===

        if v = none' [  ; don't show "none" (e.g. BLANK! isotopes)
            ;
            ; The "none" state represents the contents of an unset variable.
            ; It is also used by commands like HELP that want to keep the focus
            ; on what they are printing out, without an `== xxx` evaluated
            ; result showing up.  Functions that need a handy way of returning
            ; an "uninteresting" result, such as PRINT, use it too.
            ;
            ; Note: It was considered to use a different state for this, with
            ; a name like `~quiet~`...to get this behavior:
            ;
            ;      >> print "returns ~quiet~ isotope, no output"
            ;
            ;      >> get/any 'some-unset-var
            ;      == ~  ; isotope
            ;
            ; But something feels more intrinsically coherent about not showing
            ; an isotope which has no associated string.  It reduces the total
            ; number of parts, and helps `~` earn its name of "none".
            ;
            ;     >> x: if false [<a>]
            ;     == ~void~  ; isotope
            ;
            ;     >> x: decay if false [<a>]
            ;
            ; This gives the slightly weird dichotomy that "none" and "void"
            ; are distinct states, but we don't print the "value-bearing" one:
            ;
            ;      >> maybe if false [<a>]
            ;      ; void
            ;
            ; But doing it the other way around would force functions like
            ; HELP to be invisible, and that's not desirable.
            ;
            return none
        ]

        if let d: select [
            ~null~ "null"
            ~false~ "false"
            ~blank~ "blank"
            ~blackhole~ "#"
        ] v [
            ; An unstable isotope will decay to an ordinary value.  We make
            ; a note that they are unstable to help ground users when they see
            ; behaviors that might appear confusing:
            ;
            ;     >> x: match logic! false
            ;     == ~false~  ; isotope (decays to false)
            ;
            ;     >> get/any 'x
            ;     == #[false]
            ;
            print unspaced [
                result _ mold v _ _ {;} _ {isotope} _ "(" {decays to} _ d ")"
            ]
            return none
        ]

        if quasi? v [  ; all other isotopes
            ;
            ; All other isotope bad words display with an "isotope" annotation.
            ;
            ;     >> do [~something~]
            ;     == ~something~  ; isotope
            ;
            ; Isotopes are evaluative products only, so you won't see the
            ; annotation for anything you picked out of a block:
            ;
            ;     >> first [~something~]
            ;     == ~something~
            ;
            ; That's the plain form.  Those kinds of bad-words are received
            ; quoted by this routine like other ordinary values; this case is
            ; just for the isotopes.
            ;
            print unspaced [result _ mold v _ _ {;} _ "isotope"]
            return none
        ]

        === ISOTOPIC BLOCKS AND OTHER TYPES (NEW!) ===

        if not quoted? v [
            print "; isotope"
            v: quote v  ; print normally
        ]

        === "ORDINARY" VALUES (^META v parameter means they get quoted) ===

        set 'v unquote v  ; Avoid SET-WORD!--would cache action names as "v"

        case [
            free? :v [
                ;
                ; Molding a freed value would cause an error...which is
                ; usually okay (you shouldn't be working with freed series)
                ; but if we didn't special case it here, the error would seem
                ; to be in the console code itself.
                ;
                print-error make error! "Series data unavailable due to FREE"
            ]

            port? :v [
                ; PORT!s are returned by many operations on files, to
                ; permit chaining.  They contain many fields so their
                ; molding is excessive, and there's not a ton to learn
                ; about them.  Cut down the output more than the mold/limit.
                ;
                print [result "#[port! [...] [...]]"]
            ]
        ]
        else [
            ; print the first 20 lines of the first 2048 characters of mold
            ;
            let pos: let molded: mold/limit get 'v 2048
            repeat 20 [
                pos: next (find pos newline else [break])
            ] then [  ; e.g. didn't break
                insert clear pos "..."
            ]
            print [result (molded)]
        ]
    ]

    print-warning: meth [return: <none> s] [print [warning reduce s]]

    print-error: meth [return: <none> e [error!]] [
        if :e.file = 'tmp-boot.r [
            e.file: e.line: _  ; errors in console showed this, junk
        ]
        print [e]
    ]

    print-halted: meth [return: <none>] [
        print newline  ; interrupts happen anytime, clearer to start newline
        print "[interrupted by Ctrl-C or HALT instruction]"
    ]

    print-info: meth [return: <none> s] [print [info reduce s]]

    print-gap: meth [return: <none>] [print newline]

    === BEHAVIOR (can be overridden) ===

    input-hook: meth [
        {Receives line input, parse/transform, send back to CONSOLE eval}

        return: "null if EOF, ~escape~ if canceled, else line of text input"
            [<opt> text! bad-word!]
    ][
        return read-line
    ]

    dialect-hook: meth [
        {Receives code block, parse/transform/bind, send back to CONSOLE eval}
        return: [block!]
        b [block!]
    ][
        ; By default we bind the code to system.contexts.user
        ;
        ; See the Debug console skin for example of binding the code to the
        ; currently "focused" FRAME!, or this example on the forum of injecting
        ; the last value:
        ;
        ; https://forum.rebol.info/t/1071

        return bind b system.contexts.user
    ]

    shortcuts: make object! compose/deep [
        d: [dump]
        h: [help]
        q: [quit]
        dt: [delta-time]
        dp: [delta-profile]

        list-shortcuts: [print [system.console.shortcuts]]
        changes: [
            let gitroot: https://github.com/metaeducation/ren-c/blob/master/
            browse join gitroot spread reduce [
                %CHANGES.md "#"
                system.version.1 "." system.version.2 "." system.version.3
            ]
        ]
        topics: [
            browse https://r3n.github.io/topics/
        ]
    ]

    === HELPERS (could be overridden!) ===

    add-shortcut: meth [
        {Add/Change console shortcut}
        return: <none>
        name [any-word!] "Shortcut name"
        block [block!] "Command(s) expanded to"
    ][
        extend shortcuts name block
    ]
]


start-console: func [
    "Called when a REPL is desired after command-line processing, vs quitting"

    return: <none>
    /skin "Custom skin (e.g. derived from MAKE CONSOLE!) or file"
        [file! object!]
    <static>
        o (system.options)  ; shorthand since options are often read/written
][
    === MAKE CONSOLE! INSTANCE FOR SKINNING ===

    ; Instantiate console! object into system.console.  This is updated via
    ; %console-skin.reb if in system.options.resources

    let skin-file: case [
        file? skin [skin]
        object? skin [blank]
    ] else [%console-skin.reb]

    loud-print "Starting console..."
    loud-print newline
    let proto-skin: match object! skin else [make console! []]
    let skin-error: null

    all [
        skin-file
        not try find o.suppress skin-file
        o.resources
        exists? skin-file: join o.resources skin-file
    ] then [
        trap [
            let new-skin: do load skin-file

            ; if loaded skin returns console! object then use as prototype
            all [
                object? new-skin
                select new-skin 'repl  ; quacks like REPL, it's a console!
            ] then [
                proto-skin: new-skin
                proto-skin.was-updated: true
                proto-skin.name: default ["updated"]
            ]

            proto-skin.is-loaded: true
            proto-skin.name: default ["loaded"]
            append o.loaded skin-file

        ] then e -> [
            skin-error: e  ; show error later if `--verbose`
            proto-skin.name: "error"
        ]
    ]

    proto-skin.name: default ["default"]

    system.console: proto-skin

    === HOOK FOR HELP ABOUT LAST ERROR ===

    ; The WHY command lets the user get help about the last error printed.
    ; To do so, it has to save the last error.  Adjust the error printing
    ; hook to save the last error printed.  Also inform people of the
    ; existence of the WHY function on the first error delivery.
    ;
    proto-skin.print-error: adapt :proto-skin.print-error [
        if not system.state.last-error [
            system.console.print-info "Info: use WHY for error information"
        ]

        system.state.last-error: e
    ]

    === PRINT BANNER ===

    if o.about [
        boot-print make-banner boot-banner  ; the fancier banner
    ]

    system.console.print-greeting

    === VERBOSE CONSOLE SKINNING MESSAGES ===

    loud-print [newline {Console skinning:} newline]
    if skin-error [
        loud-print [
            {  Error loading console skin  -} skin-file LF LF
            skin-error LF LF
            {  Fix error and restart CONSOLE}
        ]
    ] else [
       loud-print [
            space space
            if proto-skin.is-loaded [
                {Loaded skin}
            ] else [
                {Skin does not exist}
            ]
            "-" skin-file
            "(CONSOLE" if not proto-skin.was-updated [{not}] "updated)"
        ]
    ]
]


ext-console-impl: func [
    {Rebol ACTION! that is called from C in a loop to implement the console}

    return: "Code for C caller to sandbox, exit status, RESUME code, or hook"
        [block! group! integer! meta-group! handle!]  ; RETURN is hooked below!
    prior "BLOCK! or GROUP! that last invocation of HOST-CONSOLE requested"
        [blank! block! group!]
    result "^META result from evaluating PRIOR, or non-quoted error"
        [<opt> any-value!]
    resumable "Is the RESUME function allowed to exit this console"
        [logic!]
    skin "Console skin to use if the console has to be launched"
        [<opt> object! file!]
][
    === HOOK RETURN FUNCTION TO GIVE EMITTED INSTRUCTION ===

    ; The C caller can be given a BLOCK! representing an code the console is
    ; executing on its own behalf, as part of its "skin".  Building these
    ; blocks is made easier by collaboration between EMIT and a hooked version
    ; of the underlying RETURN of this function.

    let instruction: copy []

    let emit: func [
        {Builds up sandboxed code to submit to C, hooked RETURN will finalize}

        return: <none>
        item "ISSUE! directive, TEXT! comment, (<*> composed) code BLOCK!"
            [block! issue! text!]
        <with> instruction
    ][
        switch type of item [
            issue! [
                if not empty? instruction [append/line instruction ',]
                insert instruction item
            ]
            text! [
                append/line instruction spread compose [comment (item)]
            ]
            block! [
                if not empty? instruction [append/line instruction ',]
                append/line instruction spread compose/deep <*> item
            ]
            fail
        ]
    ]

    return: func [
        {Hooked RETURN function which finalizes any gathered EMIT lines}

        state "Describes the RESULT that the next call to HOST-CONSOLE gets"
            [integer! tag! group! datatype! meta-group! handle!]
        <with> instruction prior
        <local> return-to-c (:return)  ; capture HOST-CONSOLE's RETURN
    ][
        switch state [
            <prompt> [
                emit [system.console.print-gap]
                emit [system.console.print-prompt]
                emit [reduce [
                    system.console.input-hook  ; can return NULL
                ]]  ; gather first line (or null), put in BLOCK!
            ]
            <halt> [
                emit [halt]
                emit [fail {^-- Shouldn't get here, due to HALT}]
            ]
            <die> [
                emit [quit 1]  ; bash exit code for any generic error
                emit [fail {^-- Shouldn't get here, due to QUIT}]
            ]
            <bad> [
                emit #no-unskin-if-error
                emit [print mold '(<*> prior)]
                emit [fail ["Bad REPL continuation:" (<*> result)]]
            ]
        ] then [
            return-to-c instruction
        ]

        return-to-c switch type of state [
            integer! [  ; just tells the calling C loop to exit() process
                assert [empty? instruction]
                state
            ]
            datatype! [  ; type assertion, how to enforce this?
                emit spaced ["^-- Result should be" an state]
                instruction
            ]
            group! [  ; means "submit user code"
                assert [empty? instruction]
                state
            ]
            meta-group! [  ; means "resume instruction"
                state
            ]
            handle! [  ; means "evaluator hook request" (handle is the hook)
                state
            ]
        ] else [
            emit [fail [{Bad console instruction:} (<*> mold state)]]
        ]
    ]

    === DO STARTUP HOOK IF THIS IS THE FIRST TIME THE CONSOLE HAS RUN ===

    if not prior [
        ;
        ; !!! This was the first call before, and it would do some startup.
        ; Now it's probably reasonable to assume if there's anything to be
        ; done on a first call (printing notice of "you broke into debug" or
        ; something like that) then whoever broke into the REPL takes
        ; care of that.
        ;
        assert [result = '~startup~]
        any [
            unset? 'system.console
            not system.console
        ] then [
            emit [start-console/skin '(<*> skin)]
        ]
        return <prompt>
    ]

    === GATHER DIRECTIVES ===

    ; #directives may be at the head of BLOCK!s the console ran for itself.
    ;
    let directives: collect [
        let i
        if block? prior [
            parse3 prior [some [set i: issue! (keep i)] end]
        ]
    ]

    if find directives #start-console [
        emit [start-console/skin '(<*> skin)]
        return <prompt>
    ]

    === QUIT handling ===

    ; https://en.wikipedia.org/wiki/Exit_status

    all [
        error? :result
        result.id = 'no-catch
        :result.arg2 = :quit  ; throw's /NAME
    ] then [
        if '~quit~ = ^result.arg1 [
            return 0  ; plain QUIT with no argument, treat it as success
        ]
        if bad-word? ^result.arg1 [
            return 1  ; treat all other QUIT with isotopes as generic error
        ]
        return switch type of :result.arg1 [
            logic! [either :result.arg1 [0] [1]]  ; logic true is success

            integer! [result.arg1]  ; Note: may be too big for status range

            error! [1]  ; currently there's no default error-to-int mapping
        ] else [
            1  ; generic error code
        ]
    ]

    === HALT handling (e.g. Ctrl-C) ===

    ; Note: Escape is handled during input gathering by a dedicated signal.

    all [
        error? :result
        result.id = 'no-catch
        :result.arg2 = :halt  ; throw's /NAME
    ] then [
        if find directives #quit-if-halt [
            return 128 + 2 ; standard cancellation exit status for bash
        ]
        if find directives #console-if-halt [
            emit [start-console/skin '(<*> skin)]
            return <prompt>
        ]
        if find directives #unskin-if-halt [
            print "** UNSAFE HALT ENCOUNTERED IN CONSOLE SKIN"
            print "** REVERTING TO DEFAULT SKIN"
            system.console: make console! []
            print mold prior  ; Might help debug to see what was running
        ]

        ; !!! This would add an "unskin if halt" which would stop you from
        ; halting the print response to the halt message.  But that was still
        ; in effect during <prompt> which is part of the same "transaaction"
        ; as PRINT-HALTED.  To the extent this is a good idea, it needs to
        ; guard -only- the PRINT-HALTED and put the prompt in a new state.
        ;
        comment [emit #unskin-if-halt]

        emit [system.console.print-halted]
        return <prompt>
    ]

    === RESUME handling ===

    ; !!! This is based on debugger work-in-progress.  A nested console that
    ; has been invoked via a breakpoint the console will sandbox most errors
    ; and throws.  But if it recognizes a special "resume instruction" being
    ; thrown, it will consider its nested level to be done and yield that
    ; result so the program can continue.

    all [
        in lib 'resume
        error? :result
        result.id = 'no-catch
        :result.arg2 = :lib.resume  ; throw's /NAME
    ] then [
        assert [match [meta-group! handle!] :result.arg1]
        if not resumable [
            e: make error! "Can't RESUME top-level CONSOLE (use QUIT to exit)"
            e.near: result.near
            e.where: result.where
            emit [system.console.print-error (<*> e)]
            return <prompt>
        ]
        return :result.arg1
    ]

    if error? :result [  ; all other errors
        ;
        ; Errors can occur during MAIN-STARTUP, before the system.CONSOLE has
        ; a chance to be initialized (it may *never* be initialized if the
        ; interpreter is being called non-interactively from the shell).
        ;
        if object? system.console [
            emit [system.console.print-error (<*> :result)]
        ] else [
            emit [print [(<*> :result)]]
        ]
        if find directives #die-if-error [
            return <die>
        ]
        if find directives #halt-if-error [
            return <halt>
        ]
        if find directives #countdown-if-error [
            emit #console-if-halt
            emit [
                print newline
                print "** Hit Ctrl-C to break into the console in 5 seconds"

                repeat n 25 [
                    if 1 = remainder n 5 [
                        write-stdout form (5 - to-integer (n / 5))
                    ] else [
                        write-stdout "."
                    ]
                    wait 0.25
                ]
                print newline
            ]
            emit {Only gets here if user did not hit Ctrl-C}
            return <die>
        ]
        if block? prior [
            case [
                find directives #host-console-error [
                    print "** HOST-CONSOLE ACTION! ITSELF RAISED ERROR"
                    print "** SAFE RECOVERY NOT LIKELY, BUT TRYING ANYWAY"
                ]
                not find directives #no-unskin-if-error [
                    print "** UNSAFE ERROR ENCOUNTERED IN CONSOLE SKIN"
                ]
                print mold result
            ] then [
                print "** REVERTING TO DEFAULT SKIN"
                system.console: make console! []
                print mold prior  ; Might help debug to see what was running
            ]
        ]
        return <prompt>
    ]

    === HANDLE RESULT FROM EXECUTION OF CODE ON USER'S BEHALF ===

    if result = void' [
        ;
        ; !!! You can get nothing from an empty string, and having that print
        ; out "; void" is somewhat pedantic if you're just hitting enter to
        ; see if the console is frozen or taking input.
        ;
        ;    >>
        ;    ; void
        ;
        ; But we do want this, for reasons explained in PRINT-RESULT:
        ;
        ;     >> comment "hi"
        ;     ; void
        ;
        ; Review making the console check specifically for empty strings and
        ; not even submitting them.
    ]

    if group? prior [
        ;
        ; RESULT has been ^META'd...but it could be pure NULL.  We can't
        ; compose pure NULL into a block, so we compose it with a quote...
        ; it decays to just a single apostrophe (') which will evaluate to
        ; pure NULL so PRINT-RESULT's argument can preserve the distinction.
        ;
        ; We also build a raw frame to call SYSTEM.CONSOLE.PRINT-RESULT.  We
        ; could use a convention where we pass it already meta'd results in
        ; order to avoid losing the distinction between pure void and a void
        ; isotope...but it's nice to have its interface contract be non-meta
        ; (in case people want to call it directly.)  So here a low-level frame
        ; is built to avoid lossiness.
        ;
        emit [
            let f: make frame! :system.console.print-result
            f.v: '(<*> result)  ; avoid conflating pure void and void isotope
            do f
        ]
        return <prompt>
    ]

    === HANDLE CONTINUATION THE CONSOLE SENT TO ITSELF ===

    assert [block? prior]

    ; `result` of console instruction can be:
    ;
    ; GROUP! - code to be run in a sandbox on behalf of the user
    ; BLOCK! - block of gathered input lines so far, need another one
    ;
    result: unmeta result

    if group? result [
        return result  ; GROUP! signals we're running user-requested code
    ]

    if not block? result [
        return <bad>
    ]

    === TRY ADDING LINE OF INPUT TO CODE REGENERATED FROM BLOCK ===

    ; Note: INPUT-HOOK has already run once per line in this block

    assert [not empty? result]  ; should have at least one item

    if '~escape~ = last result [  ; Escape key pressed during READ-LINE
        ;
        ; Note: At one time it had to be Ctrl-D on Windows, as ReadConsole()
        ; could not trap escape.  But input was changed to use more granular
        ; APIs on windows, on a keystroke-by-keystroke basis vs reading a
        ; whole line at a time.
        ;
        return <prompt>
    ]

    let code
    trap [
        ; Note LOAD now makes BLOCK! even for a single item,
        ; e.g. `load "word"` => `[word]`
        ;
        code: transcode/where (delimit newline result) sys.contexts.user
        assert [block? code]

    ] then error -> [
        ;
        ; If loading the string gave back an error, check to see if it
        ; was the kind of error that comes from having partial input
        ; (scan-missing).  If so, CONTINUE and read more data until
        ; it's complete (or until an empty line signals to just report
        ; the error as-is)
        ;
        if error.id = 'scan-missing [
            ;
            ; Error message tells you what's missing, not what's open and
            ; needs to be closed.  Invert the symbol.
            ;
            switch error.arg1 [
                "}" ["{"]
                ")" ["("]
                "]" ["["]
            ] also unclosed -> [
                ;
                ; Backslash is used in the second column to help make a
                ; pattern that isn't legal in Rebol code, which is also
                ; uncommon in program output.  This enables detection of
                ; transcripts, potentially to replay them without running
                ; program output or evaluation results.
                ;
                write-stdout unspaced [unclosed "\" _ _]
                emit [reduce [  ; reduce will runs in sandbox
                    (<*> spread result)  ; splice previous inert literal lines
                    system.console.input-hook  ; hook to run in sandbox
                ]]

                return block!
            ]
        ]

        ; Could be an unclosed double quote (unclosed tag?) which more input
        ; on a new line cannot legally close ATM
        ;
        emit [system.console.print-error (<*> error)]
        return <prompt>
    ]

    === HANDLE CODE THAT HAS BEEN SUCCESSFULLY LOADED ===

    if let shortcut: try select system.console.shortcuts first code [
        ;
        ; Shortcuts like `q => [quit]`, `d => [dump]`
        ;
        if (bound? code.1) and (set? code.1) [
            ;
            ; Help confused user who might not know about the shortcut not
            ; panic by giving them a message.  Reduce noise for the casual
            ; shortcut by only doing so when a bound variable exists.
            ;
            emit [system.console.print-warning (<*>
                spaced [
                    uppercase to text! code.1
                        "interpreted by console as:" mold :shortcut
                ]
            )]
            emit [system.console.print-warning (<*>
                spaced ["use" to get-word! code.1 "to get variable."]
            )]
        ]
        take code
        insert code shortcut
    ]

    ; Run the "dialect hook", which can transform the completed code block
    ;
    emit #unskin-if-halt  ; Ctrl-C during dialect hook is a problem
    emit [
        comment {not all users may want CONST result, review configurability}
        as group! system.console.dialect-hook (<*> code)
    ]
    return group!  ; a group RESULT should come back to HOST-CONSOLE
]


=== WHY and UPGRADE (do these belong here?) ===

; We can choose to expose certain functionality only in the console prompt,
; vs. needing to be added to global visibility.  Adding to the lib context
; means these will be seen by scripts, e.g. `do "why"` will work.
;

export why: func [
    "Explain the last error in more detail."
    return: <none>
    'err [<end> word! path! error!] "Optional error value"
][
    let err: default [system.state.last-error]

    if match [word! path!] err [
        err: get err
    ]

    if error? err [
        err: lowercase unspaced [err.type "-" err.id]
        let docroot: http://www.rebol.com/r3/docs/errors/
        browse join docroot spread reduce [err ".html"]
    ] else [
        print "No information is available."
    ]
]


export upgrade: func [
    "Check for newer versions."
    return: <none>
][
    ; Should this be a console-detected command, like Q, or is it meaningful
    ; to define this as a function you could call from code?
    ;
    do <upgrade>
]
