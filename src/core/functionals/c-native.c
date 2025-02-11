//
//  File: %c-native.c
//  Summary: "Function that executes implementation as native code"
//  Section: datatypes
//  Project: "Ren-C Language Interpreter and Run-time Environment"
//  Homepage: https://github.com/metaeducation/ren-c/
//
//=////////////////////////////////////////////////////////////////////////=//
//
// Copyright 2012 REBOL Technologies
// Copyright 2012-2020 Ren-C Open Source Contributors
// REBOL is a trademark of REBOL Technologies
//
// See README.md and CREDITS.md for more information.
//
// Licensed under the GNU Lesser General Public License (LGPL), Version 3.0.
// You may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// https://www.gnu.org/licenses/lgpl-3.0.en.html
//
//=////////////////////////////////////////////////////////////////////////=//
//
// A native is unique from other function types, because instead of there
// being a "Native_Dispatcher()", each native has a C function that acts as
// its dispatcher.
//
// Also unique about natives is that the native function constructor must be
// built "by hand", since it is required to get the ball rolling on having
// functions to call at all.  See %make-natives.r
//

#include "sys-core.h"


//
//  Make_Native: C
//
// Reused function in Startup_Natives() as well as extensions loading natives,
// which can be parameterized with a different context in which to look up
// bindings by deafault in the API when that native is on the stack.
//
// Entries look like:
//
//    some-name: native [spec content]
//
// It is optional to put ENFIX between the SET-WORD! and the spec.
//
// If refinements are added, this will have to get more sophisticated.
//
// Though the manual building of this table is not as "nice" as running the
// evaluator, the evaluator makes comparisons against native values.  Having
// all natives loaded fully before ever running Eval_Core() helps with
// stability and invariants...also there's "state" in keeping track of which
// native index is being loaded, which is non-obvious.  But these issues
// could be addressed (e.g. by passing the native index number / DLL in).
//
Action(*) Make_Native(
    REBVAL *spec,
    bool is_combinator,
    Dispatcher* dispatcher,
    Context(*) module
){
    // There are implicit parameters to both NATIVE/COMBINATOR and usermode
    // COMBINATOR.  The native needs the full spec.
    //
    // !!! Note: This will manage the combinator's array.  Changing this would
    // need a version of Make_Paramlist_Managed() which took an array + index
    //
    DECLARE_LOCAL (expanded_spec);
    if (is_combinator) {
        Init_Block(expanded_spec, Expanded_Combinator_Spec(spec));
        spec = expanded_spec;
    }

    // With the components extracted, generate the native and add it to
    // the Natives table.  The associated C function is provided by a
    // table built in the bootstrap scripts, `Native_C_Funcs`.

    Context(*) meta;
    Flags flags = MKF_KEYWORDS | MKF_RETURN;
    Array(*) paramlist = Make_Paramlist_Managed_May_Fail(
        &meta,
        spec,
        &flags  // return type checked only in debug build
    );
    ASSERT_SERIES_TERM_IF_NEEDED(paramlist);

    Action(*) native = Make_Action(
        paramlist,
        nullptr,  // no partials
        dispatcher,  // "dispatcher" is unique to this "native"
        IDX_NATIVE_MAX  // details array capacity
    );
    Set_Action_Flag(native, IS_NATIVE);

    Array(*) details = ACT_DETAILS(native);
    Init_Blank(ARR_AT(details, IDX_NATIVE_BODY));
    Copy_Cell(ARR_AT(details, IDX_NATIVE_CONTEXT), CTX_ARCHETYPE(module));

    // NATIVE-COMBINATORs actually aren't *quite* their own dispatchers, they
    // all share a common hook to help with tracing and doing things like
    // calculating the furthest amount of progress in the parse.  So we call
    // that the actual "native" in that case.
    //
    if (is_combinator) {
        Action(*) native_combinator = native;
        native = Make_Action(
            ACT_PARAMLIST(native_combinator),
            nullptr,  // no partials
            &Combinator_Dispatcher,
            2  // IDX_COMBINATOR_MAX  // details array capacity
        );

        Copy_Cell(
            ARR_AT(ACT_DETAILS(native), 1),  // IDX_COMBINATOR_BODY
            ACT_ARCHETYPE(native_combinator)
        );
    }

    // We want the meta information on the wrapped version if it's a
    // NATIVE-COMBINATOR.
    //
    assert(ACT_META(native) == nullptr);
    mutable_ACT_META(native) = meta;

    return native;
}


//
//  native: native [
//
//  {(Internal Function) Create a native, using compiled C code}
//
//      return: "Isotopic ACTION!"
//          [isotope!]  ; [activation?] needs NATIVE to define it!
//      spec [block!]
//      /combinator
//  ]
//
DECLARE_NATIVE(native)
{
    INCLUDE_PARAMS_OF_NATIVE;

    Value(*) spec = ARG(spec);
    bool is_combinator = REF(combinator);

    if (not PG_Next_Native_Dispatcher)
        fail ("NATIVE is for internal use during boot and extension loading");

    Dispatcher* dispatcher = *PG_Next_Native_Dispatcher;
    ++PG_Next_Native_Dispatcher;

    Action(*) native = Make_Native(
        spec,
        is_combinator,
        dispatcher,
        PG_Currently_Loading_Module
    );

    return Init_Activation(OUT, native, ANONYMOUS, UNBOUND);
}


