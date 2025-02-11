//
//  File: %t-integer.c
//  Summary: "integer datatype"
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

#include "sys-int-funcs.h"

#include "datatypes/sys-money.h"

//
//  CT_Integer: C
//
REBINT CT_Integer(noquote(Cell(const*)) a, noquote(Cell(const*)) b, bool strict)
{
    UNUSED(strict);  // no lax form of comparison

    if (VAL_INT64(a) == VAL_INT64(b))
        return 0;
    return (VAL_INT64(a) > VAL_INT64(b)) ? 1 : -1;
}


//
//  MAKE_Integer: C
//
Bounce MAKE_Integer(
    Frame(*) frame_,
    enum Reb_Kind kind,
    option(const REBVAL*) parent,
    const REBVAL *arg
){
    assert(kind == REB_INTEGER);
    if (parent)
        return RAISE(Error_Bad_Make_Parent(kind, unwrap(parent)));

    if (IS_LOGIC(arg)) {
        //
        // !!! Due to Rebol's policies on conditional truth and falsehood,
        // it refuses to say TO FALSE is 0.  MAKE has shades of meaning
        // that are more "dialected", e.g. MAKE BLOCK! 10 creates a block
        // with capacity 10 and not literally `[10]` (or a block with ten
        // BLANK! values in it).  Under that liberal umbrella it decides
        // that it will make an integer 0 out of FALSE due to it having
        // fewer seeming "rules" than TO would.

        if (VAL_LOGIC(arg))
            Init_Integer(OUT, 1);
        else
            Init_Integer(OUT, 0);

        // !!! The same principle could suggest MAKE is not bound by
        // the "reversibility" requirement and hence could interpret
        // binaries unsigned by default.  Before getting things any
        // weirder should probably leave it as is.
    }
    else {
        Context(*) error = Maybe_Value_To_Int64(OUT, arg, false);
        if (error)
            return RAISE(error);
    }

    return OUT;
}


//
//  TO_Integer: C
//
Bounce TO_Integer(Frame(*) frame_, enum Reb_Kind kind, const REBVAL *arg)
{
    assert(kind == REB_INTEGER);
    UNUSED(kind);

    if (IS_ISSUE(arg))
        return RAISE(
            "Use CODEPOINT OF for INTEGER! from single-character ISSUE!"
        );

    Context(*) error = Maybe_Value_To_Int64(OUT, arg, false);
    if (error)
        return RAISE(error);

    return OUT;
}


// Like converting a binary, except uses a string of ASCII characters.  Does
// not allow for signed interpretations, e.g. #FFFF => 65535, not -1.
// Unsigned makes more sense as these would be hexes likely typed in by users,
// who rarely do 2s-complement math in their head.
//
void Hex_String_To_Integer(REBVAL *out, const REBVAL *value)
{
    Size utf8_size;
    Utf8(const*) bp = VAL_UTF8_SIZE_AT(&utf8_size, value);

    if (utf8_size > MAX_HEX_LEN) {
        // Lacks BINARY!'s accommodation of leading 00s or FFs
        fail (Error_Out_Of_Range_Raw(value));
    }

    if (not Scan_Hex(out, bp, utf8_size, utf8_size))
        fail (Error_Bad_Make(REB_INTEGER, value));

    // !!! Unlike binary, always assumes unsigned (should it?).  Yet still
    // might run afoul of 64-bit range limit.
    //
    if (VAL_INT64(out) < 0)
        fail (Error_Out_Of_Range_Raw(value));
}


