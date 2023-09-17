REBOL [
    File: %rebmake.r
    Title: {Rebol-Based C/C++ Makefile and Project File Generator}

    Type: module
    Name: Rebmake

    Rights: {
        Copyright 2017 Atronix Engineering
        Copyright 2017-2018 Ren-C Open Source Contributors
    }
    License: {
        Licensed under the Apache License, Version 2.0
        See: http://www.apache.org/licenses/LICENSE-2.0
    }
    Description: {
        R3-Alpha's bootstrap process depended on the GNU Make Tool, with a
        makefile generated from minor adjustments to a boilerplate copy of
        the makefile text.  As needs grew, a second build process arose
        which used CMake...which was also capable of creating files for
        various IDEs, such as Visual Studio.

        %rebmake.r arose to try and reconcile these two build processes, and
        eliminate dependency on an external make tool completely.  It can
        generate project files for Microsoft Visual Studio, makefiles for
        GNU Make or Microsoft's Nmake, or just carry out a full build by
        invoking compiler processes and command lines itself.

        In theory this code is abstracted such that it could be used by other
        projects.  In practice, it is tailored to the specific needs and
        settings of the Rebol project.
    }
]

if trap [:import/into] [  ; See %import-shim.r
    do load append copy system/script/path %import-shim.r
]

import <bootstrap-shim.r>

default-compiler: null
default-linker: null
default-strip: null
target-platform: null

map-files-to-local: func [
    return: [block!]
    files [<maybe> file! block!]
][
    if null? :files [return copy []]
    if not block? files [files: reduce [files]]
    return map-each f files [
        file-to-local f
    ]
]

ends-with?: func [
    return: [logic!]
    s [any-string!]
    suffix [<opt> any-string!]
][
    return to-logic any [
        null? :suffix
        empty? suffix
        suffix = (skip tail-of s negate length of suffix)
    ]
]

filter-flag: function [
    return: [<opt> text! file!]
    flag [tag! text! file!]
        {If TAG! then must be <prefix:flag>, e.g. <gnu:-Wno-unknown-warning>}
    prefix [text!]
        {gnu -> GCC-compatible compilers, msc -> Microsoft C}
][
    if not tag? flag [return flag]  ; no filtering

    parse2 to text! flag [
        copy header: to ":"
        ":" copy option: to end
    ] else [
        fail ["Tag must be <prefix:flag> ->" (flag)]
    ]

    return all [
        prefix = header
        to-text option
    ]
]

run-command: func [
    return: [text!]
    cmd [block! text!]
][
    let x: copy ""
    call/shell/output cmd x
    return trim/with x "^/^M"
]

pkg-config: func [  ; !!! Note: Does not appear to be used
    return: [text! block!]
    pkg [any-string!]
    var [word!]
    lib [any-string!]
][
    let [dlm opt]
    switch var [
        'includes [
            dlm: "-I"
            opt: "--cflags-only-I"
        ]
        'searches [
            dlm: "-L"
            opt: "--libs-only-L"
        ]
        'libraries [
            dlm: "-l"
            opt: "--libs-only-l"
        ]
        'cflags [
            dlm: null
            opt: "--cflags-only-other"
        ]
        'ldflags [
            dlm: null
            opt: "--libs-only-other"
        ]
        fail ["Unsupported pkg-config word:" var]
    ]

    let x: run-command spaced [pkg lib]

    if not dlm [
        return x
    ]

    let ret: make block! 1
    let item
    parse2 x [
        some [
            thru dlm
            copy item: to [dlm | end] (
                ;dump item
                append ret to file! item
            )
        ]
        end
    ]
    return ret
]

platform-class: make object! [
    name: ~
    exe-suffix: ~
    dll-suffix: ~
    archive-suffix: ~  ;static library
    obj-suffix: ~

    gen-cmd-create: ~
    gen-cmd-delete: ~
    gen-cmd-strip: ~
]

unknown-platform: make platform-class [
    name: 'unknown
]

posix: make platform-class [
    name: 'POSIX
    exe-suffix: ""
    dll-suffix: ".so"
    obj-suffix: ".o"
    archive-suffix: ".a"

    gen-cmd-create: meth [
        return: [text!]
        cmd [object!]
    ][
        return either dir? cmd/file [
            spaced ["mkdir -p" cmd/file]
        ][
            spaced ["touch" cmd/file]
        ]
    ]

    gen-cmd-delete: meth [
        return: [text!]
        cmd [object!]
    ][
        return spaced ["rm -fr" cmd/file]
    ]

    gen-cmd-strip: meth [
        return: [text!]
        cmd [object!]
    ][
        if let tool: any [:cmd/strip :default-strip] [
            let b: ensure block! tool/commands cmd/file cmd/options
            assert [1 = length of b]
            return b/1
        ]
        return ""
    ]
]

linux: make posix [
    name: 'Linux
]

haiku: make posix [
    name: 'Haiku
]

android: make linux [
    name: 'Android
]

