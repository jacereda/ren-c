//
//  File: %n-reduce.h
//  Summary: {REDUCE and COMPOSE natives and associated service routines}
//  Project: "Rebol 3 Interpreter and Run-time (Ren-C branch)"
//  Homepage: https://github.com/metaeducation/ren-c/
//
//=////////////////////////////////////////////////////////////////////////=//
//
// Copyright 2012 REBOL Technologies
// Copyright 2012-2017 Ren-C Open Source Contributors
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

#include "sys-core.h"


//
//  reduce: native [
//
//  {Evaluates expressions, keeping each result (DO only gives last result)}
//
//      return: "New array or value"
//          [<opt> any-value!]
//      value "GROUP! and BLOCK! evaluate each item, single values evaluate"
//          [any-value!]
//      /predicate "Applied after evaluation, default is IDENTITY"
//          [<unrun> action!]
//  ]
//
DECLARE_NATIVE(reduce)
//
// 1. It's not completely clear what the semantics of non-block REDUCE should
//    be, but right now single value REDUCE does a REEVALUATE where it does
//    not allow arguments.  This is a variant of REEVAL with an END feed.
//
//    (R3-Alpha, would return the input, e.g. `reduce ':foo` => :foo)
//
// 2. We want the output newline status to mirror the newlines of the start
//    of the eval positions.  But when the evaluation callback happens, we
//    won't have the starting value anymore.  Cache the newline flag on the
//    ARG(value) cell, as newline flags on ARG()s are available.
//
// 3. The subframe that is pushed to run the reduce evaluations uses the data
//    stack position captured in BASELINE to tell things like whether a
//    function dispatch has pushed refinements, etc.  So when the REDUCE frame
//    underneath it pushes a value to the data stack, that frame must be
//    informed the stack element is "not for it" before the next call.
{
    INCLUDE_PARAMS_OF_REDUCE;

    Value(*) v = ARG(value);  // newline flag on `v` cell is leveraged, see [2]
    Value(*) predicate = ARG(predicate);

    enum {
        ST_REDUCE_INITIAL_ENTRY = STATE_0,
        ST_REDUCE_EVAL_STEP,
        ST_REDUCE_RUNNING_PREDICATE
    };

    switch (STATE) {
      case ST_REDUCE_INITIAL_ENTRY:
        if (ANY_ARRAY(v))
            goto initial_entry_any_array;
        goto initial_entry_non_array;  // semantics in question, see [1]

      case ST_REDUCE_EVAL_STEP:
        goto reduce_step_result_in_out;

      case ST_REDUCE_RUNNING_PREDICATE:
        goto process_out;

      default: assert(false);
    }

  initial_entry_non_array: {  /////////////////////////////////////////////////

    if (ANY_INERT(v))
        return COPY(v);  // save time if it's something like a TEXT!

    Frame(*) subframe = Make_End_Frame(
        FLAG_STATE_BYTE(ST_EVALUATOR_REEVALUATING)
    );
    Push_Frame(OUT, subframe);

    subframe->u.eval.current = v;
    subframe->u.eval.current_gotten = nullptr;
    subframe->u.eval.enfix_reevaluate = 'N';  // detect?

    return DELEGATE_SUBFRAME(subframe);

} initial_entry_any_array: {  ////////////////////////////////////////////////

    Frame(*) subframe = Make_Frame_At(
        v,  // REB_BLOCK or REB_GROUP
        FRAME_FLAG_ALLOCATED_FEED
            | FRAME_FLAG_TRAMPOLINE_KEEPALIVE  // reused for each step
    );
    Push_Frame(OUT, subframe);
    goto next_reduce_step;

} next_reduce_step: {  ///////////////////////////////////////////////////////

    if (Is_Feed_At_End(SUBFRAME->feed))
        goto finished;

    if (Get_Cell_Flag(At_Feed(SUBFRAME->feed), NEWLINE_BEFORE))
        Set_Cell_Flag(v, NEWLINE_BEFORE);  // cache newline flag, see [2]
    else
        Clear_Cell_Flag(v, NEWLINE_BEFORE);

    SUBFRAME->executor = &Evaluator_Executor;
    STATE = ST_REDUCE_EVAL_STEP;
    Restart_Evaluator_Frame(SUBFRAME);
    return CONTINUE_SUBFRAME(SUBFRAME);

} reduce_step_result_in_out: {  //////////////////////////////////////////////

    if (Is_Nulled(predicate))  // default is no processing
        goto process_out;

    if (Is_Nihil(OUT) or Is_Void(OUT))  // not offered to predicates, by design
        goto next_reduce_step;  // reduce skips over voids and nones

    SUBFRAME->executor = &Just_Use_Out_Executor;
    STATE = ST_REDUCE_RUNNING_PREDICATE;
    return CONTINUE(OUT, predicate, OUT);  // arg can be same as output

} process_out: {  ////////////////////////////////////////////////////////////

    if (Is_Nihil(OUT) or Is_Void(OUT))
        goto next_reduce_step;  // void results are skipped by reduce

    Decay_If_Unstable(OUT);

    if (Is_Nulled(OUT))
        return RAISE(Error_Need_Non_Null_Raw());  // error enables e.g. CURTAIL

    if (Is_Splice(OUT)) {
        Cell(const*) tail;
        Cell(const*) at = VAL_ARRAY_AT(&tail, OUT);
        bool newline = Get_Cell_Flag(v, NEWLINE_BEFORE);
        for (; at != tail; ++at) {
            Derelativize(PUSH(), at, VAL_SPECIFIER(OUT));
            SUBFRAME->baseline.stack_base += 1;  // see [3]
            if (newline) {
                Set_Cell_Flag(TOP, NEWLINE_BEFORE);  // see [2]
                newline = false;
            }
        }
    }
    else if (Is_Isotope(OUT))
        return RAISE(Error_Bad_Isotope(OUT));
    else {
        Move_Cell(PUSH(), OUT);
        SUBFRAME->baseline.stack_base += 1;  // see [3]

        if (Get_Cell_Flag(v, NEWLINE_BEFORE))  // see [2]
            Set_Cell_Flag(TOP, NEWLINE_BEFORE);
    }

    goto next_reduce_step;

} finished: {  ///////////////////////////////////////////////////////////////

    Drop_Frame_Unbalanced(SUBFRAME);  // Drop_Frame() asserts on accumulation

    Flags pop_flags = NODE_FLAG_MANAGED | ARRAY_MASK_HAS_FILE_LINE;
    if (Get_Subclass_Flag(ARRAY, VAL_ARRAY(v), NEWLINE_AT_TAIL))
        pop_flags |= ARRAY_FLAG_NEWLINE_AT_TAIL;

    return Init_Array_Cell(
        OUT,
        VAL_TYPE(v),
        Pop_Stack_Values_Core(STACK_BASE, pop_flags)
    );
}}


