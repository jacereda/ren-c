//
//  File: %exec-scan.h
//  Summary: {Flags and Frame State for Scanner_Executor()}
//  Project: "Rebol 3 Interpreter and Run-time (Ren-C branch)"
//  Homepage: https://github.com/metaeducation/ren-c/
//
//=////////////////////////////////////////////////////////////////////////=//
//
// Copyright 2012-2022 Ren-C Open Source Contributors
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
// The executor state has to be defined in order to be used (easily) in the
// union of the Reb_Frame.
//

// (Note: %sys-do.h needs to call into the scanner if Fetch_Next_In_Frame() is
// to be inlined at all--at its many time-critical callsites--so the scanner
// has to be in the internal API)
//
#include "sys-scan.h"


#define EXECUTOR_SCAN &Scanner_Executor  // shorthand in Xxx_Executor_Flag()


//=//// SCAN_EXECUTOR_FLAG_NEWLINE_PENDING ////////////////////////////////=//
//
// CELL_FLAG_LINE appearing on a value means that there is a line break
// *before* that value.  Hence when a newline is seen, it means the *next*
// value to be scanned will receive the flag.
//
#define SCAN_EXECUTOR_FLAG_NEWLINE_PENDING \
    FRAME_FLAG_24


//=//// SCAN_EXECUTOR_FLAG_JUST_ONCE //////////////////////////////////////=//
//
// Supporting flag for TRANSCODE/NEXT
//
#define SCAN_EXECUTOR_FLAG_JUST_ONCE \
    FRAME_FLAG_25


//=//// SCAN_EXECUTOR_FLAG_NULLEDS_LEGAL //////////////////////////////////=//
//
// NULL splice in top level of rebValue()
//
// !!! Appears no longer used; pure NULL now always splices as ~_~ QUASI!
// That's probably a bad idea.
//
#define SCAN_EXECUTOR_FLAG_NULLEDS_LEGAL \
    FRAME_FLAG_26


//=//// SCAN_EXECUTOR_FLAG_LOCK_SCANNED ///////////////////////////////////=//
//
// Lock series as they are loaded
//
// !!! This also does not seem to be used, likely supplanted by CONST.
//
#define SCAN_EXECUTOR_FLAG_LOCK_SCANNED \
    FRAME_FLAG_27


//=//// SCAN_EXECUTOR_27 //////////////////////////////////////////////////=//
//
#define SCAN_EXECUTOR_FLAG_27 \
    FRAME_FLAG_27


//=//// SCAN_EXECUTOR_FLAG_24 /////////////////////////////////////////////=//
//
#define SCAN_EXECUTOR_FLAG_28 \
    FRAME_FLAG_28


//=//// SCAN_EXECUTOR_FLAG_29 /////////////////////////////////////////////=//
//
#define SCAN_EXECUTOR_FLAG_29 \
    FRAME_FLAG_29


//=//// SCAN_EXECUTOR_FLAG_30 /////////////////////////////////////////////=//
//
#define SCAN_EXECUTOR_FLAG_30 \
    FRAME_FLAG_30


//=//// SCAN_EXECUTOR_FLAG_31 /////////////////////////////////////////////=//
//
#define SCAN_EXECUTOR_FLAG_31 \
    FRAME_FLAG_31


// Flags that should be preserved when recursing the scanner
//
#define SCAN_EXECUTOR_MASK_RECURSE \
    (SCAN_EXECUTOR_FLAG_NULLEDS_LEGAL \
        | SCAN_EXECUTOR_FLAG_LOCK_SCANNED)


typedef struct rebol_scan_state {  // shared across all levels of a scan
    //
    // Beginning and end positions of currently processed token.
    //
    const Byte* begin;
    const Byte* end;

    const Raw_String* file;  // currently scanning (or anonymous)

    LineNumber line;  // line number where current scan position is
    const Byte* line_head;  // pointer to head of current line (for errors)

    // The "limit" feature was not implemented, scanning just stopped at '\0'.
    // It may be interesting in the future, but it doesn't mix well with
    // scanning variadics which merge REBVAL and UTF-8 strings together...
    //
    /* const Byte* limit; */
} SCAN_STATE;

typedef struct rebol_scan_level {  // each array scan corresponds to a level
    SCAN_STATE *ss;  // shared state of where the scanner head currently is

    // '\0' => top level scan
    // ']' => this level is scanning a block
    // ')' => this level is scanning a group
    // '/' => this level is scanning a path
    // '.' => this level is scanning a tuple
    //
    // (Chosen as the terminal character to use in error messages for the
    // character we are seeking to find a match for).
    //
    Byte mode;

    REBLEN start_line;
    const Byte* start_line_head;

    // !!! Before stackless, these were locals in Scan_To_Stack()
    //
    REBLEN quotes_pending;
    enum Reb_Token token;
    enum Reb_Token prefix_pending;
    bool quasi_pending;

} SCAN_LEVEL;
