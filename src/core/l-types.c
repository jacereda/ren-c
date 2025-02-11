//
//  File: %l-types.c
//  Summary: "special lexical type converters"
//  Section: lexical
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
#include "sys-dec-to-char.h"
#include <errno.h>


//
// The scanning code in R3-Alpha used NULL to return failure during the scan
// of a value, possibly leaving the value itself in an incomplete or invalid
// state.  Rather than write stray incomplete values into these spots, Ren-C
// puts it back to an erased cell.
//

#define return_NULL \
    do { Erase_Cell(out); return nullptr; } while (1)


//
//  MAKE_Fail: C
//
Bounce MAKE_Fail(
    Frame(*) frame_,
    enum Reb_Kind kind,
    option(const REBVAL*) parent,
    const REBVAL *arg
){
    UNUSED(kind);
    UNUSED(parent);
    UNUSED(arg);

    return RAISE("Datatype does not have a MAKE handler registered");
}


//
//  MAKE_Unhooked: C
//
// MAKE STRUCT! is part of the FFI extension, but since user defined types
// aren't ready yet as a general concept, this hook is overwritten in the
// dispatch table when the extension loads.
//
Bounce MAKE_Unhooked(
    Frame(*) frame_,
    enum Reb_Kind kind,
    option(const REBVAL*) parent,
    const REBVAL *arg
){
    UNUSED(parent);
    UNUSED(arg);

    const REBVAL *type = Datatype_From_Kind(kind);
    UNUSED(type); // !!! put in error message?

    return RAISE(
        "Datatype is provided by an extension that's not currently loaded"
    );
}


//
//  make: native [
//
//  {Constructs or allocates the specified datatype.}
//
//      return: [<opt> any-value!]
//          "Constructed value, or null if BLANK! input"
//      type [<maybe> meta-word! any-value!]
//          {The datatype or parent value to construct from}
//      def [<maybe> <unrun> element?]  ; accept activation for FRAME!
//          {Definition or size of the new value (binding may be modified)}
//  ]
//
DECLARE_NATIVE(make)
{
    INCLUDE_PARAMS_OF_MAKE;

    REBVAL *type = ARG(type);
    REBVAL *arg = ARG(def);

    // See notes in DECLARE_NATIVE(do) for why this is the easiest way to pass
    // a flag to Do_Any_Array(), to help us discern the likes of:
    //
    //     foo: does [make object! [x: [1 2 3]]]  ; x inherits frame const
    //
    //     data: [x: [1 2 3]]
    //     bar: does [make object! data]  ; x wasn't const, don't add it
    //
    // So if the MAKE is evaluative (as OBJECT! is) this stops the "wave" of
    // evaluativeness of a frame (e.g. body of DOES) from applying.
    //
    if (Not_Cell_Flag(arg, CONST))
        Set_Cell_Flag(arg, EXPLICITLY_MUTABLE);

    option(const REBVAL*) parent;
    enum Reb_Kind kind;
    if (IS_TYPE_WORD(type)) {
        kind = VAL_TYPE_KIND(type);
        parent = nullptr;
    }
    else {
        kind = VAL_TYPE(type);
        parent = type;
    }

    MAKE_HOOK *hook = Make_Hook_For_Kind(kind);

    Bounce b = hook(frame_, kind, parent, arg);  // might throw, fail...
    if (b == BOUNCE_DELEGATE)
        return b;  // !!! Doesn't check result if continuation used, review
    if (b == BOUNCE_THROWN)
        return b;
    REBVAL *r = Value_From_Bounce(b);
    if (r != nullptr) {
        if (Is_Raised(r))
            return r;
        if (VAL_TYPE(r) == kind)
            return r;
    }
    return RAISE("MAKE dispatcher did not return correct type");
}


//
//  TO_Fail: C
//
Bounce TO_Fail(REBVAL *out, enum Reb_Kind kind, const REBVAL *arg)
{
    UNUSED(out);
    UNUSED(kind);
    UNUSED(arg);

    fail ("Cannot convert to datatype");
}


//
//  TO_Unhooked: C
//
Bounce TO_Unhooked(REBVAL *out, enum Reb_Kind kind, const REBVAL *arg)
{
    UNUSED(out);
    UNUSED(arg);

    const REBVAL *type = Datatype_From_Kind(kind);
    UNUSED(type); // !!! put in error message?

    fail ("Datatype does not have extension with a TO handler registered");
}


