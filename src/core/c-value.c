//
//  File: %c-value.c
//  Summary: "Generic REBVAL Support Services and Debug Routines"
//  Project: "Rebol 3 Interpreter and Run-time (Ren-C branch)"
//  Homepage: https://github.com/metaeducation/ren-c/
//
//=////////////////////////////////////////////////////////////////////////=//
//
// Copyright 2016 Ren-C Open Source Contributors
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
// These are mostly DEBUG-build routines to support the macros and definitions
// in %sys-value.h.
//
// These are not specific to any given type.  For the type-specific REBVAL
// code, see files with names like %t-word.c, %t-logic.c, %t-integer.c...
//

#include "sys-core.h"


#if DEBUG_FANCY_PANIC  // !!! Separate setting for Dump routines?

//
//  Dump_Value_Debug: C
//
// Returns the containing node.
//
Node* Dump_Value_Debug(Cell(const*) v)
{
    fflush(stdout);
    fflush(stderr);

    Node* containing = Try_Find_Containing_Node_Debug(v);

  #if DEBUG_TRACK_EXTEND_CELLS
    printf("REBVAL init");

    printf(" @ tick #%d", cast(unsigned int, v->tick));
    if (v->touch != 0)
        printf(" @ touch #%d", cast(unsigned int, v->touch));

    printf(" @ %s:%ld\n", v->file, cast(unsigned long, v->line));
  #else
    printf("- no track info (see DEBUG_TRACK_EXTEND_CELLS)\n");
  #endif
    fflush(stdout);

    printf("kind_byte=%d\n", cast(int, VAL_TYPE_UNCHECKED(v)));

    enum Reb_Kind heart = CELL_HEART(VAL_UNESCAPED(v));
    const char *type = STR_UTF8(Canon_Symbol(SYM_FROM_KIND(heart)));
    printf("cell_heart=%s\n", type);
    fflush(stdout);

    if (Get_Cell_Flag(v, FIRST_IS_NODE))
        printf("has first node: %p\n", cast(void*, VAL_NODE1(v)));
    if (Get_Cell_Flag(v, SECOND_IS_NODE))
        printf("has second node: %p\n", cast(void*, VAL_NODE2(v)));

    if (not containing)
        return nullptr;

    if (Is_Node_A_Stub(containing)) {
        printf(
            "Containing series for value pointer found, %p:\n",
            cast(void*, containing)
        );
    }
    else{
        printf(
            "Containing pairing for value pointer found %p:\n",
            cast(void*, containing)
        );
    }

    return containing;
}


//
//  Panic_Value_Debug: C
//
// This is a debug-only "error generator", which will hunt through all the
// series allocations and panic on the series that contains the value (if
// it can find it).  This will allow those using Address Sanitizer or
// Valgrind to know a bit more about where the value came from.
//
// Additionally, it can dump out where the initialization happened if that
// information was stored.  See DEBUG_TRACK_EXTEND_CELLS.
//
ATTRIBUTE_NO_RETURN void Panic_Value_Debug(Cell(const*) v) {
    Node* containing = Dump_Value_Debug(v);

    if (containing) {
        printf("Panicking the containing REBSER...\n");
        Panic_Series_Debug(SER(containing));
    }

    printf("No containing series for value, panicking for stack dump:\n");
    Panic_Series_Debug(EMPTY_ARRAY);
}

#endif // !defined(NDEBUG)


#if DEBUG_HAS_PROBE

inline static void Probe_Print_Helper(
    const void *p,  // the REBVAL*, REBSER*, or UTF-8 char*
    const char *expr,  // stringified contents of the PROBE() macro
    const char *label,  // detected type of `p` (see %rebnod.h)
    const char *file,  // file where this PROBE() was invoked
    int line  // line where this PROBE() was invoked
){
    printf("\n-- (%s)=0x%p : %s", expr, p, label);
  #if DEBUG_COUNT_TICKS
    printf(" : tick %d", cast(int, TG_tick));
  #endif
    printf(" %s @%d\n", file, line);

    fflush(stdout);
    fflush(stderr);
}