emscripten: make posix [
    name: 'Emscripten
    exe-suffix: ".wasm"
    dll-suffix: ".js"  ; !!! We want libr3.js for "main" lib, but .so for rest
]

osx: make posix [
    name: 'OSX
    dll-suffix: ".dylib"  ; !!! This was .dyn - but no one uses that
]

windows: make platform-class [
    name: 'Windows

    exe-suffix: ".exe"
    dll-suffix: ".dll"
    obj-suffix: ".obj"
    archive-suffix: ".lib"

    gen-cmd-create: meth [
        return: [text!]
        cmd [object!]
    ][
        let f: file-to-local cmd/file
        if #"\" = last f [remove back tail-of f]
        return either dir? cmd/file [
            spaced ["if not exist" f "mkdir" f]
        ][
            unspaced ["echo . 2>" f]
        ]
    ]

    gen-cmd-delete: meth [
        return: [text!]
        cmd [object!]
    ][
        let f: file-to-local cmd/file
        if #"\" = last f [remove back tail-of f]
        return either dir? cmd/file [
            ;
            ; Note: If you have Git shell tools installed on Windows, then
            ; `rmdir` here might run `C:\Program Files\Git\usr\bin\rmdir.EXE`
            ; and not understand the /S /Q flags.  `rd` is an alias.
            ;
            spaced ["if exist" f "rd /S /Q" f]
        ][
            spaced ["if exist" f "del /Q" f]
        ]
    ]

    gen-cmd-strip: meth [
        return: [text!]
        cmd [object!]
    ][
        print "Note: STRIP command not implemented for MSVC"
        return ""
    ]
]

set-target-platform: func [
    return: <none>
    platform
][
    switch platform [
        'posix [
            target-platform: posix
        ]
        'linux [
            target-platform: linux
        ]
        'haiku [
            target-platform: haiku
        ]
        'android [
            target-platform: android
        ]
        'windows [
            target-platform: windows
        ]
        'osx [
            target-platform: osx
        ]
        'emscripten [
            target-platform: emscripten
        ]
    ] else [
        print ["Unknown platform:" platform "falling back to POSIX"]
        target-platform: posix
    ]
]

project-class: make object! [
    class: #project
    name: null
    id: null
    type: null  ;  dynamic, static, object or application
    depends: null  ; a dependency could be a library, object file
    output: null  ; file path
    basename: null   ; output without extension part
    generated?: false
    implib: null  ; Windows exe/lib with exported symbols generates implib file

    post-build-commands: null  ; commands to run after the "build" command

    compiler: null

    ; common settings applying to all included obj-files
    ; setting inheritage:
    ; they can only be inherited from project to obj-files
    ; _not_ from project to project.
    ; They will be applied _in addition_ to the obj-file level settings
    ;
    includes: null
    definitions: null
    cflags: null

    ; These can be inherited from project to obj-files and will be overwritten
    ; at the obj-file level
    ;
    optimization: null
    debug: null
]

solution-class: make project-class [
    class: #solution
]

ext-dynamic-class: make object! [
    class: #dynamic-extension
    output: null
    flags: null  ;static?
]

ext-static-class: make object! [
    class: #static-extension
    output: null
    flags: null  ;static?
]

application-class: make project-class [
    class: #application
    type: 'application
    generated?: false

    linker: null
    searches: null
    ldflags: null

    link: meth [return: <none>] [
        linker/link output depends ldflags
    ]

    command: meth [return: [text!]] [
        let ld: any [linker, default-linker]
        return apply :ld/command [
            output, depends, searches, ldflags
            /debug debug
        ]
    ]

]

dynamic-library-class: make project-class [
    class: #dynamic-library
    type: 'dynamic
    generated?: false
    linker: null

    searches: null
    ldflags: null
    link: meth [return: <none>] [
        linker/link output depends ldflags
    ]

    command: meth [
        return: [text!]
        <with>
        default-linker
    ][
        let l: any [linker, default-linker]
        return apply :l/command [
            output, depends, searches, ldflags
            /dynamic true
        ]
    ]
]

; !!! This is an "object library" class which seems to be handled in some of
; the same switches as #static-library.  But there is no static-library-class
; for some reason, despite several #static-library switches.  What is the
; reasoning behind this?
;
object-library-class: make project-class [
    class: #object-library
    type: 'object
]

compiler-class: make object! [
    class: #compiler
    name: null
    id: null  ; flag prefix
    version: null
    exec-file: null
    compile: meth [
        return: <none>
        output [file!]
        source [file!]
        include [file! block!]
        definition [any-string!]
        cflags [any-string!]
    ][
    ]

    command: meth [
        return: [text!]
        output
        source
        includes
        definitions
        cflags
    ][
    ]
    ;check if the compiler is available
    check: meth [
        return: [logic!]
        path [<maybe> any-string!]
    ][
        fail ~tbd~
    ]
]

