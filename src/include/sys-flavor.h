//
//  File: %sys-flavor.h
//  Summary: {Series Subclass Type Enumeration}
//  Project: "Ren-C Interpreter and Run-time"
//  Homepage: https://github.com/metaeducation/ren-c/
//
//=////////////////////////////////////////////////////////////////////////=//
//
// Copyright 2021 Ren-C Open Source Contributors
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
// A byte in the series node header is used to store an enumeration value of
// the kind of node that it is.  This takes the place of storing a special
// element "width" in the series (which R3-Alpha did).  Instead, the element
// width is determined by the "flavor".
//
// In order to maximize the usefulness of this value, the enumeration is
// organized in a way where the ordering conveys value.  So all the arrays are
// grouped together so a single test can tell if a subclass is an array type.
// This saves on needing to have separate flags like SERIES_FLAG_IS_ARRAY.
//
//=//// NOTES ////////////////////////////////////////////////////////////=//
//
// * It would be nice if this file could be managed by a %flavors.r file that
//   would be something like the %types.r for value types...where the process
//   of auto-generation generated testing macros automatically.
//


enum Reb_Stub_Flavor {
    //
    // The 0 value is used for just plain old arrays, so that you can call
    // Make_Array_Core() with some additional flags but leave out a flavor...
    // and it will assume you just want a usermode array.
    //
    // !!! Should this flavor automatically imply file and line numbering
    // should be captured?
    //
    FLAVOR_ARRAY,

    // A "use" is a request in a virtual binding chain to make an object's
    // fields visible virtually in the code.  LETs can also be in the chain,
    // and a frame varlist is also allowed to temrinate it.
    //
    FLAVOR_USE,

    // A FLAVOR_HITCH is an ephemeral element which is chained into the
    // "hitch" list on a symbol, when that symbol is being bound.  Currently
    // it holds an integer for a binding position, but allowing it to hold
    // arbitrary things for a mapping is being considered.
    //
    // !!! Think how this might relate to locking and inodes.  Does it?
    //
    FLAVOR_HITCH,

    // To make it possible to reuse exemplars and paramlists in action
    // variations that have different partial specializations, a splice of
    // partial refinements sit between the action cell and its "speciality".
    //
    FLAVOR_PARTIALS,

    FLAVOR_LIBRARY,
    FLAVOR_HANDLE,

    FLAVOR_FEED,
    FLAVOR_API,

    // This is used by rebINLINE() to place an array of content as raw
    // material to execute.  (It leverages similar code as MACRO.)
    //
    FLAVOR_INSTRUCTION_SPLICE,

    // Pairlists are used by map!.  They can't hold isotopes, but voids are
    // used to signal missing keys.
    //
    FLAVOR_PAIRLIST,
    FLAVOR_MIN_VOIDS_OK = FLAVOR_PAIRLIST,

    FLAVOR_MIN_ISOTOPES_OK,  //=//// BELOW HERE, THE ARRAYS CAN HOLD ISOTOPES

    // This indicates this series represents the "varlist" of a context (which
    // is interchangeable with the identity of the varlist itself).  A second
    // series can be reached from it via the LINK() in the series node, which
    // is known as a "keylist".
    //
    // See notes on Raw_Context for further details about what a context is.
    //
    FLAVOR_VARLIST = FLAVOR_MIN_ISOTOPES_OK,

    FLAVOR_PARAMLIST = FLAVOR_VARLIST,  // review

    // "Details" are the per-ACTION! instance information (e.g. this would be
    // the body array for a usermode function, or the datatype that a type
    // checker dispatcher would want to check against.)  The first element of
    // the array is an archetypal value for the action (no binding/phase).
    //
    FLAVOR_DETAILS,

    // The concept of "Virtual Binding" is that instances of ANY-ARRAY! values
    // can carry along a collection of contexts that override the bindings of
    // words that are encountered.  This collection is done by means of
    // "lets" that make a linked list of overrides.
    //
    FLAVOR_LET,

    // A "patch" is a container for a single variable for a context.  Rather
    // than live in the context directly, it stands on its own.  Modules are
    // made up of patches vs. using the packed array VARLIST of frames and
    // contexts.
    //
    FLAVOR_PATCH,

    // The data stack is implemented as an array but has its own special
    // marking routine.  However, isotopes are legal in the data stack... but
    // when popping the stack it is checked that the array being popped *into*
    // allows isotopes.
    //
    FLAVOR_DATASTACK,

    FLAVOR_PLUG,

    FLAVOR_MAX_ARRAY = FLAVOR_PLUG,  //=//// ABOVE HERE WIDTH IS sizeof(REBVAL)