//
//  to: native [
//
//  {Converts to a specified datatype, copying any underying data}
//
//      return: "VALUE converted to TYPE, null if type or value are blank"
//          [<opt> any-value!]
//      type [<maybe> type-word!]
//      value [<maybe> any-value!]
//  ]
//
DECLARE_NATIVE(to)
{
    INCLUDE_PARAMS_OF_TO;

    REBVAL *v = ARG(value);
    REBVAL *type = ARG(type);

    enum Reb_Kind new_kind = VAL_TYPE_KIND(type);
    enum Reb_Kind old_kind = VAL_TYPE(v);

    if (new_kind == old_kind) {
        return rebValue("copy @", v);
    }

    TO_HOOK* hook = To_Hook_For_Type(type);

    Bounce b = hook(frame_, new_kind, v); // may fail();
    if (b == BOUNCE_THROWN) {
        assert(!"Illegal throw in TO conversion handler");
        fail (Error_No_Catch_For_Throw(FRAME));
    }
    REBVAL *r = Value_From_Bounce(b);
    if (Is_Raised(r))
        return r;

    if (r == nullptr or VAL_TYPE(r) != new_kind) {
        assert(!"TO conversion did not return intended type");
        return RAISE(Error_Invalid_Type(VAL_TYPE(r)));
    }
    return r;  // must be either OUT or an API handle
}


//
//  REBTYPE: C
//
// There's no actual "Unhooked" data type, it is used as a placeholder for
// if a datatype (such as STRUCT!) is going to have its behavior loaded by
// an extension.
//
REBTYPE(Unhooked)
{
    UNUSED(verb);

    return RAISE(
        "Datatype does not have its REBTYPE() handler loaded by extension"
    );
}


// !!! Some reflectors are more general and apply to all types (e.g. TYPE)
// while others only apply to some types (e.g. LENGTH or HEAD only to series,
// or perhaps things like PORT! that wish to act like a series).  This
// suggests a need for a kind of hierarchy of handling.
//
// The series common code is in Series_Common_Action_Maybe_Unhandled(), but
// that is only called from series.  Handle a few extra cases here.
//
Bounce Reflect_Core(Frame(*) frame_)
{
    INCLUDE_PARAMS_OF_REFLECT;

    REBVAL *v = ARG(value);

    option(SymId) id = VAL_WORD_ID(ARG(property));
    if (not id) {
        //
        // If a word wasn't in %words.r, it has no integer SYM.  There is
        // no way for a built-in reflector to handle it...since they just
        // operate on SYMs in a switch().  Longer term, a more extensible
        // idea will be necessary.
        //
        fail (Error_Cannot_Reflect(CELL_HEART(v), ARG(property)));
    }

    switch (id) {
      case SYM_KIND:
      case SYM_TYPE:  // currently synonym for KIND, may change
        if (Is_Void(v))
            return nullptr;
        return Init_Builtin_Datatype(OUT, VAL_TYPE(v));

      case SYM_QUOTES:
        return Init_Integer(OUT, VAL_NUM_QUOTES(v));

      default:
        // !!! Are there any other universal reflectors?
        break;
    }

    mutable_QUOTE_BYTE(ARG(value)) = UNQUOTED_1;  // ignore QUASI! or QUOTED!

    INIT_FRM_PHASE(frame_, VAL_ACTION(Lib(REFLECT)));  // switch to generic
    return BOUNCE_CONTINUE;
}


//
//  reflect-native: native [
//
//  {Returns specific details about a datatype.}
//
//      return: [<opt> any-value!]
//      value "Accepts isotopes for the purposes of TYPE OF"
//          [<maybe> <opt> any-value!]
//      property [word!]
//          "Such as: type, length, spec, body, words, values, title"
//  ]
//
DECLARE_NATIVE(reflect_native)
//
// Although REFLECT goes through dispatch to the REBTYPE(), it was needing
// a null check in Type_Action_Dispatcher--which no other type needs.  So
// it is its own native.  Consider giving it its own dispatcher as well, as
// the question of exactly what a "REFLECT" or "OF" actually *is*.
{
    return Reflect_Core(frame_);
}


