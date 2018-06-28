//
//  File: %sys-series.h
//  Summary: {Definitions for Series (REBSER) plus Array, Frame, and Map}
//  Project: "Rebol 3 Interpreter and Run-time (Ren-C branch)"
//  Homepage: https://github.com/metaeducation/ren-c/
//
//=////////////////////////////////////////////////////////////////////////=//
//
// Copyright 2012 REBOL Technologies
// Copyright 2012-2017 Rebol Open Source Contributors
// REBOL is a trademark of REBOL Technologies
//
// See README.md and CREDITS.md for more information
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
//=////////////////////////////////////////////////////////////////////////=//
//
// Note: the word "Series" is overloaded in Rebol to refer to two related but
// distinct concepts:
//
// * The internal system datatype, also known as a REBSER.  It's a low-level
//   implementation of something similar to a vector or an array in other
//   languages.  It is an abstraction which represents a contiguous region
//   of memory containing equally-sized elements.
//
// * The user-level value type ANY-SERIES!.  This might be more accurately
//   called ITERATOR!, because it includes both a pointer to a REBSER of
//   data and an index offset into that data.  Attempts to reconcile all
//   the naming issues from historical Rebol have not yielded a satisfying
//   alternative, so the ambiguity has stuck.
//
// This file regards the first meaning of the word "series" and covers the
// low-level implementation details of a REBSER and its subclasses.  For info
// about the higher-level ANY-SERIES! value type and its embedded index,
// see %sys-value.h in the definition of `struct Reb_Any_Series`.
//
//=////////////////////////////////////////////////////////////////////////=//
//
// A REBSER is a contiguous-memory structure with an optimization of behaving
// like a kind of "double-ended queue".  It is able to reserve capacity at
// both the tail and the head, and when data is taken from the head it will
// retain that capacity...reusing it on later insertions at the head.
//
// The space at the head is called the "bias", and to save on pointer math
// per-access, the stored data pointer is actually adjusted to include the
// bias.  This biasing is backed out upon insertions at the head, and also
// must be subtracted completely to free the pointer using the address
// originally given by the allocator.
//
// The element size in a REBSER is known as the "width".  It is designed
// to support widths of elements up to 255 bytes.  (See note on SER_FREED
// about accomodating 256-byte elements.)
//
// REBSERs may be either manually memory managed or delegated to the garbage
// collector.  Free_Unmanaged_Series() may only be called on manual series.
// See MANAGE_SERIES()/PUSH_GUARD_SERIES() for remarks on how to work safely
// with pointers to garbage-collected series, to avoid having them be GC'd
// out from under the code while working with them.
//
// This file defines series subclasses which are type-incompatible with
// REBSER for safety.  (In C++ they would be derived classes, so common
// operations would not require casting...but this is C.)  The subclasses
// are explained where they are defined.
//
// Notes:
//
// * For the struct definition of REBSER, see %sys-rebser.h
//
// * It is desirable to have series subclasses be different types, even though
//   there are some common routines for processing them.  e.g. not every
//   function that would take a REBSER* would actually be handled in the same
//   way for a REBARR*.  Plus, just because a REBCTX* is implemented as a
//   REBARR* with a link to another REBARR* doesn't mean most clients should
//   be accessing the array--in a C++ build this would mean it would have some
//   kind of protected inheritance scheme.
//


//
// For debugging purposes, it's nice to be able to crash on some kind of guard
// for tracking the call stack at the point of allocation if we find some
// undesirable condition that we want a trace from.  Generally, series get
// set with this guard at allocation time.  But if you want to mark a moment
// later, you can.
//
// This works with Address Sanitizer or with Valgrind, but the config flag to
// enable it only comes automatically with address sanitizer.
//
#if defined(DEBUG_SERIES_ORIGINS) || defined(DEBUG_COUNT_TICKS)
    inline static void Touch_Series(REBSER *s) {
      #if defined(DEBUG_SERIES_ORIGINS)
        s->guard = cast(intptr_t*, malloc(sizeof(*s->guard)));
        free(s->guard);
      #endif

      #if defined(DEBUG_COUNT_TICKS)
        s->tick = TG_Tick;
      #else
        s->tick = 0;
      #endif
    }

    #define TOUCH_SERIES_IF_DEBUG(s) \
        Touch_Series(s)
#else
    #define TOUCH_SERIES_IF_DEBUG(s) \
        NOOP
#endif


//
// Series header FLAGs (distinct from INFO bits)
//