gcc: make compiler-class [
    name: 'gcc
    id: "gnu"
    check: meth [
        return: [logic!]
        /exec [file!]
    ][
        ; !!! This used to be static, but the bootstrap executable's non
        ; gathering form could not do <static>
        ;
        let digit: charset "0123456789"

        version: copy ""
        attempt [
            exec-file: exec: default ["gcc"]
            call/output [(exec) "--version"] version
            let letter: charset [#"a" - #"z" #"A" - #"Z"]
            parse2 version [
                "gcc (" some [letter | digit | #"_"] ")" space
                copy major: some digit "."
                copy minor: some digit "."
                copy macro: some digit
                to end
            ] then [
                version: reduce [  ; !!! It appears this is not used (?)
                    to integer! major
                    to integer! minor
                    to integer! macro
                ]
                return true
            ]
            return false
        ]
    ]

    command: meth [
        return: [text!]
        output [file!]
        source [file!]
        /I "includes" [block!]
        /D "definitions" [block!]
        /F "cflags" [block!]
        /O "opt-level" [any-value!]  ; !!! datatypes?
        /g "debug" [any-value!]  ; !!! datatypes?
        /PIC "https://en.wikipedia.org/wiki/Position-independent_code"
        /E "only preprocessing"
    ][
        return spaced collect [
            keep any [
                file-to-local/pass maybe exec-file
                to text! name  ; the "gcc" may get overridden as "g++"
            ]

            keep either E ["-E"]["-c"]

            if PIC [
                keep "-fPIC"
            ]
            if I [
                for-each inc (map-files-to-local I) [
                    keep unspaced ["-I" inc]
                ]
            ]
            if D [
                for-each flg D [
                    if word? flg [flg: as text! flg]

                    ; !!! For cases like `#include MBEDTLS_CONFIG_FILE` then
                    ; quotes are expected to work in defines...but when you
                    ; pass quotes on the command line it's different than
                    ; inside of a visual studio project (for instance) because
                    ; bash strips them out unless escaped with backslash.
                    ; This is a stopgap workaround that ultimately would
                    ; permit cross-platform {MBEDTLS_CONFIG_FILE="filename.h"}
                    ;
                    if find [gcc g++ cl] name [
                        flg: replace/all copy flg {"} {\"}
                    ]

                    ; Note: bootstrap executable hangs on:
                    ;
                    ;     keep unspaced [
                    ;         "-D" (filter-flag flg id else [continue])
                    ;     ]
                    ;
                    if flg: filter-flag flg id [
                        keep unspaced ["-D" flg]
                    ]
                ]
            ]
            if O [
                case [
                    O = true [keep "-O2"]
                    O = false [keep "-O0"]
                    integer? O [keep unspaced ["-O" O]]
                    find ["s" "z" "g" 's 'z 'g] O [
                        keep unspaced ["-O" O]
                    ]

                    fail ["unrecognized optimization level:" O]
                ]
            ]
            if g [
                case [
                    g = true [keep "-g -g3"]
                    g = false []
                    integer? g [keep unspaced ["-g" g]]

                    fail ["unrecognized debug option:" g]
                ]
            ]
            if F [
                for-each flg F [
                    keep maybe filter-flag flg id
                ]
            ]

            keep "-o"

            output: file-to-local output

            any [E, ends-with? output target-platform/obj-suffix] then [
                keep output
            ] else [
                keep [output target-platform/obj-suffix]
            ]

            keep file-to-local source
        ]
    ]
]

; !!! In the original rebmake.r, tcc was a full copy of the GCC code, while
; clang was just `make gcc [name: 'clang]`.  TCC was not used as a compiler
; for Rebol itself--only to do some preprocessing of %sys-core.i, but this
; mechanism is no longer used (see %extensions/tcc/README.md)

tcc: make gcc [
    name: 'tcc
]

clang: make gcc [
    name: 'clang
]

; Microsoft CL compiler
cl: make compiler-class [
    name: 'cl
    id: "msc" ;flag id
    command: meth [
        return: [text!]
        output [file!]
        source
        /I "includes" [block!]
        /D "definitions" [block!]
        /F "cflags" [block!]
        /O "opt-level" [any-value!]  ; !!! datatypes?
        /g "debug" [any-value!]  ; !!! datatypes?
        /PIC "https://en.wikipedia.org/wiki/Position-independent_code"
        ; Note: PIC is ignored for this Microsoft CL compiler handler
        /E "only preprocessing"
    ][
        return spaced collect [
            keep any [(file-to-local/pass maybe exec-file) "cl"]
            keep "/nologo"  ; don't show startup banner (must be lowercase)
            keep either E ["/P"]["/c"]

            ; NMAKE is not multi-core, only CL.EXE is when you pass it more
            ; than one file at a time with /MP.  To get around this, you can
            ; use Qt's JOM which is a drop-in replacement for NMAKE that does
            ; parallel building.  But it requires /FS "force synchronous pdb"
            ; so that the multiple CL calls don't try and each lock the pdb.
            ;
            keep "/FS"

            if I [
                for-each inc (map-files-to-local I) [
                    keep unspaced ["/I" inc]
                ]
            ]
            if D [
                for-each flg D [
                    if word? flg [flg: as text! flg]

                    ; !!! For cases like `#include MBEDTLS_CONFIG_FILE` then
                    ; quotes are expected to work in defines...but when you
                    ; pass quotes on the command line it's different than
                    ; inside of a visual studio project (for instance) because
                    ; bash strips them out unless escaped with backslash.
                    ; This is a stopgap workaround that ultimately would
                    ; permit cross-platform {MBEDTLS_CONFIG_FILE="filename.h"}
                    ;
                    flg: replace/all copy flg {"} {\"}

                    ; Note: bootstrap executable hangs on:
                    ;
                    ;     keep unspaced [
                    ;         "/D" (filter-flag flg id else [continue])
                    ;     ]
                    ;
                    if flg: filter-flag flg id [
                        keep unspaced ["/D" flg]
                    ]
                ]
            ]
            if O [
                case [
                    O = true [keep "/O2"]
                    all [O, not zero? O] [
                        keep unspaced ["/O" O]
                    ]
                ]
            ]
            if g [
                case [
                    any [
                        g = true
                        integer? g  ; doesn't map to a CL option
                    ][
                        keep "/Od /Zi"
                    ]
                    debug = false []

                    fail ["unrecognized debug option:" g]
                ]
            ]
            if F [
                for-each flg F [
                    keep maybe filter-flag flg id
                ]
            ]

            output: file-to-local output
            keep unspaced [
                either E ["/Fi"]["/Fo"]
                any [
                    E
                    ends-with? output target-platform/obj-suffix
                ] then [
                    output
                ] else [
                    unspaced [output target-platform/obj-suffix]
                ]
            ]

            keep file-to-local/pass source
        ]
    ]
]

linker-class: make object! [
    class: #linker
    name: null
    id: null  ; flag prefix
    version: null
    link: meth [
        return: <none>
    ][
        ...  ; overridden
    ]
    commands: meth [
        return: [<opt> block!]
        output [file!]
        depends [<opt> block!]
        searches [<opt> block!]
        ldflags [<opt> block! any-string!]
    ][
        ...  ; overridden
    ]
    check: does [
        ...  ; overridden
    ]
]

ld: make linker-class [
    ;
    ; Note that `gcc` is used as the ld executable by default.  There are
    ; some switches (such as -m32) which it seems `ld` does not recognize,
    ; even when processing a similar looking link line.
    ;
    name: 'ld
    version: null
    exec-file: null
    id: "gnu"
    command: meth [
        return: [text!]
        output [file!]
        depends [<opt> block!]
        searches [<opt> block!]
        ldflags [<opt> block! any-string!]
        /dynamic
        /debug [logic!]
    ][
        let suffix: either dynamic [
            target-platform/dll-suffix
        ][
            target-platform/exe-suffix
        ]
        return spaced collect [
            keep any [(file-to-local/pass maybe exec-file) "gcc"]

            ; !!! This was breaking emcc.  However, it is needed in order to
            ; get shared libraries on Posix.  That feature is being resurrected
            ; so turn it back on.
            ; https://github.com/emscripten-core/emscripten/issues/11814
            ;
            if dynamic [keep "-shared"]

            keep "-o"

            output: file-to-local output
            either ends-with? output :suffix [
                keep output
            ][
                keep unspaced [output :suffix]
            ]

            for-each search (maybe map-files-to-local maybe searches) [
                keep unspaced ["-L" search]
            ]

            for-each flg ldflags [
                keep maybe filter-flag flg id
            ]

            for-each dep depends [
                keep maybe accept dep
            ]
        ]
    ]

    accept: meth [
        return: [<opt> text!]
        dep [object!]
    ][
        return degrade switch dep/class [
            #object-file [
                file-to-local dep/output
            ]
            #dynamic-extension [
                either tag? dep/output [
                    if let lib: filter-flag dep/output id [
                        unspaced ["-l" lib]
                    ]
                ][
                    spaced [
                        if dep/flags [
                            if find dep/flags 'static ["-static"]
                        ]
                        unspaced ["-l" dep/output]
                    ]
                ]
            ]
            #static-extension [
                file-to-local dep/output
            ]
            #object-library [
                spaced map-each ddep dep/depends [
                    file-to-local ddep/output
                ]
            ]
            #application [
                '~null~
            ]
            #variable [
                '~null~
            ]
            #entry [
                '~null~
            ]
            (elide dump dep)
            fail "unrecognized dependency"
        ]
    ]

    check: meth [
        return: [logic!]
        /exec [file!]
    ][
        let version: copy ""
        ;attempt [
            exec-file: exec: default ["gcc"]
            call/output [(exec) "--version"] version
        ;]
        return false  ; !!! Ever called?
    ]
]