//
//  of: enfix native [
//
//  {Infix form of REFLECT which quotes its left (X OF Y => REFLECT Y 'X)}
//
//      return: [<opt> any-value!]
//      'property "Will be escapable, ':property (bootstrap permitting)"
//          [word! get-word! get-path! get-group!]
//      value "Accepts null so TYPE OF NULL can be returned as null"
//          [<maybe> <opt> any-value!]
//  ]
//
DECLARE_NATIVE(of)
//
// !!! ':PROPERTY is not loadable by the bootstrap executable at time of
// writing.  But that is desired over 'PROPERTY or :PROPERTY so that both
// these cases would work:
//
//     >> integer! = kind of 1
//     == ~true~  ; isotope
//
//     >> integer! = :(second [length kind]) of 1
//     == ~true~  ; isotope
//
// For the moment the behavior is manually simulated.
//
// Common enough to be worth it to do some kind of optimization so it's not
// much slower than a REFLECT; e.g. you don't want it building a separate
// frame to make the REFLECT call in just because of the parameter reorder.
{
    INCLUDE_PARAMS_OF_OF;

    REBVAL *prop = ARG(property);

    if (ANY_ESCAPABLE_GET(prop)) {  // !!! See note above
        if (Eval_Value_Throws(SPARE, prop, SPECIFIED))
            return THROWN;

        if (not IS_WORD(SPARE)) {
            Move_Cell(prop, SPARE);
            fail (Error_Invalid_Arg(frame_, PARAM(property)));
        }
    }
    else
        Copy_Cell(SPARE, prop);

    // !!! Ugly hack to make OF frame-compatible with REFLECT.  If there was
    // a separate dispatcher for REFLECT it could be called with proper
    // parameterization, but as things are it expects the arguments to
    // fit the type action dispatcher rule... dispatch item in first arg,
    // property in the second.
    //
    Copy_Cell(ARG(property), ARG(value));
    Copy_Cell(ARG(value), SPARE);

    return Reflect_Core(frame_);
}


//
//  Scan_Hex: C
//
// Scans hex while it is valid and does not exceed the maxlen.
// If the hex string is longer than maxlen - it's an error.
// If a bad char is found less than the minlen - it's an error.
// String must not include # - ~ or other invalid chars.
// If minlen is zero, and no string, that's a valid zero value.
//
// Note, this function relies on LEX_WORD lex values having a LEX_VALUE
// field of zero, except for hex values.
//
const Byte* Scan_Hex(
    REBVAL *out,
    const Byte* cp,
    REBLEN minlen,
    REBLEN maxlen
){
    if (maxlen > MAX_HEX_LEN)
        return_NULL;

    REBI64 i = 0;
    REBLEN len = 0;
    Byte lex;
    while ((lex = Lex_Map[*cp]) > LEX_WORD) {
        Byte v;
        if (++len > maxlen)
            return_NULL;
        v = cast(Byte, lex & LEX_VALUE); // char num encoded into lex
        if (!v && lex < LEX_NUMBER)
            return_NULL;  // invalid char (word but no val)
        i = (i << 4) + v;
        cp++;
    }

    if (len < minlen)
        return_NULL;

    Init_Integer(out, i);
    return cp;
}


//
//  Scan_Hex2: C
//
// Decode a %xx hex encoded sequence into a byte value.
//
// The % should already be removed before calling this.
//
// Returns new position after advancing or NULL.  On success, it always
// consumes two bytes (which are two codepoints).
//
const Byte* Scan_Hex2(Byte* decoded_out, const Byte* bp)
{
    Byte c1 = bp[0];
    if (c1 >= 0x80)
        return NULL;

    Byte c2 = bp[1];
    if (c2 >= 0x80)
        return NULL;

    Byte lex1 = Lex_Map[c1];
    Byte d1 = lex1 & LEX_VALUE;
    if (lex1 < LEX_WORD || (d1 == 0 && lex1 < LEX_NUMBER))
        return NULL;

    Byte lex2 = Lex_Map[c2];
    Byte d2 = lex2 & LEX_VALUE;
    if (lex2 < LEX_WORD || (d2 == 0 && lex2 < LEX_NUMBER))
        return NULL;

    *decoded_out = cast(Codepoint, (d1 << 4) + d2);

    return bp + 2;
}