//
//  reduce-each: native [
//
//  {Evaluates expressions, keeping each result (DO only gives last result)}
//
//      return: "Last body result"
//          [<opt> <void> any-value!]
//      :vars "Variable to receive each reduced value (multiple TBD)"
//          [word! meta-word!]
//      block "Input block of expressions (@[block] acts like FOR-EACH)"
//          [block! the-block!]
//      body "Code to run on each step"
//          [block!]
//      /commas "Don't skip source-level commas (turn into comma! isotopes)"
//  ]
//
DECLARE_NATIVE(reduce_each)
//
// 1. This current REDUCE-EACH only works with one variable; it should be able
//    to take a block of variables.
{
    INCLUDE_PARAMS_OF_REDUCE_EACH;

    Value(*) vars = ARG(vars);
    Value(*) block = ARG(block);
    Value(*) body = ARG(body);

    bool breaking = false;

    enum {
        ST_REDUCE_EACH_INITIAL_ENTRY = STATE_0,
        ST_REDUCE_EACH_REDUCING_STEP,
        ST_REDUCE_EACH_RUNNING_BODY
    };

    switch (STATE) {
      case ST_REDUCE_EACH_INITIAL_ENTRY : goto initial_entry;
      case ST_REDUCE_EACH_REDUCING_STEP : goto reduce_step_output_in_spare;
      case ST_REDUCE_EACH_RUNNING_BODY : goto body_result_in_out;
      default : assert(false);
    }

  initial_entry: {  //////////////////////////////////////////////////////////

    Flags flags = FRAME_FLAG_TRAMPOLINE_KEEPALIVE;

    if (IS_META_WORD(vars)) {  // Note: gets converted to object in next step
        flags |= FRAME_FLAG_META_RESULT | FRAME_FLAG_FAILURE_RESULT_OK;
    }

    Context(*) context = Virtual_Bind_Deep_To_New_Context(
        ARG(body),  // may be updated, will still be GC safe
        ARG(vars)
    );
    Init_Object(ARG(vars), context);  // keep GC safe

    if (IS_THE_BLOCK(block))
        flags |= EVAL_EXECUTOR_FLAG_NO_EVALUATIONS;

    Frame(*) subframe = Make_Frame_At(block, flags);
    Push_Frame(SPARE, subframe);
    goto reduce_next;

} reduce_next: {  ////////////////////////////////////////////////////////////

    if (Is_Feed_At_End(SUBFRAME->feed))
        goto finished;

    if (IS_COMMA(At_Frame(SUBFRAME))) {
        Fetch_Next_Forget_Lookback(SUBFRAME);

        if (not REF(commas))
            goto reduce_next;

        Init_Comma(SPARE);
        mutable_QUOTE_BYTE(SPARE) = ISOTOPE_0;  // conflates if not meta
        goto reduce_step_output_in_spare;
    }

    SUBFRAME->executor = &Evaluator_Executor;  // restore from pass through

    STATE = ST_REDUCE_EACH_REDUCING_STEP;
    Restart_Evaluator_Frame(SUBFRAME);
    return CONTINUE_SUBFRAME(SUBFRAME);

} reduce_step_output_in_spare: {  ////////////////////////////////////////////

    if (not IS_META_WORD(vars) and (Is_Void(SPARE) or Is_Nihil(SPARE))) {
        Init_Nihil(OUT);
        goto reduce_next;
    }

    Move_Cell(CTX_VAR(VAL_CONTEXT(vars), 1), SPARE);  // do multiple? see [1]

    SUBFRAME->executor = &Just_Use_Out_Executor;  // pass through subframe

    STATE = ST_REDUCE_EACH_RUNNING_BODY;
    return CATCH_CONTINUE_BRANCH(OUT, body);

} body_result_in_out: {  /////////////////////////////////////////////////////

    if (THROWING) {
        if (not Try_Catch_Break_Or_Continue(OUT, FRAME, &breaking))
            goto finished;

        if (breaking)
            goto finished;
    }

    goto reduce_next;

} finished: {  ///////////////////////////////////////////////////////////////

    if (THROWING)  // subframe has already been dropped if thrown
        return THROWN;

    Drop_Frame(SUBFRAME);

    if (Is_Fresh(OUT))  // body never ran
        return VOID;

    if (breaking)
        return nullptr;  // BREAK encountered

    return BRANCHED(OUT);
}}