#define SET_SER_FLAG(s,f) \
    cast(void, SER(s)->header.bits |= (f))

#define CLEAR_SER_FLAG(s,f) \
    cast(void, SER(s)->header.bits &= ~(f))

#define GET_SER_FLAG(s,f) \
    cast(REBOOL, did (SER(s)->header.bits & (f))) // !!! single-flag check?

#define ANY_SER_FLAGS(s,f) \
    cast(REBOOL, did (SER(s)->header.bits & (f)))

#define ALL_SER_FLAGS(s,f) \
    cast(REBOOL, (SER(s)->header.bits & (f)) == (f))

#define NOT_SER_FLAG(s,f) \
    cast(REBOOL, not (SER(s)->header.bits & (f)))

#define SET_SER_FLAGS(s,f) \
    SET_SER_FLAG((s), (f))

#define CLEAR_SER_FLAGS(s,f) \
    CLEAR_SER_FLAG((s), (f))


//
// Series INFO bits (distinct from header FLAGs)
//

#define SET_SER_INFO(s,f) \
    cast(void, SER(s)->info.bits |= (f))

#define CLEAR_SER_INFO(s,f) \
    cast(void, SER(s)->info.bits &= ~(f))

#define GET_SER_INFO(s,f) \
    cast(REBOOL, did (SER(s)->info.bits & (f))) // !!! single-flag check?

#define ANY_SER_INFOS(s,f) \
    cast(REBOOL, did (SER(s)->info.bits & (f)))

#define ALL_SER_INFOS(s,f) \
    cast(REBOOL, (SER(s)->info.bits & (f)) == (f))

#define NOT_SER_INFO(s,f) \
    cast(REBOOL, not (SER(s)->info.bits & (f)))

#define SET_SER_INFOS(s,f) \
    SET_SER_INFO((s), (f))

#define CLEAR_SER_INFOS(s,f) \
    CLEAR_SER_INFO((s), (f))


//
// The mechanics of the macros that get or set the length of a series are a
// little bit complicated.  This is due to the optimization that allows data
// which is sizeof(REBVAL) or smaller to fit directly inside the series node.
//
// If a series is not "dynamic" (e.g. has a full pooled allocation) then its
// length is stored in the header.  But if a series is dynamically allocated
// out of the memory pools, then without the data itself taking up the
// "content", there's room for a length in the node.
//

#define SER_WIDE(s) \
    FOURTH_BYTE((s)->info)

inline static REBCNT SER_LEN(REBSER *s) {
    return (s->info.bits & SERIES_INFO_HAS_DYNAMIC)
        ? s->content.dynamic.len
        : THIRD_BYTE(s->info);
}

inline static void SET_SERIES_LEN(REBSER *s, REBCNT len) {
    assert(NOT_SER_FLAG(s, SERIES_FLAG_STACK));

    if (s->info.bits & SERIES_INFO_HAS_DYNAMIC)
        s->content.dynamic.len = len;
    else {
        assert(len < sizeof(s->content));
        THIRD_BYTE(s->info) = len;
        assert(SER_LEN(s) == len); // !!! is this an over-protective assert?
    }
}

inline static REBCNT SER_REST(REBSER *s) {
    if (s->info.bits & SERIES_INFO_HAS_DYNAMIC)
        return s->content.dynamic.rest;

    if (s->header.bits & SERIES_FLAG_ARRAY)
        return 2; // includes info bits acting as trick "terminator"

    assert(sizeof(s->content) % SER_WIDE(s) == 0);
    return sizeof(s->content) / SER_WIDE(s);
}

// Raw access does not demand that the caller know the contained type.  So
// for instance a generic debugging routine might just want a byte pointer
// but have no element type pointer to pass in.
//
inline static REBYTE *SER_DATA_RAW(REBSER *s) {
    // if updating, also update manual inlining in SER_AT_RAW
    if (s->info.bits & SERIES_INFO_INACCESSIBLE)
        fail (Error_Series_Data_Freed_Raw());

    return (s->info.bits & SERIES_INFO_HAS_DYNAMIC)
        ? cast(REBYTE*, s->content.dynamic.data)
        : cast(REBYTE*, &s->content);
}

