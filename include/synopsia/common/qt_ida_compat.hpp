/// @file qt_ida_compat.hpp
/// @brief Compatibility header for using Qt with IDA SDK
///
/// IDA SDK's pro.h defines qstrlen, qstrcmp, qstrncmp, etc. which conflict
/// with Qt's definitions in qbytearrayalgorithms.h.
///
/// Strategy: Include Qt headers first, then use macros to redirect IDA's
/// qstr functions to use Qt's versions, avoiding the redefinition error.
///
/// Usage in .cpp files that need both Qt and IDA:
///   #include <synopsia/qt_ida_compat.hpp>  // FIRST - before any other includes
///   #include <QWidget>                      // Qt headers
///   #include <QPainter>
///   // ... more Qt headers ...
///   #include <synopsia/qt_ida_compat_post.hpp>  // After Qt, before IDA
///   #include <synopsia/types.hpp>           // Now IDA headers are safe

#pragma once

// Check if IDA headers were already included
#if defined(__PRO_H)
#error "qt_ida_compat.hpp must be included BEFORE any IDA headers (pro.h was already included)"
#endif

// Mark that we're in Qt compatibility mode
#ifndef SYNOPSIA_QT_IDA_COMPAT
#define SYNOPSIA_QT_IDA_COMPAT 1
#endif