//
//  Init_Action_Meta_Shim: C
//
// Make_Paramlist_Managed_May_Fail() needs the object archetype ACTION-META
// from %sysobj.r, to have the keylist to use in generating the info used
// by HELP for the natives.  However, natives themselves are used in order
// to run the object construction in %sysobj.r
//
// To break this Catch-22, this code builds a field-compatible version of
// ACTION-META.  After %sysobj.r is loaded, an assert checks to make sure
// that this manual construction actually matches the definition in the file.
//
static void Init_Action_Meta_Shim(void) {
    SymId field_syms[3] = {
        SYM_DESCRIPTION, SYM_PARAMETER_TYPES, SYM_PARAMETER_NOTES
    };
    Context(*) meta = Alloc_Context_Core(REB_OBJECT, 4, NODE_FLAG_MANAGED);
    REBLEN i = 1;
    for (; i != 4; ++i)
        Init_Nulled(Append_Context(meta, Canon_Symbol(field_syms[i - 1])));

    Root_Action_Meta = Init_Object(Alloc_Value(), meta);
    Force_Value_Frozen_Deep(Root_Action_Meta);
}

static void Shutdown_Action_Meta_Shim(void) {
    rebRelease(Root_Action_Meta);
}


//
//  Startup_Natives: C
//
// Returns an array of words bound to natives for SYSTEM.CATALOG.NATIVES
//
Array(*) Startup_Natives(const REBVAL *boot_natives)
{
    Array(*) catalog = Make_Array(Num_Natives);

    // Must be called before first use of Make_Paramlist_Managed_May_Fail()
    //
    Init_Action_Meta_Shim();

    assert(VAL_INDEX(boot_natives) == 0); // should be at head, sanity check
    Cell(const*) tail;
    Cell(*) item = VAL_ARRAY_KNOWN_MUTABLE_AT(&tail, boot_natives);
    assert(VAL_SPECIFIER(boot_natives) == SPECIFIED);

    // !!! We could avoid this by making NATIVE a specialization of a NATIVE*
    // function which carries those arguments, which would be cleaner.  The
    // C function could be passed as a HANDLE!.
    //
    assert(PG_Next_Native_Dispatcher == nullptr);
    PG_Next_Native_Dispatcher = Native_C_Funcs;
    assert(PG_Currently_Loading_Module == nullptr);
    PG_Currently_Loading_Module = Lib_Context;

    // Due to the recursive nature of `native: native [...]`, we can't actually
    // create NATIVE itself that way.  So the prep process should have moved
    // it to be the first native in the list, and we make it manually.
    //
    assert(IS_SET_WORD(item) and VAL_WORD_ID(item) == SYM_NATIVE);
    ++item;
    assert(IS_WORD(item) and VAL_WORD_ID(item) == SYM_NATIVE);
    ++item;
    assert(IS_BLOCK(item));
    REBVAL *spec = SPECIFIC(item);
    ++item;

    Action(*) the_native_action = Make_Native(
        spec,
        false,  // not a combinator
        *PG_Next_Native_Dispatcher,
        PG_Currently_Loading_Module
    );
    ++PG_Next_Native_Dispatcher;

    Init_Activation(
        Append_Context(Lib_Context, Canon(NATIVE)),
        the_native_action,
        Canon(NATIVE),  // label
        UNBOUND
    );

    assert(VAL_ACTION(Lib(NATIVE)) == the_native_action);

    // The current rule in "Sea of Words" is that all SET-WORD!s that are just
    // "attached" to a context can materialize variables.  It's not as safe
    // as something like JavaScript's strict mode...but rather than institute
    // some new policy we go with the somewhat laissez faire historical rule.
    //
    // *HOWEVER* the rule does not apply to Lib_Context.  You will get an
    // error if you try to assign to something attached to Lib before being
    // explicitly added.  So we have to go over the SET-WORD!s naming natives
    // (first one at time of writing is `api-transient: native [...]`) and
    // BIND/SET them.
    //
    Bind_Values_Set_Midstream_Shallow(item, tail, Lib_Context_Value);

    DECLARE_LOCAL (skipped);
    Init_Array_Cell_At(skipped, REB_BLOCK, VAL_ARRAY(boot_natives), 3);

    DECLARE_LOCAL (discarded);
    if (Do_Any_Array_At_Throws(discarded, skipped, SPECIFIED))
        panic (Error_No_Catch_For_Throw(TOP_FRAME));
    if (not Is_Word_Isotope_With_Id(discarded, SYM_DONE))
        panic (discarded);

  #if !defined(NDEBUG)
    //
    // Ensure the evaluator called NATIVE as many times as we had natives,
    // and check that a couple of functions can be successfully looked up
    // by their symbol ID numbers.

    assert(PG_Next_Native_Dispatcher == Native_C_Funcs + Num_Natives);

    if (not Is_Activation(Lib(GENERIC)))
        panic (Lib(GENERIC));

    if (not Is_Activation(Lib(PARSE_REJECT)))
        panic (Lib(PARSE_REJECT));
  #endif

    assert(PG_Next_Native_Dispatcher == Native_C_Funcs + Num_Natives);

    PG_Next_Native_Dispatcher = nullptr;
    PG_Currently_Loading_Module = nullptr;

    return catalog;
}


//
//  Shutdown_Natives: C
//
// Being able to run Recycle() during the native startup process means being
// able to holistically check the system state.  This relies on initialized
// data in the natives table.  Since the interpreter can be shutdown and
// started back up in the same session, we can't rely on zero initialization
// for startups after the first, unless we manually null them out.
//
void Shutdown_Natives(void) {
    Shutdown_Action_Meta_Shim();
}
