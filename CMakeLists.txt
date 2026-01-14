cmake_minimum_required(VERSION 3.20)
project(synopsia VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# =============================================================================
# Build Configuration
# =============================================================================

# macOS deployment target (12.0 = Monterey, matches IDA SDK requirement)
if(APPLE)
    if(NOT CMAKE_OSX_DEPLOYMENT_TARGET OR CMAKE_OSX_DEPLOYMENT_TARGET STREQUAL "")
        set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0" CACHE STRING "Minimum macOS version" FORCE)
    endif()
endif()

# Build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# =============================================================================
# IDA SDK Configuration
# =============================================================================

if(NOT DEFINED IDA_SDK_DIR)
    if(DEFINED ENV{IDA_SDK_DIR})
        set(IDA_SDK_DIR $ENV{IDA_SDK_DIR})
    elseif(DEFINED ENV{IDASDK})
        set(IDA_SDK_DIR $ENV{IDASDK})
    else()
        message(FATAL_ERROR "IDA_SDK_DIR must be set to the IDA SDK directory")
    endif()
endif()

# Handle SDK in subdirectory
if(EXISTS "${IDA_SDK_DIR}/src/include/pro.h")
    set(IDA_SDK_DIR "${IDA_SDK_DIR}/src")
endif()

if(NOT DEFINED IDA_INSTALL_DIR)
    if(DEFINED ENV{IDA_INSTALL_DIR})
        set(IDA_INSTALL_DIR $ENV{IDA_INSTALL_DIR})
    endif()
endif()

# =============================================================================
# Platform Detection
# =============================================================================

if(WIN32)
    set(IDA_PLATFORM "win")
    set(IDA_LIB_SUFFIX ".lib")
    set(PLUGIN_EXT ".dll")
elseif(APPLE)
    set(IDA_PLATFORM "mac")
    set(IDA_LIB_SUFFIX ".dylib")
    set(PLUGIN_EXT ".dylib")
else()
    set(IDA_PLATFORM "linux")
    set(IDA_LIB_SUFFIX ".so")
    set(PLUGIN_EXT ".so")
endif()

# Architecture detection
message(STATUS "CMAKE_OSX_ARCHITECTURES: ${CMAKE_OSX_ARCHITECTURES}")
message(STATUS "CMAKE_SYSTEM_PROCESSOR: ${CMAKE_SYSTEM_PROCESSOR}")

if(APPLE)
    if(CMAKE_OSX_ARCHITECTURES STREQUAL "arm64")
        set(IDA_ARCH "arm64")
    elseif(CMAKE_OSX_ARCHITECTURES STREQUAL "x86_64")
        set(IDA_ARCH "x64")
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64")
        set(IDA_ARCH "arm64")
    else()
        set(IDA_ARCH "x64")
    endif()
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64")
    set(IDA_ARCH "arm64")
else()
    set(IDA_ARCH "x64")
endif()

message(STATUS "IDA_ARCH: ${IDA_ARCH}")

# 64-bit IDA build
option(IDA_EA64 "Build for 64-bit IDA (ida64)" ON)
if(IDA_EA64)
    set(IDA_SUFFIX "64")
    add_compile_definitions(__EA64__)
else()
    set(IDA_SUFFIX "")
endif()

# =============================================================================
# IDA SDK Paths
# =============================================================================

set(IDA_INCLUDE_DIR "${IDA_SDK_DIR}/include")

if(WIN32)
    set(IDA_LIB_DIR "${IDA_SDK_DIR}/lib/x64_win_vc_64")
elseif(APPLE)
    set(IDA_LIB_DIR "${IDA_SDK_DIR}/lib/${IDA_ARCH}_mac_clang_64")
else()
    set(IDA_LIB_DIR "${IDA_SDK_DIR}/lib/x64_linux_gcc_64")
endif()

# Find IDA library
find_library(IDA_LIB NAMES ida libida PATHS ${IDA_LIB_DIR} NO_DEFAULT_PATH)

if(NOT IDA_LIB)
    message(WARNING "IDA library not found at ${IDA_LIB_DIR}, proceeding anyway")
endif()

# =============================================================================
# Qt Configuration
# =============================================================================

# IDA bundles its own Qt and loads it at startup. Our plugin loads into IDA's
# process, so we get IDA's Qt symbols automatically. The challenge is:
# - We need Qt headers to compile
# - We must NOT link Qt libraries (to avoid symbol conflicts)
# - At runtime, IDA's Qt provides all symbols
#
# Strategy: Use system Qt for HEADERS ONLY, don't link Qt libraries.

set(QT_FOUND FALSE)
set(QT_HEADERS_ONLY FALSE)

# Auto-detect IDA installation
if(APPLE AND NOT IDA_INSTALL_DIR)
    foreach(IDA_PATH
        "/Applications/IDA Professional 9.3.app"
        "/Applications/IDA Pro 9.3.app"
        "/Applications/IDA Professional.app"
        "/Applications/IDA Pro.app"
        "$ENV{HOME}/Applications/IDA Professional 9.3.app"
    )
        if(EXISTS "${IDA_PATH}/Contents/Frameworks/QtCore.framework")
            set(IDA_INSTALL_DIR "${IDA_PATH}")
            message(STATUS "Auto-detected IDA at: ${IDA_INSTALL_DIR}")
            break()
        endif()
    endforeach()
endif()

# Find Qt for headers (we won't link against it)
find_package(Qt6 COMPONENTS Core Gui Widgets QUIET)
if(Qt6_FOUND)
    message(STATUS "Found Qt6 ${Qt6_VERSION} (using headers only, not linking)")
    
    # Get include directories from Qt targets
    get_target_property(QT_CORE_INCLUDES Qt6::Core INTERFACE_INCLUDE_DIRECTORIES)
    get_target_property(QT_GUI_INCLUDES Qt6::Gui INTERFACE_INCLUDE_DIRECTORIES)
    get_target_property(QT_WIDGETS_INCLUDES Qt6::Widgets INTERFACE_INCLUDE_DIRECTORIES)
    
    set(QT_INCLUDE_DIRS ${QT_CORE_INCLUDES} ${QT_GUI_INCLUDES} ${QT_WIDGETS_INCLUDES})
    set(QT_FOUND TRUE)
    set(QT_HEADERS_ONLY TRUE)
    
    # We need Qt's MOC definitions
    get_target_property(QT_CORE_DEFS Qt6::Core INTERFACE_COMPILE_DEFINITIONS)
    if(QT_CORE_DEFS)
        set(QT_COMPILE_DEFS ${QT_CORE_DEFS})
    endif()
else()
    find_package(Qt5 COMPONENTS Core Gui Widgets QUIET)
    if(Qt5_FOUND)
        message(STATUS "Found Qt5 ${Qt5_VERSION} (using headers only, not linking)")
        
        get_target_property(QT_CORE_INCLUDES Qt5::Core INTERFACE_INCLUDE_DIRECTORIES)
        get_target_property(QT_GUI_INCLUDES Qt5::Gui INTERFACE_INCLUDE_DIRECTORIES)
        get_target_property(QT_WIDGETS_INCLUDES Qt5::Widgets INTERFACE_INCLUDE_DIRECTORIES)
        
        set(QT_INCLUDE_DIRS ${QT_CORE_INCLUDES} ${QT_GUI_INCLUDES} ${QT_WIDGETS_INCLUDES})
        set(QT_FOUND TRUE)
        set(QT_HEADERS_ONLY TRUE)
    endif()
endif()

if(NOT QT_FOUND)
    message(WARNING "Qt not found - building without Qt support")
    message(STATUS "Hint: Install Qt6 via Homebrew: brew install qt")
endif()

# =============================================================================
# Compile Definitions
# =============================================================================

add_compile_definitions(
    __IDP__
    USE_STANDARD_FILE_FUNCTIONS
    USE_DANGEROUS_FUNCTIONS
)

if(WIN32)
    add_compile_definitions(__NT__)
elseif(APPLE)
    add_compile_definitions(__MAC__)
else()
    add_compile_definitions(__LINUX__)
endif()

# =============================================================================
# Compiler Flags
# =============================================================================

if(MSVC)
    add_compile_options(/W4 /WX- /permissive- /Zc:__cplusplus)
else()
    add_compile_options(
        -Wall -Wextra -Wpedantic
        -Wno-unused-parameter
        -Wno-sign-compare
        -fPIC
        -fvisibility=hidden
    )
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_compile_options(-g -O0)
    else()
        add_compile_options(-O3)
    endif()
endif()

# =============================================================================
# Source Files
# =============================================================================

set(SYNOPSIA_SOURCES
    src/entropy.cpp
    src/color.cpp
    src/minimap_data.cpp
    src/minimap_widget.cpp
    src/widget_bridge.cpp
    src/qt_compat.cpp
    src/plugin.cpp
)

set(SYNOPSIA_HEADERS
    include/synopsia/types.hpp
    include/synopsia/entropy.hpp
    include/synopsia/color.hpp
    include/synopsia/minimap_data.hpp
    include/synopsia/minimap_widget.hpp
    include/synopsia/plugin.hpp
)

# =============================================================================
# Plugin Target
# =============================================================================

add_library(synopsia${IDA_SUFFIX} SHARED ${SYNOPSIA_SOURCES} ${SYNOPSIA_HEADERS})

target_include_directories(synopsia${IDA_SUFFIX} PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${IDA_INCLUDE_DIR}
)

if(IDA_LIB)
    target_link_libraries(synopsia${IDA_SUFFIX} PRIVATE ${IDA_LIB})
endif()

if(QT_FOUND)
    # Add Qt include directories from system Qt (for headers)
    target_include_directories(synopsia${IDA_SUFFIX} SYSTEM PRIVATE
        ${QT_INCLUDE_DIRS}
    )
    
    # Add Qt compile definitions
    # CRITICAL: IDA uses a namespaced Qt build (QT_NAMESPACE=QT)
    # All Qt symbols are in the QT:: namespace, e.g., QT::QWidget instead of QWidget
    target_compile_definitions(synopsia${IDA_SUFFIX} PRIVATE 
        SYNOPSIA_USE_QT
        QT_NAMESPACE=QT
        QT_CORE_LIB
        QT_GUI_LIB
        QT_WIDGETS_LIB
        ${QT_COMPILE_DEFS}
    )
    
    if(APPLE AND IDA_INSTALL_DIR)
        # Link against IDA's Qt frameworks (not system Qt!)
        # We use Qt 6.8.x headers but IDA's Qt 6.8.x libraries for binary compatibility
        set(IDA_QT_DIR "${IDA_INSTALL_DIR}/Contents/Frameworks")
        
        # Link IDA's Qt as raw library paths (AFTER object files via LINK_LIBRARIES)
        # Using INTERFACE library to add linker flags without affecting compile
        add_library(ida_qt_link INTERFACE)
        target_link_options(ida_qt_link INTERFACE
            "LINKER:-rpath,${IDA_QT_DIR}"
        )
        target_link_libraries(ida_qt_link INTERFACE
            "${IDA_QT_DIR}/QtWidgets.framework/Versions/A/QtWidgets"
            "${IDA_QT_DIR}/QtGui.framework/Versions/A/QtGui"
            "${IDA_QT_DIR}/QtCore.framework/Versions/A/QtCore"
        )
        target_link_libraries(synopsia${IDA_SUFFIX} PRIVATE ida_qt_link)
        
        set_target_properties(synopsia${IDA_SUFFIX} PROPERTIES
            INSTALL_RPATH "@executable_path/../Frameworks"
        )
    else()
        # Non-macOS or no IDA dir: link system Qt (may have issues)
        target_link_libraries(synopsia${IDA_SUFFIX} PRIVATE ${QT_LIBS})
    endif()
endif()

# Set plugin output properties
set_target_properties(synopsia${IDA_SUFFIX} PROPERTIES
    OUTPUT_NAME "synopsia"
    SUFFIX ${PLUGIN_EXT}
    PREFIX ""
)

# =============================================================================
# Install Target
# =============================================================================

if(IDA_INSTALL_DIR)
    if(APPLE)
        install(TARGETS synopsia${IDA_SUFFIX}
            LIBRARY DESTINATION "${IDA_INSTALL_DIR}/Contents/MacOS/plugins"
        )
    else()
        install(TARGETS synopsia${IDA_SUFFIX}
            LIBRARY DESTINATION "${IDA_INSTALL_DIR}/plugins"
            RUNTIME DESTINATION "${IDA_INSTALL_DIR}/plugins"
        )
    endif()
endif()

# =============================================================================
# Configuration Summary
# =============================================================================

message(STATUS "")
message(STATUS "Synopsia Plugin Configuration:")
message(STATUS "  IDA SDK: ${IDA_SDK_DIR}")
message(STATUS "  Platform: ${IDA_PLATFORM}")
message(STATUS "  Architecture: ${IDA_ARCH}")
message(STATUS "  64-bit build: ${IDA_EA64}")
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  Qt found: ${QT_FOUND}")
message(STATUS "")
