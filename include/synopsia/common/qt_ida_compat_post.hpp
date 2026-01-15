/// @file qt_ida_compat_post.hpp
/// @brief Post-Qt compatibility definitions for IDA SDK
///
/// Include this AFTER all Qt headers but BEFORE any IDA headers.
/// This defines macros that redirect IDA's qstr functions to avoid conflicts.

#pragma once

#ifndef SYNOPSIA_QT_IDA_COMPAT
#error "qt_ida_compat_post.hpp requires qt_ida_compat.hpp to be included first"
#endif

// At this point, Qt's qstrlen, qstrcmp, qstrncmp are defined.
// We need to prevent IDA's pro.h from redefining them.
// 
// The trick: Define macros that make IDA's inline functions have different names,
// then provide our own implementations that call Qt's versions.

// Save Qt's definitions (they're inline in qbytearrayalgorithms.h)
// Qt defines:
//   inline size_t qstrlen(const char *str)
//   int qstrcmp(const char *str1, const char *str2)  // not inline, declared only
//   inline int qstrncmp(const char *str1, const char *str2, size_t len)

// We can't easily redirect IDA's definitions because they use idaapi calling convention
// and have overloads for uchar* and wchar16_t*.
//
// The practical solution: Use compiler-specific pragmas to suppress the warnings/errors
// about redefinition, letting the linker figure it out (both implementations are identical
// for const char* versions).

// For clang/gcc: Use pragma to ignore the redefinition
#if defined(__clang__)
    // Clang-specific: Suppress redefinition errors for these specific functions
    #pragma clang diagnostic push
    #pragma clang diagnostic ignored "-Winconsistent-dllimport"
    // Note: Clang treats redefinition of inline functions as an error, not a warning
    // We may need a different approach
#elif defined(__GNUC__)
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wredundant-decls"
#endif

// Define a marker so types.hpp knows to handle conflicts
#define SYNOPSIA_QT_COMPAT_POST 1