inline static REBYTE *SER_AT_RAW(REBYTE w, REBSER *s, REBCNT i) {
#if !defined(NDEBUG)
    if (w != SER_WIDE(s)) {
        //
        // This is usually a sign that the series was GC'd, as opposed to the
        // caller passing in the wrong width (freeing sets width to 0).  But
        // give some debug tracking either way.
        //
        REBYTE wide = SER_WIDE(s);
        if (wide == 0)
            printf("SER_AT_RAW asked on freed series\n");
        else
            printf("SER_AT_RAW asked %d on width=%d\n", w, SER_WIDE(s));
        panic (s);
    }
#endif
    if (s->info.bits & SERIES_INFO_INACCESSIBLE)
        fail (Error_Series_Data_Freed_Raw());

    return ((w) * (i)) + ( // v-- inlining of SER_DATA_RAW
        (s->info.bits & SERIES_INFO_HAS_DYNAMIC)
            ? cast(REBYTE*, s->content.dynamic.data)
            : cast(REBYTE*, &s->content)
        );
}


//
// In general, requesting a pointer into the series data requires passing in
// a type which is the correct size for the series.  A pointer is given back
// to that type.
//
// Note that series indexing in C is zero based.  So as far as SERIES is
// concerned, `SER_HEAD(t, s)` is the same as `SER_AT(t, s, 0)`
//
// Use C-style cast instead of cast() macro, as it will always be safe and
// this is used very frequently.

#define SER_AT(t,s,i) \
    ((t*)SER_AT_RAW(sizeof(t), (s), (i)))

#define SER_HEAD(t,s) \
    SER_AT(t, (s), 0)

inline static REBYTE *SER_TAIL_RAW(size_t w, REBSER *s) {
    return SER_AT_RAW(w, s, SER_LEN(s));
}

#define SER_TAIL(t,s) \
    ((t*)SER_TAIL_RAW(sizeof(t), (s)))

inline static REBYTE *SER_LAST_RAW(size_t w, REBSER *s) {
    assert(SER_LEN(s) != 0);
    return SER_AT_RAW(w, s, SER_LEN(s) - 1);
}

#define SER_LAST(t,s) \
    ((t*)SER_LAST_RAW(sizeof(t), (s)))


#define SER_FULL(s) \
    (SER_LEN(s) + 1 >= SER_REST(s))

#define SER_AVAIL(s) \
    (SER_REST(s) - (SER_LEN(s) + 1)) // space available (minus terminator)

#define SER_FITS(s,n) \
    ((SER_LEN(s) + (n) + 1) <= SER_REST(s))


//
// Optimized expand when at tail (but, does not reterminate)
//

inline static void EXPAND_SERIES_TAIL(REBSER *s, REBCNT delta) {
    if (SER_FITS(s, delta))
        SET_SERIES_LEN(s, SER_LEN(s) + delta);
    else
        Expand_Series(s, SER_LEN(s), delta);
}

//
// Termination
//

inline static void TERM_SEQUENCE(REBSER *s) {
    assert(NOT_SER_FLAG(s, SERIES_FLAG_ARRAY));
    memset(SER_AT_RAW(SER_WIDE(s), s, SER_LEN(s)), 0, SER_WIDE(s));
}

inline static void TERM_SEQUENCE_LEN(REBSER *s, REBCNT len) {
    SET_SERIES_LEN(s, len);
    TERM_SEQUENCE(s);
}

#ifdef NDEBUG
    #define ASSERT_SERIES_TERM(s) \
        NOOP
#else
    #define ASSERT_SERIES_TERM(s) \
        Assert_Series_Term_Core(s)
#endif

// Just a No-Op note to point out when a series may-or-may-not be terminated
//
#define NOTE_SERIES_MAYBE_TERM(s) NOOP


//=////////////////////////////////////////////////////////////////////////=//
//
//  SERIES MANAGED MEMORY
//
//=////////////////////////////////////////////////////////////////////////=//
//
// When a series is allocated by the Make_Series routine, it is not initially
// visible to the garbage collector.  To keep from leaking it, then it must
// be either freed with Free_Unmanaged_Series or delegated to the GC to manage
// with MANAGE_SERIES.
//
// (In debug builds, there is a test at the end of every Rebol function
// dispatch that checks to make sure one of those two things happened for any
// series allocated during the call.)
//
// The implementation of MANAGE_SERIES is shallow--it only sets a bit on that
// *one* series, not any series referenced by values inside of it.  This
// means that you cannot build a hierarchical structure that isn't visible
// to the GC and then do a single MANAGE_SERIES call on the root to hand it
// over to the garbage collector.  While it would be technically possible to
// deeply walk the structure, the efficiency gained from pre-building the
// structure with the managed bit set is significant...so that's how deep
// copies and the scanner/load do it.
//
// (In debug builds, if any unmanaged series are found inside of values
// reachable by the GC, it will raise an alert.)
//

