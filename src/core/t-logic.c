//
//  File: %t-logic.c
//  Summary: "logic datatype"
//  Section: datatypes
//  Project: "Rebol 3 Interpreter and Run-time (Ren-C branch)"
//  Homepage: https://github.com/metaeducation/ren-c/
//
//=////////////////////////////////////////////////////////////////////////=//
//
// Copyright 2012 REBOL Technologies
// Copyright 2012-2017 Ren-C Open Source Contributors
// REBOL is a trademark of REBOL Technologies
//
// See README.md and CREDITS.md for more information.
//
// Licensed under the Lesser GPL, Version 3.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// https://www.gnu.org/licenses/lgpl-3.0.html
//
//=////////////////////////////////////////////////////////////////////////=//
//

#include "sys-core.h"

#include "datatypes/sys-money.h" // !!! For conversions (good dependency?)

//
//  and?: native [
//
//  {Returns true if both values are conditionally true (no "short-circuit")}
//
//      return: [logic?]
//      value1 [any-value!]
//      value2 [any-value!]
//  ]
//
DECLARE_NATIVE(and_q)
{
    INCLUDE_PARAMS_OF_AND_Q;

    if (Is_Truthy(ARG(value1)) && Is_Truthy(ARG(value2)))
        return Init_True(OUT);

    return Init_False(OUT);
}


//
//  nor?: native [
//
//  {Returns true if both values are conditionally false (no "short-circuit")}
//
//      return: [logic?]
//      value1 [any-value!]
//      value2 [any-value!]
//  ]
//
DECLARE_NATIVE(nor_q)
{
    INCLUDE_PARAMS_OF_NOR_Q;

    if (Is_Falsey(ARG(value1)) && Is_Falsey(ARG(value2)))
        return Init_True(OUT);

    return Init_False(OUT);
}


//
//  nand?: native [
//
//  {Returns false if both values are conditionally true (no "short-circuit")}
//
//      return: [logic?]
//      value1 [any-value!]
//      value2 [any-value!]
//  ]
//
DECLARE_NATIVE(nand_q)
{
    INCLUDE_PARAMS_OF_NAND_Q;

    return Init_Logic(
        OUT,
        Is_Truthy(ARG(value1)) and Is_Truthy(ARG(value2))
    );
}


//
//  to-logic: native [
//
//  "true if value is NOT a LOGIC! false or NULL"
//
//      return: [logic?]
//      value [<opt> any-value!]
//  ]
//
DECLARE_NATIVE(to_logic)
{
    INCLUDE_PARAMS_OF_TO_LOGIC;

    return Init_Logic(OUT, Is_Truthy(ARG(value)));
}


//
//  false-if-zero: native [
//
//  "False if the integer input is a zero"
//
//      return: [logic?]
//      integer [integer!]
//  ]
//
DECLARE_NATIVE(false_if_zero)
{
    INCLUDE_PARAMS_OF_FALSE_IF_ZERO;

    return Init_Logic(OUT, VAL_INT64(ARG(integer)) != 0);
}


//
//  not: native [
//
//  "Returns the logic complement."
//
//      return: "Only LOGIC!'s FALSE, BLANK!, and NULL return TRUE"
//          [logic!]
//      optional [<opt> any-value! logic!]
//  ]
//
DECLARE_NATIVE(not_1)  // see TO-C-NAME
{
    INCLUDE_PARAMS_OF_NOT_1;

    return Init_Logic(OUT, Is_Falsey(ARG(optional)));
}


// The handling of logic has gone through several experiments, some of which
// made it more like a branching structure (so able to pass the result of the
// left hand side to the right).  There was also behavior for GET-GROUP!, to
// run the provided code whether the condition on the left was true or not.
//
// This scales the idea back to a very simple concept of a quoted GROUP!,
// WORD!, or TUPLE!.
//
inline static bool Do_Logic_Right_Side_Throws(
    REBVAL *out,
    const REBVAL *right
){
    if (IS_GROUP(right)) {
        if (Do_Any_Array_At_Throws(out, right, SPECIFIED))
            return true;
        Decay_If_Unstable(out);
        return false;
    }

    assert(IS_WORD(right) or IS_TUPLE(right));

    Get_Var_May_Fail(out, right, SPECIFIED, false);

    if (IS_ACTION(out))
        fail ("words/tuples can't be ACTION! as right hand of OR, AND, XOR");

    return false;
}


//
//  and: enfix native [
//
//  {Boolean AND, right hand side must be in GROUP! to allow short-circuit}
//
//      return: [logic?]
//      left [<opt> any-value! logic!]
//      'right "Right is evaluated if left is true"
//          [group! tuple! word!]
//  ]
//
DECLARE_NATIVE(and_1)  // see TO-C-NAME
{
    INCLUDE_PARAMS_OF_AND_1;

    REBVAL *left = ARG(left);
    REBVAL *right = ARG(right);

    if (Get_Cell_Flag(left, UNEVALUATED))
        fail (Error_Unintended_Literal_Raw(left));

    if (Is_Falsey(left))
        return Init_False(OUT);

    if (Do_Logic_Right_Side_Throws(SPARE, right))
        return THROWN;

    return Init_Logic(OUT, Is_Truthy(SPARE));
}


//
//  or: enfix native [
//
//  {Boolean OR, right hand side must be in GROUP! to allow short-circuit}
//
//      return: [logic?]
//      left [<opt> any-value! logic!]
//      'right "Right is evaluated if left is false"
//          [group! tuple! word!]
//  ]
//
DECLARE_NATIVE(or_1)  // see TO-C-NAME
{
    INCLUDE_PARAMS_OF_OR_1;

    REBVAL *left = ARG(left);
    REBVAL *right = ARG(right);

    if (Get_Cell_Flag(left, UNEVALUATED))
        fail (Error_Unintended_Literal_Raw(left));

    if (Is_Truthy(left))
        return Init_True(OUT);

    if (Do_Logic_Right_Side_Throws(SPARE, right))
        return THROWN;

    return Init_Logic(OUT, Is_Truthy(SPARE));
}