    // For the moment all series that aren't a REBVAL or a binary store items
    // of size pointer.
    //
    FLAVOR_KEYLIST,  // width = sizeof(Symbol(*))
    FLAVOR_POINTER,  // generic
    FLAVOR_CANONTABLE,  // for canons table
    FLAVOR_NODELIST,  // e.g. GC protect list
    FLAVOR_SERIESLIST,  // e.g. manually allocated series list
    FLAVOR_MOLDSTACK,

    FLAVOR_HASHLIST,  // outlier, sizeof(REBLEN)...
    FLAVOR_BOOKMARKLIST,  // also outlier, sizeof(struct Reb_Bookmark)

    FLAVOR_MIN_BYTESIZE,  //=//////// EVERYTHING BELOW THIS LINE HAS WIDTH = 1

    FLAVOR_BINARY = FLAVOR_MIN_BYTESIZE,

    FLAVOR_MIN_UTF8,  //=////// EVERYTHING BELOW THIS LINE IS UTF-8 (OR TRASH)

    FLAVOR_STRING = FLAVOR_MIN_UTF8,

    // While the content format is UTF-8 for both ANY-STRING! and ANY-WORD!,
    // MISC() and LINK() fields are used differently.  String caches its length
    // in codepoints so that doesn't have to be recalculated, and it also has
    // caches of "bookmarks" mapping codepoint indexes to byte offsets.  Words
    // store a pointer that is used in a circularly linked list to find their
    // canon spelling form...as well as hold binding information.
    //
    FLAVOR_SYMBOL,

    // Right now there is only one instance of FLAVOR_THE_GLOBAL_INACCESSIBLE
    // series.  All nodes that have SERIES_FLAG_INACCESSIBLE will be canonized
    // to this node.  This allows a decayed series to still convey what flavor
    // it was before being decayed.  That's useful at least for debugging, but
    // maybe for other mechanisms that sometimes might want to propagate some
    // residual information from decayed series to the referencing sites.
    //
    // (For instance: Such a mechanism would've been necessary for propagating
    // symbols back into words, when bound words gave up their symbols...if the
    // series they were bound to went away.  Not needed now--but an example.)
    //
    FLAVOR_THE_GLOBAL_INACCESSIBLE,

  #if !defined(NDEBUG)
    FLAVOR_TRASH,
  #endif

    FLAVOR_MAX
};

typedef enum Reb_Stub_Flavor Flavor;


// Most accesses of series via SER_AT(...) and ARR_AT(...) macros already
// know at the callsite the size of the access.  The width is only a double
// check in the debug build, and used at allocation time and other moments
// when the system has to know the size but doesn't yet know the type.  Hence
// This doesn't need to be particularly fast...so a lookup table is probably
// not needed.  Still, the common cases (array and strings) are put first.
//
inline static size_t Wide_For_Flavor(Flavor flavor) {
    assert(flavor != FLAVOR_TRASH);
    if (flavor <= FLAVOR_MAX_ARRAY)
        return sizeof(REBVAL);
    if (flavor >= FLAVOR_MIN_BYTESIZE)
        return 1;
    if (flavor == FLAVOR_BOOKMARKLIST)
        return sizeof(struct Reb_Bookmark);
    if (flavor == FLAVOR_HASHLIST)
        return sizeof(REBLEN);
    return sizeof(void*);
}

#define SER_WIDE(s) \
    Wide_For_Flavor(SER_FLAVOR(s))



#define IS_SER_ARRAY(s)         (SER_FLAVOR(s) <= FLAVOR_MAX_ARRAY)
#define IS_SER_UTF8(s)          (SER_FLAVOR(s) >= FLAVOR_MIN_UTF8)

#define IS_NONSYMBOL_STRING(s)  (SER_FLAVOR(s) == FLAVOR_STRING)
#define IS_SYMBOL(s)            (SER_FLAVOR(s) == FLAVOR_SYMBOL)

#define IS_KEYLIST(s)           (SER_FLAVOR(s) == FLAVOR_KEYLIST)

#define IS_LET(s)               (SER_FLAVOR(s) == FLAVOR_LET)
#define IS_USE(s)               (SER_FLAVOR(s) == FLAVOR_USE)
#define IS_PATCH(s)             (SER_FLAVOR(s) == FLAVOR_PATCH)
#define IS_VARLIST(s)           (SER_FLAVOR(s) == FLAVOR_VARLIST)
#define IS_PAIRLIST(s)          (SER_FLAVOR(s) == FLAVOR_PAIRLIST)
#define IS_DETAILS(s)           (SER_FLAVOR(s) == FLAVOR_DETAILS)
#define IS_PARTIALS(s)          (SER_FLAVOR(s) == FLAVOR_PARTIALS)