bool Match_For_Compose(noquote(Cell(const*)) group, const REBVAL *label) {
    assert(ANY_GROUP_KIND(CELL_HEART(group)));

    if (Is_Nulled(label))
        return true;

    assert(IS_TAG(label) or IS_FILE(label));

    if (VAL_LEN_AT(group) == 0) // you have a pattern, so leave `()` as-is
        return false;

    Cell(const*) first = VAL_ARRAY_ITEM_AT(group);
    if (VAL_TYPE(first) != VAL_TYPE(label))
        return false;

    return (CT_String(label, first, 1) == 0);
}


// This is a helper common to the Composer_Executor() and the COMPOSE native
// which will push a frame that does composing to the trampoline stack.
//
/////////////////////////////////////////////////////////////////////////////
//
// 1. COMPOSE relies on frame enumeration...and frames are only willing to
//    enumerate arrays.  Paths and tuples may be in a more compressed form.
//    While this is being rethought, we just reuse the logic of AS so it's in
//    one place and gets tested more, to turn sequences into arrays.
//
// 2. The easiest way to pass along options to the composing subframes is by
//    passing the frame of the COMPOSE to it.  Though Composer_Executor() has
//    no varlist of its own, it can read the frame variables of the native
//    so long as it is passed in the `main_frame` member of Frame.
//
static void Push_Composer_Frame(
    Value(*) out,
    Frame(*) main_frame,
    Cell(const*) arraylike,
    REBSPC *specifier
){
    Value(const*) adjusted = nullptr;
    if (ANY_PATH(arraylike)) {  // allow sequences, see [1]
        Derelativize(out, arraylike, specifier);
        adjusted = rebValue(Canon(AS), Canon(BLOCK_X), rebQ(out));
    }

    Frame(*) subframe = Make_Frame_At_Core(
        adjusted ? adjusted : arraylike,
        adjusted ? SPECIFIED : specifier,
        EVAL_EXECUTOR_FLAG_NO_EVALUATIONS
            | FRAME_FLAG_TRAMPOLINE_KEEPALIVE  // allows stack accumulation
            | FRAME_FLAG_FAILURE_RESULT_OK  // bubbles up definitional errors
    );
    Push_Frame(out, subframe);  // subframe may raise definitional failure

    if (adjusted)
        rebRelease(adjusted);

    subframe->executor = &Composer_Executor;

    subframe->u.compose.main_frame = main_frame;   // pass options, see [2]
    subframe->u.compose.changed = false;
}