//
//  Scan_Dec_Buf: C
//
// Validate a decimal number. Return on first invalid char (or end).
// Returns NULL if not valid.
//
// Scan is valid for 1 1.2 1,2 1'234.5 1x 1.2x 1% 1.2% etc.
//
// !!! Is this redundant with Scan_Decimal?  Appears to be similar code.
//
const Byte* Scan_Dec_Buf(
    Byte* out, // may live in data stack (do not call PUSH(), GC, eval)
    bool *is_integral,
    const Byte* cp,
    REBLEN len // max size of buffer
) {
    assert(len >= MAX_NUM_LEN);

    *is_integral = true;

    Byte* bp = out;
    Byte* be = bp + len - 1;

    if (*cp == '+' || *cp == '-')
        *bp++ = *cp++;

    bool digit_present = false;
    while (IS_LEX_NUMBER(*cp) || *cp == '\'') {
        if (*cp != '\'') {
            *bp++ = *cp++;
            if (bp >= be)
                return NULL;
            digit_present = true;
        }
        else
            ++cp;
    }

    if (*cp == ',' || *cp == '.') {
        *is_integral = false;
        cp++;
    }

    *bp++ = '.';
    if (bp >= be)
        return NULL;

    while (IS_LEX_NUMBER(*cp) || *cp == '\'') {
        if (*cp != '\'') {
            *bp++ = *cp++;
            if (bp >= be)
                return NULL;
            digit_present = true;
        }
        else
            ++cp;
    }

    if (not digit_present)
        return NULL;

    if (*cp == 'E' || *cp == 'e') {
        *bp++ = *cp++;
        if (bp >= be)
            return NULL;

        digit_present = false;

        if (*cp == '-' || *cp == '+') {
            *bp++ = *cp++;
            if (bp >= be)
                return NULL;
        }

        while (IS_LEX_NUMBER(*cp)) {
            *bp++ = *cp++;
            if (bp >= be)
                return NULL;
            digit_present = true;
        }

        if (not digit_present)
            return NULL;
    }

    *bp = '\0';
    return cp;
}


//
//  Scan_Decimal: C
//
// Scan and convert a decimal value.  Return zero if error.
//
const Byte* Scan_Decimal(
    Cell(*) out,
    const Byte* cp,
    REBLEN len,
    bool dec_only
){
    Byte buf[MAX_NUM_LEN + 4];
    Byte* ep = buf;
    if (len > MAX_NUM_LEN)
        return_NULL;

    const Byte* bp = cp;

    if (*cp == '+' || *cp == '-')
        *ep++ = *cp++;

    bool digit_present = false;

    while (IS_LEX_NUMBER(*cp) || *cp == '\'') {
        if (*cp != '\'') {
            *ep++ = *cp++;
            digit_present = true;
        }
        else
            ++cp;
    }

    if (*cp == ',' || *cp == '.')
        ++cp;

    *ep++ = '.';

    while (IS_LEX_NUMBER(*cp) || *cp == '\'') {
        if (*cp != '\'') {
            *ep++ = *cp++;
            digit_present = true;
        }
        else
            ++cp;
    }

    if (not digit_present)
        return_NULL;

    if (*cp == 'E' || *cp == 'e') {
        *ep++ = *cp++;
        digit_present = false;

        if (*cp == '-' || *cp == '+')
            *ep++ = *cp++;

        while (IS_LEX_NUMBER(*cp)) {
            *ep++ = *cp++;
            digit_present = true;
        }

        if (not digit_present)
            return_NULL;
    }

    if (*cp == '%') {
        if (dec_only)
            return_NULL;

        ++cp; // ignore it
    }

    *ep = '\0';

    if (cast(REBLEN, cp - bp) != len)
        return_NULL;

    Reset_Unquoted_Header_Untracked(TRACK(out), CELL_MASK_DECIMAL);

    char *se;
    VAL_DECIMAL(out) = strtod(s_cast(buf), &se);

    // !!! TBD: need check for NaN, and INF

    if (fabs(VAL_DECIMAL(out)) == HUGE_VAL)
        fail (Error_Overflow_Raw());

    return cp;
}


//
//  Scan_Integer: C
//
// Scan and convert an integer value.  Return zero if error.
// Allow preceding + - and any combination of ' marks.
//
const Byte* Scan_Integer(
    Cell(*) out,
    const Byte* cp,
    REBLEN len
){
    // Super-fast conversion of zero and one (most common cases):
    if (len == 1) {
        if (*cp == '0') {
            Init_Integer(out, 0);
            return cp + 1;
        }
        if (*cp == '1') {
            Init_Integer(out, 1);
            return cp + 1;
         }
    }

    Byte buf[MAX_NUM_LEN + 4];
    if (len > MAX_NUM_LEN)
        return_NULL; // prevent buffer overflow

    Byte* bp = buf;

    bool neg = false;

    REBINT num = cast(REBINT, len);

    // Strip leading signs:
    if (*cp == '-') {
        *bp++ = *cp++;
        --num;
        neg = true;
    }
    else if (*cp == '+') {
        ++cp;
        --num;
    }

    // Remove leading zeros:
    for (; num > 0; num--) {
        if (*cp == '0' || *cp == '\'')
            ++cp;
        else
            break;
    }

    if (num == 0) { // all zeros or '
        // return early to avoid platform dependant error handling in CHR_TO_INT
        Init_Integer(out, 0);
        return cp;
    }

    // Copy all digits, except ' :
    for (; num > 0; num--) {
        if (*cp >= '0' && *cp <= '9')
            *bp++ = *cp++;
        else if (*cp == '\'')
            ++cp;
        else
            return_NULL;
    }
    *bp = '\0';

    // Too many digits?
    len = bp - &buf[0];
    if (neg)
        --len;
    if (len > 19) {
        // !!! magic number :-( How does it relate to MAX_INT_LEN (also magic)
        return_NULL;
    }

    // Convert, check, and return:
    errno = 0;

    Reset_Unquoted_Header_Untracked(TRACK(out), CELL_MASK_INTEGER);

    VAL_INT64(out) = CHR_TO_INT(buf);
    if (errno != 0)
        return_NULL; // overflow

    if ((VAL_INT64(out) > 0 && neg) || (VAL_INT64(out) < 0 && !neg))
        return_NULL;

    return cp;
}


