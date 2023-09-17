//
//  File: %reb-config.h
//  Summary: "General build configuration"
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
// This is the first file included by %sys-core.h.
//
// Many of the flags controlling the build (such as
// the TO_<target> definitions) come from -DTO_<target> in the
// compiler command-line.  These command lines are generally
// produced automatically, based on the build that is picked
// from %systems.r.
//
// However, some flags require the preprocessor's help to
// decide if they are relevant, for instance if they involve
// detecting features of the compiler while it's running.
// Or they may adjust a feature so narrowly that putting it
// into the system configuration would seem unnecessary.
//
// Over time, this file should be balanced and adjusted with
// %systems.r in order to make the most convenient and clear
// build process.  If there is difficulty in making a build
// work on a system, use that as an opportunity to reflect
// how to make this better.
//
//=//// NOTES //////////////////////////////////////////////////////////////=//
//
// * This favors #if with defined values of 0 and 1, instead of #ifdef, because
//   over the long run it makes typos easier to spot.  It also offers more
//   control over defaults.  For good arguments supporting this choice, see:
//
//     https://www.iar.com/knowledge/learn/programming/advanced-preprocessor-tips-and-tricks/
//


#ifndef REB_CONFIG_H_1020_0304  // "include guard" allows multiple #includes
#define REB_CONFIG_H_1020_0304  // #s in case REB_CONFIG_H defined elsewhere

//=//// [REDUNDANT] CPLUSPLUS_11 PREPROCESSOR DEFINE ///////////////////////=//
//
// There is a Catch-22 in the CPLUSPLUS_11 definition from %reb-c.h, because
// it currently depends on TO_WINDOWS and %reb-config.h depends on CPLUSPLUS_11.
// For the moment, sort that out by defining the macro here...but we might
// just say it's the responsibility of whoever's doing the compiling to supply
// it on the command line, as you have to for MSVC anyway.
//
// (See notes on definition in %reb-c.h)

#if !defined(CPLUSPLUS_11)
  #if defined(__cplusplus) && __cplusplus >= 201103L
    #define CPLUSPLUS_11 1
  #else
    #define CPLUSPLUS_11 0
  #endif
#endif


//=//// [REDUNDANT] STATIC_ASSERT PREPROCESSOR DEFINE /////////////////////=//
//
// It's inconvenient to not have STATIC_ASSERT in this file, but we want it
// in %reb-c.h as well.  Allow the redundant definition (%reb-c checks to see
// if it's already defined).