// Another helper common to the Composer_Executor() and the COMPOSE native
// itself, which pops the processed array depending on the output type.
//
//////////////////////////////////////////////////////////////////////////////
//
// 1. If you write something like `compose '(void)/3:`, it would try to leave
//    behind something like the "SET-INTEGER!" of `3:`.  Currently that is
//    not allowed, though it could be a WORD! (like |3|:) ?
//
// 2. See Try_Pop_Sequence_Or_Element_Or_Nulled() for how reduced cases like
//    `(void).1` will turn into just INTEGER!, not `.1` -- this is in contrast
//    to `(blank).1` which does turn into `.1`
//
// 3. There are N instances of the NEWLINE_BEFORE flags on the pushed items,
//    and we need N + 1 flags.  Borrow the tail flag from the input array.
//
static Value(*) Finalize_Composer_Frame(
    Value(*) out,
    Frame(*) composer_frame,
    Cell(const*) composee  // special handling if the output kind is a sequence
){
    if (Is_Raised(out)) {
        Drop_Data_Stack_To(composer_frame->baseline.stack_base);
        return out;
    }

    enum Reb_Kind heart = CELL_HEART(composee);
    REBLEN quotes = VAL_NUM_QUOTES(composee);

    if (ANY_SEQUENCE_KIND(heart)) {
        if (not Try_Pop_Sequence_Or_Element_Or_Nulled(
            out,
            CELL_HEART(composee),
            composer_frame->baseline.stack_base
        )){
            if (Is_Valid_Sequence_Element(heart, out))
                fail (Error_Cant_Decorate_Type_Raw(out));  // no `3:`, see [1]

            fail (Error_Bad_Sequence_Init(out));
        }

        return Quotify(out, quotes);  // may not be sequence, see [2]
    }

    Flags flags = NODE_FLAG_MANAGED | ARRAY_MASK_HAS_FILE_LINE;
    if (Get_Subclass_Flag(ARRAY, VAL_ARRAY(composee), NEWLINE_AT_TAIL))
        flags |= ARRAY_FLAG_NEWLINE_AT_TAIL;  // proxy newline flag, see [3]

    Init_Array_Cell(
        out,
        heart,
        Pop_Stack_Values_Core(composer_frame->baseline.stack_base, flags)
    );

    return Quotify(out, quotes);
}


