//
//  File: %n-io.c
//  Summary: "native functions for input and output"
//  Section: natives
//  Project: "Rebol 3 Interpreter and Run-time (Ren-C branch)"
//  Homepage: https://github.com/metaeducation/ren-c/
//
//=////////////////////////////////////////////////////////////////////////=//
//
// Copyright 2012 REBOL Technologies
// Copyright 2012-2017 Rebol Open Source Contributors
// REBOL is a trademark of REBOL Technologies
//
// See README.md and CREDITS.md for more information.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
//=////////////////////////////////////////////////////////////////////////=//
//

#include "sys-core.h"


//
//  form: native [
//
//  "Converts a value to a human-readable string."
//
//      value [<opt> any-value!]
//          "The value to form"
//  ]
//
REBNATIVE(form)
{
    INCLUDE_PARAMS_OF_FORM;

    return Init_Text(D_OUT, Copy_Form_Value(ARG(value), 0));
}


//
//  mold: native [
//
//  "Converts a value to a REBOL-readable string."
//
//      value "The value to mold"
//          [any-value!]
//      /only "For a block value, mold only its contents, no outer []"
//      /all "Use construction syntax"
//      /flat "No indentation"
//      /limit "Limit to a certain length"
//          [integer!]
//  ]
//
REBNATIVE(mold)
{
    INCLUDE_PARAMS_OF_MOLD;

    DECLARE_MOLD (mo);
    if (REF(all))
        SET_MOLD_FLAG(mo, MOLD_FLAG_ALL);
    if (REF(flat))
        SET_MOLD_FLAG(mo, MOLD_FLAG_INDENT);
    if (REF(limit)) {
        SET_MOLD_FLAG(mo, MOLD_FLAG_LIMIT);
        mo->limit = Int32(ARG(limit));
    }

    Push_Mold(mo);

    if (REF(only) and IS_BLOCK(ARG(value)))
        SET_MOLD_FLAG(mo, MOLD_FLAG_ONLY);

    Mold_Value(mo, ARG(value));

    return Init_Text(D_OUT, Pop_Molded_String(mo));
}


//
//  write-stdout: native [
//
//  "Boot-only implementation of WRITE-STDOUT (HIJACK'd by STDIO module)"
//
//      return: [<opt> void!]
//      value [<blank> text! char! binary!]
//          "Text to write, if a STRING! or CHAR! is converted to OS format"
//  ]
//
REBNATIVE(write_stdout)
//
// This code isn't supposed to run during normal bootup.  But for debugging
// we don't want a parallel set of PRINT operations and specializations just
// on the off chance something goes wrong in boot.  So this stub is present
// to do debug I/O.
{
    INCLUDE_PARAMS_OF_WRITE_STDOUT;

    REBVAL *v = ARG(value);

  #if defined(NDEBUG)
    UNUSED(v);
    fail ("Boot cannot print output in release build, must load I/O module");
  #else
    if (IS_BINARY(v)) {
        PROBE(v);
    }
    else if (IS_TEXT(v)) {
        printf("%s", STR_HEAD(VAL_STRING(v)));
        fflush(stdout);
    }
    else {
        assert(IS_CHAR(v));
        printf("%s", VAL_CHAR_ENCODED(v));
    }
    return Init_Void(D_OUT);
  #endif
}