//
//  xor: enfix native [
//
//  {Boolean XOR (operation cannot be short-circuited)}
//
//      return: [logic?]
//      left [<opt> any-value! logic!]
//      'right "Always evaluated"
//          [group! tuple! word!]
//  ]
//
DECLARE_NATIVE(xor_1)  // see TO-C-NAME
{
    INCLUDE_PARAMS_OF_XOR_1;

    REBVAL *left = ARG(left);
    REBVAL *right = ARG(right);

    if (Get_Cell_Flag(left, UNEVALUATED))
        fail (Error_Unintended_Literal_Raw(left));

    if (Do_Logic_Right_Side_Throws(SPARE, right))
        return THROWN;

    if (Is_Falsey(left))
        return Init_Logic(OUT, Is_Truthy(SPARE));

    return Init_Logic(OUT, Is_Falsey(SPARE));
}


//
//  unless: enfix native [
//
//  {Variant of non-short-circuit OR which favors the right-hand side result}
//
//      return: "Conditionally true or false value (not coerced to LOGIC!)"
//          [<opt> any-value!]
//      left "Expression which will always be evaluated"
//          [<opt> any-value!]
//      ^right "Expression that's also always evaluated (can't short circuit)"
//          [<opt> <void> any-value!]  ; not a literal GROUP! as with XOR
//  ]
//
DECLARE_NATIVE(unless)
//
// Though this routine is similar to XOR, it is different enough in usage and
// looks from AND/OR/XOR to warrant not needing XOR's protection (e.g. forcing
// a GROUP! on the right hand side, prohibiting literal blocks on left)
{
    INCLUDE_PARAMS_OF_UNLESS;

    REBVAL *left = ARG(left);
    REBVAL *right = ARG(right);

    if (Is_Meta_Of_Void(right))  // if right disappears (no branching), left
        return COPY(left);

    Meta_Unquotify(right);

    if (Is_Truthy(right))
        return COPY(right);

    return COPY(left); // preserve the exact truthy or falsey value
}


inline static bool Math_Arg_For_Logic(REBVAL *arg)
{
    if (IS_LOGIC(arg))
        return VAL_LOGIC(arg);

    if (IS_BLANK(arg))
        return false;

    fail (Error_Unexpected_Type(REB_ISOTOPE, VAL_TYPE(arg)));
}


//
//  MAKE_Isotope: C
//
Bounce MAKE_Isotope(
    Frame(*) frame_,
    enum Reb_Kind kind,
    option(const REBVAL*) parent,
    const REBVAL *arg
){
    assert(kind == REB_ISOTOPE);
    if (parent)
        return RAISE(Error_Bad_Make_Parent(kind, unwrap(parent)));

    return Quotify(Copy_Cell(OUT, arg), 1);
}


//
//  TO_Isotope: C
//
Bounce TO_Isotope(Frame(*) frame_, enum Reb_Kind kind, const REBVAL *data) {
    return RAISE(Error_Bad_Make(kind, data));
}


//
//  REBTYPE: C
//
REBTYPE(Isotope)
{
    if (not IS_LOGIC(D_ARG(1))) {
        //
        // Need a special exemption for COPY on ACTION! isotopes.
        //
        if (Is_Activation(D_ARG(1)) and ID_OF_SYMBOL(verb) == SYM_COPY) {
            Deactivate_If_Activation(D_ARG(1));
            return rebValue(Canon(RUNS), Canon(COPY), rebQ(D_ARG(1)));
        }

        fail ("Isotope handler only supports LOGIC! (legacy workaround)");
    }

    bool b1 = VAL_LOGIC(D_ARG(1));
    bool b2;

    switch (ID_OF_SYMBOL(verb)) {

    case SYM_BITWISE_AND:
        b2 = Math_Arg_For_Logic(D_ARG(2));
        return Init_Logic(OUT, b1 and b2);

    case SYM_BITWISE_OR:
        b2 = Math_Arg_For_Logic(D_ARG(2));
        return Init_Logic(OUT, b1 or b2);

    case SYM_BITWISE_XOR:
        b2 = Math_Arg_For_Logic(D_ARG(2));
        return Init_Logic(OUT, b1 != b2);

    case SYM_BITWISE_AND_NOT:
        b2 = Math_Arg_For_Logic(D_ARG(2));
        return Init_Logic(OUT, b1 and not b2);

    case SYM_BITWISE_NOT:
        return Init_Logic(OUT, not b1);

    case SYM_RANDOM: {
        INCLUDE_PARAMS_OF_RANDOM;

        UNUSED(PARAM(value));

        if (REF(only))
            fail (Error_Bad_Refines_Raw());

        if (REF(seed)) {
            //
            // !!! For some reason, a random LOGIC! used OS_DELTA_TIME, while
            // it wasn't used elsewhere:
            //
            //     /* random/seed false restarts; true randomizes */
            //     Set_Random(b1 ? cast(REBINT, OS_DELTA_TIME(0)) : 1);
            //
            // This created a dependency on the host's model for time, which
            // the core is trying to be agnostic about.  This one appearance
            // for getting a random LOGIC! was a non-sequitur which was in
            // the way of moving time to an extension, so it was removed.
            //
            fail ("LOGIC! random seed currently not implemented");
        }

        if (Random_Int(REF(secure)) & 1)
            return Init_True(OUT);
        return Init_False(OUT); }

    default:
        break;
    }

    fail (UNHANDLED);
}