//
//  Composer_Executor: C
//
// Use rules of composition to do template substitutions on values matching
// `pattern` by evaluating those slots, leaving all other slots as is.
//
// Values are pushed to the stack because it is a "hot" preallocated large
// memory range, and the number of values can be calculated in order to
// accurately size the result when it needs to be allocated.  Not returning
// an array also offers more options for avoiding that intermediate if the
// caller wants to add part or all of the popped data to an existing array.
//
// At the end of the process, `f->u.compose.changed` will be false if the
// composed series is identical to the input, true if there were compositions.
//
//////////////////////////////////////////////////////////////////////////////
//
// 1. label -> e.g. if <*>, only match `(<*> ...)`
//    deep -> recurse into sub-blocks
//    predicate -> function to run on each spliced slot
//
// 2. HEART byte is used as a GROUP! matches regardless of quoting, so:
//
//        >> compose [a ''(1 + 2) b]
//        == [a ''3 b]
//
// 3. Splicing semantics match the rules for APPEND/etc.
//
// 4. Only proxy newline flag from the template on *first* value spliced in,
//    where it may have its own newline flag.  Not necessarily obvious; e.g.
//    would you want the composed block below to all fit on one line?
//
//        >> block-of-things: [
//               thing2  ; newline flag on thing1
//               thing3
//           ]
//
//        >> compose [thing1 ((block-of-things))]  ; no newline flag on (( ))
//        == [thing1
//               thing2  ; we proxy the flag, but is this what you wanted?
//               thing3
//           ]
//
// 5. At the end of the composer, we do not Drop_Data_Stack_To() and the frame
//    will still be alive for the caller.  This lets them have access to this
//    frame's BASELINE->stack_base, so it knows what all was pushed...and it
//    also means the caller can decide if they want the accrued items or not
//    depending on the `changed` field in the frame.
//
Bounce Composer_Executor(Frame(*) f)
{
    Frame(*) frame_ = f;

    if (THROWING)
        return THROWN;  // no state to cleanup (just data stack, auto-cleaned)

    DECLARE_PARAM(1, return);
    DECLARE_PARAM(2, label);
    DECLARE_PARAM(3, value);
    DECLARE_PARAM(4, deep);
    DECLARE_PARAM(5, predicate);

    Frame(*) main_frame = f->u.compose.main_frame;  // the invoked COMPOSE native

    UNUSED(FRM_ARG(main_frame, p_return_));
    Value(*) label = FRM_ARG(main_frame, p_label_);
    UNUSED(FRM_ARG(main_frame, p_value_));
    bool deep = not Is_Nulled(FRM_ARG(main_frame, p_deep_));
    Value(*) predicate = FRM_ARG(main_frame, p_predicate_);

    assert(Is_Nulled(predicate) or IS_ACTION(predicate));

    enum {
        ST_COMPOSER_INITIAL_ENTRY = STATE_0,
        ST_COMPOSER_EVAL_GROUP,
        ST_COMPOSER_RUNNING_PREDICATE,
        ST_COMPOSER_RECURSING_DEEP
    };

    switch (STATE) {
      case ST_COMPOSER_INITIAL_ENTRY :
        goto handle_current_item;

      case ST_COMPOSER_EVAL_GROUP :
      case ST_COMPOSER_RUNNING_PREDICATE :
        goto process_out;

      case ST_COMPOSER_RECURSING_DEEP :
        goto composer_finished_recursion;

      default : assert(false);
    }

  handle_next_item: {  ///////////////////////////////////////////////////////

   Fetch_Next_Forget_Lookback(f);
   goto handle_current_item;

} handle_current_item: {  ////////////////////////////////////////////////////

    if (Is_Frame_At_End(f))
        goto finished;

    Cell(const*) at = At_Frame(f);

    if (not ANY_ARRAYLIKE(at)) {  // won't substitute/recurse
        Derelativize(PUSH(), at, f_specifier);  // keep newline flag
        goto handle_next_item;
    }

    enum Reb_Kind heart = CELL_HEART(at);  // quoted groups match, see [1]

    REBSPC *match_specifier = nullptr;
    noquote(Cell(const*)) match = nullptr;

    if (not ANY_GROUP_KIND(heart)) {
        //
        // Don't compose at this level, but may need to walk deeply to
        // find compositions inside it if /DEEP and it's an array
    }
    else {  // plain compose, if match
        if (Match_For_Compose(at, label)) {
            match = at;
            match_specifier = f_specifier;
        }
    }

    if (not match) {
        if (deep) {
            // compose/deep [does [(1 + 2)] nested] => [does [3] nested]

            Push_Composer_Frame(OUT, main_frame, at, f_specifier);
            STATE = ST_COMPOSER_RECURSING_DEEP;
            return CATCH_CONTINUE_SUBFRAME(SUBFRAME);
        }

        // compose [[(1 + 2)] (3 + 4)] => [[(1 + 2)] 7]  ; non-deep
        //
        Derelativize(PUSH(), at, f_specifier);  // keep newline flag
        goto handle_next_item;
    }

    if (Is_Nulled(predicate))
        goto evaluate_group;

    Derelativize(SPARE, cast(Cell(const*), match), match_specifier);
    Dequotify(SPARE);  // cast was needed because there may have been quotes
    mutable_HEART_BYTE(SPARE) = REB_GROUP;  // don't confuse with decoration
    if (not Is_Nulled(label))
        VAL_INDEX_RAW(SPARE) += 1;  // wasn't possibly at END

    STATE = ST_COMPOSER_RUNNING_PREDICATE;
    return CONTINUE(OUT, predicate, SPARE);

  evaluate_group: { //////////////////////////////////////////////////////////

    // If <*> is the label and (<*> 1 + 2) is found, run just (1 + 2).
    //
    Feed(*) subfeed = Make_At_Feed_Core(match, match_specifier);
    if (not Is_Nulled(label))
        Fetch_Next_In_Feed(subfeed);  // wasn't possibly at END

    Frame(*) subframe = Make_Frame(
        subfeed,  // used subfeed so we could skip the label if there was one
        FRAME_FLAG_ALLOCATED_FEED
    );
    subframe->executor = &Array_Executor;

    Push_Frame(OUT, subframe);

    STATE = ST_COMPOSER_EVAL_GROUP;
    return CATCH_CONTINUE_SUBFRAME(subframe);

}} process_out: {  ///////////////////////////////////////////////////////////

    assert(
        STATE == ST_COMPOSER_EVAL_GROUP
        or STATE == ST_COMPOSER_RUNNING_PREDICATE
    );

    enum Reb_Kind group_heart = CELL_HEART(At_Frame(f));
    REBLEN group_quotes = VAL_NUM_QUOTES(At_Frame(f));

    if (Is_Splice(OUT))
        goto push_out_spliced;

    if (Is_Nulled(OUT))
        return RAISE(Error_Need_Non_Null_Raw());  // [(null)] => error!

    if (Is_Void(OUT)) {
        if (group_heart == REB_GROUP and group_quotes == 0)
            goto handle_next_item;  // compose [(void)] => []

        // [''(void)] => ['']
    }
    else
        Decay_If_Unstable(OUT);

    if (Is_Isotope(OUT))
        return RAISE(Error_Bad_Isotope(OUT));

    goto push_out_as_is;

  push_out_as_is:  ///////////////////////////////////////////////////////////

    // compose [(1 + 2) inserts as-is] => [3 inserts as-is]
    // compose [([a b c]) unmerged] => [[a b c] unmerged]

    if (Is_Void(OUT)) {
        assert(group_quotes != 0);  // handled above
        Init_Void(PUSH());
    }
    else
        Copy_Cell(PUSH(), OUT);  // can't stack eval direct

    if (group_heart == REB_SET_GROUP)
        Setify(TOP);
    else if (group_heart == REB_GET_GROUP)
        Getify(TOP);
    else if (group_heart == REB_META_GROUP)
        Metafy(TOP);
    else if (group_heart == REB_THE_GROUP)
        Theify(TOP);
    else
        assert(group_heart == REB_GROUP);

    Quotify(TOP, group_quotes);  // match original quotes

    // Use newline intent from the GROUP! in the compose pattern
    //
    if (Get_Cell_Flag(At_Frame(f), NEWLINE_BEFORE))
        Set_Cell_Flag(TOP, NEWLINE_BEFORE);
    else
        Clear_Cell_Flag(TOP, NEWLINE_BEFORE);

    f->u.compose.changed = true;
    goto handle_next_item;

  push_out_spliced:  /////////////////////////////////////////////////////////

    // compose [(([a b])) merges] => [a b merges]... see [3]

    if (group_quotes != 0 or group_heart != REB_GROUP)
        return RAISE("Currently can only splice plain unquoted GROUP!s");

    if (Is_Splice(OUT)) {  // GROUP! at "quoting level -1" means splice
        Quasify_Isotope(OUT);

        Cell(const*) push_tail;
        Cell(const*) push = VAL_ARRAY_AT(&push_tail, OUT);
        if (push != push_tail) {
            Derelativize(PUSH(), push, VAL_SPECIFIER(OUT));
            if (Get_Cell_Flag(At_Frame(f), NEWLINE_BEFORE))
                Set_Cell_Flag(TOP, NEWLINE_BEFORE);  // first, see [4]
            else
                Clear_Cell_Flag(TOP, NEWLINE_BEFORE);

            while (++push, push != push_tail)
                Derelativize(PUSH(), push, VAL_SPECIFIER(OUT));
        }
    }
    else {
        assert(not ANY_ARRAY(OUT));
        Copy_Cell(PUSH(), OUT);
    }

    f->u.compose.changed = true;
    goto handle_next_item;

} composer_finished_recursion: {  ////////////////////////////////////////////

    // The compose stack of the nested compose is relative to *its* baseline.

    if (Is_Raised(OUT)) {
        Drop_Data_Stack_To(SUBFRAME->baseline.stack_base);
        Drop_Frame(SUBFRAME);
        return OUT;
    }

    assert(Is_Void(OUT));

    if (not SUBFRAME->u.compose.changed) {
        //
        // To save on memory usage, Ren-C does not make copies of
        // arrays that don't have some substitution under them.  This
        // may be controlled by a switch if it turns out to be needed.
        //
        Drop_Data_Stack_To(SUBFRAME->baseline.stack_base);
        Drop_Frame(SUBFRAME);

        Derelativize(PUSH(), At_Frame(f), f_specifier);
        // Constify(TOP);
        goto handle_next_item;
    }

    Finalize_Composer_Frame(OUT, SUBFRAME, At_Frame(f));
    Drop_Frame(SUBFRAME);
    Move_Cell(PUSH(), OUT);

    if (Get_Cell_Flag(At_Frame(f), NEWLINE_BEFORE))
        Set_Cell_Flag(TOP, NEWLINE_BEFORE);

    f->u.compose.changed = true;
    goto handle_next_item;

} finished: {  ///////////////////////////////////////////////////////////////

    assert(Get_Frame_Flag(f, TRAMPOLINE_KEEPALIVE));  // caller needs, see [5]

    return Init_Void(OUT);  // signal finished, avoid leaking temp evaluations
}}