//
//  Scan_Date: C
//
// Scan and convert a date. Also can include a time and zone.
//
const Byte* Scan_Date(
    Cell(*) out,
    const Byte* cp,
    REBLEN len
) {
    const Byte* end = cp + len;

    // Skip spaces:
    for (; *cp == ' ' && cp != end; cp++);

    // Skip day name, comma, and spaces:
    const Byte* ep;
    for (ep = cp; *ep != ',' && ep != end; ep++);
    if (ep != end) {
        cp = ep + 1;
        while (*cp == ' ' && cp != end) cp++;
    }
    if (cp == end)
        return_NULL;

    REBINT num;

    // Day or 4-digit year:
    ep = Grab_Int(cp, &num);
    if (num < 0)
        return_NULL;

    REBINT day;
    REBINT month;
    REBINT year;
    REBINT tz = NO_DATE_ZONE;
    PAYLOAD(Time, out).nanoseconds = NO_DATE_TIME; // may be overwritten

    REBLEN size = cast(REBLEN, ep - cp);
    if (size >= 4) {
        // year is set in this branch (we know because day is 0)
        // Ex: 2009/04/20/19:00:00+0:00
        year = num;
        day = 0;
    }
    else if (size) {
        // year is not set in this branch (we know because day ISN'T 0)
        // Ex: 12-Dec-2012
        day = num;
        if (day == 0)
            return_NULL;

        // !!! Clang static analyzer doesn't know from test of `day` below
        // how it connects with year being set or not.  Suppress warning.
        year = INT32_MIN; // !!! Garbage, should not be read.
    }
    else
        return_NULL;

    cp = ep;

    // Determine field separator:
    if (*cp != '/' && *cp != '-' && *cp != '.' && *cp != ' ')
        return_NULL;

    Byte sep = *cp++;

    // Month as number or name:
    ep = Grab_Int(cp, &num);
    if (num < 0)
        return_NULL;

    size = cast(REBLEN, ep - cp);

    if (size > 0)
        month = num; // got a number
    else { // must be a word
        for (ep = cp; IS_LEX_WORD(*ep); ep++)
            NOOP; // scan word

        size = cast(REBLEN, ep - cp);
        if (size < 3)
            return_NULL;

        for (num = 0; num != 12; ++num) {
            const Byte* month_name = cb_cast(Month_Names[num]);
            if (0 == Compare_Ascii_Uncased(month_name, cp, size))
                break;
        }
        month = num + 1;
    }

    if (month < 1 || month > 12)
        return_NULL;

    cp = ep;
    if (*cp++ != sep)
        return_NULL;

    // Year or day (if year was first):
    ep = Grab_Int(cp, &num);
    if (*cp == '-' || num < 0)
        return_NULL;

    size = cast(REBLEN, ep - cp);
    if (size == 0)
        return_NULL;

    if (day == 0) {
        // year already set, but day hasn't been
        day = num;
    }
    else {
        // day has been set, but year hasn't been.
        if (size >= 3)
            year = num;
        else {
            // !!! Originally this allowed shorthands, so that 96 = 1996, etc.
            //
            //     if (num >= 70)
            //         year = 1900 + num;
            //     else
            //         year = 2000 + num;
            //
            // It was trickier than that, because it actually used the current
            // year (from the clock) to guess what the short year meant.  That
            // made it so the scanner would scan the same source code
            // differently based on the clock, which is bad.  By allowing
            // short dates to be turned into their short year equivalents, the
            // user code can parse such dates and fix them up after the fact
            // according to their requirements, `if date/year < 100 [...]`
            //
            year = num;
        }
    }

    if (year > MAX_YEAR || day < 1 || day > Month_Max_Days[month-1])
        return_NULL;

    // Check February for leap year or century:
    if (month == 2 && day == 29) {
        if (
            ((year % 4) != 0) ||        // not leap year
            ((year % 100) == 0 &&       // century?
            (year % 400) != 0)
        ){
            return_NULL; // not leap century
        }
    }

    cp = ep;

    if (cp >= end)
        goto end_date;

    if (*cp == '/' || *cp == ' ') {
        sep = *cp++;

        if (cp >= end)
            goto end_date;

        cp = Scan_Time(out, cp, 0); // writes PAYLOAD(Time, out).nanoseconds
        if (
            cp == NULL
            or not IS_TIME(out)
            or VAL_NANO(out) < 0
            or VAL_NANO(out) >= SECS_TO_NANO(24 * 60 * 60)
        ){
            return_NULL;
        }
        assert(PAYLOAD(Time, out).nanoseconds != NO_DATE_TIME);
    }

    // past this point, header is set, so `goto end_date` is legal.

    if (*cp == sep)
        ++cp;

    // Time zone can be 12:30 or 1230 (optional hour indicator)
    if (*cp == '-' || *cp == '+') {
        if (cp >= end)
            goto end_date;

        ep = Grab_Int(cp + 1, &num);
        if (ep - cp == 0)
            return_NULL;

        if (*ep != ':') {
            if (num < -1500 || num > 1500)
                return_NULL;

            int h = (num / 100);
            int m = (num - (h * 100));

            tz = (h * 60 + m) / ZONE_MINS;
        }
        else {
            if (num < -15 || num > 15)
                return_NULL;

            tz = num * (60 / ZONE_MINS);

            if (*ep == ':') {
                ep = Grab_Int(ep + 1, &num);
                if (num % ZONE_MINS != 0)
                    return_NULL;

                tz += num / ZONE_MINS;
            }
        }

        if (ep != end)
            return_NULL;

        if (*cp == '-')
            tz = -tz;

        cp = ep;
    }

  end_date:

    // Overwriting scanned REB_TIME...
    //
    Reset_Unquoted_Header_Untracked(TRACK(out), CELL_MASK_DATE);

    // payload.time.nanoseconds is set, may be NO_DATE_TIME, don't FRESHEN()

    VAL_YEAR(out) = year;
    VAL_MONTH(out) = month;
    VAL_DAY(out) = day;
    VAL_DATE(out).zone = NO_DATE_ZONE;  // Adjust_Date_Zone() requires this

    Adjust_Date_Zone_Core(out, tz);

    VAL_DATE(out).zone = tz;

    return cp;
}