//
//  Value_To_Int64: C
//
// Interpret `value` as a 64-bit integer and return it in `out`.
//
// If `no_sign` is true then use that to inform an ambiguous conversion
// (e.g. #{FF} is 255 instead of -1).  However, it won't contradict the sign
// of unambiguous source.  So the string "-1" will raise an error if you try
// to convert it unsigned.  (For this, use `abs to-integer "-1"`.)
//
// Because Rebol's INTEGER! uses a signed REBI64 and not an unsigned
// REBU64, a request for unsigned interpretation is limited to using
// 63 of those bits.  A range error will be thrown otherwise.
//
// If a type is added or removed, update DECLARE_NATIVE(to_integer)'s spec
//
Context(*) Maybe_Value_To_Int64(REBVAL *out, const REBVAL *value, bool no_sign)
{
    // !!! Code extracted from REBTYPE(Integer)'s A_MAKE and A_TO cases
    // Use SWITCH instead of IF chain? (was written w/ANY_STR test)

    if (IS_INTEGER(value)) {
        Copy_Cell(out, value);
        goto check_sign;
    }
    if (IS_DECIMAL(value) || IS_PERCENT(value)) {
        if (VAL_DECIMAL(value) < MIN_D64 || VAL_DECIMAL(value) >= MAX_D64)
            return Error_Overflow_Raw();

        Init_Integer(out, cast(REBI64, VAL_DECIMAL(value)));
        goto check_sign;
    }
    else if (IS_MONEY(value)) {
        Init_Integer(out, deci_to_int(VAL_MONEY_AMOUNT(value)));
        goto check_sign;
    }
    else if (IS_BINARY(value)) { // must be before ANY_STRING() test...

        // !!! While historical Rebol TO INTEGER! of BINARY! would interpret
        // the bytes as a big-endian form of their internal representations,
        // wanting to futureproof for BigNum integers has changed Ren-C's
        // point of view...delegating that highly parameterized conversion
        // to operations currently called ENBIN and DEBIN.
        //
        // https://forum.rebol.info/t/1270
        //
        // This is a stopgap while ENBIN and DEBIN are hammered out which
        // preserves the old behavior in the TO INTEGER! case.
        //
        Size n;
        const Byte* bp = VAL_BINARY_SIZE_AT(&n, value);
        if (n == 0) {
            Init_Integer(out, 0);
            return nullptr;
        }
        REBVAL *sign = (*bp >= 0x80)
            ? rebValue("'+/-")
            : rebValue("'+");

        REBVAL *result = rebValue("debin [be", rebR(sign), "]", value);

        Copy_Cell(out, result);
        rebRelease(result);
        return nullptr;
    }
    else if (IS_ISSUE(value) or ANY_STRING(value)) {
        Size size;
        const Length max_len = VAL_LEN_AT(value);  // e.g. "no maximum"
        const Byte* bp = Analyze_String_For_Scan(&size, value, max_len);
        if (
            memchr(bp, '.', size)
            || memchr(bp, 'e', size)
            || memchr(bp, 'E', size)
        ){
            DECLARE_LOCAL (d);
            if (Scan_Decimal(d, bp, size, true)) {
                if (
                    VAL_DECIMAL(d) < INT64_MAX
                    && VAL_DECIMAL(d) >= INT64_MIN
                ){
                    Init_Integer(out, cast(REBI64, VAL_DECIMAL(d)));
                    goto check_sign;
                }

                return Error_Overflow_Raw();
            }
        }
        if (Scan_Integer(out, bp, size))
            goto check_sign;

        return Error_Bad_Make(REB_INTEGER, value);
    }
    else if (IS_LOGIC(value)) {
        //
        // Rebol's choice is that no integer is uniquely representative of
        // "falsehood" condition, e.g. `if 0 [print "this prints"]`.  So to
        // say TO LOGIC! 0 is FALSE would be disingenuous.
        //
        return Error_Bad_Make(REB_INTEGER, value);
    }
    else if (IS_TIME(value)) {
        Init_Integer(out, SECS_FROM_NANO(VAL_NANO(value))); // always unsigned
        return nullptr;
    }
    else
        return Error_Bad_Make(REB_INTEGER, value);

check_sign:

    if (no_sign && VAL_INT64(out) < 0)
        return Error_Positive_Raw();

    return nullptr;
}


//
//  MF_Integer: C
//
void MF_Integer(REB_MOLD *mo, noquote(Cell(const*)) v, bool form)
{
    UNUSED(form);

    Byte buf[60];
    REBINT len = Emit_Integer(buf, VAL_INT64(v));
    Append_Ascii_Len(mo->series, s_cast(buf), len);
}


