//
//  File: %sys-rebarr.h
//  Summary: {any-array! defs BEFORE %tmp-internals.h (see: %sys-array.h)}
//  Project: "Rebol 3 Interpreter and Run-time (Ren-C branch)"
//  Homepage: https://github.com/metaeducation/ren-c/
//
//=////////////////////////////////////////////////////////////////////////=//
//
// Copyright 2012-2021 Ren-C Open Source Contributors
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
// In the C build, a Array(*) and REBSER* are the same type.  The C++ build
// derives Raw_Array from Raw_Series...meaning you can pass an array to a
// function that expects a series, but not vice-versa.
//
// There are several subclasses (FLAVOR_XXX) whose elements are value cells,
// and hence are arrays.  However the "plain" array, e.g. the kind used in
// BLOCK!s and GROUP!s, is its own subclass...which interprets the subclass
// bits in particular ways not relevant to other arrays (e.g. object variable
// lists do not need a flag tracking if there's a newline that needs to be
// output at the end of the varlist).
//

#if CPLUSPLUS_11
    struct Raw_Array : public Raw_Series {};
#else
    typedef Raw_Series Raw_Array;
#endif

#define Array(star_maybe_const) \
    Raw_Array star_maybe_const


// It may become interesting to say that a specifier can be a pairing or
// a REBVAL* of some kind.  But for the moment, that just complicates the
// issue of not being able to check the ->header bits safely (it would require
// checking the NODE_BYTE() first, then casting to a VAL() or SER()).  In
// the interests of making the code strict-aliasing-safe for starters, assume
// all specifiers are arrays.
//
typedef Raw_Array REBSPC;


//=//// TYPE HOOK ACCESS //////////////////////////////////////////////////=//
//
// Built-in types identify themselves as one of ~64 fundamental "kinds".  This
// occupies a byte in the header (64 is chosen as a limit currently in order
// to be used with 64-bit typesets, but this is due for change).
//
// For efficiency, what's put in the extra is what would be like that type's
// row in the `Builtin_Type_Hooks` if it had been built-in.  These table
// rows are speculatively implemented as an untyped array of CFUNC* which is
// null terminated (vs. a struct with typed fields) so that the protocol can
// be expanded without breaking strict aliasing.
//

enum Reb_Type_Hook_Index {
    IDX_GENERIC_HOOK,
    IDX_COMPARE_HOOK,
    IDX_MAKE_HOOK,
    IDX_TO_HOOK,
    IDX_MOLD_HOOK,
    IDX_HOOK_NULLPTR,  // see notes on why null termination convention
    IDX_HOOKS_MAX
};


//=//// ARRAY_FLAG_HAS_FILE_LINE_UNMASKED /////////////////////////////////=//
//
// The Raw_Series node has two pointers in it, ->link and ->misc, which are
// used for a variety of purposes (pointing to the keylist for an object,
// the C code that runs as the dispatcher for a function, etc.)  But for
// regular source series, they can be used to store the filename and line
// number, if applicable.
//
// Only arrays preserve file and line info, as UTF-8 strings need to use the
// ->misc and ->link fields for caching purposes in strings.
//
#define ARRAY_FLAG_HAS_FILE_LINE_UNMASKED \
    SERIES_FLAG_24

#define ARRAY_MASK_HAS_FILE_LINE \
    (ARRAY_FLAG_HAS_FILE_LINE_UNMASKED | SERIES_FLAG_LINK_NODE_NEEDS_MARK)


//=//// ARRAY_FLAG_25 /////////////////////////////////////////////////////=//
//
#define ARRAY_FLAG_25 \
    SERIES_FLAG_25


//=//// ARRAY_FLAG_26 /////////////////////////////////////////////////////=//
//
#define ARRAY_FLAG_26 \
    SERIES_FLAG_26


//=//// ARRAY_FLAG_27 /////////////////////////////////////////////////////=//
//
#define ARRAY_FLAG_27 \
    SERIES_FLAG_27


//=//// ARRAY_FLAG_28 /////////////////////////////////////////////////////=//
//
#define ARRAY_FLAG_28 \
    SERIES_FLAG_28


//=//// ARRAY_FLAG_CONST_SHALLOW //////////////////////////////////////////=//
//
// When a COPY is made of an ANY-ARRAY! that has CELL_FLAG_CONST, the new
// value shouldn't be const, as the goal of copying it is generally to modify.
// However, if you don't copy it deeply, then mere copying should not be
// giving write access to levels underneath it that would have been seen as
// const if they were PICK'd out before.  This flag tells the copy operation
// to mark any cells that are shallow references as const.  For convenience
// it is the same bit as the const flag one would find in the value.
//
#define ARRAY_FLAG_CONST_SHALLOW \
    SERIES_FLAG_30
STATIC_ASSERT(ARRAY_FLAG_CONST_SHALLOW == CELL_FLAG_CONST);


//=//// ARRAY_FLAG_NEWLINE_AT_TAIL ////////////////////////////////////////=//
//
// The mechanics of how Rebol tracks newlines is that there is only one bit
// per value to track the property.  Yet since newlines are conceptually
// "between" values, that's one bit too few to represent all possibilities.
//
// Ren-C carries a bit for indicating when there's a newline intended at the
// tail of an array.
//
#define ARRAY_FLAG_NEWLINE_AT_TAIL \
    SERIES_FLAG_31


// Ordinary source arrays use their ->link field to point to an interned file
// name string (or URL string) from which the code was loaded.  If a series
// was not created from a file, then the information from the source that was
// running at the time is propagated into the new second-generation series.
//
// !!! LINK_FILENAME_HACK is needed in %sys-array.h due to dependencies not
// having STR() available.
//
#define LINK_Filename_TYPE          String(const*)
#define LINK_Filename_CAST          (String(const*))STR
#define HAS_LINK_Filename           FLAVOR_ARRAY