inline static REBOOL IS_SERIES_MANAGED(REBSER *s) {
    return did (s->header.bits & NODE_FLAG_MANAGED);
}

#define MANAGE_SERIES(s) \
    Manage_Series(s)

inline static void ENSURE_SERIES_MANAGED(REBSER *s) {
    if (not IS_SERIES_MANAGED(s))
        MANAGE_SERIES(s);
}

#ifdef NDEBUG
    #define ASSERT_SERIES_MANAGED(s) \
        NOOP
#else
    inline static void ASSERT_SERIES_MANAGED(REBSER *s) {
        if (not IS_SERIES_MANAGED(s))
            panic (s);
    }
#endif


//=////////////////////////////////////////////////////////////////////////=//
//
// SERIES COLORING API
//
//=////////////////////////////////////////////////////////////////////////=//
//
// R3-Alpha re-used the same marking flag from the GC in order to do various
// other bit-twiddling tasks when the GC wasn't running.  This is an
// unusually dangerous thing to be doing...because leaving a stray mark on
// during some other traversal could lead the GC to think it had marked
// things reachable from that series when it had not--thus freeing something
// that was still in use.
//
// While leaving a stray mark on is a bug either way, GC bugs are particularly
// hard to track down.  So one doesn't want to risk them if not absolutely
// necessary.  Not to mention that sharing state with the GC that you can
// only use when it's not running gets in the way of things like background
// garbage collection, etc.
//
// Ren-C keeps the term "mark" for the GC, since that's standard nomenclature.
// A lot of basic words are taken other places for other things (tags, flags)
// so this just goes with a series "color" of black or white, with white as
// the default.  The debug build keeps a count of how many black series there
// are and asserts it's 0 by the time each evaluation ends, to ensure balance.
//

static inline REBOOL Is_Series_Black(REBSER *s) {
    return GET_SER_INFO(s, SERIES_INFO_BLACK);
}

static inline REBOOL Is_Series_White(REBSER *s) {
    return NOT_SER_INFO(s, SERIES_INFO_BLACK);
}

static inline void Flip_Series_To_Black(REBSER *s) {
    assert(NOT_SER_INFO(s, SERIES_INFO_BLACK));
    SET_SER_INFO(s, SERIES_INFO_BLACK);
#if !defined(NDEBUG)
    ++TG_Num_Black_Series;
#endif
}

static inline void Flip_Series_To_White(REBSER *s) {
    assert(GET_SER_INFO(s, SERIES_INFO_BLACK));
    CLEAR_SER_INFO(s, SERIES_INFO_BLACK);
#if !defined(NDEBUG)
    --TG_Num_Black_Series;
#endif
}


//
// Freezing and Locking
//

inline static void Freeze_Sequence(REBSER *s) { // there is no unfreeze!
    assert(NOT_SER_FLAG(s, SERIES_FLAG_ARRAY)); // use Deep_Freeze_Array
    SET_SER_INFO(s, SERIES_INFO_FROZEN);
}

inline static REBOOL Is_Series_Frozen(REBSER *s) {
    assert(NOT_SER_FLAG(s, SERIES_FLAG_ARRAY)); // use Is_Array_Deeply_Frozen
    return GET_SER_INFO(s, SERIES_INFO_FROZEN);
}

inline static REBOOL Is_Series_Read_Only(REBSER *s) { // may be temporary...
    return ANY_SER_INFOS(
        s, SERIES_INFO_FROZEN | SERIES_INFO_HOLD | SERIES_INFO_PROTECTED
    );
}

// Gives the appropriate kind of error message for the reason the series is
// read only (frozen, running, protected, locked to be a map key...)
//
// !!! Should probably report if more than one form of locking is in effect,
// but if only one error is to be reported then this is probably the right
// priority ordering.
//
inline static void FAIL_IF_READ_ONLY_SERIES(REBSER *s) {
    if (Is_Series_Read_Only(s)) {
        if (GET_SER_INFO(s, SERIES_INFO_AUTO_LOCKED))
            fail (Error_Series_Auto_Locked_Raw());

        if (GET_SER_INFO(s, SERIES_INFO_HOLD))
            fail (Error_Series_Held_Raw());

        if (GET_SER_INFO(s, SERIES_INFO_FROZEN))
            fail (Error_Series_Frozen_Raw());

        assert(GET_SER_INFO(s, SERIES_INFO_PROTECTED));
        fail (Error_Series_Protected_Raw());
    }
}