//
//  REBTYPE: C
//
REBTYPE(Integer)
{
    REBVAL *val = D_ARG(1);
    REBI64 num = VAL_INT64(val);

    REBI64 arg;

    option(SymId) id = ID_OF_SYMBOL(verb);

    // !!! This used to rely on IS_BINARY_ACT, which is no longer available
    // in the symbol based dispatch.  Consider doing another way.
    //
    if (
        id == SYM_ADD
        or id == SYM_SUBTRACT
        or id == SYM_MULTIPLY
        or id == SYM_DIVIDE
        or id == SYM_POWER
        or id == SYM_BITWISE_AND
        or id == SYM_BITWISE_OR
        or id == SYM_BITWISE_XOR
        or id == SYM_BITWISE_AND_NOT
        or id == SYM_REMAINDER
    ){
        REBVAL *val2 = D_ARG(2);

        if (IS_INTEGER(val2))
            arg = VAL_INT64(val2);
        else if (IS_CHAR(val2))
            arg = VAL_CHAR(val2);
        else {
            // Decimal or other numeric second argument:
            REBLEN n = 0; // use to flag special case
            switch (id) {
            // Anything added to an integer is same as adding the integer:
            case SYM_ADD:
            case SYM_MULTIPLY: {
                // Swap parameter order:
                Move_Cell(OUT, val2);  // Use as temp workspace
                Move_Cell(val2, val);
                Move_Cell(val, OUT);
                return Run_Generic_Dispatch_Core(val, frame_, verb); }

            // Only type valid to subtract from, divide into, is decimal/money:
            case SYM_SUBTRACT:
                n = 1;
                /* fall through */
            case SYM_DIVIDE:
            case SYM_REMAINDER:
            case SYM_POWER:
                if (IS_DECIMAL(val2) || IS_PERCENT(val2)) {
                    Init_Decimal(val, cast(REBDEC, num));  // convert
                    return T_Decimal(frame_, verb);
                }
                if (IS_MONEY(val2)) {
                    Init_Money(val, int_to_deci(VAL_INT64(val)));
                    return T_Money(frame_, verb);
                }
                if (n > 0) {
                    if (IS_TIME(val2)) {
                        Init_Time_Nanoseconds(val, SEC_TIME(VAL_INT64(val)));
                        return T_Time(frame_, verb);
                    }
                    if (IS_DATE(val2))
                        return T_Date(frame_, verb);
                }

            default:
                break;
            }
            fail (Error_Math_Args(REB_INTEGER, verb));
        }
    }
    else
        arg = 0xDECAFBAD; // wasteful, but avoid maybe unassigned warning

    switch (id) {

    case SYM_COPY:
        Copy_Cell(OUT, val);
        return OUT;

    case SYM_ADD: {
        REBI64 anum;
        if (REB_I64_ADD_OF(num, arg, &anum))
            return RAISE(Error_Overflow_Raw());
        return Init_Integer(OUT, anum); }

    case SYM_SUBTRACT: {
        REBI64 anum;
        if (REB_I64_SUB_OF(num, arg, &anum))
            return RAISE(Error_Overflow_Raw());
        return Init_Integer(OUT, anum); }

    case SYM_MULTIPLY: {
        REBI64 p;
        if (REB_I64_MUL_OF(num, arg, &p))
            return RAISE(Error_Overflow_Raw());
        return Init_Integer(OUT, p); }

    case SYM_DIVIDE:
        if (arg == 0)
            return RAISE(Error_Zero_Divide_Raw());
        if (num == INT64_MIN && arg == -1)
            return RAISE(Error_Overflow_Raw());
        if (num % arg == 0)
            return Init_Integer(OUT, num / arg);
        // Fall thru
    case SYM_POWER:
        Init_Decimal(D_ARG(1), cast(REBDEC, num));
        Init_Decimal(D_ARG(2), cast(REBDEC, arg));
        return T_Decimal(frame_, verb);

    case SYM_REMAINDER:
        if (arg == 0)
            return RAISE(Error_Zero_Divide_Raw());
        return Init_Integer(OUT, (arg != -1) ? (num % arg) : 0);

    case SYM_BITWISE_AND:
        return Init_Integer(OUT, num & arg);

    case SYM_BITWISE_OR:
        return Init_Integer(OUT, num | arg);

    case SYM_BITWISE_XOR:
        return Init_Integer(OUT, num ^ arg);

    case SYM_BITWISE_AND_NOT:
        return Init_Integer(OUT, num & ~arg);

    case SYM_NEGATE:
        if (num == INT64_MIN)
            return RAISE(Error_Overflow_Raw());
        return Init_Integer(OUT, -num);

    case SYM_BITWISE_NOT:
        return Init_Integer(OUT, ~num);

    case SYM_ABSOLUTE:
        if (num == INT64_MIN)
            return RAISE(Error_Overflow_Raw());
        return Init_Integer(OUT, num < 0 ? -num : num);

    case SYM_EVEN_Q:
        num = ~num;
        // falls through
    case SYM_ODD_Q:
        if (num & 1)
            return Init_True(OUT);
        return Init_False(OUT);

    case SYM_ROUND: {
        INCLUDE_PARAMS_OF_ROUND;
        USED(ARG(value));  // extracted as d1, others are passed via frame_
        USED(ARG(even)); USED(ARG(down)); USED(ARG(half_down));
        USED(ARG(floor)); USED(ARG(ceiling)); USED(ARG(half_ceiling));

        if (not REF(to))
            return Init_Integer(OUT, Round_Int(num, frame_, 0L));

        REBVAL *to = ARG(to);

        if (IS_MONEY(to))
            return Init_Money(
                OUT,
                Round_Deci(
                    int_to_deci(num), frame_, VAL_MONEY_AMOUNT(to)
                )
            );

        if (IS_DECIMAL(to) || IS_PERCENT(to)) {
            REBDEC dec = Round_Dec(
                cast(REBDEC, num), frame_, VAL_DECIMAL(to)
            );
            Reset_Unquoted_Header_Untracked(
                TRACK(OUT),
                FLAG_HEART_BYTE(VAL_TYPE(to)) | CELL_MASK_NO_NODES
            );
            VAL_DECIMAL(OUT) = dec;
            return OUT;
        }

        if (IS_TIME(ARG(to)))
            fail (PARAM(to));

        return Init_Integer(OUT, Round_Int(num, frame_, VAL_INT64(to))); }

    case SYM_RANDOM: {
        INCLUDE_PARAMS_OF_RANDOM;

        UNUSED(PARAM(value));

        if (REF(only))
            fail (Error_Bad_Refines_Raw());

        if (REF(seed)) {
            Set_Random(num);
            return nullptr;
        }
        if (num == 0)
            fail (ARG(value));
        return Init_Integer(OUT, Random_Range(num, REF(secure))); }

    default:
        break;
    }

    fail (UNHANDLED);
}