llvm-link: make linker-class [
    name: 'llvm-link
    version: null
    exec-file: null
    id: "llvm"
    command: meth [
        return: [text!]
        output [file!]
        depends [<opt> block!]
        searches [<opt> block!]
        ldflags [<opt> block! any-string!]
        /dynamic
        /debug [logic!]
    ][
        let suffix: either dynamic [
            target-platform/dll-suffix
        ][
            target-platform/exe-suffix
        ]

        return spaced collect [
            keep any [(file-to-local/pass maybe exec-file) "llvm-link"]

            keep "-o"

            output: file-to-local output
            either ends-with? output :suffix [
                keep output
            ][
                keep unspaced [output :suffix]
            ]

            ; llvm-link doesn't seem to deal with libraries
            comment [
                for-each search (maybe map-files-to-local maybe searches) [
                    keep unspaced ["-L" search]
                ]
            ]

            for-each flg ldflags [
                keep maybe filter-flag flg id
            ]

            for-each dep depends [
                keep maybe accept dep
            ]
        ]
    ]

    accept: meth [
        return: [<opt> text!]
        dep [object!]
    ][
        return degrade switch dep/class [
            #object-file [
                file-to-local dep/output
            ]
            #dynamic-extension [
                '~null~
            ]
            #static-extension [
                '~null~
            ]
            #object-library [
                spaced map-each ddep dep/depends [
                    file-to-local ddep/output
                ]
            ]
            #application [
                '~null~
            ]
            #variable [
                '~null~
            ]
            #entry [
                '~null~
            ]
            (elide dump dep)
            fail "unrecognized dependency"
        ]
    ]
]

