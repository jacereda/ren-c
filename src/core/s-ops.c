//
//  File: %s-ops.c
//  Summary: "string handling utilities"
//  Section: strings
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


//
//  All_Bytes_ASCII: C
//
// Returns true if byte string does not use upper code page
// (e.g. no 128-255 characters)
//
bool All_Bytes_ASCII(Byte* bp, REBLEN len)
{
    for (; len > 0; len--, bp++)
        if (*bp >= 0x80)
            return false;

    return true;
}


//
//  Analyze_String_For_Scan: C
//
// Locate beginning byte pointer and number of bytes to prepare a string
// into a form that can be used with a Scan_XXX routine.  Used for instance
// to MAKE DATE! from a STRING!.  Rules are:
//
//     1. it's actual content (less space, newlines) <= max len
//     2. it does not contain other values ("123 456")
//     3. it's not empty or only whitespace
//
// !!! This seems to be an addition to R3-Alpha for things like TO WORD! of
// a TEXT! to use with arbitrary whitespace (Rebol2 would just include the
// whitespace in the WORD!).  In cases such like that, it is redundant with
// work done by TRANSCODE...though it is lighter weight.  It also permits
// clipping out syntax that may not be accepted by the scanner (e.g. if
// TO DATE! permitted textual syntax that was not independently LOAD-able).
// It should be reviewed.
//
const Byte* Analyze_String_For_Scan(
    option(Size*) size_out,
    const REBVAL *any_string,
    REBLEN max_len  // maximum length in *codepoints*
){
    REBLEN len;
    Utf8(const*) up = VAL_UTF8_LEN_SIZE_AT(&len, nullptr, any_string);
    if (len == 0)
        fail (Error_Index_Out_Of_Range_Raw());

    // Skip leading whitespace
    //
    Codepoint c;
    Codepoint i;
    for (i = 0; IS_SPACE(c = CHR_CODE(up)) and (i < len); ++i, --len)
        up = NEXT_STR(up);

    if (len == 0)
        fail (Error_Index_Out_Of_Range_Raw());

    Utf8(const*) at_index = up;

    // Skip up to max_len non-space characters.
    //
    // !!! The R3-Alpha code would fail with Error_Invalid_Chars_Raw() if
    // there were UTF-8 characters in most calls.  Only ANY-WORD! from
    // ANY-STRING! allowed it.  Though it's not clear why it wouldn't be
    // better to delegate to the scanning routine itself to give a more
    // pointed error... allow c >= 0x80 for now.
    //
    REBLEN num_chars = 0;
    do {
        ++num_chars;
        if (num_chars > max_len)
            fail (Error_Too_Long_Raw());

        --len;
        up = NEXT_STR(up);
    } while (len > 0 and not IS_SPACE(c = CHR_CODE(up)));

    if (size_out)  // give back byte size before trailing spaces
        *unwrap(size_out) = up - at_index;

    // Rest better be just spaces
    //
    for (; len > 0; --len) {
        if (not IS_SPACE(c))
            fail (Error_Invalid_Chars_Raw());
        up = NEXT_CHR(&c, up);
    }

    return at_index;
}


//
//  Trim_Tail: C
//
// Used to trim off hanging spaces during FORM and MOLD.
//
void Trim_Tail(REB_MOLD *mo, Byte ascii)
{
    assert(ascii < 0x80);  // more work needed for multi-byte characters

    Length len = STR_LEN(mo->series);
    Size size = STR_SIZE(mo->series);

    for (; size > 0; --size, --len) {
        Byte b = *BIN_AT(mo->series, size - 1);
        if (b != ascii)
            break;
    }

    TERM_STR_LEN_SIZE(mo->series, len, size);
}