#if CPLUSPLUS_11
    #define STATIC_ASSERT(cond) \
        static_assert((cond), #cond) // callsite has semicolon, see C trick
#else
    #define STATIC_ASSERT(cond) \
        struct GlobalScopeNoopTrick // https://stackoverflow.com/q/53923706
#endif


/** Primary Configuration **********************************************

The primary target system is defined by:

    TO_(os-base)    - for example TO_WINDOWS or TO_LINUX
    TO_(os-name)    - for example TO_WINDOWS_X86 or TO_LINUX_X64

The default config builds an R3 HOST executable program.

To change the config, host-kit developers can define:

    REB_EXT         - build an extension module
                      * create a DLL, not a host executable
                      * do not export a host lib (OS_ lib)
                      * call r3lib via struct and macros

    REB_CORE        - build /core only, no graphics, windows, etc.

Special internal defines used by RT, not Host-Kit developers:

    REB_API         - build r3lib as API
                      * export r3lib functions
                      * build r3lib dispatch table
                      * call host lib (OS_) via struct and macros

    REB_EXE         - build r3 as a standalone executable
*/

//* Common *************************************************************


#ifdef REB_EXE
    // standalone exe from RT
    // Export all of the APIs such that they can be referenced by extensions.
    // The purpose is to have one exe and some dynamic libraries for extensions (.dll, .so etc.)
    #define RL_API API_EXPORT
#else
    #ifdef REB_API
        // r3lib dll from RT
        #define RL_API API_EXPORT
    #elif defined(REB_EXT) || defined(REB_HOST)
        // Building extensions as external libraries (.dll, .so etc.)
        // or r3 host against r3lib dll
        #define RL_API API_IMPORT
    #else
        // Extensions are builtin
        #define RL_API
    #endif
#endif


//=//// WINDOWS ////////////////////////////////////////////////////////////=//

#if !defined(TO_WINDOWS_X86)
    #define TO_WINDOWS_X86 0
#endif

#if !defined(TO_WINDOWS_X64)
    #define TO_WINDOWS_X64 0
#endif

#if !defined(TO_WINDOWS)
    #define TO_WINDOWS 0
#endif

#if TO_WINDOWS
    // ASCII strings to Integer
    #define ATOI                    // supports it
    #define ATOI64                  // supports it
    #define ITOA64                  // supports it

    // Used when we build REBOL as a DLL:
    #define API_EXPORT __declspec(dllexport)
    #define API_IMPORT __declspec(dllimport)

#else
    #define API_IMPORT
    // Note: Unsupported by gcc 2.95.3-haiku-121101
    // (We #undef it in the Haiku section)
    #define API_EXPORT __attribute__((visibility("default")))
#endif


// Windows headers define the macros IN and OUT as part of an interface
// definition language.  Inside of Ren-C the OUT macro is used as a shorthand
// for accessing `FRM_OUT(frame_)` in a native.  You can #undef the Windows
// macros after you #include <windows.h>, but having the definitions under
// a switch gives more flexibility to define your own macros and leave the
// Windows ones alone.
//
#if !defined(REBOL_FRAME_SHORTHAND_MACROS)
    #define REBOL_FRAME_SHORTHAND_MACROS 1
#endif


//=//// LINUX //////////////////////////////////////////////////////////////=//

#if !defined(TO_LINUX_X86)
    #define TO_LINUX_X86 0
#endif

#if !defined(TO_LINUX_X64)
    #define TO_LINUX_X64 0
#endif

#if !defined(TO_LINUX_PPC)
    #define TO_LINUX_PPC 0
#endif

#if !defined(TO_LINUX_ARM)
    #define TO_LINUX_ARM 0
#endif

#if !defined(TO_LINUX_AARCH64)
    #define TO_LINUX_AARCH64 0
#endif

#if !defined(TO_LINUX_MIPS)
    #define TO_LINUX_MIPS 0
#endif

#if !defined(TO_LINUX)
    #define TO_LINUX 0
#else
    #define HAS_POSIX_SIGNAL

    // !!! The Atronix build introduced a differentiation between
    // a Linux build and a POSIX build, and one difference is the
    // usage of some signal functions that are not available if
    // you compile with a strict --std=c99 switch:
    //
    //      http://stackoverflow.com/a/22913324/211160
    //
    // Yet it appears that defining _POSIX_C_SOURCE is good enough
    // to get it working in --std=gnu99.  Because there are some
    // other barriers to pure C99 for the moment in the additions
    // from Saphirion (such as the use of alloca()), backing off the
    // pure C99 and doing it this way for now.
    //
    // These files may not include reb-config.h as the first include,
    // so be sure to say:
    //
    //     #define _POSIX_C_SOURCE 199309L
    //
    // ...at the top of the file.

    #define PROC_EXEC_PATH "/proc/self/exe"
#endif


//=//// APPLE //////////////////////////////////////////////////////////////=//

#if !defined(TO_OSX_PPC)
    #define TO_OSX_PPC 0
#endif

#if !defined(TO_OSX_X86)
    #define TO_OSX_X86 0
#endif

#if !defined(TO_OSX_X64)
    #define TO_OSX_X64 0
#endif

#if !defined(TO_OSX)
    #define TO_OSX 0
#endif


//=//// ANDROID ////////////////////////////////////////////////////////////=//

#if !defined(TO_ANDROID_ARM)
    #define TO_ANDROID_ARM 0
#endif

#if !defined(TO_ANDROID)
    #define TO_ANDROID 0
#elif TO_ANDROID
    #define PROC_EXEC_PATH "/proc/self/exe"
#endif


//=//// BSD ////////////////////////////////////////////////////////////////=//

#if !defined(TO_FREEBSD_X86)
    #define TO_FREEBSD_X86 0
#endif

#if !defined(TO_FREEBSD_X64)
    #define TO_FREEBSD_X64 0
#endif

#if !defined(TO_FREEBSD)
    #define TO_FREEBSD 0
#elif TO_FREEBSD
    #define HAVE_PROC_PATHNAME
#endif

#if !defined(TO_NETBSD)
    #define TO_NETBSD 0
#elif TO_NETBSD
    #define PROC_EXEC_PATH "/proc/curproc/exe"
#endif

#if !defined(TO_OPENBSD)
    #define TO_OPENBSD 0
#endif

#if !defined(TO_OPENBSD_X64)
    #define TO_OPENBSD_X64 0
#endif

#if !defined(TO_POSIX)
    #define TO_POSIX 0
#endif


//=//// HAIKU OS ///////////////////////////////////////////////////////////=//

#if !defined(TO_HAIKU)
    #define TO_HAIKU 0
#elif TO_HAIKU
    #undef API_EXPORT
    #define API_EXPORT

    #define DEF_UINT
#endif

#if !defined(TO_HAIKU_X64)
    #define TO_HAIKU_X64 0
#endif

//=//// EMSCRIPTEN /////////////////////////////////////////////////////////=//

#if !defined(TO_EMSCRIPTEN)
    #define TO_EMSCRIPTEN 0
#endif


//=//// AMIGA //////////////////////////////////////////////////////////////=//

// Note: The Amiga target is kept for its historical significance.
// Rebol required Amiga OS4 to be able to run, and the only
// machines that could run it had third-party add-on boards with
// PowerPC processors.  Hence stock machines like the Amiga4000
// which had a Motorola 68040 cannot built Rebol.
//
// To date, there has been no success reported in building Rebol
// for an Amiga emulator.  The last known successful build on
// Amiga hardware is dated 5-Mar-2011

#if !defined(TO_AMIGA)
    #define TO_AMIGA 0
#elif TO_AMIGA
    #define NO_DL_LIB
#endif


// NDEBUG is the variable that is either #defined or not by the C assert.h
// convention.  The reason NDEBUG was used was because it was a weird name and
// unlikely to compete with codebases that had their own DEBUG definition.
//
#if defined(NDEBUG)
    #define DEBUG 0
#else
    #define DEBUG 1
#endif


// Initially the debug build switches were all (default) or nothing (-DNDEBUG)
// but needed to be broken down into a finer-grained list.  This way, more
// constrained systems (like emscripten) can build in just the features it
// needs for a specific debug scenario.
//
// !!! Revisit a more organized way to inventory these settings and turn them
// on and off as time permits.
//
#if !defined(DEBUG_HAS_PROBE)
    #define DEBUG_HAS_PROBE DEBUG
#endif

#if !defined(DEBUG_FANCY_PANIC)
    #define DEBUG_FANCY_PANIC DEBUG
#endif

#if !defined(DEBUG_MONITOR_SERIES)
    #define DEBUG_MONITOR_SERIES DEBUG
#endif

#if !defined(DEBUG_COUNT_TICKS)
    #define DEBUG_COUNT_TICKS DEBUG
#endif

#if !defined(DEBUG_FRAME_LABELS)
    #define DEBUG_FRAME_LABELS DEBUG
#endif

#if !defined(DEBUG_UNREADABLE_TRASH)
    #define DEBUG_UNREADABLE_TRASH DEBUG
#endif

#if !defined(DEBUG_POISON_EXCESS_CAPACITY)
    #define DEBUG_POISON_EXCESS_CAPACITY DEBUG
#endif

#if !defined(DEBUG_BALANCE_STATE)
    #define DEBUG_BALANCE_STATE DEBUG
#endif


// See debugbreak.h and DECLARE_NATIVE(c_debug_break)...useful!
//
#if !defined(INCLUDE_C_DEBUG_BREAK_NATIVE)
    #define INCLUDE_C_DEBUG_BREAK_NATIVE DEBUG
#endif

// See DECLARE_NATIVE(test_librebol)
//
#if !defined(INCLUDE_TEST_LIBREBOL_NATIVE)
    #define INCLUDE_TEST_LIBREBOL_NATIVE DEBUG
#endif

#if !defined(DEBUG_CELL_WRITABILITY)
    #define DEBUG_CELL_WRITABILITY DEBUG
#endif

// !!! Checking the memory alignment is an important invariant but may be
// overkill to run on all platforms at all times.  It requires the
// DEBUG_CELL_WRITABILITY flag to be enabled, since it's the moment of
// writing that is when the check has an opportunity to run.
//
// !!! People using MLton to compile found that GCC 4.4.3 does not always
// align doubles to 64-bit boundaries on Windows, even when -malign-double
// is used.  It's a very old compiler, and may be a bug.  Disable align
// checking for GCC 4 on Windows, hope it just means slower loads/stores.
//
// https://stackoverflow.com/a/11110283/211160
//
#if !defined(DEBUG_MEMORY_ALIGN)
  #ifdef __GNUC__
    #if !defined(TO_WINDOWS) || (__GNUC__ >= 5) // only  least version 5
        #define DEBUG_MEMORY_ALIGN DEBUG
    #else
        #define DEBUG_MEMORY_ALIGN 0
    #endif
  #else
    #define DEBUG_MEMORY_ALIGN DEBUG
  #endif
#endif

// System V ABI for X86 says alignment can be 4 bytes for double.  But
// you can change this in the compiler settings.  We should either sync
// with that setting or just skip it, and assume that we do enough
// checking on the 64-bit builds.
//
// https://stackoverflow.com/q/14893802/
//
// !!! We are overpaying for the ALIGN_SIZE if it's not needed for double,
// so perhaps ALIGN_SIZE should be configured in build settings...
//
#if !defined(DEBUG_DONT_CHECK_ALIGN)  // !!! Appears unused?
  #if (! TO_WINDOWS_X86) && (! TO_LINUX_X86)
    #define DEBUG_DONT_CHECK_ALIGN 1
  #else
    #define DEBUG_DONT_CHECK_ALIGN 0
  #endif
#endif

#if DEBUG_MEMORY_ALIGN
    STATIC_ASSERT(DEBUG_CELL_WRITABILITY == 1);  // required for align check
#endif


// When it comes to exception-handling mechanisms, we have 3 choices:
//
//    * REBOL_FAIL_USES_LONGJMP to use C's setjmp()/longjmp()
//    * REBOL_FAIL_USES_TRY_CATCH to use C++'s try {...} catch {...}
//    * REBOL_FAIL_JUST_ABORTS will panic() and terminate the program
//
// It's considered desirable to support both a C and C++ approach.  Plain C
// compilation (e.g. with TCC) runs on many legacy/embedded platforms.  But
// structured exception handling has support on other systems like WasmEdge
// that cannot handle setjmp()/longjmp().
//
// To abstract this, Ren-C uses a keyword-like-macro called `fail()` that
// hides the differences.  See TRAP_BLOCK_IN_CASE_OF_ABRUPT_FAILURE for a
// breakdown of how this is pulled off.
//
// 1. setjmp()/longjmp() are essentially "goto on steroids", and on a
//    traditional platform they introduce the least baggage in terms of the
//    runtime needed to support them.  But while they are simple for many
//    traditional platforms, a runtime that enforces a de-facto structured
//    model may find it difficult-if-not-impossible to emulate them.
//
#if !defined(REBOL_FAIL_USES_LONGJMP) \
        && !defined(REBOL_FAIL_USES_TRY_CATCH) \
        && !defined(REBOL_FAIL_JUST_ABORTS)

    #define REBOL_FAIL_USES_LONGJMP 1  // often simplest, not always: see [1]
    #define REBOL_FAIL_USES_TRY_CATCH 0
    #define REBOL_FAIL_JUST_ABORTS 0

#elif defined(REBOL_FAIL_USES_LONGJMP)

    STATIC_ASSERT(REBOL_FAIL_USES_LONGJMP == 1);
    #if defined(REBOL_FAIL_USES_TRY_CATCH)
        STATIC_ASSERT(REBOL_FAIL_USES_TRY_CATCH == 0);
    #else
        #define REBOL_FAIL_USES_TRY_CATCH 0
    #endif
    #if defined(REBOL_FAIL_JUST_ABORTS)
        STATIC_ASSERT(REBOL_FAIL_JUST_ABORTS == 0);
    #else
        #define REBOL_FAIL_JUST_ABORTS 0
    #endif

#elif defined(REBOL_FAIL_USES_TRY_CATCH)

  #if !defined(__cplusplus)
    #error "REBOL_FAIL_USES_TRY_CATCH requires compiling Ren-C with C++"
    #include <stophere>  // https://stackoverflow.com/a/45661130
  #endif

    STATIC_ASSERT(REBOL_FAIL_USES_TRY_CATCH == 1);
    #if defined(REBOL_FAIL_USES_LONGJMP)
        STATIC_ASSERT(REBOL_FAIL_USES_LONGJMP == 0)
    #else
        #define REBOL_FAIL_USES_LONGJMP 0
    #endif
    #if defined(REBOL_FAIL_JUST_ABORTS)
        STATIC_ASSERT(REBOL_FAIL_JUST_ABORTS == 0);
    #else
        #define REBOL_FAIL_JUST_ABORTS 0
    #endif

#else

    STATIC_ASSERT(REBOL_FAIL_JUST_ABORTS == 1);
    #if defined(REBOL_FAIL_USES_LONGJMP)
        STATIC_ASSERT(REBOL_FAIL_USES_LONGJMP == 0)
    #else
        #define REBOL_FAIL_USES_LONGJMP 0
    #endif
    #if defined(REBOL_FAIL_USES_TRY_CATCH)
        STATIC_ASSERT(REBOL_FAIL_USES_TRY_CATCH == 0);
    #else
        #define REBOL_FAIL_USES_TRY_CATCH 0
    #endif

#endif


// Natives can be decorated with a RETURN: annotation, but this is not
// checked in the release build.  It's assumed they will only return the
// correct types.  This switch is used to panic() if they're wrong.
//
#if !defined(DEBUG_NATIVE_RETURNS)
    #define DEBUG_NATIVE_RETURNS DEBUG
#endif

// It can be nice to see aliases of platform pointers as if they were
// individual bytes, through union "puns".  Though this behavior is not
// well defined, it can be useful a lot of the time.
//
// https://en.wikipedia.org/wiki/Type_punning
//
#if !defined(DEBUG_USE_UNION_PUNS)
    #define DEBUG_USE_UNION_PUNS DEBUG
#endif

// Bitfields are poorly specified, and so even if it looks like your bits
// should pack into a struct exactly, they might not.  Only try this on
// Linux, where it has seemed to work out (MinGW64 build on Cygwin made
// invalid REBVAL sizes with this on)
//
#if defined(ENDIAN_LITTLE) && TO_LINUX_X64
    #define DEBUG_USE_BITFIELD_HEADER_PUNS 1
#else
    #define DEBUG_USE_BITFIELD_HEADER_PUNS 0
#endif

#if !defined(DEBUG_ENABLE_ALWAYS_MALLOC)
    //
    // We may want to test the valgrind build even if it's release so that
    // it checks the R3_ALWAYS_MALLOC environment variable
    //
    #if defined(INCLUDE_CALLGRIND_NATIVE)
        #define DEBUG_ENABLE_ALWAYS_MALLOC 1
    #else
        #define DEBUG_ENABLE_ALWAYS_MALLOC DEBUG
    #endif
#endif

#if CPLUSPLUS_11
    //
    // Each PUSH() on the data stack can potentially move all the pointers on
    // the stack.  Hence there is a debug setting for managing these pointers
    // in a special C++ container called StackValue(*).  This counts to see
    // how many stack pointers the user has in local variables, and if that
    // number is not zero then it asserts when a push or pop is requested, or
    // when the evaluator is invoked.
    //
    #define DEBUG_EXTANT_STACK_POINTERS DEBUG
#else
    #define DEBUG_EXTANT_STACK_POINTERS 0
#endif

// The PG_Reb_Stats structure is only tracked in the debug build, as this
// data gathering is a sort of constant "tax" on the system.  While it
// might arguably be interesting to non-debug build users who are trying
// to optimize their code, the compromise of having to maintain the
// numbers suggests those users should be empowered with a debug build if
// they are doing such work (they should probably have one for other
// reasons; note this has been true of things like Windows NT where there
// were indeed "checked" builds given to those who had such interest.)
//
#if !defined(DEBUG_COLLECT_STATS)
    #define DEBUG_COLLECT_STATS DEBUG
#endif


// UTF-8 Everywhere is a particularly large system change, which requires
// careful bookkeeping to allow the caching of positions to work.  These
// checks are too slow to run on most builds, but should be turned on if
// any problems are seen.
//
#if !defined(DEBUG_UTF8_EVERYWHERE)
    #define DEBUG_UTF8_EVERYWHERE 0
#endif

#if !defined(DEBUG_VERIFY_STR_AT)  // check cache correctness on every STR_AT
    #define DEBUG_VERIFY_STR_AT DEBUG_UTF8_EVERYWHERE
#endif

#if !defined(DEBUG_SPORADICALLY_DROP_BOOKMARKS)
    #define DEBUG_SPORADICALLY_DROP_BOOKMARKS DEBUG_UTF8_EVERYWHERE
#endif

#if !defined(DEBUG_BOOKMARKS_ON_MODIFY)  // test bookmark absence
    #define DEBUG_BOOKMARKS_ON_MODIFY DEBUG_UTF8_EVERYWHERE
#endif

#if !defined(DEBUG_TRACE_BOOKMARKS)
    #define DEBUG_TRACE_BOOKMARKS 0
#endif


// Cast checks in SER(), NOD(), ARR() are expensive--they ensure that when you
// cast a void pointer to a REBSER, that the header actually is for a REBSER
// (etc.)  Disable this by default unless you are using address sanitizer,
// where you expect things to be slow.
//
#if !defined(DEBUG_CHECK_CASTS)
  #if defined(__SANITIZE_ADDRESS__) && CPLUSPLUS_11
    #define DEBUG_CHECK_CASTS DEBUG
  #else
    #define DEBUG_CHECK_CASTS 0  // requires C++
  #endif
#endif


// Both Valgrind and Address Sanitizer can provide the call stack at the moment
// of allocation when a freed pointer is used.  Touch_Series() exploits this
// to use a bogus allocation to help mark series origins that can later be used
// by `panic()`.  But the feature is a waste if you're not using such tools.
//
// If you plan to use Valgrind with this, you'll have to set it explicitly...
// only Address Sanitizer can be detected here.
//
#if !defined(DEBUG_SERIES_ORIGINS)
  #if defined(__SANITIZE_ADDRESS__)
    #define DEBUG_SERIES_ORIGINS DEBUG
  #else
    #define DEBUG_SERIES_ORIGINS 0
  #endif
#endif


// The DEBUG_COUNT_LOCALS switch is an experimental application of SeriesHolder
// to determine when a GC_Kill_Series happens to a series with references still
// outstanding on the stack.  The mechanism may evolve to permit things like
// the C++ build being able to GC at arbitrary moments.
//
#if !defined(DEBUG_COUNT_LOCALS)
    #define DEBUG_COUNT_LOCALS 0
#endif
#if DEBUG_COUNT_LOCALS && CPLUSPLUS_11 == 0
    #error "DEBUG_COUNT_LOCALS requires C++11 or higher"
    #include <stophere>  // https://stackoverflow.com/a/45661130
#endif


// option(TYPE*) is a poor-man's implementation of optionals that lets you
// mark when a pointer is supposed to be passable as a nullptr.  It has some
// runtime costs because it will assert if you unwrap() the pointer and it is
// null when it shouldn't be.  Add it to the sanitized build.
//
#if !defined(DEBUG_CHECK_OPTIONALS)
  #if defined(__SANITIZE_ADDRESS__)
    #define DEBUG_CHECK_OPTIONALS DEBUG
  #else
    #define DEBUG_CHECK_OPTIONALS 0
  #endif
#endif

#if !defined(DEBUG_PROTECT_FEED_CELLS)
    #define DEBUG_PROTECT_FEED_CELLS DEBUG
#endif


// In order to make sure that a good mix of debug settings get tested, this
// does array termination checks on non-sanitizer debug builds.  Arrays are not
// usually marked at their tails (unlike R3-Alpha which used END! cells to
// terminate)...but the residual functionality helps catch overruns.
//
#if !defined(DEBUG_POISON_SERIES_TAILS)
  #if defined(__SANITIZE_ADDRESS__)
    #define DEBUG_POISON_SERIES_TAILS 0  // *not* when sanitized
  #else
    #define DEBUG_POISON_SERIES_TAILS DEBUG
  #endif
#endif

#if !defined(DEBUG_CHECK_ENDS)
  #if defined(__SANITIZE_ADDRESS__)
    #define DEBUG_CHECK_ENDS 0  // *not* when sanitized
  #else
    #define DEBUG_CHECK_ENDS (CPLUSPLUS_11 && DEBUG) ? 1 : 0
  #endif
#endif


#if !defined(DEBUG_TRACK_EXTEND_CELLS)
    #define DEBUG_TRACK_EXTEND_CELLS 0
#endif

#if !defined(UNUSUAL_CELL_SIZE)  // sizeof(Cell)*2 may be > sizeof(Stub)
    #define UNUSUAL_CELL_SIZE DEBUG_TRACK_EXTEND_CELLS
#endif

#if !defined(DEBUG_POISON_DROPPED_STACK_CELLS)
    #define DEBUG_POISON_DROPPED_STACK_CELLS DEBUG
#endif

#if !defined(DEBUG_ERASE_ALLOC_TAIL_CELLS)
    #define DEBUG_ERASE_ALLOC_TAIL_CELLS DEBUG
#endif


// It can be very difficult in release builds to know where a fail came
// from.  This arises in pathological cases where an error only occurs in
// release builds, or if making a full debug build bloats the code too much.
// (e.g. the JavaScript asyncify version).  A small but helpful debug
// switch does a printf of the __FILE__ and __LINE__ of fail() callsites.
//
#if !defined(DEBUG_PRINTF_FAIL_LOCATIONS)
    #define DEBUG_PRINTF_FAIL_LOCATIONS 0
#endif

#if !defined(DEBUG_VIRTUAL_BINDING)
    #define DEBUG_VIRTUAL_BINDING 0
#endif

#if !defined(DEBUG_EXPIRED_LOOKBACK)
    #define DEBUG_EXPIRED_LOOKBACK 0
#endif

// The third-party "dtoa.c" file was sensitive to whether DEBUG was #ifdef'd.
// In the world where #if is used instead of #ifdef, that means it includes the
// debug code whether DEBUG is 1 or 0.  The file was tweaked to include a more
// specific flag for debugging dtoa.c, which we will hopefully never need.
//
#if !defined(DEBUG_DTOA)
    #define DEBUG_DTOA 0
#endif

// It would seem that cells like REB_BLANK which don't use their payloads
// could just leave them uninitialized...saving time on the assignments.
//
// Unfortunately, this is a technically gray area in C.  If you try to
// copy the memory of that cell (as cells are often copied), it might be a
// "trap representation".  Reading such representations to copy them...
// even if not interpreted... is undefined behavior:
//
// https://stackoverflow.com/q/60112841
// https://stackoverflow.com/q/33393569/
//
// Odds are it would still work fine if you didn't zero them.  However,
// compilers will warn you--especially at higher optimization levels--if
// they notice uninitialized values being used in copies.  This is a bad
// warning to turn off, because it often points out defective code.
//
// So to play it safe and be able to keep warnings on, fields are zeroed out.
// But it's set up as its own independent flag, so that someone looking
// to squeak out a tiny bit more optimization could turn this off in a
// release build.  It would save on a few null assignments.
//
// (In release builds, the fields are assigned 0 because it's presumably a
// fast value to assign as an immediate.  In debug builds, they're assigned
// a trash value because it's more likely to cause trouble if accessed.)
//
#if DEBUG
    #define ZEROTRASH nullptr
#else
    #define ZEROTRASH cast(void*, cast(intptr_t, 0xDECAFBAD))
#endif

#define ZERO_UNUSED_CELL_FIELDS 1

#endif