; Microsoft linker
link: make linker-class [
    name: 'link
    id: "msc"
    version: null
    exec-file: null
    command: meth [
        return: [text!]
        output [file!]
        depends [<opt> block!]
        searches [<opt> block!]
        ldflags [<opt> block! any-string!]
        /dynamic
        /debug [logic!]
    ][
        let suffix: either dynamic [
            target-platform/dll-suffix
        ][
            target-platform/exe-suffix
        ]
        return spaced collect [
            keep any [(file-to-local/pass maybe exec-file) "link"]

            ; https://docs.microsoft.com/en-us/cpp/build/reference/debug-generate-debug-info
            if debug [keep "/DEBUG"]

            keep "/NOLOGO"  ; don't show startup banner (link takes uppercase!)
            if dynamic [keep "/DLL"]

            output: file-to-local output
            keep unspaced [
                "/OUT:" either ends-with? output suffix [
                    output
                ][
                    unspaced [output suffix]
                ]
            ]

            for-each search (maybe map-files-to-local maybe searches) [
                keep unspaced ["/LIBPATH:" search]
            ]

            for-each flg ldflags [
                keep maybe filter-flag flg id
            ]

            for-each dep depends [
                keep maybe accept dep
            ]
        ]
    ]

    accept: meth [
        return: [<opt> text!]
        dep [object!]
    ][
        return degrade switch dep/class [
            #object-file [
                file-to-local to-file dep/output
            ]
            #dynamic-extension [
                comment [import file]  ; static property is ignored

                reify either tag? dep/output [
                    filter-flag dep/output id
                ][
                    ;dump dep/output
                    file-to-local/pass either ends-with? dep/output ".lib" [
                        dep/output
                    ][
                        join dep/output ".lib"
                    ]
                ]
            ]
            #static-extension [
                file-to-local dep/output
            ]
            #object-library [
                spaced map-each ddep dep/depends [
                    file-to-local to-file ddep/output
                ]
            ]
            #application [
                file-to-local any [:dep/implib, join dep/basename ".lib"]
            ]
            #variable [
                '~null~
            ]
            #entry [
                '~null~
            ]
            (elide dump dep)
            fail "unrecognized dependency"
        ]
    ]
]

strip-class: make object! [
    class: #strip
    name: null
    id: null  ; flag prefix
    exec-file: null
    options: null
    commands: meth [
        return: [block!]
        target [file!]
        params [blank! block! any-string! null!]
    ][
        return reduce [spaced collect [
            keep any [(file-to-local/pass maybe exec-file) "strip"]
            params: default [options]
            switch kind of params [  ; switch/type not in bootstrap
                block! [
                    for-each flag params [
                        keep filter-flag flag id
                    ]
                ]
                text! [
                    keep params
                ]
            ]
            keep file-to-local target
        ]]
    ]
    check: does [
        ...  ; overridden
    ]
]

strip: make strip-class [
    id: "gnu"
    check: meth [
        return: [logic!]
        /exec [file!]
    ][
        exec-file: exec: default ["strip"]
        return true
    ]

]