//=////////////////////////////////////////////////////////////////////////=//
//
//  GUARDING SERIES FROM GARBAGE COLLECTION
//
//=////////////////////////////////////////////////////////////////////////=//
//
// The garbage collector can run anytime the evaluator runs (and also when
// ports are used).  So if a series has had MANAGE_SERIES run on it, the
// potential exists that any C pointers that are outstanding may "go bad"
// if the series wasn't reachable from the root set.  This is important to
// remember any time a pointer is held across a call that runs arbitrary
// user code.
//
// This simple stack approach allows pushing protection for a series, and
// then can release protection only for the last series pushed.  A parallel
// pair of macros exists for pushing and popping of guard status for values,
// to protect any series referred to by the value's contents.  (Note: This can
// only be used on values that do not live inside of series, because there is
// no way to guarantee a value in a series will keep its address besides
// guarding the series AND locking it from resizing.)
//
// The guard stack is not meant to accumulate, and must be cleared out
// before a command ends.
//

inline static void PUSH_GUARD_SERIES(REBSER *s) {
    ASSERT_SERIES_MANAGED(s); // see PUSH_GUARD_ARRAY_CONTENTS if you need it
    Guard_Node_Core(cast(const REBNOD*, s));
}

inline static void PUSH_GUARD_VALUE(const RELVAL *v) {
    Guard_Node_Core(cast(const REBNOD*, v));
}

inline static void Drop_Guard_Series_Common(REBSER *s) {
    UNUSED(s);
    GC_Guarded->content.dynamic.len--;
}

inline static void Drop_Guard_Value_Common(const RELVAL *v) {
    UNUSED(v);
    GC_Guarded->content.dynamic.len--;
}

#ifdef NDEBUG
    #define DROP_GUARD_SERIES(s) \
        Drop_Guard_Series_Common(s);

    #define DROP_GUARD_VALUE(v) \
        Drop_Guard_Value_Common(v);
#else
    inline static void Drop_Guard_Series_Debug(
        REBSER *s,
        const char *file,
        int line
    ){
        if (s != *SER_LAST(REBSER*, GC_Guarded))
            panic_at (s, file, line);
        Drop_Guard_Series_Common(s);
    }

    inline static void Drop_Guard_Value_Debug(
        const RELVAL *v,
        const char *file,
        int line
    ){
        if (v != *SER_LAST(RELVAL*, GC_Guarded))
            panic_at (v, file, line);
        Drop_Guard_Value_Common(v);
    }

    #define DROP_GUARD_SERIES(s) \
        Drop_Guard_Series_Debug(s, __FILE__, __LINE__);

    #define DROP_GUARD_VALUE(v) \
        Drop_Guard_Value_Debug(v, __FILE__, __LINE__);
#endif


//=////////////////////////////////////////////////////////////////////////=//
//
//  ANY-SERIES!
//
//=////////////////////////////////////////////////////////////////////////=//

inline static REBSER *VAL_SERIES(const RELVAL *v) {
    assert(ANY_SERIES(v) or IS_MAP(v) or IS_IMAGE(v)); // !!! gcc 5.4 -O2 bug
    return v->payload.any_series.series;
}

inline static void INIT_VAL_SERIES(RELVAL *v, REBSER *s) {
    assert(NOT_SER_FLAG(s, SERIES_FLAG_ARRAY));
    assert(IS_SERIES_MANAGED(s));
    v->payload.any_series.series = s;
}

#if defined(NDEBUG) || !defined(CPLUSPLUS_11)
    #define VAL_INDEX(v) \
        ((v)->payload.any_series.index)
#else
    // allows an assert, but also lvalue: `VAL_INDEX(v) = xxx`
    //
    inline static REBCNT & VAL_INDEX(RELVAL *v) { // C++ reference type
        assert(ANY_SERIES(v));
        return v->payload.any_series.index;
    }
    inline static REBCNT VAL_INDEX(const RELVAL *v) {
        assert(ANY_SERIES(v));
        return v->payload.any_series.index;
    }
#endif

#define VAL_LEN_HEAD(v) \
    SER_LEN(VAL_SERIES(v))

inline static REBCNT VAL_LEN_AT(const RELVAL *v) {
    if (VAL_INDEX(v) >= VAL_LEN_HEAD(v))
        return 0; // avoid negative index
    return VAL_LEN_HEAD(v) - VAL_INDEX(v); // take current index into account
}

