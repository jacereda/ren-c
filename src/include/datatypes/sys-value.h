//
//  File: %sys-value.h
//  Summary: {any-value! defs AFTER %tmp-internals.h (see: %sys-rebval.h)}
//  Project: "Rebol 3 Interpreter and Run-time (Ren-C branch)"
//  Homepage: https://github.com/metaeducation/ren-c/
//
//=////////////////////////////////////////////////////////////////////////=//
//
// Copyright 2012-2020 Ren-C Open Source Contributors
// Copyright 2012 REBOL Technologies
// REBOL is a trademark of REBOL Technologies
//
// See README.md and CREDITS.md for more information
//
// Licensed under the Lesser GPL, Version 3.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// https://www.gnu.org/licenses/lgpl-3.0.html
//
//=////////////////////////////////////////////////////////////////////////=//
//
// This file provides basic accessors for value types.  Because these
// accessors dereference REBVAL (or RELVAL) pointers, the inline functions
// need the complete struct definition available from all the payload types.
//
// See notes in %sys-rebval.h for the definition of the REBVAL structure.
//
// While some REBVALs are in C stack variables, most reside in the allocated
// memory block for a Rebol array.  The memory block for an array can be
// resized and require a reallocation, or it may become invalid if the
// containing series is garbage-collected.  This means that many pointers to
// REBVAL are unstable, and could become invalid if arbitrary user code
// is run...this includes values on the data stack, which is implemented as
// an array under the hood.  (See %sys-stack.h)
//
// A REBVAL in a C stack variable does not have to worry about its memory
// address becoming invalid--but by default the garbage collector does not
// know that value exists.  So while the address may be stable, any series
// it has in the payload might go bad.  Use PUSH_GC_GUARD() to protect a
// stack variable's payload, and then DROP_GC_GUARD() when the protection
// is not needed.  (You must always drop the most recently pushed guard.)
//
// Function invocations keep their arguments in FRAME!s, which can be accessed
// via ARG() and have stable addresses as long as the function is running.
//


//=//// DEBUG PROBE <== **THIS IS VERY USEFUL** //////////////////////////=//
//
// The PROBE macro can be used in debug builds to mold a REBVAL much like the
// Rebol `probe` operation.  But it's actually polymorphic, and if you have
// a REBSER*, REBCTX*, or REBARR* it can be used with those as well.  In C++,
// you can even get the same value and type out as you put in...just like in
// Rebol, permitting things like `return PROBE(Make_Some_Series(...));`
//
// In order to make it easier to find out where a piece of debug spew is
// coming from, the file and line number will be output as well.
//
// Note: As a convenience, PROBE also flushes the `stdout` and `stderr` in
// case the debug build was using printf() to output contextual information.
//