; includes/definitions/cflags will be inherited from its immediately ancester
object-file-class: make object! [
    class: #object-file
    compiler: null
    cflags: null
    definitions:
    source: ~
    output: ~
    basename: null  ; output without extension part
    optimization: null
    debug: null
    includes: null
    generated?: false
    depends: null

    compile: meth [return: <none>] [
        compiler/compile
    ]

    command: meth [
        return: [text!]
        /I "extra includes" [block!]
        /D "extra definitions" [block!]
        /F "extra cflags (override)" [block!]
        /O "opt-level" [any-value!]  ; !!! datatypes?
        /g "dbg" [any-value!]  ; !!! datatypes?
        /PIC "https://en.wikipedia.org/wiki/Position-independent_code"
        /E "only preprocessing"
    ][
        let cc: any [compiler, default-compiler]

        if optimization = #prefer-O2-optimization [
            any [
                not O
                O = "s"
            ] then [
                O: 2  ; don't override e.g. "-Oz"
            ]
            optimization: false
        ]

        return apply :cc/command [  ; reduced APPLY in bootstrap!
            output
            source

            /I compose [(maybe spread includes) (maybe spread I)]
            /D compose [(maybe spread definitions) (maybe spread D)]
            /F compose [(maybe spread F) (maybe spread cflags)]
                                                ; ^-- reverses priority, why?

            ; "current setting overwrites /refinement"
            ; "because the refinements are inherited from the parent" (?)

            /O any [O, optimization]
            /g any [g, debug]

            /PIC PIC
            /E E
        ]
    ]

    gen-entries: meth [
        return: [object!]
        parent [object!]
        /PIC "https://en.wikipedia.org/wiki/Position-independent_code"
    ][
        assert [
            find [
                #application
                #dynamic-library
                #static-library
                #object-library
            ] parent/class
        ]

        return make entry-class [
            target: output
            depends: append (copy any [depends []]) source
            commands: reduce [apply :command [
                /I maybe parent/includes
                /D maybe parent/definitions
                /F maybe parent/cflags
                /O maybe parent/optimization
                /g maybe parent/debug
                /PIC to-logic any [PIC, parent/class = #dynamic-library]
            ]]
        ]
    ]
]

entry-class: make object! [
    class: #entry
    id: null
    target: ~
    depends: null
    commands: ~
    generated?: false
]

var-class: make object! [
    class: #variable
    name: ~
    value: null  ; behavior is `any [value, default]`, so start as blank
    default: ~
    generated?: false
]

cmd-create-class: make object! [
    class: #cmd-create
    file: ~
]

cmd-delete-class: make object! [
    class: #cmd-delete
    file: ~
]

cmd-strip-class: make object! [
    class: #cmd-strip
    file: ~
    options: null
    strip: null
]

generator-class: make object! [
    class: #generator

    vars: make map! 128

    gen-cmd-create: null
    gen-cmd-delete: null
    gen-cmd-strip: null

    gen-cmd: meth [
        return: [text!]
        cmd [object!]
    ][
        return switch cmd/class [
            #cmd-create [
                applique any [
                    :gen-cmd-create :target-platform/gen-cmd-create
                ] compose [
                    cmd: (cmd)
                ]
            ]
            #cmd-delete [
                applique any [
                    :gen-cmd-delete :target-platform/gen-cmd-delete
                ] compose [
                    cmd: (cmd)
                ]
            ]
            #cmd-strip [
                applique any [
                    :gen-cmd-strip :target-platform/gen-cmd-strip
                ] compose [
                    cmd: (cmd)
                ]
            ]

            fail ["Unknown cmd class:" cmd/class]
        ]
    ]

    do-substitutions: meth [
        {Substitute variables in the command with its value}
        {(will recursively substitute if the value has variables)}

        return: [<opt> object! any-string!]
        cmd [object! any-string!]
    ][
        ; !!! These were previously static, but bootstrap executable's non
        ; gathering function form could not handle statics.
        ;
        let letter: charset [#"a" - #"z" #"A" - #"Z"]
        let digit: charset "0123456789"
        let localize: func [v][return either file? v [file-to-local v][v]]

        if object? cmd [
            assert [
                find [
                    #cmd-create #cmd-delete #cmd-strip
                ] cmd/class
            ]
            cmd: gen-cmd cmd
        ]
        if not cmd [return null]

        let stop: false
        let name
        let val
        while [not stop][
            stop: true
            parse2 cmd [
                opt some [
                    change [
                        [
                            "$(" copy name: some [letter | digit | #"_"] ")"
                            | "$" copy name: letter
                        ] (
                            val: localize select vars name
                            stop: false
                        )
                    ] (val)
                    | skip
                ]
            ] else [
                fail ["failed to do var substitution:" cmd]
            ]
        ]
        return cmd
    ]

    prepare: meth [
        return: <none>
        solution [object!]
    ][
        if find words-of solution 'output [
            setup-outputs solution
        ]
        flip-flag solution false

        if find words-of solution 'depends [
            for-each dep (maybe solution/depends) [
                if dep/class = #variable [
                    append vars spread reduce [
                        dep/name
                        any [dep/value, dep/default]
                    ]
                ]
            ]
        ]
    ]

    flip-flag: meth [
        return: <none>
        project [object!]
        to [logic!]
    ][
        all [
            find words-of project 'generated?
            to != project/generated?
        ] then [
            project/generated?: to
            if find words-of project 'depends [
                for-each dep project/depends [
                    flip-flag dep to
                ]
            ]
        ]
    ]

    setup-output: meth [
        return: <none>
        project [object!]
    ][
        assert [project/class]
        let suffix: switch project/class [
            #application [target-platform/exe-suffix]
            #dynamic-library [target-platform/dll-suffix]
            #static-library [target-platform/archive-suffix]
            #object-library [target-platform/archive-suffix]
            #object-file [target-platform/obj-suffix]
        ] else [
            return none
        ]

        case [
            null? project/output [
                switch project/class [
                    #object-file [
                        project/output: copy project/source
                    ]
                    #object-library [
                        project/output: to text! project/name
                    ]

                    fail ["Unexpected project class:" (project/class)]
                ]
                if output-ext: find-last project/output #"." [
                    remove output-ext
                ]

                basename: project/output
                project/output: join basename suffix
            ]
            ends-with? project/output :suffix [
                basename: either :suffix [
                    copy/part project/output
                        (length of project/output) - (length of suffix)
                ][
                    copy project/output
                ]
            ]
        ] else [
            basename: project/output
            project/output: join basename suffix
        ]

        project/basename: basename
    ]

    setup-outputs: meth [
        {Set the output/implib for the project tree}
        return: <none>
        project [object!]
    ][
        ;print ["Setting outputs for:"]
        ;dump project
        switch project/class [
            #application
            #dynamic-library
            #static-library
            #solution
            #object-library [
                if project/generated? [return none]
                setup-output project
                project/generated?: true
                for-each dep project/depends [
                    setup-outputs dep
                ]
            ]
            #object-file [
                setup-output project
            ]
        ] else [return none]
    ]
]

