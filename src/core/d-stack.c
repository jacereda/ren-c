//
// Rebol 3 Language Interpreter and Run-time Environment
// "Ren-C" branch @ https://github.com/metaeducation/ren-c
//
//=////////////////////////////////////////////////////////////////////////=//
//
//  Summary: Debug Stack Reflection and Querying
//  File: %d-stack.h
//
//=////////////////////////////////////////////////////////////////////////=//
//
// Copyright 2015-2017 Ren-C Open Source Contributors
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
// This file contains interactive debugging support for examining and
// interacting with the stack.
//
// !!! Interactive debugging is a work in progress, and comments are in the
// functions below.
//

#include "sys-core.h"


//
//  Collapsify_Array: C
//
// This will replace "long" nested blocks with collapsed versions with
// ellipses to show they have been cut off.  It does not change the arrays
// in question, but replaces them with copies.
//
void Collapsify_Array(Array(*) array, REBSPC *specifier, REBLEN limit)
{
    Cell(const*) tail = ARR_TAIL(array);
    Cell(*) item = ARR_HEAD(array);
    for (; item != tail; ++item) {
        if (ANY_ARRAY(item) and VAL_LEN_AT(item) > limit) {
            REBSPC *derived = Derive_Specifier(specifier, item);
            Array(*) copy = Copy_Array_At_Max_Shallow(
                VAL_ARRAY(item),
                VAL_INDEX(item),
                derived,
                limit + 1
            );

            Init_Word(ARR_AT(copy, limit), Canon(ELLIPSIS_1));

            Collapsify_Array(
                copy,
                SPECIFIED,
                limit
            );

            enum Reb_Kind kind = VAL_TYPE(item);
            Init_Array_Cell_At(item, kind, copy, 0);  // at 0 now
            assert(IS_SPECIFIC(item));
            assert(Not_Cell_Flag(item, NEWLINE_BEFORE));  // gets cleared
        }
    }
}


//
//  Init_Near_For_Frame: C
//
// Each call frame maintains the array it is executing in, the current index
// in that array, and the index of where the current expression started.
// This can be deduced into a segment of code to display in the debug views
// to indicate roughly "what's running" at that stack level.  The code is
// a shallow copy of the array content.
//
// The resulting WHERE information only includes the range of the array being
// executed up to the point of currently relevant evaluation.  It does not
// go all the way to the tail of the block (where future potential evaluation
// should be.
//
// !!! DO also offers a feature whereby values can be supplied at the start
// of an evaluation which are not resident in the array.  It also can run
// on an irreversible C va_list of REBVAL*, where these disappear as the
// evaluation proceeds.  A special debug setting would be needed to hang
// onto these values for the purposes of better error messages (at the cost
// of performance).
//
REBVAL *Init_Near_For_Frame(Cell(*) out, Frame(*) f)
{
    StackIndex base = TOP_INDEX;

    if (FRM_IS_VARIADIC(f)) {
        //
        // A variadic feed may not be able to be reified, if the data is
        // malformed.  But it also might be able to be... *unless this is
        // a scanner frame itself raising the error*.
        //
        const bool truncated = true;
        Reify_Variadic_Feed_As_Array_Feed(f->feed, truncated);
    }

    // Get at most 6 values out of the array.  Ideally 3 before and after
    // the error point.  If truncating either the head or tail of the
    // values, put ellipses.

    // !!! We may be running a function where the value for the function was a
    // "head" value not in the array.  These cases could substitute the symbol
    // for the currently executing function.  Reconsider when such cases
    // appear and can be studied.

    REBINT start = FRM_INDEX(f) - 3;
    if (start > 0)
        Init_Word(PUSH(), Canon(ELLIPSIS_1));
    else if (start < 0)
        start = 0;

    REBLEN count = 0;
    Cell(const*) tail = ARR_TAIL(FRM_ARRAY(f));
    Cell(const*) item = ARR_AT(FRM_ARRAY(f), start);
    for (; item != tail and count < 6; ++item, ++count) {
        assert(not Is_Void(item));  // can't be in arrays, API won't splice
        assert(not Is_Isotope(item));  // can't be in arrays, API won't splice
        Derelativize(PUSH(), item, f_specifier);

        if (count == FRM_INDEX(f) - start - 1) {
            //
            // Leave a marker at the point of the error, currently `**`.
            //
            // This is the marker for an execution point, so it can either
            // mean "error source is to the left" or just "frame is at a
            // breakpoint at that position".
            //
            Init_Word(PUSH(), Canon(_P_P));
        }
    }

    if (item != tail)
        Init_Word(PUSH(), Canon(ELLIPSIS_1));

    // !!! This code can be called on an executing frame, such as when an
    // error happens in that frame.  Or it can be called on a pending frame
    // when examining a backtrace...where the function hasn't been called
    // yet.  This needs some way of differentiation, consider it.
    //
    /*
    if (Is_Action_Frame(f) and Is_Action_Frame_Fulfilling(f)) {
        ???
    }
    */

    Array(*) near = Pop_Stack_Values_Core(base, NODE_FLAG_MANAGED);

    // Simplify overly-deep blocks embedded in the where so they show (...)
    // instead of printing out fully.
    //
    Collapsify_Array(near, SPECIFIED, 3);

    Init_Block(out, near);

    return SPECIFIC(out);
}


//
//  Is_Context_Running_Or_Pending: C
//
bool Is_Context_Running_Or_Pending(Context(*) frame_ctx)
{
    Frame(*) f = CTX_FRAME_IF_ON_STACK(frame_ctx);
    if (not f)
        return false;

    if (Is_Action_Frame_Fulfilling(f))
        return false;

    return true;
}


//
//  running?: native [
//
//  "Returns TRUE if a FRAME! is on the stack and executing (arguments done)."
//
//      return: [logic?]
//      frame [frame!]
//  ]
//
DECLARE_NATIVE(running_q)
{
    INCLUDE_PARAMS_OF_RUNNING_Q;

    Context(*) frame_ctx = VAL_CONTEXT(ARG(frame));

    Frame(*) f = CTX_FRAME_MAY_FAIL(frame_ctx);

    if (Is_Action_Frame_Fulfilling(f))
        return Init_False(OUT);

    return Init_True(OUT);
}


//
//  pending?: native [
//
//  "Returns TRUE if a FRAME! is on the stack, but is gathering arguments."
//
//      return: [logic?]
//      frame [frame!]
//  ]
//
DECLARE_NATIVE(pending_q)
{
    INCLUDE_PARAMS_OF_PENDING_Q;

    Context(*) frame_ctx = VAL_CONTEXT(ARG(frame));

    Frame(*) f = CTX_FRAME_MAY_FAIL(frame_ctx);

    if (Is_Action_Frame_Fulfilling(f))
        return Init_True(OUT);

    return Init_False(OUT);
}