inline static void Probe_Molded_Value(const REBVAL *v)
{
    DECLARE_MOLD (mo);
    Push_Mold(mo);
    Mold_Value(mo, v);

    printf("%s\n", cast(const char*, STR_AT(mo->series, mo->base.index)));
    fflush(stdout);

    Drop_Mold(mo);
}

void Probe_Cell_Print_Helper(
  REB_MOLD *mo,
  const void *p,
  const char *expr,
  const char *file,
  int line
){
    Probe_Print_Helper(p, expr, "Value", file, line);

    const REBVAL *v = cast(const REBVAL*, p);

  #if DEBUG_UNREADABLE_TRASH
    if (IS_TRASH(v)) {  // Is_Nulled() asserts on trash
        Append_Ascii(mo->series, "~trash~");
        return;
    }
  #endif

    if (Is_Cell_Poisoned(v)) {
        Append_Ascii(mo->series, "**POISONED CELL**");
    }
    else if (Is_Void(v)) {
        Append_Ascii(mo->series, "; void");
    }
    else if (Is_Isotope(v)) {
        DECLARE_LOCAL (reified);
        Quasify_Isotope(Copy_Cell(reified, v));
        Mold_Value(mo, reified);
        Append_Ascii(mo->series, "  ; isotope");
    }
    else
        Mold_Value(mo, v);
}