#if defined(DEBUG_HAS_PROBE)
    #ifdef CPLUSPLUS_11
        template <
            typename T,
            typename std::enable_if<
                std::is_pointer<T>::value  // assume pointers are REBNOD*
            >::type* = nullptr
        >
        T Probe_Cpp_Helper(T v, const char *expr, const char *file, int line)
        {
            Probe_Core_Debug(v, expr, file, line);
            return v;
        }

        template <
            typename T,
            typename std::enable_if<
                !std::is_pointer<T>::value  // ordinary << output operator
            >::type* = nullptr
        >
        T Probe_Cpp_Helper(T v, const char *expr, const char *file, int line)
        {
            std::stringstream ss;
            ss << v;
            printf("PROBE(%s) => %s\n", expr, ss.str().c_str());
            UNUSED(file);
            UNUSED(line);
            return v;
        }

        #define PROBE(v) \
            Probe_Cpp_Helper((v), #v, __FILE__, __LINE__)
    #else
        #define PROBE(v) \
            Probe_Core_Debug((v), #v, __FILE__, __LINE__)  // returns void*
    #endif
#elif !defined(NDEBUG) // don't cause compile time error on PROBE()
    #define PROBE(v) \
        do { \
            printf("DEBUG_HAS_PROBE disabled %s %d\n", __FILE__, __LINE__); \
            fflush(stdout); \
        } while (0)
#endif


//=//// CELL WRITABILITY //////////////////////////////////////////////////=//
//
// Asserting writablity helps avoid very bad catastrophies that might ensue
// if "implicit end markers" could be overwritten.  These are the ENDs that
// are actually other bitflags doing double duty inside a data structure, and
// there is no REBVAL storage backing the position.
//
// (A fringe benefit is catching writes to other unanticipated locations.)
//

#if defined(DEBUG_CELL_WRITABILITY)
    //
    // In the debug build, functions aren't inlined, and the overhead actually
    // adds up very quickly.  Run the risk of repeating macro arguments to
    // speed up this critical test, then wrap in READABLE() and WRITABLE()
    // functions for higher-level callers that don't mind the cost.
    //
    // Note this isn't in a `do {...} while (0)` either, also for speed.

    #define ASSERT_CELL_READABLE_EVIL_MACRO(c) \
        if ( \
            (FIRST_BYTE((c)->header) & ( \
                NODE_BYTEMASK_0x01_CELL | NODE_BYTEMASK_0x80_NODE \
                    | NODE_BYTEMASK_0x40_FREE \
            )) != 0x81 \
        ){ \
            if (not ((c)->header.bits & NODE_FLAG_CELL)) \
                printf("Non-cell passed to cell read/write routine\n"); \
            else if (not ((c)->header.bits & NODE_FLAG_NODE)) \
                printf("Non-node passed to cell read/write routine\n"); \
            else \
                printf("Free node passed to cell read/write routine\n"); \
            panic (c); \
        }

    #define ASSERT_CELL_WRITABLE_EVIL_MACRO(c) \
        ASSERT_CELL_READABLE_EVIL_MACRO(c); \
        if ((c)->header.bits & CELL_FLAG_PROTECTED) { \
            printf("Protected cell passed to writing routine\n"); \
            panic (c); \
        }

    inline static unstable REBCEL(const*) READABLE(unstable REBCEL(const*) c) {
        ASSERT_CELL_READABLE_EVIL_MACRO(c);
        return c;
    }

    inline static unstable RELVAL *WRITABLE(unstable RELVAL *c) {
        ASSERT_CELL_WRITABLE_EVIL_MACRO(c);
        return c;
    }
#else
    #define ASSERT_CELL_READABLE_EVIL_MACRO(c)    NOOP
    #define ASSERT_CELL_WRITABLE_EVIL_MACRO(c)    NOOP

    #define READABLE(c) (c)
    #define WRITABLE(c) (c)
#endif


//=//// "KIND3Q" HEADER BYTE [REB_XXX + (n * REB_64)] /////////////////////=//
//
// The "kind" of fundamental datatype a cell is lives in the second byte for
// a very deliberate reason.  This means that the signal for an end can be
// a zero byte, allow a C string that is one character long (plus zero
// terminator) to function as an end signal...using only two bytes, while
// still not conflicting with arbitrary UTF-8 strings (including empty ones).
//
// An additional trick is that while there are only up to 64 fundamental types
// in the system (including END), higher values in the byte are used to encode
// escaping levels.  Up to 3 encoding levels can be in the cell itself, with
// additional levels achieved with REB_QUOTED and pointing to another cell.
//
// The "3Q" in the name is to remind usage sites that the byte may contain
// "up to 3 levels of quoting", in addition to the "KIND", which can be masked
// out with `% REB_64`.  (Be sure to use REB_64 for this purpose instead of
// just `64`, to make it easier to find places that are doing this.)
//

#define FLAG_KIND3Q_BYTE(kind) \
    FLAG_SECOND_BYTE(kind)

#define KIND3Q_BYTE_UNCHECKED(v) \
    SECOND_BYTE((v)->header)

#if defined(NDEBUG)
    #define KIND3Q_BYTE KIND3Q_BYTE_UNCHECKED
#else
    inline static REBYTE KIND3Q_BYTE_Debug(
        unstable const RELVAL *v,
        const char *file,
        int line
    ){
        if (
            (v->header.bits & (
                NODE_FLAG_NODE
                | NODE_FLAG_CELL
                | NODE_FLAG_FREE
            )) == (NODE_FLAG_CELL | NODE_FLAG_NODE)
        ){
            // Unreadable void is signified in the Extra by a negative tick
            //
            if (KIND3Q_BYTE_UNCHECKED(v) == REB_VOID) {
                if (v->payload.Any.first.node == nullptr) {
                    printf("KIND3Q_BYTE() called on unreadable VOID!\n");
                  #ifdef DEBUG_COUNT_TICKS
                    printf("Made on tick: %d\n", cast(int, v->extra.tick));
                  #endif
                    panic_at (v, file, line);
                }
                return REB_VOID;
            }

            return KIND3Q_BYTE_UNCHECKED(v);  // majority return here
        }

        // Non-cells are allowed to signal REB_END; see Init_Endlike_Header.
        // (We should not be seeing any rebEND signals here, because we have
        // a REBVAL*, and rebEND is a 2-byte character string that can be
        // at any alignment...not necessarily that of a Reb_Header union!)
        //
        if (KIND3Q_BYTE_UNCHECKED(v) == REB_0_END)
            if (v->header.bits & NODE_FLAG_NODE)
                return REB_0_END;

        if (not (v->header.bits & NODE_FLAG_CELL)) {
            printf("KIND3Q_BYTE() called on non-cell\n");
            panic_at (v, file, line);
        }
        if (v->header.bits & NODE_FLAG_FREE) {
            printf("KIND3Q_BYTE() called on invalid cell--marked FREE\n");
            panic_at (v, file, line);
        }
        return KIND3Q_BYTE_UNCHECKED(v);
    }

  #ifdef CPLUSPLUS_11
    //
    // Since KIND3Q_BYTE() is used by checks like IS_WORD() for efficiency, we
    // don't want that to contaminate the use with cells, because if you
    // bothered to change the type view to a cell you did so because you
    // were interested in its unquoted kind.
    //
    inline static REBYTE KIND3Q_BYTE_Debug(
        unstable REBCEL(const*) v,
        const char *file,
        int line
    ) = delete;
  #endif

    #define KIND3Q_BYTE(v) \
        KIND3Q_BYTE_Debug((v), __FILE__, __LINE__)
#endif

// Note: Only change bits of existing cells if the new type payload matches
// the type and bits (e.g. ANY-WORD! to another ANY-WORD!).  Otherwise the
// value-specific flags might be misinterpreted.
//
#define mutable_KIND3Q_BYTE(v) \
    mutable_SECOND_BYTE(WRITABLE(v)->header)


// A cell may have a larger KIND3Q_BYTE() than legal Reb_Kind to represent a
// literal in-situ...the actual kind comes from that byte % 64.  But if you
// are interested in the kind of *cell* (for purposes of knowing its bit
// layout expectations) that is stored in the HEART_BYTE().

inline static REBCEL(const*) VAL_UNESCAPED(const RELVAL *v);

#define CELL_KIND_UNCHECKED(cell) \
    cast(enum Reb_Kind, KIND3Q_BYTE_UNCHECKED(cell) % REB_64)

#define CELL_HEART_UNCHECKED(cell) \
    cast(enum Reb_Kind, HEART_BYTE(cell))


#if defined(NDEBUG) or !defined(CPLUSPLUS_11)
    #define CELL_KIND CELL_KIND_UNCHECKED
    #define CELL_HEART CELL_HEART_UNCHECKED
#else
    inline static enum Reb_Kind CELL_KIND(unstable REBCEL(const*) cell)
        { return CELL_KIND_UNCHECKED(cell); }

    inline static enum Reb_Kind CELL_HEART(unstable REBCEL(const*) cell)
      { return CELL_HEART_UNCHECKED(cell); }

    // We want to disable asking for low level implementation details on a
    // cell that may be a REB_QUOTED; you have to call VAL_UNESCAPED() first.
    //
    inline static enum Reb_Kind CELL_KIND(unstable const RELVAL *v) = delete;
    inline static enum Reb_Kind CELL_HEART(unstable const RELVAL *v) = delete;
#endif

inline static REBTYP *CELL_CUSTOM_TYPE(unstable REBCEL(const*) v) {
    assert(CELL_KIND(v) == REB_CUSTOM);
    return SER(EXTRA(Any, v).node);
}

// Sometimes you have a REBCEL* and need to pass a REBVAL* to something.  It
// doesn't seem there's too much bad that can happen if you do; you'll get
// back something that might be quoted up to 3 levels...if it's an escaped
// cell then it won't be quoted at all.  Main thing to know is that you don't
// necessarily get the original value you had back.
//
inline static const RELVAL* CELL_TO_VAL(unstable_ok REBCEL(const*) cell)
  { return cast(const RELVAL*, cell); }

#ifdef CPLUSPLUS_11
    inline static const RELVAL* CELL_TO_VAL(const RELVAL* cell) = delete;
#endif

#ifdef DEBUG_UNSTABLE_CELLS
    inline static const unstable RELVAL* CELL_TO_VAL(
        unstable REBCEL(const*) cell
    ){
        return cast(unstable const RELVAL*, cell);
    }
#endif


//=//// VALUE TYPE (always REB_XXX <= REB_MAX) ////////////////////////////=//
//
// When asking about a value's "type", you want to see something like a
// double-quoted WORD! as a QUOTED! value...despite the kind byte being
// REB_WORD + REB_64 + REB_64.  Use CELL_KIND() if you wish to know that the
// cell pointer you pass in is carrying a word payload, it does a modulus.
//
// This has additional checks as well, that you're not using "pseudotypes"
// or garbage, or REB_0_END (which should be checked separately with IS_END())
//

#if defined(NDEBUG)
    inline static enum Reb_Kind VAL_TYPE(unstable const RELVAL *v) {
        if (KIND3Q_BYTE(v) >= REB_64)
            return REB_QUOTED;
        return cast(enum Reb_Kind, KIND3Q_BYTE(v));
    }
#else
    inline static enum Reb_Kind VAL_TYPE_Debug(
        unstable const RELVAL *v,
        const char *file,
        int line
    ){
        REBYTE kind_byte = KIND3Q_BYTE(v);

        // Special messages for END and trash (as these are common)
        //
        if (kind_byte == REB_0_END) {
            printf("VAL_TYPE() on END marker (use IS_END() or KIND3Q_BYTE())\n");
            panic_at (v, file, line);
        }
        if (kind_byte % REB_64 >= REB_MAX) {
            printf("VAL_TYPE() on pseudotype/garbage (use KIND3Q_BYTE())\n");
            panic_at (v, file, line);
        }

        if (kind_byte >= REB_64)
            return REB_QUOTED;
        return cast(enum Reb_Kind, kind_byte);
    }

    #define VAL_TYPE(v) \
        VAL_TYPE_Debug(v, __FILE__, __LINE__)
#endif


//=//// GETTING, SETTING, and CLEARING VALUE FLAGS ////////////////////////=//
//
// The header of a cell contains information about what kind of cell it is,
// as well as some flags that are reserved for system purposes.  These are
// the NODE_FLAG_XXX and CELL_FLAG_XXX flags, that work on any cell.
//
// (A previous concept where cells could use some of the header bits to carry
// more data that wouldn't fit in the "extra" or "payload" is deprecated.
// If those three pointers are not enough for the data a type needs, then it
// has to use an additional allocation and point to that.)
//

#define SET_CELL_FLAG(v,name) \
    (WRITABLE(v)->header.bits |= CELL_FLAG_##name)

#define GET_CELL_FLAG(v,name) \
    ((READABLE(v)->header.bits & CELL_FLAG_##name) != 0)

#define CLEAR_CELL_FLAG(v,name) \
    (WRITABLE(v)->header.bits &= ~CELL_FLAG_##name)

#define NOT_CELL_FLAG(v,name) \
    ((READABLE(v)->header.bits & CELL_FLAG_##name) == 0)


//=//// CELL HEADERS AND PREPARATION //////////////////////////////////////=//
//
// RESET_VAL_HEADER clears out the header of *most* bits, setting it to a
// new type.  The type takes up the full second byte of the header (see
// details in %sys-quoted.h for how this byte is used).
//
// RESET_CELL is a variant of RESET_VAL_HEADER that overwrites the entire
// cell payload with tracking information.  It should not be used if the
// intent is to preserve the payload and extra.
//
// (Note: RESET_CELL is used more often than it should right now in Init_XXX()
// routines.  They should be phrased in terms of macros that pass in the
// result of TRACK_CELL_IF_DEBUG() or TRACK_CELL_IF_EXTEND_DEBUG() at the
// init callsite, so it captures the right __FILE__ and __LINE__.  This is
// an ongoing cleanup effort, to be pursued as those routines are audited.)
//
// The value is expected to already be "pre-formatted" with the NODE_FLAG_CELL
// bit, so that is left as-is.  See also CELL_MASK_PERSIST.
//

inline static REBVAL *RESET_VAL_HEADER(
    unstable RELVAL *v,
    enum Reb_Kind k,
    uintptr_t extra
){
    ASSERT_CELL_WRITABLE_EVIL_MACRO(v);
    v->header.bits &= CELL_MASK_PERSIST;
    v->header.bits |= FLAG_KIND3Q_BYTE(k) | FLAG_HEART_BYTE(k) | extra;
    return cast(REBVAL*, v);
}

#define RESET_CELL(out,kind,flags) \
    RESET_VAL_HEADER(TRACK_CELL_IF_DEBUG(out), (kind), (flags))

inline static REBVAL *RESET_CUSTOM_CELL(
    unstable RELVAL *out,
    REBTYP *type,
    REBFLGS flags
){
    RESET_CELL(out, REB_CUSTOM, flags);
    EXTRA(Any, out).node = NOD(type);
    return cast(REBVAL*, out);
}


// See notes on ALIGN_SIZE regarding why we check this, and when it does and
// does not apply (some platforms need this invariant for `double` to work).
//
// This is another case where the debug build doesn't inline functions.
// Run the risk of repeating macro args to speed up this critical check.
//
#ifdef DEBUG_DONT_CHECK_ALIGN
    #define ALIGN_CHECK_CELL_EVIL_MACRO(c)    NOOP
#else
    #define ALIGN_CHECK_CELL_EVIL_MACRO(c) \
        if (cast(uintptr_t, (c)) % ALIGN_SIZE != 0) { \
            printf( \
                "Cell address %p not aligned to %d bytes\n", \
                cast(const void*, (c)), \
                cast(int, ALIGN_SIZE) \
            ); \
            panic (c); \
        }
#endif


#define CELL_MASK_PREP \
    (NODE_FLAG_NODE | NODE_FLAG_CELL)

#define CELL_MASK_PREP_END \
    (CELL_MASK_PREP \
        | FLAG_KIND3Q_BYTE(REB_0) \
        | FLAG_HEART_BYTE(REB_0))  // a more explicit CELL_MASK_PREP

inline static RELVAL *Prep_Cell_Core(unstable_ok RELVAL *c) {
    ALIGN_CHECK_CELL_EVIL_MACRO(c);
    c->header.bits = CELL_MASK_PREP;
    return c;
}

#define Prep_Cell(c) \
    Prep_Cell_Core(TRACK_CELL_IF_DEBUG(c))


//=//// TRASH CELLS ///////////////////////////////////////////////////////=//
//
// Trash is a cell (marked by NODE_FLAG_CELL) with NODE_FLAG_FREE set.  To
// prevent it from being inspected while it's in an invalid state, VAL_TYPE
// used on a trash cell will assert in the debug build.
//
// The garbage collector is not tolerant of trash.
//

#if defined(DEBUG_TRASH_MEMORY)

    #define TRASH_VALUE \
        cast(const REBVAL*, &PG_Trash_Value_Debug)

    inline static RELVAL *Init_Trash_Debug(unstable_ok RELVAL *v) {
        ASSERT_CELL_WRITABLE_EVIL_MACRO(v);
        v->header.bits &= CELL_MASK_PERSIST;
        v->header.bits |=
            FLAG_KIND3Q_BYTE(REB_T_TRASH)
                | FLAG_HEART_BYTE(REB_T_TRASH);
        return v;
    }

  #ifdef DEBUG_UNSTABLE_CELLS
    inline static unstable RELVAL *Init_Trash_Debug(unstable RELVAL *v) {
        ASSERT_CELL_WRITABLE_EVIL_MACRO(v);
        v->header.bits &= CELL_MASK_PERSIST;
        v->header.bits |=
            FLAG_KIND3Q_BYTE(REB_T_TRASH)
                | FLAG_HEART_BYTE(REB_T_TRASH);
        return v;
    }
  #endif

    #define TRASH_CELL_IF_DEBUG(v) \
        Init_Trash_Debug(TRACK_CELL_IF_DEBUG(v))

    inline static bool IS_TRASH_DEBUG(const RELVAL *v) {
        assert(v->header.bits & NODE_FLAG_CELL);
        return KIND3Q_BYTE_UNCHECKED(v) == REB_T_TRASH;
    }
#else
    inline static REBVAL *TRASH_CELL_IF_DEBUG(RELVAL *v) {
        return cast(REBVAL*, v); // #define of (v) gives compiler warnings
    } // https://stackoverflow.com/q/29565161/
#endif


//=//// END MARKER ////////////////////////////////////////////////////////=//
//
// Historically Rebol arrays were always one value longer than their maximum
// content, and this final slot was used for a REBVAL type called END!.
// Like a '\0' terminator in a C string, it was possible to start from one
// point in the series and traverse to find the end marker without needing
// to look at the length (though the length in the series header is maintained
// in sync, also).
//
// Ren-C changed this so that END is not a user-exposed data type, and that
// it's not a requirement for the byte sequence containing the end byte be
// the full size of a cell.  The type byte (which is 0 for an END) lives in
// the second byte, hence two bytes are sufficient to indicate a terminator.
//

#define END_NODE \
    cast(const REBVAL*, &PG_End_Node) // rebEND is char*, not REBVAL* aligned!

#if defined(DEBUG_TRACK_CELLS) || defined(DEBUG_CELL_WRITABILITY)
    inline static REBVAL *SET_END_Debug(RELVAL *v) {
        ASSERT_CELL_WRITABLE_EVIL_MACRO(v);

        mutable_KIND3Q_BYTE(v) = REB_0_END; // release build behavior

        // Detection of END is designed to only be signaled by one byte.
        // See the definition of `rebEND` for how this is used to make a
        // small C string signal, and Init_Endlike_Header() for how a singular
        // array sacrifices just one byte of its SERIES_INFO bits to signal
        // an END to its contents.  Hence you cannot count on the heart
        // byte being anything in an END cell.  Set to trash in debug.
        //
        mutable_HEART_BYTE(v) = REB_T_TRASH;
        return cast(REBVAL*, v);
    }

    #define SET_END(v) \
        SET_END_Debug(TRACK_CELL_IF_DEBUG(v))
#else
    inline static REBVAL *SET_END(RELVAL *v) {
        mutable_KIND3Q_BYTE(v) = REB_0_END; // must be a prepared cell
        return cast(REBVAL*, v);
    }
#endif

// IS_END()/NOT_END() are called *a lot*, and adding costly checks to it will
// slow down the debug build dramatically--taking up to 10% of the total time.
// Hence DEBUG_CHECK_ENDS is disabled in the default debug build.
//
#if !defined(DEBUG_CHECK_ENDS)
    #define IS_END(p) \
        (((const REBYTE*)(p))[1] == REB_0_END)  // Note: needs (p) parens!
#else
    inline static bool IS_END_Debug(
        const void *p,  // may not have NODE_FLAG_CELL, maybe short as 2 bytes
        const char *file,
        int line
    ){
        if (((const REBYTE*)(p))[0] & NODE_BYTEMASK_0x40_FREE) {
            printf("NOT_END() called on garbage\n");
            panic_at(p, file, line);
        }

        if (((const REBYTE*)(p))[1] == REB_0_END)
            return true;

        if (not (((const REBYTE*)(p))[0] & NODE_BYTEMASK_0x01_CELL)) {
            printf("IS_END() found non-END pointer that's not a cell\n");
            panic_at(p, file, line);
        }

        return false;
    }

    #define IS_END(v) \
        IS_END_Debug((v), __FILE__, __LINE__)
#endif

#define NOT_END(v) \
    (not IS_END(v))


//=////////////////////////////////////////////////////////////////////////=//
//
//  RELATIVE AND SPECIFIC VALUES
//
//=////////////////////////////////////////////////////////////////////////=//
//
// Some value types use their `->extra` field in order to store a pointer to
// a REBNOD which constitutes their notion of "binding".
//
// This can be null (which indicates unbound), to a function's paramlist
// (which indicates a relative binding), or to a context's varlist (which
// indicates a specific binding.)
//
// The ordering of %types.r is chosen specially so that all bindable types
// are at lower values than the unbindable types.
//


// Note: If incoming p is mutable, we currently assume that's allowed by the
// flag bits of the node.  This could have a runtime check in debug build
// with a C++ variation that only takes mutable pointers.
//
inline static void INIT_VAL_NODE(unstable RELVAL *v, const void *p) {
    assert(GET_CELL_FLAG(v, FIRST_IS_NODE));
    PAYLOAD(Any, v).first.node = NOD(m_cast(void*, p));
}

#define VAL_NODE(v) \
    PAYLOAD(Any, (v)).first.node


// An ANY-WORD! is relative if it refers to a local or argument of a function,
// and has its bits resident in the deep copy of that function's body.
//
// An ANY-ARRAY! in the deep copy of a function body must be relative also to
// the same function if it contains any instances of such relative words.
//
inline static bool IS_RELATIVE(unstable REBCEL(const*) v) {
    if (not Is_Bindable(v) or not EXTRA(Binding, v).node)
        return false; // INTEGER! and other types are inherently "specific"

    if (EXTRA(Binding, v).node->header.bits & ARRAY_FLAG_IS_DETAILS)
        return true;

    return false;
}

#ifdef CPLUSPLUS_11
    bool IS_RELATIVE(const REBVAL *v) = delete;  // error on superfluous check
#endif

#define IS_SPECIFIC(v) \
    (not IS_RELATIVE(v))

inline static REBACT *VAL_RELATIVE(unstable const RELVAL *v) {
    assert(IS_RELATIVE(v));
    return ACT(EXTRA(Binding, v).node);
}

// When you have a RELVAL* (e.g. from a REBARR) that you KNOW to be specific,
// you might be bothered by an error like:
//
//     "invalid conversion from 'Reb_Value*' to 'Reb_Specific_Value*'"
//
// You can use SPECIFIC to cast it if you are *sure* that it has been
// derelativized -or- is a value type that doesn't have a specifier (e.g. an
// integer).  If the value is actually relative, this will assert at runtime!
//
// Because SPECIFIC has cost in the debug build, there may be situations where
// one is sure that the value is specific, and `cast(REBVAL*, v`) is a better
// choice for efficiency.  This applies to things like `Move_Value()`, which
// is called often and already knew its input was a REBVAL* to start with.

inline static REBVAL *SPECIFIC(const_if_c RELVAL *v) {
    //
    // Note: END is tolerated to help in specified array enumerations, e.g.
    //
    //     REBVAL *head = SPECIFIC(ARR_HEAD(specified_array));  // may be end
    //
    assert(IS_END(v) or IS_SPECIFIC(v));
    return m_cast(REBVAL*, cast(const REBVAL*, v));
}

#if defined(__cplusplus)
    inline static const REBVAL *SPECIFIC(const RELVAL *v) {
        assert(IS_END(v) or IS_SPECIFIC(v));  // ^-- see note about END
        return cast(const REBVAL*, v);
    }

  #ifdef DEBUG_UNSTABLE_CELLS
    inline static unstable REBVAL *SPECIFIC(unstable RELVAL *v) {
        assert(IS_END(v) or IS_SPECIFIC(v));  // ^-- see note about END
        return cast(REBVAL*, v);
    }

    inline static unstable const REBVAL *SPECIFIC(unstable const RELVAL *v) {
        assert(IS_END(v) or IS_SPECIFIC(v));  // ^-- see note about END
        return cast(const REBVAL*, v);
    }
  #endif

    inline static REBVAL *SPECIFIC(const REBVAL *v) = delete;
#endif



//=////////////////////////////////////////////////////////////////////////=//
//
//  BINDING
//
//=////////////////////////////////////////////////////////////////////////=//
//
// Some value types use their `->extra` field in order to store a pointer to
// a REBNOD which constitutes their notion of "binding".
//
// This can either be null (a.k.a. UNBOUND), or to a function's paramlist
// (indicates a relative binding), or to a context's varlist (which indicates
// a specific binding.)
//
// NOTE: Instead of using null for UNBOUND, a special global REBSER struct was
// experimented with.  It was at a location in memory known at compile time,
// and it had its ->header and ->info bits set in such a way as to avoid the
// need for some conditional checks.  e.g. instead of writing:
//
//     if (binding and binding->header.bits & NODE_FLAG_MANAGED) {...}
//
// The special UNBOUND node set some bits, such as to pretend to be managed:
//
//     if (binding->header.bits & NODE_FLAG_MANAGED) {...} // incl. UNBOUND
//
// Question was whether avoiding the branching involved from the extra test
// for null would be worth it for a consistent ability to dereference.  At
// least on x86/x64, the answer was: No.  It was maybe even a little slower.
// Testing for null pointers the processor has in its hand is very common and
// seemed to outweigh the need to dereference all the time.  The increased
// clarity of having unbound be nullptr is also in its benefit.
//
// NOTE: The ordering of %types.r is chosen specially so that all bindable
// types are at lower values than the unbindable types.
//

#define SPECIFIED \
    ((REBSPC*)nullptr)  // cast() doesn't like nullptr, fix

#define UNBOUND nullptr  // not always a REBNOD* (sometimes REBCTX)
#define UNSPECIFIED nullptr


#define VAL_WORD_BINDING_NODE(v)        EXTRA(Binding, (v)).node

#define VAL_WORD_CACHE_NODE(v) \
    PAYLOAD(Any, m_cast(RELVAL*, cast(const RELVAL*, (v)))).first.node

#define MONDEX_MOD 4095  // modulus for the cached index modulus ("mondex")
#define VAL_WORD_INDEXES_U32(v)         PAYLOAD(Any, (v)).second.u32


inline static REBNOD *VAL_WORD_BINDING(unstable const RELVAL *v) {
    assert(ANY_WORD_KIND(CELL_HEART(VAL_UNESCAPED(v))));
    REBNOD *binding = VAL_WORD_BINDING_NODE(v);
    if (binding->header.bits & SERIES_FLAG_IS_STRING)
        return UNBOUND;
    return VAL_WORD_BINDING_NODE(v);
}

inline static void INIT_VAL_WORD_BINDING(unstable RELVAL *v, const void *p) {
    assert(ANY_WORD_KIND(CELL_HEART(VAL_UNESCAPED(v))));

    const REBNOD *binding = cast(const REBNOD*, p);
    assert(binding);  // can't set word bindings to nullptr
    VAL_WORD_BINDING_NODE(v) = m_cast(REBNOD*, binding);

  #if !defined(NDEBUG)
    if (binding->header.bits & SERIES_FLAG_IS_STRING)
        return;  // e.g. UNBOUND (words use strings to indicate unbounds)

    assert(not (binding->header.bits & NODE_FLAG_CELL));  // not currently used

    if (binding->header.bits & NODE_FLAG_MANAGED) {
        assert(
            binding->header.bits & ARRAY_FLAG_IS_DETAILS  // relative
            or binding->header.bits & ARRAY_FLAG_IS_VARLIST  // specific
        );
    }
    else
        assert(binding->header.bits & ARRAY_FLAG_IS_VARLIST);
  #endif
}


inline static void INIT_SPECIFIER(unstable RELVAL *v, const void *p) {
    //
    // can be called on non-bindable series, but p must be nullptr

    const REBNOD *binding = cast(const REBNOD*, p);
    EXTRA(Binding, v).node = m_cast(REBNOD*, binding);

  #if !defined(NDEBUG)
    if (not binding or (binding->header.bits & SERIES_FLAG_IS_STRING))
        return;  // e.g. UNBOUND (words use strings to indicate unbounds)

    assert(Is_Bindable(v));  // works on partially formed values

    assert(not (binding->header.bits & NODE_FLAG_CELL));  // not currently used

    if (binding->header.bits & NODE_FLAG_MANAGED) {
        assert(
            binding->header.bits & ARRAY_FLAG_IS_DETAILS  // relative
            or binding->header.bits & ARRAY_FLAG_IS_VARLIST  // specific
            or (
                ANY_ARRAY(v)
                and (binding->header.bits & ARRAY_FLAG_IS_PATCH)  // virtual
            ) or (
                IS_VARARGS(v) and not IS_SER_DYNAMIC(binding)
            )  // varargs from MAKE VARARGS! [...], else is a varlist
        );
    }
    else
        assert(binding->header.bits & ARRAY_FLAG_IS_VARLIST);
  #endif
}


inline static void Move_Value_Header(
    unstable RELVAL *out,
    unstable const RELVAL *v
){
    assert(out != v);  // usually a sign of a mistake; not worth supporting
    assert(KIND3Q_BYTE_UNCHECKED(v) != REB_0_END);  // faster than NOT_END()

    // Note: Pseudotypes are legal to move, but none of them are bindable

    ASSERT_CELL_WRITABLE_EVIL_MACRO(out);

    out->header.bits &= CELL_MASK_PERSIST;
    out->header.bits |= v->header.bits & CELL_MASK_COPY;

  #ifdef DEBUG_TRACK_EXTEND_CELLS
    out->track = v->track;
    out->tick = TG_Tick;  // initialization tick
    out->touch = v->touch;  // arbitrary debugging use via TOUCH_CELL
  #endif
}


// Because you cannot assign REBVALs to one another (e.g. `*dest = *src`)
// a function is used.  This provides an opportunity to check things like
// moving data into protected locations, and to mask out bits that should
// not be propagated.
//
// Interface designed to line up with Derelativize()
//
inline static REBVAL *Move_Value(
    unstable_ok RELVAL *out,
    unstable const REBVAL *v
){
    Move_Value_Header(out, v);

    // Payloads cannot hold references to stackvars, raw bit transfer ok.
    //
    // Note: must be copied over *before* INIT_BINDING_MAY_MANAGE is called,
    // so that if it's a REB_QUOTED it can find the literal->cell.
    //
    out->payload = STABLE(v)->payload;

    if (Is_Bindable(v))  // extra is either a binding or a plain C value/ptr
        INIT_BINDING_MAY_MANAGE(out, EXTRA(Binding, v).node);
    else
        out->extra = STABLE(v)->extra;

    return cast(REBVAL*, out);
}

#ifdef DEBUG_UNSTABLE_CELLS
    inline static REBVAL *Move_Value(
        unstable RELVAL *out,
        unstable const REBVAL *v
    ){
        return Move_Value(STABLE(out), v);
    }
#endif

// When doing something like a COPY of an OBJECT!, the var cells have to be
// handled specially, e.g. by preserving CELL_FLAG_ARG_MARKED_CHECKED.
//
// !!! What about other non-copyable properties like CELL_FLAG_PROTECTED?
//
inline static REBVAL *Move_Var(RELVAL *out, const REBVAL *v)
{
    // This special kind of copy can only be done into another object's
    // variable slot. (Since the source may be a FRAME!, v *might* be stack
    // but it should never be relative.  If it's stack, we have to go through
    // the whole potential reification process...double-set header for now.)

    Move_Value(out, v);
    out->header.bits |= (
        v->header.bits & (CELL_FLAG_ARG_MARKED_CHECKED)
    );
    return cast(REBVAL*, out);
}


// Generally speaking, you cannot take a RELVAL from one cell and copy it
// blindly into another...it needs to be Derelativize()'d.  This routine is
// for the rare cases where it's legal, e.g. shuffling a cell from one place
// in an array to another cell in the same array.
//
inline static RELVAL *Blit_Relative(
    unstable_ok RELVAL *out,
    unstable const RELVAL *v
){
    // It's imaginable that you might try to blit a cell from a source that
    // could be an API node.  But it should never be *actually* relative
    // (just tunneled down through some chain of RELVAL*-accepting functions).
    //
    assert(not ((v->header.bits & NODE_FLAG_ROOT) and IS_RELATIVE(v)));

    // However, you should not write relative bits into API destinations,
    // not even hypothetically.  The target should not be an API cell.
    //
    assert(not (out->header.bits & (NODE_FLAG_ROOT | NODE_FLAG_MANAGED)));

    Move_Value_Header(out, v);

    out->payload = STABLE(v)->payload;
    out->extra = STABLE(v)->extra;

    return out;  // still (potentially) relative!
}

#ifdef DEBUG_UNSTABLE_CELLS
    inline static unstable RELVAL *Blit_Relative(
        unstable RELVAL *out,
        unstable const RELVAL *v
    ){
        return Blit_Relative(STABLE(out), v);
    }
#endif

#ifdef __cplusplus  // avoid blitting into REBVAL*, and use without specifier
    inline static RELVAL *Blit_Relative(REBVAL *out, const RELVAL *v) = delete;
#endif


// !!! Should this replace Move_Var() ?
//
inline static REBVAL *Blit_Specific(RELVAL *out, const REBVAL *v)
{
    Move_Value_Header(out, v);
    out->header.bits |= (v->header.bits & NODE_FLAG_MARKED);
    out->payload = v->payload;
    out->extra = v->extra;
    return cast(REBVAL*, out);
}


// !!! Super primordial experimental `const` feature.  Concept is that various
// operations have to be complicit (e.g. SELECT or FIND) in propagating the
// constness from the input series to the output value.  const input always
// gets you const output, but mutable input will get you const output if
// the value itself is const (so it inherits).
//
inline static REBVAL *Inherit_Const(REBVAL *out, const RELVAL *influencer) {
    out->header.bits |= (influencer->header.bits & CELL_FLAG_CONST);
    return out;
}
#define Trust_Const(value) \
    (value) // just a marking to say the const is accounted for already

inline static REBVAL *Constify(REBVAL *v) {
    SET_CELL_FLAG(v, CONST);
    return v;
}


//
// Rather than allow a REBVAL to be declared plainly as a local variable in
// a C function, this macro provides a generic "constructor-like" hook.
// This faciliates the differentiation of cell lifetimes (API vs. stack),
// as well as cell protection states.  It can also be useful for debugging
// scenarios, for knowing where cells are initialized.
//
// Note: because this will run instructions, a routine should avoid doing a
// DECLARE_LOCAL inside of a loop.  It should be at the outermost scope of
// the function.
//
// Note: It sets NODE_FLAG_FREE, so this is a "trash" cell by default.
//
#define DECLARE_LOCAL(name) \
    REBVAL name##_cell; \
    Prep_Cell(&name##_cell); \
    REBVAL * const name = &name##_cell;