//
//  new-line: native [
//
//  {Sets or clears the new-line marker within a block or group.}
//
//      position "Position to change marker (modified)"
//          [block! group!]
//      mark "Set TRUE for newline"
//          [logic!]
//      /all "Set/clear marker to end of series"
//      /skip "Set/clear marker periodically to the end of the series"
//          [integer!]
//  ]
//
REBNATIVE(new_line)
{
    INCLUDE_PARAMS_OF_NEW_LINE;

    bool mark = VAL_LOGIC(ARG(mark));

    REBVAL *pos = ARG(position);
    FAIL_IF_READ_ONLY(pos);

    RELVAL *item = VAL_ARRAY_AT(pos);

    if (IS_END(item)) { // no value at tail to mark; use bit in array
        if (mark)
            SET_ARRAY_FLAG(VAL_ARRAY(pos), NEWLINE_AT_TAIL);
        else
            CLEAR_ARRAY_FLAG(VAL_ARRAY(pos), NEWLINE_AT_TAIL);
        RETURN (pos);
    }

    REBINT skip;
    if (REF(all))
        skip = 1;
    else if (REF(skip)) {
        skip = Int32s(ARG(skip), 1);
        if (skip < 1)
            skip = 1;
    }
    else
        skip = 0;

    REBCNT n;
    for (n = 0; NOT_END(item); ++n, ++item) {
        if (skip != 0 and (n % skip != 0))
            continue;

        if (mark)
            SET_CELL_FLAG(item, NEWLINE_BEFORE);
        else
            CLEAR_CELL_FLAG(item, NEWLINE_BEFORE);

        if (skip == 0)
            break;
    }

    RETURN (pos);
}


//
//  new-line?: native [
//
//  {Returns the state of the new-line marker within a block or group.}
//
//      position [block! group! varargs!] "Position to check marker"
//  ]
//
REBNATIVE(new_line_q)
{
    INCLUDE_PARAMS_OF_NEW_LINE_Q;

    REBVAL *pos = ARG(position);

    REBARR *arr;
    const RELVAL *item;

    if (IS_VARARGS(pos)) {
        REBFRM *f;
        REBVAL *shared;
        if (Is_Frame_Style_Varargs_May_Fail(&f, pos)) {
            if (not f->feed->array) {
                //
                // C va_args input to frame, as from the API, but not in the
                // process of using string components which *might* have
                // newlines.  Review edge cases, like:
                //
                //    REBVAL *new_line_q = rebValue(":new-line?");
                //    bool case_one = rebDid("new-line?", "[\n]");
                //    bool case_two = rebDid(new_line_q, "[\n]");
                //
                assert(f->feed->index == TRASHED_INDEX);
                return Init_Logic(D_OUT, false);
            }

            arr = f->feed->array;
            item = f->feed->value;
        }
        else if (Is_Block_Style_Varargs(&shared, pos)) {
            arr = VAL_ARRAY(shared);
            item = VAL_ARRAY_AT(shared);
        }
        else
            panic ("Bad VARARGS!");
    }
    else {
        assert(IS_GROUP(pos) or IS_BLOCK(pos));
        arr = VAL_ARRAY(pos);
        item = VAL_ARRAY_AT(pos);
    }

    if (NOT_END(item))
        return Init_Logic(D_OUT, GET_CELL_FLAG(item, NEWLINE_BEFORE));

    return Init_Logic(D_OUT, GET_ARRAY_FLAG(arr, NEWLINE_AT_TAIL));
}


//
//  Milliseconds_From_Value: C
//
// Note that this routine is used by the SLEEP extension, as well as by WAIT.
//
REBCNT Milliseconds_From_Value(const RELVAL *v) {
    REBINT msec;

    switch (VAL_TYPE(v)) {
    case REB_INTEGER:
        msec = 1000 * Int32(v);
        break;

    case REB_DECIMAL:
        msec = cast(REBINT, 1000 * VAL_DECIMAL(v));
        break;

    case REB_TIME:
        msec = cast(REBINT, VAL_NANO(v) / (SEC_SEC / 1000));
        break;

    default:
        panic (NULL); // avoid uninitialized msec warning
    }

    if (msec < 0)
        fail (Error_Out_Of_Range(KNOWN(v)));

    return cast(REBCNT, msec);
}


//
//  local-to-file: native [
//
//  {Converts a local system file path TEXT! to a Rebol FILE! path.}
//
//      return: [<opt> file!]
//          {The returned value should be a valid natural FILE! literal}
//      path [<blank> text! file!]
//          {Path to convert (by default, only TEXT! for type safety)}
//      /pass
//          {Convert TEXT!, but pass thru FILE!, assuming it's canonized}
//      /dir
//          {Ensure input path is treated as a directory}
//  ]
//
REBNATIVE(local_to_file)
{
    INCLUDE_PARAMS_OF_LOCAL_TO_FILE;

    REBVAL *path = ARG(path);
    if (IS_FILE(path)) {
        if (not REF(pass))
            fail ("LOCAL-TO-FILE only passes through FILE! if /PASS used");

        return Init_File(D_OUT, Copy_String_At(path));  // many callers modify
    }

    return Init_File(
        D_OUT,
        To_REBOL_Path(path, REF(dir) ? PATH_OPT_SRC_IS_DIR : 0)
    );
}