makefile: make generator-class [
    nmake?: false ; Generating for Microsoft nmake

    ;by default makefiles are for POSIX platform
    gen-cmd-create: :posix/gen-cmd-create
    gen-cmd-delete: :posix/gen-cmd-delete
    gen-cmd-strip: :posix/gen-cmd-strip

    gen-rule: meth [
        return: "Possibly multi-line text for rule, with extra newline @ end"
            [text!]
        entry [object!]
    ][
        return newlined collect [switch entry/class [

            ; Makefile variable, defined on a line by itself
            ;
            #variable [
                keep either entry/value [
                    spaced [entry/name "=" entry/value]
                ][
                    spaced [entry/name either nmake? ["="]["?="] entry/default]
                ]
            ]

            #entry [
                ;
                ; First line in a makefile entry is the target followed by
                ; a colon and a list of dependencies.  Usually the target is
                ; a file path on disk, but it can also be a "phony" target
                ; that is just a word:
                ;
                ; https://stackoverflow.com/q/2145590/
                ;
                keep spaced collect [
                    case [
                        word? entry/target [  ; like `clean` in `make clean`
                            keep unspaced [entry/target ":"]
                            keep ".PHONY"
                        ]
                        file? entry/target [
                            keep unspaced [file-to-local entry/target ":"]
                        ]
                        fail ["Unknown entry/target type" entry/target]
                    ]
                    for-each w (maybe entry/depends) [
                        switch select (match object! w else [[]]) 'class [
                            #variable [
                                keep unspaced ["$(" w/name ")"]
                            ]
                            #entry [
                                keep to-text w/target
                            ]
                            #dynamic-extension #static-extension [
                                ; only contribute to command line
                            ]
                        ] else [
                            keep case [
                                file? w [file-to-local w]
                                file? w/output [file-to-local w/output]
                            ] else [w/output]
                        ]
                    ]
                ]

                ; After the line with its target and dependencies are the
                ; lines of shell code that run to build the target.  These
                ; may use escaped makefile variables that get substituted.
                ;
                if entry/commands [
                    for-each cmd (ensure block! entry/commands) [
                        let c: any [
                            match text! cmd
                            gen-cmd cmd
                            continue
                        ]
                        if empty? c [continue]  ; !!! Review why this happens
                        keep unspaced [tab c]  ; makefiles demand TAB :-(
                    ]
                ]
            ]

            fail ["Unrecognized entry class:" entry/class]
        ] keep ""]  ; final keep just adds an extra newline

        ; !!! Adding an extra newline here unconditionally means variables
        ; in the makefile get spaced out, which isn't bad--but it wasn't done
        ; in the original rebmake.r.  This could be rethought to leave it
        ; to the caller to decide to add the spacing line or not
    ]

    emit: meth [
        return: <none>
        buf [binary!]
        project [object!]
        /parent [object!]  ; !!! Not heeded?
    ][
        ;print ["emitting..."]
        ;dump project
        ;if project/generated? [return none]
        ;project/generated?: true

        for-each dep project/depends [
            if not object? dep [continue]
            ;dump dep
            if not find [#dynamic-extension #static-extension] dep/class [
                either dep/generated? [
                    continue
                ][
                    dep/generated?: true
                ]
            ]
            switch dep/class [
                #application
                #dynamic-library
                #static-library [
                    let objs: make block! 8
                    ;dump dep
                    for-each obj dep/depends [
                        ;dump obj
                        if obj/class = #object-library [
                            append objs spread obj/depends
                        ]
                    ]
                    append buf gen-rule make entry-class [
                        target: dep/output
                        depends: join objs spread map-each ddep dep/depends [
                            if ddep/class <> #object-library [ddep]
                        ]
                        commands: append reduce [dep/command] maybe (
                            spread :dep/post-build-commands
                        )
                    ]
                    emit buf dep
                ]
                #object-library [
                    comment [
                        ; !!! Said "No nested object-library-class allowed"
                        ; but was commented out (?)
                        assert [dep/class != #object-library]
                    ]
                    for-each obj dep/depends [
                        assert [obj/class = #object-file]
                        if not obj/generated? [
                            obj/generated?: true
                            append buf (gen-rule apply :obj/gen-entries [
                                dep
                                /PIC (project/class = #dynamic-library)
                            ])
                        ]
                    ]
                ]
                #object-file [
                    append buf gen-rule dep/gen-entries project
                ]
                #entry #variable [
                    append buf gen-rule dep
                ]
                #dynamic-extension #static-extension [
                    _
                ]
                (elide dump dep)
                fail ["unrecognized project type:" dep/class]
            ]
        ]
    ]

    generate: meth [
        return: <none>
        output [file!]
        solution [object!]
    ][
        let buf: make binary! 2048
        assert [solution/class = #solution]

        prepare solution

        emit buf solution

        write output append buf "^/^/.PHONY:"
    ]
]

