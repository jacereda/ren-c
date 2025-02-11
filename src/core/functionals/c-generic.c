//
//  File: %c-generic.c
//  Summary: "Function that dispatches implementation based on argument types"
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
// A "generic" is what R3-Alpha/Rebol2 had called "ACTION!" (until Ren-C took
// that as the umbrella term for all "invokables").  This kind of dispatch is
// based on the first argument's type, with the idea being a single C function
// for the type has a switch() statement in it and can handle many different
// such actions for that type.
//
// (e.g. APPEND [a b c] [d] would look at the type of the first argument,
// notice it was a BLOCK!, and call the common C function for arrays with an
// append instruction--where that instruction also handles insert, length,
// etc. for BLOCK!s.)
//
// !!! This mechanism is a very primitive kind of "multiple dispatch".  Rebol
// will certainly need to borrow from other languages to develop a more
// flexible idea for user-defined types, vs. this very limited concept.
//
// https://en.wikipedia.org/wiki/Multiple_dispatch
// https://en.wikipedia.org/wiki/Generic_function
// https://stackoverflow.com/q/53574843/
//

#include "sys-core.h"

enum {
    IDX_GENERIC_VERB = 1,  // Word whose symbol is being dispatched
    IDX_GENERIC_MAX
};


//
//  Generic_Dispatcher: C
//
Bounce Generic_Dispatcher(Frame(*) f)
{
    Action(*) phase = FRM_PHASE(f);
    Array(*) details = ACT_DETAILS(phase);
    Symbol(const*) verb = VAL_WORD_SYMBOL(DETAILS_AT(details, IDX_GENERIC_VERB));

    // !!! It's technically possible to throw in locals or refinements at
    // any point in the sequence.  D_ARG() accounts for this...hackily.
    //
    REBVAL *first_arg = D_ARG_Core(f, 1);

    return Run_Generic_Dispatch_Core(first_arg, f, verb);
}


//
//  generic: enfix native [
//
//  {Creates datatype action (currently for internal use only)}
//
//      return: <none>
//      :verb [set-word!]
//      spec [block!]
//  ]
//
DECLARE_NATIVE(generic)
//
// The `generic` native is designed to be an enfix function that quotes its
// first argument, so when you write `foo: generic [...]`, the FOO: gets quoted
// to be passed in as the "verb".
{
    INCLUDE_PARAMS_OF_GENERIC;

    REBVAL *verb = ARG(verb);
    REBVAL *spec = ARG(spec);

    Context(*) meta;
    Flags flags = MKF_KEYWORDS | MKF_RETURN;
    Array(*) paramlist = Make_Paramlist_Managed_May_Fail(
        &meta,
        spec,
        &flags  // return type checked only in debug build
    );

    Action(*) generic = Make_Action(
        paramlist,
        nullptr,  // no partials
        &Generic_Dispatcher,  // return type is only checked in debug build
        IDX_NATIVE_MAX  // details array capacity
    );

    assert(ACT_META(generic) == nullptr);
    mutable_ACT_META(generic) = meta;

    Set_Action_Flag(generic, IS_NATIVE);

    Array(*) details = ACT_DETAILS(generic);
    Init_Word(ARR_AT(details, IDX_NATIVE_BODY), VAL_WORD_SYMBOL(verb));
    Copy_Cell(ARR_AT(details, IDX_NATIVE_CONTEXT), Lib_Context_Value);

    REBVAL *verb_var = Sink_Word_May_Fail(verb, SPECIFIED);
    Init_Activation(verb_var, generic, VAL_WORD_SYMBOL(verb), UNBOUND);

    return NONE;
}


//
//  Startup_Generics: C
//
// Returns an array of words bound to generics for SYSTEM/CATALOG/ACTIONS
//
Array(*) Startup_Generics(const REBVAL *boot_generics)
{
    assert(VAL_INDEX(boot_generics) == 0); // should be at head, sanity check
    Cell(const*) tail;
    Cell(*) head = VAL_ARRAY_KNOWN_MUTABLE_AT(&tail, boot_generics);
    REBSPC *specifier = VAL_SPECIFIER(boot_generics);

    // Add SET-WORD!s that are top-level in the generics block to the lib
    // context, so there is a variable for each action.  This means that the
    // assignments can execute.
    //
    Bind_Values_Set_Midstream_Shallow(head, tail, Lib_Context_Value);

    // The above actually does bind the GENERIC word to the GENERIC native,
    // since the GENERIC word is found in the top-level of the block.  But as
    // with the natives, in order to process `foo: generic [x [integer!]]` the
    // INTEGER! word must be bound to its datatype.  Deep bind the code in
    // order to bind the words for these datatypes.
    //
    Bind_Values_Deep(head, tail, Lib_Context_Value);

    DECLARE_LOCAL (discarded);
    if (Do_Any_Array_At_Throws(discarded, boot_generics, SPECIFIED))
        panic (discarded);
    if (not Is_Word_Isotope_With_Id(discarded, SYM_DONE))
        panic (discarded);

    // Sanity check the symbol transformation
    //
    if (0 != strcmp("open", STR_UTF8(Canon(OPEN))))
        panic (Canon(OPEN));

    StackIndex base = TOP_INDEX;

    Cell(*) item = head;
    for (; item != tail; ++item)
        if (IS_SET_WORD(item)) {
            Derelativize(PUSH(), item, specifier);
            mutable_HEART_BYTE(TOP) = REB_WORD; // change pushed to WORD!
        }

    return Pop_Stack_Values(base);  // catalog of generics
}