//
//  Probe_Core_Debug: C
//
// Use PROBE() to invoke from code; this gives more information like line
// numbers, and in the C++ build will return the input (like the PROBE native
// function does).
//
// Use Probe() to invoke from the C debugger (non-macro, single-arity form).
//
void* Probe_Core_Debug(
    const void *p,
    const char *expr,
    const char *file,
    int line
){
    DECLARE_MOLD (mo);
    Push_Mold(mo);

    bool was_disabled = GC_Disabled;
    GC_Disabled = true;

    if (not p) {
        Probe_Print_Helper(p, expr, "C nullptr", file, line);
        goto cleanup;
    }
    else switch (Detect_Rebol_Pointer(p)) {
      case DETECTED_AS_UTF8:
        if (*cast(const Byte*, p) == '\0')
            Probe_Print_Helper(
                p,
                expr,
                "Erased Cell (or Empty C String)",
                file, line
              );
        else if (
            (*cast(const Byte*, p) & NODE_BYTEMASK_0x80_NODE)
            and (*cast(const Byte*, p) & NODE_BYTEMASK_0x01_CELL)
        ){
            if (not (*cast(const Byte*, p) & NODE_BYTEMASK_0x40_STALE)) {
                printf("!!! Non-FREE'd alias of cell with UTF-8 !!!");
                panic (p);  // hopefully you merely debug-probed garbage memory
            }
            Probe_Print_Helper(p, expr, "C String (or free cell)", file, line);
            printf("\"%s\"\n", cast(const char*, p));
        }
        else {
            Probe_Print_Helper(p, expr, "C String", file, line);
            printf("\"%s\"\n", cast(const char*, p));
        }
        goto cleanup;

      case DETECTED_AS_CELL:
        Probe_Cell_Print_Helper(mo, p, expr, file, line);
        goto cleanup;

      case DETECTED_AS_END:
        Probe_Print_Helper(p, expr, "END", file, line);
        goto cleanup;

      case DETECTED_AS_SERIES:
        break;  // lots of possibilities, break to handle
    }

    // If we didn't jump to cleanup above, it's a series.  New switch().

  blockscope {
    REBSER *s = m_cast(REBSER*, cast(const REBSER*, p));
    assert(not IS_FREE_NODE(s));  // Detect should have caught, above
    Flavor flavor = SER_FLAVOR(s);
    ASSERT_SERIES(s); // if corrupt, gives better info than a print crash

    switch (flavor) {

    //=//// ARRAY FLAVORS //////////////////////////////////////////////////=//

      case FLAVOR_ARRAY:
        Probe_Print_Helper(p, expr, "Generic Array", file, line);
        Mold_Array_At(mo, ARR(s), 0, "[]"); // not necessarily BLOCK!
        break;

      case FLAVOR_VARLIST:  // currently same as FLAVOR_PARAMLIST
        Probe_Print_Helper(p, expr, "Varlist (or Paramlist)", file, line);
        Probe_Molded_Value(CTX_ARCHETYPE(CTX(s)));
        break;

      case FLAVOR_DETAILS:
        Probe_Print_Helper(p, expr, "Action", file, line);
        MF_Action(mo, ACT_ARCHETYPE(ACT(m_cast(void*, p))), false);
        break;

      case FLAVOR_PAIRLIST:
        Probe_Print_Helper(p, expr, "Pairlist", file, line);
        break;

      case FLAVOR_PATCH:
        Probe_Print_Helper(p, expr, "Module Item Patch", file, line);
        break;

      case FLAVOR_LET: {
        Probe_Print_Helper(p, expr, "LET single variable", file, line);
        Append_Spelling(mo->series, INODE(LetSymbol, s));
        break; }

      case FLAVOR_USE: {
        Probe_Print_Helper(p, expr, "Virtual Bind USE", file, line);
        break; }

      case FLAVOR_HITCH:
        Probe_Print_Helper(p, expr, "Hitch", file, line);
        break;

      case FLAVOR_PARTIALS:
        Probe_Print_Helper(p, expr, "Partials", file, line);
        break;

      case FLAVOR_LIBRARY:
        Probe_Print_Helper(p, expr, "Library", file, line);
        break;

      case FLAVOR_HANDLE:
        Probe_Print_Helper(p, expr, "Handle", file, line);
        break;

      case FLAVOR_DATASTACK:
        Probe_Print_Helper(p, expr, "Datastack", file, line);
        break;

      case FLAVOR_FEED:
        Probe_Print_Helper(p, expr, "Feed", file, line);
        break;

      case FLAVOR_API:
        Probe_Print_Helper(p, expr, "API Handle", file, line);
        break;

      case FLAVOR_INSTRUCTION_SPLICE:
        Probe_Print_Helper(p, expr, "Splicing Instruction", file, line);
        break;

    //=//// SERIES WITH ELEMENTS sizeof(void*) /////////////////////////////=//

      case FLAVOR_KEYLIST: {
        assert(SER_WIDE(s) == sizeof(REBKEY));  // ^-- or is byte size
        Probe_Print_Helper(p, expr, "Keylist Series", file, line);
        const REBKEY *tail = SER_TAIL(REBKEY, s);
        const REBKEY *key = SER_HEAD(REBKEY, s);
        Append_Ascii(mo->series, "<< ");
        for (; key != tail; ++key) {
            Mold_Text_Series_At(mo, KEY_SYMBOL(key), 0);
            Append_Codepoint(mo->series, ' ');
        }
        Append_Ascii(mo->series, ">>");
        break; }

      case FLAVOR_POINTER:
        Probe_Print_Helper(p, expr, "Series of void*", file, line);
        break;

      case FLAVOR_CANONTABLE:
        Probe_Print_Helper(p, expr, "Canon Table", file, line);
        break;

      case FLAVOR_NODELIST:  // e.g. GC protect list
        Probe_Print_Helper(p, expr, "Series of NODE*", file, line);
        break;

      case FLAVOR_SERIESLIST:  // e.g. manually allocated series list
        Probe_Print_Helper(p, expr, "Series of REBSER*", file, line);
        break;

      case FLAVOR_MOLDSTACK:
        Probe_Print_Helper(p, expr, "Mold Stack", file, line);
        break;

    //=//// SERIES WITH ELEMENTS sizeof(REBLEN) ////////////////////////////=//

      case FLAVOR_HASHLIST:
        Probe_Print_Helper(p, expr, "Hashlist", file, line);
        break;

    //=//// SERIES WITH ELEMENTS sizeof(struct Reb_Bookmark) ///////////////=//

      case FLAVOR_BOOKMARKLIST:
        Probe_Print_Helper(p, expr, "Bookmarklist", file, line);
        break;

    //=//// SERIES WITH ELEMENTS WIDTH 1 ///////////////////////////////////=//

      case FLAVOR_BINARY: {
        Binary(*) bin = BIN(s);
        Probe_Print_Helper(p, expr, "Byte-Size Series", file, line);

        const bool brk = (BIN_LEN(bin) > 32);  // !!! duplicates MF_Binary code
        Append_Ascii(mo->series, "#{");
        Form_Base16(mo, BIN_HEAD(bin), BIN_LEN(bin), brk);
        Append_Ascii(mo->series, "}");
        break; }

    //=//// SERIES WITH ELEMENTS WIDTH 1 INTERPRETED AS UTF-8 //////////////=//

      case FLAVOR_STRING: {
        Probe_Print_Helper(p, expr, "String series", file, line);
        Mold_Text_Series_At(mo, STR(s), 0);  // or could be TAG!, etc.
        break; }

      case FLAVOR_SYMBOL: {
        Probe_Print_Helper(p, expr, "Interned (Symbol) series", file, line);
        Mold_Text_Series_At(mo, STR(s), 0);
        break; }

      case FLAVOR_THE_GLOBAL_INACCESSIBLE: {
        Probe_Print_Helper(p, expr, "Global Inaccessible Series", file, line);
        break; }

    #if !defined(NDEBUG)  // PROBE() is sometimes in non-debug executables
      case FLAVOR_TRASH:
        Probe_Print_Helper(p, expr, "!!! TRASH Series !!!", file, line);
        break;
    #endif

      default:
        Probe_Print_Helper(p, expr, "!!! Unknown SER_FLAVOR() !!!", file, line);
        break;
    }
  }

  cleanup:

    if (mo->base.size != STR_SIZE(mo->series))
        printf("%s\n", cast(const char*, STR_AT(mo->series, mo->base.index)));
    fflush(stdout);

    Drop_Mold(mo);

    assert(GC_Disabled);
    GC_Disabled = was_disabled;

    return m_cast(void*, p); // must be cast back to const if source was const
}