//
//  Scan_File: C
//
// Scan and convert a file name.
//
const Byte* Scan_File(
    Cell(*) out,
    const Byte* cp,
    REBLEN len
){
    if (*cp == '%') {
        cp++;
        len--;
    }

    Codepoint term;
    const Byte* invalid;
    if (*cp == '"') {
        cp++;
        len--;
        term = '"';
        invalid = cb_cast(":;\"");
    }
    else {
        term = '\0';
        invalid = cb_cast(":;()[]\"");
    }

    DECLARE_MOLD (mo);

    cp = Scan_Item_Push_Mold(mo, cp, cp + len, term, invalid);
    if (cp == NULL) {
        Drop_Mold(mo);
        return_NULL;
    }

    Init_File(out, Pop_Molded_String(mo));
    return cp;
}


//
//  Scan_Email: C
//
// Scan and convert email.
//
const Byte* Scan_Email(
    Cell(*) out,
    const Byte* cp,
    REBLEN len
){
    String(*) s = Make_String(len * 2);  // !!! guess...use mold buffer instead?
    Utf8(*) up = STR_HEAD(s);

    REBLEN num_chars = 0;

    bool found_at = false;
    for (; len > 0; len--) {
        if (*cp == '@') {
            if (found_at)
                return_NULL;
            found_at = true;
        }

        if (*cp == '%') {
            if (len <= 2)
                return_NULL;

            Byte decoded;
            cp = Scan_Hex2(&decoded, cp + 1);
            if (cp == NULL)
                return_NULL;

            up = WRITE_CHR(up, decoded);
            ++num_chars;
            len -= 2;
        }
        else {
            up = WRITE_CHR(up, *cp++);
            ++num_chars;
        }
    }

    if (not found_at)
        return_NULL;

    TERM_STR_LEN_SIZE(s, num_chars, up - STR_HEAD(s));

    Init_Email(out, s);
    return cp;
}