nmake: make makefile [
    nmake?: true

    ; reset them, so they will be chosen by the target platform
    gen-cmd-create: null
    gen-cmd-delete: null
    gen-cmd-strip: null
]

; For mingw-make on Windows
mingw-make: make makefile [
    ; reset them, so they will be chosen by the target platform
    gen-cmd-create: null
    gen-cmd-delete: null
    gen-cmd-strip: null
]

; Execute the command to generate the target directly
;
export execution: make generator-class [
    host: switch system/platform/1 [
        'Windows [windows]
        'Linux [linux]
        'BeOS [haiku]
        'Haiku [haiku]
        'OSX [osx]
        'Android [android]
    ] else [
        print [
            "Untested platform" system/platform "- assume POSIX compilant"
        ]
        posix
    ]

    gen-cmd-create: :host/gen-cmd-create
    gen-cmd-delete: :host/gen-cmd-delete
    gen-cmd-strip: :host/gen-cmd-strip

    run-target: meth [
        return: <none>
        target [object!]
        /cwd "change working directory"  ; !!! Not heeded (?)
            [file!]
    ][
        switch target/class [
            #variable [
                _  ; already been taken care of by PREPARE
            ]
            #entry [
                if all [
                    not word? target/target
                    ; so you can use words for "phony" targets
                    exists? to-file target/target
                ][
                    return none
                ]  ; TODO: Check the timestamp to see if it needs to be updated
                either block? target/commands [
                    for-each cmd target/commands [
                        cmd: do-substitutions cmd
                        print ["Running:" cmd]
                        call/shell cmd
                    ]
                ][
                    let cmd: do-substitutions target/commands
                    print ["Running:" cmd]
                    call/shell cmd
                ]
            ]
            (elide dump target)
            fail "Unrecognized target class"
        ]
    ]

    run: meth [
        return: <none>
        project [object!]
        /parent "parent project"
            [object!]
    ][
        ;dump project
        if not object? project [return none]

        prepare project

        if not find [#dynamic-extension #static-extension] project/class [
            if project/generated? [return none]
            project/generated?: true
        ]

        switch project/class [
            #application
            #dynamic-library
            #static-library [
                let objs: make block! 8
                for-each obj project/depends [
                    if obj/class = #object-library [
                        append objs spread obj/depends
                    ]
                ]
                for-each dep project/depends [
                    run/parent dep project
                ]
                run-target make entry-class [
                    target: project/output
                    depends: join project/depends spread objs
                    commands: reduce [project/command]
                ]
            ]
            #object-library [
                for-each obj project/depends [
                    assert [obj/class = #object-file]
                    if not obj/generated? [
                        obj/generated?: true
                        run-target apply :obj/gen-entries [
                            project
                            /PIC (parent/class = #dynamic-library)
                        ]
                    ]
                ]
            ]
            #object-file [
                assert [parent]
                run-target project/gen-entries p-project
            ]
            #entry #variable [
                run-target project
            ]
            #dynamic-extension #static-extension [
                _
            ]
            #solution [
                for-each dep project/depends [
                    run dep
                ]
            ]
            (elide dump project)
            fail ["unrecognized project type:" project/class]
        ]
    ]
]
