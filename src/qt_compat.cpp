/// @file qt_compat.cpp
/// @brief Qt version compatibility shim
///
/// IDA uses a namespaced Qt build (QT_NAMESPACE=QT), so all Qt symbols
/// including version tags are in the QT:: namespace.
///
/// Qt 6.8.3 headers with QT_NAMESPACE=QT expect _qt_version_tag_QT_6_8 symbol,
/// which IDA's Qt 6.8.2 provides - no compatibility shim needed.
///
/// This file is kept for documentation and potential future compatibility needs.

// Empty - IDA's Qt 6.8.2 provides _qt_version_tag_QT_6_8 which matches Qt 6.8.3 headers