inline static REBYTE *VAL_RAW_DATA_AT(const RELVAL *v) {
    return SER_AT_RAW(SER_WIDE(VAL_SERIES(v)), VAL_SERIES(v), VAL_INDEX(v));
}

#define Init_Any_Series_At(v,t,s,i) \
    Init_Any_Series_At_Core((v), (t), (s), (i), UNBOUND)

#define Init_Any_Series(v,t,s) \
    Init_Any_Series_At((v), (t), (s), 0)


//=////////////////////////////////////////////////////////////////////////=//
//
//  BITSET!
//
//=////////////////////////////////////////////////////////////////////////=//
//
// !!! As written, bitsets use the Any_Series structure in their
// implementation, but are not considered to be an ANY-SERIES! type.
//

#define VAL_BITSET(v) \
    VAL_SERIES(v)

#define Init_Bitset(v,s) \
    Init_Any_Series((v), REB_BITSET, (s))


// Make a series of a given width (unit size).  The series will be zero
// length to start with, and will not have a dynamic data allocation.  This
// is a particularly efficient default state, so separating the dynamic
// allocation into a separate routine is not a huge cost.
//
inline static REBSER *Make_Series_Node(REBYTE wide, REBFLGS flags) {
    assert(wide != 0);
    assert(not (flags & NODE_FLAG_CELL));

    REBSER *s = cast(REBSER*, Make_Node(SER_POOL));
    if ((GC_Ballast -= sizeof(REBSER)) <= 0)
        SET_SIGNAL(SIG_RECYCLE);

    // Out of the 8 platform pointers that comprise a series node, only 3
    // actually need to be initialized to get a functional non-dynamic
    // series or array of length 0!  See %rebser.h for an explanation, and
    // Init_Endlike_Header() for why we can't just say `s->info.bits = ...`
    //
    // Note that the optimizer *should* be able to fold together additional
    // bits for the infos in immediately subsequent SET_SER_INFO() calls.
    //
    s->header.bits = NODE_FLAG_NODE | flags; // #1
    TRASH_POINTER_IF_DEBUG(LINK(s).trash); // #2
    s->content.fixed.values[0].header.bits = CELL_MASK_NON_STACK_END; // #3
    TRACK_CELL_IF_DEBUG(&s->content.fixed.values[0], "<<make>>", 0); // #4-#6
    Init_Endlike_Header(&s->info, FLAG_FOURTH_BYTE(wide)); // #7
    TRASH_POINTER_IF_DEBUG(MISC(s).trash); // #8

    // It is more efficient if you know a series is going to become managed to
    // create it in the managed state.  But be sure no evaluations are called
    // before it's made reachable by the GC, or use PUSH_GUARD_SERIES().
    //
    if (not (flags & NODE_FLAG_MANAGED)) {
        if (SER_FULL(GC_Manuals))
            Extend_Series(GC_Manuals, 8);

        cast(REBSER**, GC_Manuals->content.dynamic.data)[
            GC_Manuals->content.dynamic.len++
        ] = s; // start out managed to not need to find/remove from this later
    }

  #if !defined(NDEBUG)
    TOUCH_SERIES_IF_DEBUG(s); // tag current C stack as series origin in ASAN
    PG_Reb_Stats->Series_Made++;
  #endif

    return s;
}

// If the data is tiny enough, it will be fit into the series node itself.
// Small series will be allocated from a memory pool.
// Large series will be allocated from system memory.
//
inline static REBSER *Make_Series_Core(
    REBCNT capacity,
    REBYTE wide,
    REBFLGS flags
){
    assert(not (flags & (SERIES_FLAG_ARRAY | ARRAY_FLAG_FILE_LINE)));

    if (cast(REBU64, capacity) * wide > INT32_MAX)
        fail (Error_No_Memory(cast(REBU64, capacity) * wide));

    REBSER *s = Make_Series_Node(wide, flags);

    if (capacity * wide > sizeof(s->content)) {
        //
        // Data won't fit in a REBSER node, needs a dynamic allocation.  The
        // capacity given back as the ->rest may be larger than the requested
        // size, because the memory pool reports the full rounded allocation.

        if (not Did_Series_Data_Alloc(s, capacity))
            fail (Error_No_Memory(capacity * wide));

      #if !defined(NDEBUG)
        PG_Reb_Stats->Series_Memory += capacity * wide;
      #endif
    }

    return s;
}

#define Make_Series(capacity, wide) \
    Make_Series_Core((capacity), (wide), SERIES_FLAGS_NONE)