//
//  compose: native [
//
//  {Evaluates only contents of GROUP!-delimited expressions in an array}
//
//      return: [blackhole! any-array! any-sequence! any-word! action!]
//      'label "Distinguish compose groups, e.g. [(plain) (<*> composed)]"
//          [<skip> tag! file!]
//      value "The template to fill in (no-op if WORD!, ACTION! or SPACE!)"
//          [blackhole! any-array! any-sequence! any-word! action!]
//      /deep "Compose deeply into nested arrays"
//      /predicate "Function to run on composed slots (default: META)"
//          [<unrun> action!]
//  ]
//
//  ; Note: /INTO is intentionally no longer supported
//  ; https://forum.rebol.info/t/stopping-the-into-virus/705
//
//  ; Note: /ONLY is intentionally no longer supported
//  https://forum.rebol.info/t/the-superpowers-of-ren-cs-revamped-compose/979/7
//
DECLARE_NATIVE(compose)
{
    INCLUDE_PARAMS_OF_COMPOSE;

    Value(*) v = ARG(value);

    USED(ARG(predicate));  // used by Composer_Executor() via main_frame
    USED(ARG(label));
    USED(ARG(deep));

    enum {
        ST_COMPOSE_INITIAL_ENTRY = STATE_0,
        ST_COMPOSE_COMPOSING
    };

    switch (STATE) {
      case ST_COMPOSE_INITIAL_ENTRY: goto initial_entry;
      case ST_COMPOSE_COMPOSING: goto composer_finished;
      default: assert(false);
    }

  initial_entry: {  //////////////////////////////////////////////////////////

    if (Is_Blackhole(v))
        return COPY(v);  // sink locations composed to avoid double eval

    if (ANY_WORD(v) or IS_ACTION(v))
        return COPY(v);  // makes it easier to `set compose target`

    Push_Composer_Frame(OUT, frame_, v, VAL_SPECIFIER(v));

    STATE = ST_COMPOSE_COMPOSING;
    return CONTINUE_SUBFRAME(SUBFRAME);

} composer_finished: {  //////////////////////////////////////////////////////

    Finalize_Composer_Frame(OUT, SUBFRAME, v);
    Drop_Frame(SUBFRAME);

    if (Is_Raised(OUT))  // subframe was killed
        return OUT;

    return OUT;
}}