//
//  Scan_URL: C
//
// While Rebol2, R3-Alpha, and Red attempted to apply some amount of decoding
// (e.g. how %20 is "space" in http:// URL!s), Ren-C leaves URLs "as-is".
// This means a URL may be copied from a web browser bar and pasted back.
// It also means that the URL may be used with custom schemes (odbc://...)
// that have different ideas of the meaning of characters like `%`.
//
// !!! The current concept is that URL!s typically represent the *decoded*
// forms, and thus express unicode codepoints normally...preserving either of:
//
//     https://duckduckgo.com/?q=hergé+&+tintin
//     https://duckduckgo.com/?q=hergé+%26+tintin
//
// Then, the encoded forms with UTF-8 bytes expressed in %XX form would be
// converted as TEXT!, where their datatype suggests the encodedness:
//
//     {https://duckduckgo.com/?q=herg%C3%A9+%26+tintin}
//
// (This is similar to how local FILE!s, where e.g. slashes become backslash
// on Windows, are expressed as TEXT!.)
//
const Byte* Scan_URL(
    Cell(*) out,
    const Byte* cp,
    REBLEN len
){
    return Scan_Any(out, cp, len, REB_URL, STRMODE_NO_CR);
}


//
//  Scan_Pair: C
//
// Scan and convert a pair
//
const Byte* Scan_Pair(
    Cell(*) out,
    const Byte* cp,
    REBLEN len
) {
    Byte buf[MAX_NUM_LEN + 4];

    bool is_integral;
    const Byte* ep = Scan_Dec_Buf(&buf[0], &is_integral, cp, MAX_NUM_LEN);
    if (ep == NULL)
        return_NULL;
    if (*ep != 'x' && *ep != 'X')
        return_NULL;

    REBVAL *paired = Alloc_Pairing();

    // X is in the key pairing cell
    if (is_integral)
        Init_Integer(PAIRING_KEY(paired), atoi(cast(char*, &buf[0])));
    else
        Init_Decimal(PAIRING_KEY(paired), atof(cast(char*, &buf[0])));

    ep++;

    const Byte* xp = Scan_Dec_Buf(&buf[0], &is_integral, ep, MAX_NUM_LEN);
    if (!xp) {
        Free_Pairing(paired);
        return_NULL;
    }

    // Y is in the non-key pairing cell
    if (is_integral)
        Init_Integer(paired, atoi(cast(char*, &buf[0])));
    else
        Init_Decimal(paired, atof(cast(char*, &buf[0])));

    if (len > cast(REBLEN, xp - cp)) {
        Free_Pairing(paired);
        return_NULL;
    }

    Manage_Pairing(paired);

    Reset_Unquoted_Header_Untracked(TRACK(out), CELL_MASK_PAIR);
    INIT_VAL_PAIR(out, paired);
    return xp;
}


//
//  Scan_Binary: C
//
// Scan and convert binary strings.
//
const Byte* Scan_Binary(
    Cell(*) out,
    const Byte* cp,
    REBLEN len
) {
    REBINT base = 16;

    if (*cp != '#') {
        const Byte* ep = Grab_Int(cp, &base);
        if (cp == ep || *ep != '#')
            return_NULL;
        len -= cast(REBLEN, ep - cp);
        cp = ep;
    }

    cp++;  // skip #
    if (*cp++ != '{')
        return_NULL;

    len -= 2;

    cp = Decode_Binary(out, cp, len, base, '}');
    if (cp == NULL)
        return_NULL;

    cp = Skip_To_Byte(cp, cp + len, '}');
    if (cp == NULL)
        return_NULL; // series will be gc'd

    return cp + 1; // include the "}" in the scan total
}


//
//  Scan_Any: C
//
// Scan any string that does not require special decoding.
//
const Byte* Scan_Any(
    Cell(*) out,
    const Byte* cp,
    REBLEN num_bytes,
    enum Reb_Kind type,
    enum Reb_Strmode strmode
){
    // The range for a curly braced string may span multiple lines, and some
    // files may have CR and LF in the data:
    //
    //     {line one  ; imagine this line has CR LF...not just LF
    //     line two}
    //
    // Despite the presence of the CR in the source file, the scanned literal
    // should only support LF (if it supports files with it at all)
    //
    // http://blog.hostilefork.com/death-to-carriage-return/
    //
    // So at time of writing it is always STRMODE_NO_CR, but the option is
    // being left open to make the scanner flexible in this respect...to
    // either convert CR LF sequences to just LF, or to preserve the CR.
    //
    String(*) s = Append_UTF8_May_Fail(
        nullptr,
        cs_cast(cp),
        num_bytes,
        strmode
    );
    Init_Any_String(out, type, s);

    return cp + num_bytes;
}