//
//  file-to-local: native [
//
//  {Converts a Rebol FILE! path to TEXT! of the local system file path}
//
//      return: [<opt> text!]
//          {A TEXT! like "\foo\bar" is not a "natural" FILE! %\foo\bar}
//      path [<blank> file! text!]
//          {Path to convert (by default, only FILE! for type safety)}
//      /pass
//          {Convert FILE!s, but pass thru TEXT!, assuming it's local}
//      /full
//          {For relative paths, prepends current dir for full path}
//      /no-tail-slash
//          {For directories, do not add a slash or backslash to the tail}
//      /wild
//          {For directories, add a * to the end}
//  ]
//
REBNATIVE(file_to_local)
{
    INCLUDE_PARAMS_OF_FILE_TO_LOCAL;

    REBVAL *path = ARG(path);
    if (IS_TEXT(path)) {
        if (not REF(pass))
            fail ("FILE-TO-LOCAL only passes through STRING! if /PASS used");

        return Init_Text(D_OUT, Copy_String_At(path));  // callers modify
    }

    return Init_Text(
        D_OUT,
        To_Local_Path(
            path,
            REB_FILETOLOCAL_0
                | (REF(full) ? REB_FILETOLOCAL_FULL : 0)
                | (REF(no_tail_slash) ? REB_FILETOLOCAL_NO_TAIL_SLASH : 0)
                | (REF(wild) ? REB_FILETOLOCAL_WILD : 0)
        )
    );
}


//
//  what-dir: native [
//
//  {Returns the current directory path}
//
//  ]
//
REBNATIVE(what_dir)
{
    INCLUDE_PARAMS_OF_WHAT_DIR;

    REBVAL *current_path = Get_System(SYS_OPTIONS, OPTIONS_CURRENT_PATH);

    if (IS_FILE(current_path) || IS_BLANK(current_path)) {
        //
        // !!! Because of the need to track a notion of "current path" which
        // could be a URL! as well as a FILE!, the state is stored in the
        // system options.  For now--however--it is "duplicate" in the case
        // of a FILE!, because the OS has its own tracked state.  We let the
        // OS state win for files if they have diverged somehow--because the
        // code was already here and it would be more compatible.  But
        // reconsider the duplication.

        REBVAL *refresh = OS_GET_CURRENT_DIR();
        Move_Value(current_path, refresh);
        rebRelease(refresh);
    }
    else if (not IS_URL(current_path)) {
        //
        // Lousy error, but ATM the user can directly edit system/options.
        // They shouldn't be able to (or if they can, it should be validated)
        //
        fail (current_path);
    }

    return rebValue("copy", current_path, rebEND);  // caller mutates, copy
}


//
//  change-dir: native [
//
//  {Changes the current path (where scripts with relative paths will be run).}
//
//      path [file! url!]
//  ]
//
REBNATIVE(change_dir)
{
    INCLUDE_PARAMS_OF_CHANGE_DIR;

    REBVAL *arg = ARG(path);
    REBVAL *current_path = Get_System(SYS_OPTIONS, OPTIONS_CURRENT_PATH);

    if (IS_URL(arg)) {
        // There is no directory listing protocol for HTTP (although this
        // needs to be methodized to work for SFTP etc.)  So this takes
        // your word for it for the moment that it's a valid "directory".
        //
        // !!! Should it at least check for a trailing `/`?
    }
    else {
        assert(IS_FILE(arg));

        Check_Security(Canon(SYM_FILE), POL_EXEC, arg);

        bool success = OS_SET_CURRENT_DIR(arg);

        if (not success)
            fail (PAR(path));
    }

    Move_Value(current_path, arg);

    RETURN (ARG(path));
}