// Version with fewer parameters, useful to call from the C debugger (which
// cannot call macros like PROBE())
//
void Probe(const void *p)
  { Probe_Core_Debug(p, "C debug", "N/A", 0); }


//
//  Where_Core_Debug: C
//
void Where_Core_Debug(Frame(*) f) {
    if (FEED_IS_VARIADIC(f->feed))
        Reify_Variadic_Feed_As_Array_Feed(f->feed, false);

    REBLEN index = FEED_INDEX(f->feed);

    if (index > 0) {
        DECLARE_MOLD (mo);
        SET_MOLD_FLAG(mo, MOLD_FLAG_LIMIT);
        mo->limit = 40 * 20;  // 20 lines of length 40, or so?

        REBLEN before_index = index > 3 ? index - 3 : 0;
        Push_Mold(mo);
        Mold_Array_At(mo, FEED_ARRAY(f->feed), before_index, "[]");
        Throttle_Mold(mo);
        printf("Where(Before):\n");
        printf("%s\n\n", BIN_AT(mo->series, mo->base.size));
        Drop_Mold(mo);
    }

    DECLARE_MOLD (mo);
    SET_MOLD_FLAG(mo, MOLD_FLAG_LIMIT);
    mo->limit = 40 * 20;  // 20 lines of length 40, or so?
    Push_Mold(mo);
    Mold_Array_At(mo, FEED_ARRAY(f->feed), index, "[]");
    Throttle_Mold(mo);
    printf("Where(At):\n");
    printf("%s\n\n", BIN_AT(mo->series, mo->base.size));
    Drop_Mold(mo);
}

void Where(Frame(*) f)
  { Where_Core_Debug(f); }

#endif  // DEBUG_HAS_PROBE