//
//  scan-net-header: native [
//      {Scan an Internet-style header (HTTP, SMTP)}
//
//      return: [block!]
//      header "Fields with duplicate words will be merged into a block"
//          [binary!]
//  ]
//
DECLARE_NATIVE(scan_net_header)
//
// !!! This routine used to be a feature of CONSTRUCT in R3-Alpha, and was
// used by %prot-http.r.  The idea was that instead of providing a parent
// object, a STRING! or BINARY! could be provided which would be turned
// into a block by this routine.
//
// It doesn't make much sense to have this coded in C rather than using PARSE
// It's only being converted into a native to avoid introducing bugs by
// rewriting it as Rebol in the middle of other changes.
{
    INCLUDE_PARAMS_OF_SCAN_NET_HEADER;

    Array(*) result = Make_Array(10); // Just a guess at size (use STD_BUF?)

    REBVAL *header = ARG(header);
    Size size;
    const Byte* cp = VAL_BYTES_AT(&size, header);
    UNUSED(size);  // !!! Review semantics

    while (IS_LEX_ANY_SPACE(*cp)) cp++; // skip white space

    const Byte* start;
    REBINT len;

    while (true) {
        // Scan valid word:
        if (IS_LEX_WORD(*cp)) {
            start = cp;
            while (
                IS_LEX_WORD_OR_NUMBER(*cp)
                || *cp == '.'
                || *cp == '-'
                || *cp == '_'
            ) {
                cp++;
            }
        }
        else break;

        if (*cp != ':')
            break;

        Cell(*) val = nullptr;  // suppress maybe uninitialized warning

        Symbol(const*) name = Intern_UTF8_Managed(start, cp - start);

        cp++;
        // Search if word already present:

        Cell(const*) item_tail = ARR_TAIL(result);
        Cell(*) item = ARR_HEAD(result);

        for (; item != item_tail; item += 2) {
            assert(IS_TEXT(item + 1) || IS_BLOCK(item + 1));
            if (Are_Synonyms(VAL_WORD_SYMBOL(item), name)) {
                // Does it already use a block?
                if (IS_BLOCK(item + 1)) {
                    // Block of values already exists:
                    val = Alloc_Tail_Array(VAL_ARRAY_ENSURE_MUTABLE(item + 1));
                }
                else {
                    // Create new block for values:
                    Array(*) a = Make_Array(2);
                    Derelativize(
                        Alloc_Tail_Array(a),
                        item + 1, // prior value
                        SPECIFIED // no relative values added
                    );
                    val = Alloc_Tail_Array(a);
                    Init_Block(item + 1, a);
                }
                break;
            }
        }

        if (item == item_tail) {  // didn't break, add space for new word/value
            Init_Set_Word(Alloc_Tail_Array(result), name);
            val = Alloc_Tail_Array(result);
        }

        while (IS_LEX_SPACE(*cp)) cp++;
        start = cp;
        len = 0;
        while (!ANY_CR_LF_END(*cp)) {
            len++;
            cp++;
        }
        // Is it continued on next line?
        while (*cp) {
            if (*cp == CR)
                ++cp;
            if (*cp == LF)
                ++cp;
            if (not IS_LEX_SPACE(*cp))
                break;
            while (IS_LEX_SPACE(*cp))
                ++cp;
            while (!ANY_CR_LF_END(*cp)) {
                ++len;
                ++cp;
            }
        }

        // Create string value (ignoring lines and indents):
        //
        // !!! This is written to deal with unicode lengths in terms of *size*
        // in bytes, not *length* in characters.  If it were to be done
        // correctly, it would need to use NEXT_CHR to count the characters
        // in the loop above.  Better to convert to usermode.

        String(*) string = Make_String(len * 2);
        Utf8(*) str = STR_HEAD(string);
        cp = start;

        // "Code below *MUST* mirror that above:"

        while (!ANY_CR_LF_END(*cp))
            str = WRITE_CHR(str, *cp++);
        while (*cp != '\0') {
            if (*cp == CR)
                ++cp;
            if (*cp == LF)
                ++cp;
            if (not IS_LEX_SPACE(*cp))
                break;
            while (IS_LEX_SPACE(*cp))
                ++cp;
            while (!ANY_CR_LF_END(*cp))
                str = WRITE_CHR(str, *cp++);
        }
        TERM_STR_LEN_SIZE(string, len, str - STR_HEAD(string));
        Init_Text(val, string);
    }

    return Init_Block(OUT, result);
}