//
//  Change_Case: C
//
// Common code for string case handling.
//
void Change_Case(
    REBVAL *out,
    REBVAL *val, // !!! Not const--uses Partial(), may change index, review
    const REBVAL *part,
    bool upper
){
    if (IS_CHAR(val)) {
        Codepoint c = VAL_CHAR(val);
        Init_Char_Unchecked(out, upper ? UP_CASE(c) : LO_CASE(c));
        return;
    }

    assert(ANY_STRING(val));

    // This is a mutating operation, and we want to return the same series at
    // the same index.  However, R3-Alpha code would use Partial() and may
    // change val's index.  Capture it before potential change, review.
    //
    Copy_Cell(out, val);

    REBLEN len = Part_Len_May_Modify_Index(val, part);

    // !!! This assumes that all case changes will preserve the encoding size,
    // but that's not true (some strange multibyte accented characters have
    // capital or lowercase versions that are single byte).  This may be
    // uncommon enough to have special handling (only do something weird, e.g.
    // use the mold buffer, if it happens...for the remaining portion of such
    // a string...and only if the size *expands*).  Expansions also may never
    // be possible, only contractions (is that true?)  Review when UTF-8
    // Everywhere is more mature to the point this is worth worrying about.
    //
    Utf8(*) up = VAL_STRING_AT_ENSURE_MUTABLE(val);
    Utf8(*) dp;
    if (upper) {
        REBLEN n;
        for (n = 0; n < len; n++) {
            dp = up;

            Codepoint c;
            up = NEXT_CHR(&c, up);
            if (c < UNICODE_CASES) {
                dp = WRITE_CHR(dp, UP_CASE(c));
                assert(dp == up); // !!! not all case changes same byte size?
            }
        }
    }
    else {
        REBLEN n;
        for (n = 0; n < len; n++) {
            dp = up;

            Codepoint c;
            up = NEXT_CHR(&c, up);
            if (c < UNICODE_CASES) {
                dp = WRITE_CHR(dp, LO_CASE(c));
                assert(dp == up); // !!! not all case changes same byte size?
            }
        }
    }
}


//
//  Split_Lines: C
//
// Given a string series, split lines on CR-LF.  Give back array of strings.
//
// Note: The definition of "line" in POSIX is a sequence of characters that
// end with a newline.  Hence, the last line of a file should have a newline
// marker, or it's not a "line")
//
// https://stackoverflow.com/a/729795
//
// This routine does not require it.
//
// !!! CR support is likely to be removed...and CR will be handled as a normal
// character, with special code needed to process it.
//
Array(*) Split_Lines(const REBVAL *str)
{
    StackIndex base = TOP_INDEX;

    REBLEN len = VAL_LEN_AT(str);
    REBLEN i = VAL_INDEX(str);
    if (i == len)
        return Make_Array(0);

    DECLARE_MOLD (mo);
    Push_Mold(mo);

    Utf8(const*) cp = VAL_STRING_AT(str);

    Codepoint c;
    cp = NEXT_CHR(&c, cp);

    for (; i < len; ++i, cp = NEXT_CHR(&c, cp)) {
        if (c != LF && c != CR) {
            Append_Codepoint(mo->series, c);
            continue;
        }

        Init_Text(PUSH(), Pop_Molded_String(mo));
        Set_Cell_Flag(TOP, NEWLINE_BEFORE);

        Push_Mold(mo);

        if (c == CR) {
            Utf8(const*) tp = NEXT_CHR(&c, cp);
            if (c == LF) {
                ++i;
                cp = tp; // treat CR LF as LF, lone CR as LF
            }
        }
    }

    // If there's any remainder we pushed in the buffer, consider the end of
    // string to be an implicit line-break

    if (STR_SIZE(mo->series) == mo->base.size)
        Drop_Mold(mo);
    else {
        Init_Text(PUSH(), Pop_Molded_String(mo));
        Set_Cell_Flag(TOP, NEWLINE_BEFORE);
    }

    return Pop_Stack_Values_Core(base, ARRAY_FLAG_NEWLINE_AT_TAIL);
}