enum FLATTEN_LEVEL {
    FLATTEN_NOT,
    FLATTEN_ONCE,
    FLATTEN_DEEP
};


static void Flatten_Core(
    Cell(*) head,
    Cell(const*) tail,
    REBSPC *specifier,
    enum FLATTEN_LEVEL level
) {
    Cell(*) item = head;
    for (; item != tail; ++item) {
        if (IS_BLOCK(item) and level != FLATTEN_NOT) {
            REBSPC *derived = Derive_Specifier(specifier, item);

            Cell(const*) sub_tail;
            Cell(*) sub = VAL_ARRAY_AT_ENSURE_MUTABLE(&sub_tail, item);
            Flatten_Core(
                sub,
                sub_tail,
                derived,
                level == FLATTEN_ONCE ? FLATTEN_NOT : FLATTEN_DEEP
            );
        }
        else
            Derelativize(PUSH(), item, specifier);
    }
}


//
//  flatten: native [
//
//  {Flattens a block of blocks.}
//
//      return: [block!]
//          {The flattened result block}
//      block [block!]
//          {The nested source block}
//      /deep
//  ]
//
DECLARE_NATIVE(flatten)
{
    INCLUDE_PARAMS_OF_FLATTEN;

    StackIndex base = TOP_INDEX;

    Cell(const*) tail;
    Cell(*) at = VAL_ARRAY_AT_ENSURE_MUTABLE(&tail, ARG(block));
    Flatten_Core(
        at,
        tail,
        VAL_SPECIFIER(ARG(block)),
        REF(deep) ? FLATTEN_DEEP : FLATTEN_ONCE
    );

    return Init_Block(OUT, Pop_Stack_Values(base));
}
