include(CheckCXXSourceCompiles)
include(FeatureSummary)
include(ECMEnableSanitizers)
include(ECMPackageConfigHelpers)

set(ECM_MODULE_DIR ${CMAKE_CURRENT_LIST_DIR})

set(PROJECT_VERSION_MAJOR 6)
set(PROJECT_VERSION_MINOR 212)
set(PROJECT_VERSION_PATCH 0)
set(PROJECT_VERSION ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH})
set(PROJECT_VERSION_STRING "${PROJECT_VERSION}")

set(QT_CONAN_DIR "" CACHE PATH "Directory containing conanbuildinfo.cmake and conanfile.txt")
if (QT_CONAN_DIR)
    if (NOT QT_CONAN_FILE)
        set(QT_CONAN_FILE "${QT_CONAN_DIR}/conanfile.txt")
    endif ()
    message(STATUS "Using conan directory: ${QT_CONAN_DIR}")
    find_program(CONAN_COMMAND NAMES conan PATHS $ENV{PIP3_PATH})
    if (NOT CONAN_COMMAND)
        message(FATAL_ERROR "conan executable not found. Make sure that Conan is installed and available in PATH")
    endif ()
    include("${QT_CONAN_DIR}/conanbuildinfo.cmake")
    conan_basic_setup(TARGETS)

    install(CODE "
        set(_conan_imports_dest \${CMAKE_INSTALL_PREFIX})
        if (DEFINED ENV{DESTDIR})
            get_filename_component(_absolute_destdir \$ENV{DESTDIR} ABSOLUTE)
            string(REGEX REPLACE \"^[A-z]:\" \"\" _conan_imports_dest \${CMAKE_INSTALL_PREFIX})
            set(_conan_imports_dest \"\${_absolute_destdir}\${_conan_imports_dest}\")
        endif ()

        message(STATUS \"Importing dependencies from conan to \${_conan_imports_dest}\")
        execute_process(
            COMMAND \"${CONAN_COMMAND}\" imports --import-folder \${_conan_imports_dest} \"${QT_CONAN_FILE}\"
            WORKING_DIRECTORY \"${QT_CONAN_DIR}\"
            RESULT_VARIABLE _conan_imports_result
        )

        if (NOT _conan_imports_result EQUAL 0)
            message(FATAL_ERROR \"conan imports failed with code \${_conan_imports_result}\")
        else ()
            message(STATUS \"conan imports result: \${_conan_imports_result}\")
        endif ()

        set(_conan_imports_manifest \"\${_conan_imports_dest}/conan_imports_manifest.txt\")
        if (EXISTS \${_conan_imports_manifest})
            file(REMOVE \${_conan_imports_manifest})
            message(STATUS \"Removed conan install manifest: \${_conan_imports_manifest}\")
        endif ()
    ")
endif ()

set(STATIC_DEPENDENCIES_CMAKE_FILE "${CMAKE_BINARY_DIR}/QtStaticDependencies.cmake")
if (EXISTS ${STATIC_DEPENDENCIES_CMAKE_FILE})
    file(REMOVE ${STATIC_DEPENDENCIES_CMAKE_FILE})
endif ()

macro(CONVERT_PRL_LIBS_TO_CMAKE _qt_component)
    if (TARGET Qt6::${_qt_component})
        get_target_property(_lib_location Qt6::${_qt_component} LOCATION)
        execute_process(COMMAND ${PERL_EXECUTABLE} ${TOOLS_DIR}/qt/convert-prl-libs-to-cmake.pl
            --lib ${_lib_location}
            --out ${STATIC_DEPENDENCIES_CMAKE_FILE}
            --component ${_qt_component}
            --compiler ${CMAKE_CXX_COMPILER_ID}
        )
    endif ()
endmacro()

macro(CHECK_Qt6_PRIVATE_INCLUDE_DIRS _qt_component _header)
    set(INCLUDE_TEST_SOURCE
    "
        #include <${_header}>
        int main() { return 0; }
    "
    )
    set(CMAKE_REQUIRED_INCLUDES ${Qt6${_qt_component}_PRIVATE_INCLUDE_DIRS})
    set(CMAKE_REQUIRED_LIBRARIES Qt6::${_qt_component})

    # Avoid check_include_file_cxx() because it performs linking but doesn't support CMAKE_REQUIRED_LIBRARIES (doh!)
    check_cxx_source_compiles("${INCLUDE_TEST_SOURCE}" Qt6${_qt_component}_PRIVATE_HEADER_FOUND)

    unset(INCLUDE_TEST_SOURCE)
    unset(CMAKE_REQUIRED_INCLUDES)
    unset(CMAKE_REQUIRED_LIBRARIES)

    if (NOT Qt6${_qt_component}_PRIVATE_HEADER_FOUND)
        message(FATAL_ERROR "Header ${_header} is not found. Please make sure that:
    1. Private headers of Qt6${_qt_component} are installed
    2. Qt6${_qt_component}_PRIVATE_INCLUDE_DIRS is correctly defined in Qt6${_qt_component}Config.cmake")
    endif ()
endmacro()

macro(QT_ADD_EXTRA_WEBKIT_TARGET_EXPORT target)
    if (QT_STATIC_BUILD OR SHARED_CORE)
        install(TARGETS ${target} EXPORT WebKitTargets
            DESTINATION "${LIB_INSTALL_DIR}")
    endif ()
endmacro()

macro(QTWEBKIT_SKIP_AUTOMOC _target)
    foreach (_src ${${_target}_SOURCES})
        set_property(SOURCE ${_src} PROPERTY SKIP_AUTOMOC ON)
    endforeach ()
endmacro()

macro(QTWEBKIT_GENERATE_MOC_FILES_CPP _target)
    if (${ARGC} LESS 2)
        message(FATAL_ERROR "QTWEBKIT_GENERATE_MOC_FILES_CPP must be called with at least 2 arguments")
    endif ()
    foreach (_file ${ARGN})
        get_filename_component(_ext ${_file} EXT)
        if (NOT _ext STREQUAL ".cpp")
            message(FATAL_ERROR "QTWEBKIT_GENERATE_MOC_FILES_CPP must be used for .cpp files only")
        endif ()
        get_filename_component(_name_we ${_file} NAME_WE)
        set(_moc_name "${CMAKE_CURRENT_BINARY_DIR}/${_name_we}.moc")
        qt_generate_moc(${_file} ${_moc_name} TARGET ${_target})
        WEBKIT_ADD_SOURCE_DEPENDENCIES(${_file} ${_moc_name})
    endforeach ()
endmacro()

macro(QTWEBKIT_GENERATE_MOC_FILE_H _target _header _source)
    get_filename_component(_header_ext ${_header} EXT)
    get_filename_component(_source_ext ${_source} EXT)
    if ((NOT _header_ext STREQUAL ".h") OR (NOT _source_ext STREQUAL ".cpp"))
        message(FATAL_ERROR "QTWEBKIT_GENERATE_MOC_FILE_H must be called with arguments being .h and .cpp files")
    endif ()
    get_filename_component(_name_we ${_header} NAME_WE)
    set(_moc_name "${CMAKE_CURRENT_BINARY_DIR}/moc_${_name_we}.cpp")
    qt_generate_moc(${_header} ${_moc_name} TARGET ${_target})
    WEBKIT_ADD_SOURCE_DEPENDENCIES(${_source} ${_moc_name})
endmacro()

macro(QTWEBKIT_GENERATE_MOC_FILES_H _target)
    if (${ARGC} LESS 2)
        message(FATAL_ERROR "QTWEBKIT_GENERATE_MOC_FILES_H must be called with at least 2 arguments")
    endif ()
    foreach (_header ${ARGN})
        get_filename_component(_header_dir ${_header} DIRECTORY)
        get_filename_component(_name_we ${_header} NAME_WE)
        set(_source "${_header_dir}/${_name_we}.cpp")
        QTWEBKIT_GENERATE_MOC_FILE_H(${_target} ${_header} ${_source})
    endforeach ()
endmacro()

macro(QTWEBKIT_SEPARATE_DEBUG_INFO _target _target_debug)
    if (MINGW OR UNIX AND NOT APPLE) # Not using COMPILER_IS_GCC_OR_CLANG because other ELF compilers may work as well
        if (NOT CMAKE_OBJCOPY)
            message(WARNING "CMAKE_OBJCOPY is not defined - debug information will not be split")
        else ()
            set(_target_file "$<TARGET_FILE:${_target}>")
            set(${_target_debug} "${_target_file}.debug")

            if (DWZ_FOUND AND NOT SKIP_DWZ)
                set(EXTRACT_DEBUG_INFO_COMMAND COMMAND ${DWZ_EXECUTABLE} -L 1000000000 -o ${${_target_debug}} ${_target_file})
            else ()
                set(EXTRACT_DEBUG_INFO_COMMAND COMMAND ${CMAKE_OBJCOPY} --only-keep-debug ${_target_file} ${${_target_debug}})
            endif ()

            add_custom_command(TARGET ${_target} POST_BUILD
                ${EXTRACT_DEBUG_INFO_COMMAND}
                COMMAND ${CMAKE_OBJCOPY} --strip-debug ${_target_file}
                COMMAND ${CMAKE_OBJCOPY} --add-gnu-debuglink=${${_target_debug}} ${_target_file}
                VERBATIM
            )
            unset(_target_file)
        endif ()
    endif ()
endmacro()

if (APPLE)
    set(CMAKE_MACOSX_RPATH ON)
    set(CMAKE_FIND_FRAMEWORK LAST)
endif ()

add_definitions(-DBUILDING_QT__=1)
add_definitions(-DQT_NO_EXCEPTIONS)
add_definitions(-DQT_USE_QSTRINGBUILDER)
add_definitions(-DQT_NO_CAST_TO_ASCII -DQT_ASCII_CAST_WARNINGS)
add_definitions(-DQT_DEPRECATED_WARNINGS -DQT_DISABLE_DEPRECATED_BEFORE=0x050000)
add_definitions(-DQT_NO_NARROWING_CONVERSIONS_IN_CONNECT)
add_definitions(-DQT_NO_KEYWORDS)
add_definitions(-DQT_NO_BEARERMANAGEMENT)

# We use -fno-rtti with GCC and Clang, see OptionsCommon.cmake
if (COMPILER_IS_GCC_OR_CLANG)
    add_definitions(-DQT_NO_DYNAMIC_CAST)
endif ()

# Align build product names with QMake conventions
if (WIN32)
    if (${CMAKE_BUILD_TYPE} MATCHES "Debug")
        set(CMAKE_DEBUG_POSTFIX d)
    endif ()

    set(CMAKE_SHARED_LIBRARY_PREFIX "")
    set(CMAKE_SHARED_MODULE_PREFIX "")
    # QMake doesn't treat import libraries as a separate product kind
    set(CMAKE_IMPORT_LIBRARY_SUFFIX "${CMAKE_STATIC_LIBRARY_SUFFIX}")
endif ()

WEBKIT_OPTION_BEGIN()

if (APPLE)
    set(MACOS_COMPATIBILITY_VERSION "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}" CACHE STRING "Compatibility version that macOS dylibs should have")

    option(MACOS_FORCE_SYSTEM_XML_LIBRARIES "Use system installation of libxml2 and libxslt on macOS" ON)
    option(MACOS_USE_SYSTEM_ICU "Use system installation of ICU on macOS" ON)
    option(USE_UNIX_DOMAIN_SOCKETS "Use Unix domain sockets instead of native IPC code on macOS" OFF)
    option(USE_APPSTORE_COMPLIANT_CODE "Avoid using private macOS APIs which are not allowed on App Store (experimental)" OFF)
    option(MACOS_BUILD_FRAMEWORKS "Build QtWebKit as framework bundles" ON)

    if (USE_APPSTORE_COMPLIANT_CODE)
        set(MACOS_USE_SYSTEM_ICU OFF)
        set(USE_UNIX_DOMAIN_SOCKETS ON)
    endif ()
endif ()

if (WIN32 OR APPLE)
    set(USE_LIBHYPHEN_DEFAULT OFF)
    set(USE_GSTREAMER_DEFAULT OFF)
    set(USE_QT_MULTIMEDIA_DEFAULT ON)
else ()
    set(USE_LIBHYPHEN_DEFAULT ON)
    set(USE_GSTREAMER_DEFAULT OFF)
    set(USE_QT_MULTIMEDIA_DEFAULT OFF)
endif ()

if (MSVC)
    set(USE_QT_MULTIMEDIA_DEFAULT OFF)
    set(USE_MEDIA_FOUNDATION_DEFAULT ON)
else ()
    set(USE_MEDIA_FOUNDATION_DEFAULT OFF)
endif ()

if (WIN32)
    set(ENABLE_FTL_DEFAULT OFF)
endif ()

# FIXME: Move Qt handling here
set(REQUIRED_QT_VERSION 6.0.0)
find_package(Qt6 ${REQUIRED_QT_VERSION} REQUIRED COMPONENTS Core Gui)

get_target_property(QT_CORE_TYPE Qt6::Core TYPE)
if (QT_CORE_TYPE MATCHES STATIC)
    set(QT_STATIC_BUILD ON)
    set(SHARED_CORE OFF)
    set(MACOS_BUILD_FRAMEWORKS OFF)
endif ()

# static icu libraries on windows are build with 's' prefix
if (QT_STATIC_BUILD AND MSVC)
    set(ICU_LIBRARY_PREFIX "s")
else ()
    set(ICU_LIBRARY_PREFIX "")
endif ()

# QtWebKit only supports WebKitLegacy (WK1) at this time
set(ENABLE_WEBKIT_LEGACY ON)
set(ENABLE_WEBKIT OFF)

if (UNIX AND TARGET Qt6::QXcbIntegrationPlugin AND NOT APPLE)
    set(ENABLE_X11_TARGET_DEFAULT ON)
else ()
    set(ENABLE_X11_TARGET_DEFAULT OFF)
endif ()

# Public options specific to the Qt port. Do not add any options here unless
# there is a strong reason we should support changing the value of the option,
# and the option is not relevant to any other WebKit ports.
WEBKIT_OPTION_DEFINE(USE_GSTREAMER "Use GStreamer implementation of MediaPlayer" PUBLIC ${USE_GSTREAMER_DEFAULT})
WEBKIT_OPTION_DEFINE(USE_LIBHYPHEN "Use automatic hyphenation with LibHyphen" PUBLIC ${USE_LIBHYPHEN_DEFAULT})
WEBKIT_OPTION_DEFINE(USE_MEDIA_FOUNDATION "Use MediaFoundation implementation of MediaPlayer" PUBLIC OFF) # QTFIXME #${USE_MEDIA_FOUNDATION_DEFAULT})
WEBKIT_OPTION_DEFINE(USE_QT_MULTIMEDIA "Use Qt Multimedia implementation of MediaPlayer" PUBLIC OFF) # QTFIXME #${USE_QT_MULTIMEDIA_DEFAULT})
WEBKIT_OPTION_DEFINE(ENABLE_INSPECTOR_UI "Include Inspector UI into resources" PUBLIC ON)
WEBKIT_OPTION_DEFINE(ENABLE_OPENGL "Whether to use OpenGL." PUBLIC OFF) # QTFIXME
WEBKIT_OPTION_DEFINE(ENABLE_PRINT_SUPPORT "Enable support for printing web pages" PUBLIC OFF) # QTFIXME
WEBKIT_OPTION_DEFINE(ENABLE_QT_GESTURE_EVENTS "Enable support for gesture events (required for mouse in WK2)" PUBLIC OFF) # QTFIXME
WEBKIT_OPTION_DEFINE(ENABLE_QT_WEBCHANNEL "Enable support for Qt WebChannel" PUBLIC OFF) # QTFIXME
WEBKIT_OPTION_DEFINE(ENABLE_X11_TARGET "Whether to enable support for the X11 windowing target." PUBLIC OFF) # QTFIXME #${ENABLE_X11_TARGET_DEFAULT})
WEBKIT_OPTION_DEFINE(USE_SYSPROF_CAPTURE "Whether to use libsysprof-capture for tracing." PRIVATE OFF)
WEBKIT_OPTION_DEFINE(USE_SYSTEM_SYSPROF_CAPTURE "Whether to use a system-provided libsysprof-capture" PUBLIC OFF)
WEBKIT_OPTION_DEPEND(USE_SYSTEM_SYSPROF_CAPTURE USE_SYSPROF_CAPTURE)
WEBKIT_OPTION_DEFINE(ENABLE_JSC_RESTRICTED_OPTIONS_BY_DEFAULT "Whether to enable dangerous development options in JSC by default." PRIVATE OFF)

option(GENERATE_DOCUMENTATION "Generate HTML and QCH documentation" OFF)
cmake_dependent_option(ENABLE_TEST_SUPPORT "Build tools for running layout tests and related library code" OFF
                                           "DEVELOPER_MODE" OFF)
option(USE_STATIC_RUNTIME "Use static runtime (MSVC only)" OFF)
option(ENABLE_PCH "Use pre-compiled headers (MSVC only)" ON)

# Private options specific to the Qt port. Changing these options is
# completely unsupported. They are intended for use only by WebKit developers.
WEBKIT_OPTION_DEFINE(ENABLE_TOUCH_ADJUSTMENT "Whether to use touch adjustment" PRIVATE OFF) # QTFIXME #ON)
WEBKIT_OPTION_DEFINE(USE_LIBJPEG "Support JPEG format directly. If it is disabled, QImageReader will be used with possible degradation of user experience" PUBLIC ON)


# Public options shared with other WebKit ports. There must be strong reason
# to support changing the value of the option.
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_API_TESTS PUBLIC OFF)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_DEVICE_ORIENTATION PUBLIC OFF) # QTFIXME
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_MEDIA_SOURCE PUBLIC OFF) #QTFIXME ${USE_GSTREAMER_DEFAULT})
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_XSLT PUBLIC ON)

WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_DRAG_SUPPORT PUBLIC ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_GEOLOCATION PUBLIC ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_SPELLCHECK PUBLIC ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_TOUCH_EVENTS PUBLIC OFF) # QTFIXME
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_VIDEO PUBLIC ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_WEB_AUDIO PUBLIC ${USE_GSTREAMER_DEFAULT})
WEBKIT_OPTION_DEFAULT_PORT_VALUE(USE_SYSTEM_MALLOC PUBLIC OFF)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(USE_AVIF PRIVATE OFF)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(USE_LCMS PRIVATE OFF)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(USE_JPEGXL PUBLIC OFF)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(USE_WOFF2 PUBLIC ON)

# Private options shared with other WebKit ports. Add options here when
# we need a value different from the default defined in WebKitFeatures.cmake.
# Changing these options is completely unsupported.
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_FTL_JIT PRIVATE ${ENABLE_FTL_DEFAULT})
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_FTPDIR PRIVATE OFF)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_MHTML PRIVATE ON)

# QTFIXME: No remote inspector support for now
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_REMOTE_INSPECTOR PRIVATE OFF)

if (MINGW AND CMAKE_SIZEOF_VOID_P EQUAL 8)
    WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_JIT PRIVATE OFF)
endif ()

WEBKIT_OPTION_CONFLICT(USE_GSTREAMER USE_QT_MULTIMEDIA)
WEBKIT_OPTION_CONFLICT(USE_GSTREAMER USE_MEDIA_FOUNDATION)
WEBKIT_OPTION_CONFLICT(USE_QT_MULTIMEDIA USE_MEDIA_FOUNDATION)

WEBKIT_OPTION_DEPEND(ENABLE_WEBGL ENABLE_OPENGL)

# WebAudio and MediaSource are supported with GStreamer only
WEBKIT_OPTION_DEPEND(ENABLE_WEB_AUDIO USE_GSTREAMER)
WEBKIT_OPTION_DEPEND(ENABLE_MEDIA_SOURCE USE_GSTREAMER)

#WEBKIT_OPTION_DEPEND(ENABLE_QT_WEBCHANNEL ENABLE_WEBKIT)

WEBKIT_OPTION_DEPEND(ENABLE_TOUCH_ADJUSTMENT ENABLE_QT_GESTURE_EVENTS)

WEBKIT_OPTION_END()

# FTL JIT and IndexedDB support require GCC 4.9
# TODO: Patch code to avoid variadic lambdas
if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    if (ENABLE_FTL_JIT OR (ENABLE_WEBKIT AND ENABLE_DATABASE_PROCESS))
        if (CMAKE_CXX_COMPILER_VERSION VERSION_LESS "4.9.0")
            message(FATAL_ERROR "GCC 4.9.0 is required to build QtWebKit with FTL JIT, Indexed Database, and Database Process (WebKit 2). Use a newer GCC version or clang, or disable these features")
        endif ()
    else ()
        if (CMAKE_CXX_COMPILER_VERSION VERSION_LESS "4.8.0")
            message(FATAL_ERROR "GCC 4.8.0 is required to build QtWebKit, use a newer GCC version or clang")
        endif ()
    endif ()
elseif (MSVC AND MSVC_VERSION LESS 1900)
    message(FATAL_ERROR "MSVC 2015 is required to build QtWebKit, use a newer MSVC version")
endif ()

if (APPLE AND CMAKE_SYSTEM_VERSION VERSION_LESS 14.0.0)
    message(FATAL_ERROR "macOS 10.10 or higher is required to build and run QtWebKit")
endif ()

if (WIN32)
    set(ENABLE_WEBKIT OFF)
endif ()

if (SHARED_CORE)
    set(WebCoreTestSupport_LIBRARY_TYPE SHARED)
else ()
    set(JavaScriptCore_LIBRARY_TYPE STATIC)
    set(WebCoreTestSupport_LIBRARY_TYPE STATIC)
endif ()

SET_AND_EXPOSE_TO_BUILD(USE_TEXTURE_MAPPER TRUE)

if (WIN32)
    # bmalloc is not ported to Windows yet
    set(USE_SYSTEM_MALLOC 1)
endif ()

if (DEFINED ENV{SQLITE3SRCDIR})
    get_filename_component(SQLITE3SRC_ABS_DIR $ENV{SQLITE3SRCDIR} ABSOLUTE)
    set(SQLITE3_SOURCE_DIR ${SQLITE3SRC_ABS_DIR} CACHE PATH "Path to SQLite sources to use instead of system library" FORCE)
endif ()

if (SQLITE3_SOURCE_DIR)
    set(SQLITE_INCLUDE_DIR ${SQLITE3_SOURCE_DIR})
    set(SQLITE_SOURCE_FILE ${SQLITE3_SOURCE_DIR}/sqlite3.c)
    if (NOT EXISTS ${SQLITE_SOURCE_FILE})
        message(FATAL_ERROR "${SQLITE_SOURCE_FILE} not found.")
    endif ()
    add_library(qtsqlite STATIC ${SQLITE_SOURCE_FILE})
    target_compile_definitions(qtsqlite PUBLIC -DSQLITE_CORE -DSQLITE_OMIT_LOAD_EXTENSION -DSQLITE_OMIT_COMPLETE)
    QT_ADD_EXTRA_WEBKIT_TARGET_EXPORT(qtsqlite)
    set(SQLITE_LIBRARIES qtsqlite)
    set(SQLITE_FOUND 1)
else ()
    find_package(SQLite3 REQUIRED)
endif ()

find_package(Freetype 2.4.2 REQUIRED)
find_package(HarfBuzz 0.9.18 REQUIRED COMPONENTS ICU)
find_package(Threads REQUIRED)

if (USE_LIBJPEG)
    find_package(JPEG)
    if (NOT JPEG_FOUND)
        message(FATAL_ERROR "libjpeg not found. Please make sure that CMake can find its header files and libraries, or build with -DUSE_LIBJPEG=OFF with possible degradation of user experience")
    endif ()
else ()
    message(WARNING "USE_LIBJPEG is disabled, will attempt using QImageReader to decode JPEG with possible degradation of user experience")
endif ()

if (NOT QT_BUNDLED_PNG)
    find_package(PNG REQUIRED)
else ()
    set(PNG_FOUND 1)
endif ()

if (NOT QT_BUNDLED_ZLIB)
    find_package(ZLIB REQUIRED)
else ()
    set(ZLIB_FOUND 1)
endif ()

if (USE_APPLE_ICU)
    add_definitions(-DU_DISABLE_RENAMING=1 -DU_SHOW_CPLUSPLUS_API=0)
endif ()

find_package(ICU 61.2 REQUIRED COMPONENTS data i18n uc)

if (APPLE)
    find_library(COREFOUNDATION_LIBRARY CoreFoundation)
    find_library(FOUNDATION_LIBRARY Foundation)
    if (QT_STATIC_BUILD)
        find_library(CARBON_LIBRARY Carbon)
        find_library(COCOA_LIBRARY Cocoa)
        find_library(SYSTEMCONFIGURATION_LIBRARY SystemConfiguration)
        find_library(CORESERVICES_LIBRARY CoreServices)
        find_library(SECURITY_LIBRARY Security)
    endif ()
endif ()

find_package(LibXml2 2.8.0 REQUIRED)
if (ENABLE_XSLT)
    find_package(LibXslt 1.1.13 REQUIRED)
endif ()

if (UNIX AND NOT APPLE AND CMAKE_OBJCOPY AND NOT SKIP_DWZ)
    find_package(Dwz 0.13)
    if (DWZ_FOUND)
        message(STATUS "WARNING: dwz may use a lot of RAM - build with -DSKIP_DWZ=ON if you don't have enough")
    endif ()
endif ()

if (ENABLE_TEST_SUPPORT)
    find_package(Fontconfig)
    if (FONTCONFIG_FOUND)
        SET_AND_EXPOSE_TO_BUILD(HAVE_FONTCONFIG 1)
    endif ()
endif ()

find_package(WebP REQUIRED COMPONENTS demux)

if (WEBP_FOUND)
    SET_AND_EXPOSE_TO_BUILD(USE_WEBP 1)
endif ()

set(QT_REQUIRED_COMPONENTS Core Gui Network)

# FIXME: Allow building w/o these components
list(APPEND QT_REQUIRED_COMPONENTS
    Widgets
)
set(QT_OPTIONAL_COMPONENTS OpenGL)

if (ENABLE_API_TESTS OR ENABLE_TEST_SUPPORT)
    list(APPEND QT_REQUIRED_COMPONENTS
        Test
    )
    if (ENABLE_WEBKIT)
        list(APPEND QT_REQUIRED_COMPONENTS
            QuickTest
        )
    endif ()
endif ()

if (ENABLE_WEBKIT_LEGACY)
    # Without StateMachine, some animations will be disabled
    list(APPEND QT_OPTIONAL_COMPONENTS
        StateMachine
    )
endif ()

if (ENABLE_GEOLOCATION)
    list(APPEND QT_REQUIRED_COMPONENTS Positioning)
    SET_AND_EXPOSE_TO_BUILD(HAVE_QTPOSITIONING 1)
endif ()

if (ENABLE_DEVICE_ORIENTATION)
    list(APPEND QT_REQUIRED_COMPONENTS Sensors)
    SET_AND_EXPOSE_TO_BUILD(HAVE_QTSENSORS 1)
endif ()

if (ENABLE_OPENGL)
    # Qt6 : QOpenGLWidget is now in its separate library
    list(APPEND QT_REQUIRED_COMPONENTS OpenGLWidgets)

    # Note: Gui module is already found
    # Warning: quotes are sinificant here!
    if (NOT DEFINED Qt6Gui_OPENGL_IMPLEMENTATION OR "${Qt6Gui_OPENGL_IMPLEMENTATION}" STREQUAL "")
       message(FATAL_ERROR "Qt with OpenGL support is required for ENABLE_OPENGL")
    endif ()

    SET_AND_EXPOSE_TO_BUILD(USE_TEXTURE_MAPPER_GL TRUE)
    SET_AND_EXPOSE_TO_BUILD(ENABLE_GRAPHICS_CONTEXT_3D TRUE)

    if (WIN32)
        include(CheckCXXSymbolExists)
        set(CMAKE_REQUIRED_INCLUDES ${Qt6Gui_INCLUDE_DIRS})
        set(CMAKE_REQUIRED_FLAGS ${Qt6Gui_EXECUTABLE_COMPILE_FLAGS})
        check_cxx_symbol_exists(QT_OPENGL_DYNAMIC qopenglcontext.h HAVE_QT_OPENGL_DYNAMIC)
        if (HAVE_QT_OPENGL_DYNAMIC)
            set(Qt6Gui_OPENGL_IMPLEMENTATION DynamicGL)
        endif ()
        unset(CMAKE_REQUIRED_INCLUDES)
        unset(CMAKE_REQUIRED_FLAGS)
    endif ()

    message(STATUS "Qt OpenGL implementation: ${Qt6Gui_OPENGL_IMPLEMENTATION}")
    message(STATUS "Qt OpenGL libraries: ${Qt6Gui_OPENGL_LIBRARIES}")
    message(STATUS "Qt EGL libraries: ${Qt6Gui_EGL_LIBRARIES}")
else ()
    add_definitions(-DQT_NO_OPENGL)
endif ()

if (ENABLE_PRINT_SUPPORT)
    list(APPEND QT_REQUIRED_COMPONENTS PrintSupport)
    SET_AND_EXPOSE_TO_BUILD(HAVE_QTPRINTSUPPORT 1)
endif ()

if (ENABLE_WEBKIT)
    list(APPEND QT_REQUIRED_COMPONENTS
        Quick
    )
    SET_AND_EXPOSE_TO_BUILD(USE_COORDINATED_GRAPHICS 0)
endif ()

# Mach ports and Unix sockets are currently used by WK2, but their USE() values
# affect building WorkQueue
if (APPLE AND NOT USE_UNIX_DOMAIN_SOCKETS)
    SET_AND_EXPOSE_TO_BUILD(USE_MACH_PORTS 1) # Qt-specific
elseif (UNIX)
    SET_AND_EXPOSE_TO_BUILD(USE_UNIX_DOMAIN_SOCKETS 1)
endif ()

if (ENABLE_QT_WEBCHANNEL)
    list(APPEND QT_REQUIRED_COMPONENTS
        WebChannel
    )
endif ()

if (SHARED_CORE)
    set(WebKit_LIBRARY_TYPE SHARED)
else ()
    set(WebKit_LIBRARY_TYPE STATIC)
endif ()

find_package(Qt6 ${REQUIRED_QT_VERSION} REQUIRED COMPONENTS ${QT_REQUIRED_COMPONENTS})

CHECK_Qt6_PRIVATE_INCLUDE_DIRS(Gui private/qhexstring_p.h)
if (Qt6_VERSION VERSION_GREATER 5.10.1)
    CHECK_Qt6_PRIVATE_INCLUDE_DIRS(Network private/http2protocol_p.h)
endif ()
if (ENABLE_WEBKIT)
    CHECK_Qt6_PRIVATE_INCLUDE_DIRS(Quick private/qsgrendernode_p.h)
endif ()

if (QT_STATIC_BUILD)
    foreach (qt_module ${QT_REQUIRED_COMPONENTS})
        CONVERT_PRL_LIBS_TO_CMAKE(${qt_module})
    endforeach ()
    # HACK: We must explicitly add LIB path of the Qt installation
    # to correctly find qtpcre
    link_directories(${_Qt6_install_prefix}/../)
endif ()

foreach (qt_module ${QT_OPTIONAL_COMPONENTS})
    find_package("Qt6${qt_module}" ${REQUIRED_QT_VERSION} PATHS ${_Qt6_install_prefix} NO_DEFAULT_PATH)
    if (QT_STATIC_BUILD)
        CONVERT_PRL_LIBS_TO_CMAKE(${qt_module})
    endif ()
endforeach ()

if (QT_STATIC_BUILD)
    if (NOT EXISTS ${STATIC_DEPENDENCIES_CMAKE_FILE})
        message(FATAL_ERROR "Unable to find ${STATIC_DEPENDENCIES_CMAKE_FILE}")
    endif ()
    include(${STATIC_DEPENDENCIES_CMAKE_FILE})
    list(REMOVE_DUPLICATES STATIC_LIB_DEPENDENCIES)
endif ()

if (COMPILER_IS_GCC_OR_CLANG AND UNIX)
    if (APPLE OR CMAKE_SYSTEM_NAME MATCHES "Android" OR ${Qt6_VERSION} VERSION_LESS 5.6)
        set(USE_LINKER_VERSION_SCRIPT_DEFAULT OFF)
    else ()
        set(USE_LINKER_VERSION_SCRIPT_DEFAULT ON)
    endif ()
else ()
    set(USE_LINKER_VERSION_SCRIPT_DEFAULT OFF)
endif ()

option(USE_LINKER_VERSION_SCRIPT "Use linker script for ABI compatibility with Qt libraries" ${USE_LINKER_VERSION_SCRIPT_DEFAULT})

if (USE_LINKER_VERSION_SCRIPT AND COMPILER_IS_CLANG AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER 14.0)
    # clang 15+ is strict with version symbols; set back to old behavior
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--undefined-version")
endif ()

# Find includes in corresponding build directories
set(CMAKE_INCLUDE_CURRENT_DIR ON)

# TODO: figure out if we can run automoc only on Qt sources

# From OptionsEfl.cmake
# Optimize binary size for release builds by removing dead sections on unix/gcc.
if (COMPILER_IS_GCC_OR_CLANG AND UNIX)
    if (NOT APPLE)
        set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -ffunction-sections -fdata-sections")
        set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -ffunction-sections -fdata-sections")
        set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} -Wl,--gc-sections")
    endif ()

    if (NOT SHARED_CORE)
        set(CMAKE_C_FLAGS "-fvisibility=hidden ${CMAKE_C_FLAGS}")
        set(CMAKE_CXX_FLAGS "-fvisibility=hidden -fvisibility-inlines-hidden ${CMAKE_CXX_FLAGS}")
    endif ()
endif ()

if (WIN32 AND COMPILER_IS_GCC_OR_CLANG)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-keep-inline-dllexport")
endif ()

# See also FORCE_DEBUG_INFO in Source/PlatformQt.cmake
if (FORCE_DEBUG_INFO)
    if (COMPILER_IS_GCC_OR_CLANG)
        # Enable debug info in Release builds
        set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -g")
        set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -g")
    endif ()
    if (USE_LD_GOLD)
       set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--gdb-index")
       set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--gdb-index")
    endif ()

    if (MSVC AND CMAKE_SIZEOF_VOID_P EQUAL 8)
        # Create pdb files for debugging purposes, also for Release builds
        set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} /Zi")
        set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /Zi")
        set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS} /DEBUG")
        set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS} /DEBUG")
    endif ()
endif ()

if (APPLE)
    SET_AND_EXPOSE_TO_BUILD(HAVE_QOS_CLASSES 1)
endif ()

if (ENABLE_MATHML)
    SET_AND_EXPOSE_TO_BUILD(ENABLE_OPENTYPE_MATH 1)
endif ()

SET_AND_EXPOSE_TO_BUILD(WTF_PLATFORM_X11 ${ENABLE_X11_TARGET})

if (ENABLE_X11_TARGET)
    find_package(X11 REQUIRED)
    if (NOT X11_Xcomposite_FOUND)
        message(FATAL_ERROR "libXcomposite is required for ENABLE_X11_TARGET")
    elseif (NOT X11_Xrender_FOUND)
        message(FATAL_ERROR "libXrender is required for ENABLE_X11_TARGET")
    endif ()
endif ()

if (NOT ENABLE_VIDEO)
    set(USE_MEDIA_FOUNDATION OFF)
    set(USE_QT_MULTIMEDIA OFF)

    if (NOT ENABLE_WEB_AUDIO)
        set(USE_GSTREAMER OFF) # TODO: What about MEDIA_STREAM?
    endif ()
endif ()

if (USE_QT_MULTIMEDIA)
    find_package(Qt6Multimedia ${REQUIRED_QT_VERSION} REQUIRED PATHS ${_Qt6_install_prefix} NO_DEFAULT_PATH)
    # FIXME: Allow building w/o widgets
    find_package(Qt6MultimediaWidgets ${REQUIRED_QT_VERSION} REQUIRED PATHS ${_Qt6_install_prefix} NO_DEFAULT_PATH)
endif ()

if (APPLE)
    set(USE_COMMONCRYPTO TRUE)
else ()
    if (TARGET CONAN_PKG::libtasn1)
        set(LIBTASN1_LIBRARIES CONAN_PKG::libtasn1)
    else ()
        find_package(Libtasn1 REQUIRED)
    endif ()
    if (TARGET CONAN_PKG::libgcrypt)
        set(LIBGCRYPT_LIBRARIES CONAN_PKG::libgcrypt)
    else ()
        find_package(LibGcrypt 1.7.0 REQUIRED)
    endif ()
    SET_AND_EXPOSE_TO_BUILD(USE_GCRYPT TRUE)
endif ()

if (USE_GSTREAMER)
    SET_AND_EXPOSE_TO_BUILD(USE_GLIB 1)
    find_package(GLIB 2.36 REQUIRED COMPONENTS gio gobject)
    include(GStreamerChecks)
endif ()

if (USE_LIBHYPHEN)
    find_package(Hyphen REQUIRED)
    if (NOT HYPHEN_FOUND)
       message(FATAL_ERROR "libhyphen is needed for USE_LIBHYPHEN.")
    endif ()
endif ()

if (USE_JPEGXL)
    find_package(JPEGXL)
    if (NOT JPEGXL_FOUND)
        message(FATAL_ERROR "libjxl is required for USE_JPEGXL")
    endif ()
endif ()

if (USE_WOFF2)
    if (TARGET CONAN_PKG::woff2)
        set(WOFF2DEC_LIBRARIES CONAN_PKG::woff2)
    else ()
	find_package(WOFF2 1.0.2 COMPONENTS dec)
        if (NOT WOFF2_FOUND)
            message(FATAL_ERROR "libwoff2dec is needed for USE_WOFF2.")
        endif ()
    endif ()
endif ()

# From OptionsGTK.cmake
if (CMAKE_MAJOR_VERSION LESS 3)
    # Before CMake 3 it was necessary to use a build script instead of using cmake --build directly
    # to preserve colors and pretty-printing.

    build_command(COMMAND_LINE_TO_BUILD)
    # build_command unconditionally adds -i (ignore errors) for make, and there's
    # no reasonable way to turn that off, so we just replace it with -k, which has
    # the same effect, except that the return code will indicate that an error occurred.
    # See: http://www.cmake.org/cmake/help/v3.0/command/build_command.html
    string(REPLACE " -i" " -k" COMMAND_LINE_TO_BUILD ${COMMAND_LINE_TO_BUILD})
    file(WRITE
        ${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/build.sh
        "#!/bin/sh\n"
        "${COMMAND_LINE_TO_BUILD} $@"
    )
    file(COPY ${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/build.sh
        DESTINATION ${CMAKE_BINARY_DIR}
        FILE_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE
    )
endif ()

# You can build JavaScriptCore as a static library if you specify it as STATIC
# set(JavaScriptCore_LIBRARY_TYPE STATIC)

# From OptionsWin.cmake
if (WIN32)
    add_definitions(-DNOMINMAX -DUNICODE -D_UNICODE -D_WINDOWS)

    # If <winsock2.h> is not included before <windows.h> redefinition errors occur
    # unless _WINSOCKAPI_ is defined before <windows.h> is included
    add_definitions(-D_WINSOCKAPI_=)
endif ()

if (MSVC)
    add_definitions(-DWINVER=0x601)

    add_definitions(
        /wd4018 /wd4068 /wd4099 /wd4100 /wd4127 /wd4138 /wd4146 /wd4180 /wd4189
        /wd4201 /wd4244 /wd4251 /wd4267 /wd4275 /wd4288 /wd4291 /wd4305 /wd4309
        /wd4344 /wd4355 /wd4389 /wd4396 /wd4456 /wd4457 /wd4458 /wd4459 /wd4481
        /wd4503 /wd4505 /wd4510 /wd4512 /wd4530 /wd4577 /wd4610 /wd4611 /wd4702
        /wd4706 /wd4800 /wd4819 /wd4951 /wd4952 /wd4996 /wd6011 /wd6031 /wd6211
        /wd6246 /wd6255 /wd6387
    )

    add_compile_options(/GS)

    # We do not use exceptions
    add_definitions(-D_HAS_EXCEPTIONS=0)
    add_compile_options(/EHa- /EHc- /EHs- /fp:except-)

    # We have some very large object files that have to be linked
    add_compile_options(/analyze- /bigobj)

    # Use CRT security features
    add_definitions(-D_CRT_SECURE_NO_WARNINGS -D_CRT_SECURE_CPP_OVERLOAD_STANDARD_NAMES=1)

    # Turn off certain link features
    add_compile_options(/Gy- /openmp- /GF-)

    # Turn off some linker warnings
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /ignore:4049 /ignore:4217")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /ignore:4049 /ignore:4217")

    # Make sure incremental linking is turned off, as it creates unacceptably long link times.
    string(REPLACE "INCREMENTAL:YES" "INCREMENTAL:NO" replace_CMAKE_SHARED_LINKER_FLAGS ${CMAKE_SHARED_LINKER_FLAGS})
    set(CMAKE_SHARED_LINKER_FLAGS "${replace_CMAKE_SHARED_LINKER_FLAGS} /INCREMENTAL:NO")
    string(REPLACE "INCREMENTAL:YES" "INCREMENTAL:NO" replace_CMAKE_EXE_LINKER_FLAGS ${CMAKE_EXE_LINKER_FLAGS})
    set(CMAKE_EXE_LINKER_FLAGS "${replace_CMAKE_EXE_LINKER_FLAGS} /INCREMENTAL:NO")

    string(REPLACE "INCREMENTAL:YES" "INCREMENTAL:NO" replace_CMAKE_SHARED_LINKER_FLAGS_DEBUG ${CMAKE_SHARED_LINKER_FLAGS_DEBUG})
    set(CMAKE_SHARED_LINKER_FLAGS_DEBUG "${replace_CMAKE_SHARED_LINKER_FLAGS_DEBUG} /INCREMENTAL:NO")
    string(REPLACE "INCREMENTAL:YES" "INCREMENTAL:NO" replace_CMAKE_EXE_LINKER_FLAGS_DEBUG ${CMAKE_EXE_LINKER_FLAGS_DEBUG})
    set(CMAKE_EXE_LINKER_FLAGS_DEBUG "${replace_CMAKE_EXE_LINKER_FLAGS_DEBUG} /INCREMENTAL:NO")

    string(REPLACE "INCREMENTAL:YES" "INCREMENTAL:NO" replace_CMAKE_SHARED_LINKER_FLAGS_RELWITHDEBINFO ${CMAKE_SHARED_LINKER_FLAGS_RELWITHDEBINFO})
    set(CMAKE_SHARED_LINKER_FLAGS_RELWITHDEBINFO "${replace_CMAKE_SHARED_LINKER_FLAGS_RELWITHDEBINFO} /INCREMENTAL:NO")
    string(REPLACE "INCREMENTAL:YES" "INCREMENTAL:NO" replace_CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO ${CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO})
    set(CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO "${replace_CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO} /INCREMENTAL:NO")

    if (${CMAKE_BUILD_TYPE} MATCHES "Debug")
        set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /OPT:NOREF /OPT:NOICF")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /OPT:NOREF /OPT:NOICF")

        # To debug linking time issues, uncomment the following three lines:
        #add_compile_options(/Bv)
        #set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /VERBOSE /VERBOSE:INCR /TIME")
        #set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /VERBOSE /VERBOSE:INCR /TIME")

        # enable fast link
        set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /DEBUG:FASTLINK")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /DEBUG:FASTLINK")
    elseif (${CMAKE_BUILD_TYPE} MATCHES "Release")
        add_compile_options(/Oy-)
    endif ()

    if (NOT ${CMAKE_GENERATOR} MATCHES "Ninja")
        link_directories("${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_BUILD_TYPE}")
        add_definitions(/MP)
    endif ()
    if (NOT ${CMAKE_CXX_FLAGS} STREQUAL "")
        string(REGEX REPLACE "(/EH[a-z]+) " "\\1- " CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS}) # Disable C++ exceptions
        string(REGEX REPLACE "/EHsc$" "/EHs- /EHc- " CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS}) # Disable C++ exceptions
        string(REGEX REPLACE "/EHsc- " "/EHs- /EHc- " CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS}) # Disable C++ exceptions
        string(REGEX REPLACE "/W3" "/W4" CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS}) # Warnings are important
    endif ()

    if (USE_STATIC_RUNTIME)
        foreach (flag_var
            CMAKE_CXX_FLAGS CMAKE_CXX_FLAGS_DEBUG CMAKE_CXX_FLAGS_RELEASE
            CMAKE_CXX_FLAGS_MINSIZEREL CMAKE_CXX_FLAGS_RELWITHDEBINFO)
            # Use the multithreaded static runtime library instead of the default DLL runtime.
            string(REGEX REPLACE "/MD" "/MT" ${flag_var} "${${flag_var}}")
        endforeach ()
    endif ()

    if (NOT QT_CONAN_DIR)
        set(ICU_LIBRARIES ${ICU_LIBRARY_PREFIX}icuuc${CMAKE_DEBUG_POSTFIX} ${ICU_LIBRARY_PREFIX}icuin${CMAKE_DEBUG_POSTFIX} ${ICU_LIBRARY_PREFIX}icudt${CMAKE_DEBUG_POSTFIX})
    endif ()
endif ()

if (NOT RUBY_FOUND AND RUBY_EXECUTABLE AND NOT RUBY_VERSION VERSION_LESS 1.9)
    get_property(_packages_found GLOBAL PROPERTY PACKAGES_FOUND)
    list(APPEND _packages_found Ruby)
    set_property(GLOBAL PROPERTY PACKAGES_FOUND ${_packages_found})

    get_property(_packages_not_found GLOBAL PROPERTY PACKAGES_NOT_FOUND)
    list(REMOVE_ITEM _packages_not_found Ruby)
    set_property(GLOBAL PROPERTY PACKAGES_NOT_FOUND ${_packages_not_found})
endif ()

set_package_properties(Ruby PROPERTIES TYPE REQUIRED)
set_package_properties(Qt6PrintSupport PROPERTIES PURPOSE "Required for ENABLE_PRINT_SUPPORT=ON")
feature_summary(WHAT ALL FATAL_ON_MISSING_REQUIRED_PACKAGES)


include(ECMQueryQmake)

query_qmake(qt_install_prefix_dir QT_INSTALL_PREFIX)
if (CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "${qt_install_prefix_dir}" CACHE PATH "Install path prefix, prepended onto install directories." FORCE)
endif ()

include(KDEInstallDirs)

if (NOT qt_install_prefix_dir STREQUAL "${CMAKE_INSTALL_PREFIX}")
    set(KDE_INSTALL_USE_QT_SYS_PATHS OFF)
endif ()

# We split all installed files into 2 components: Code and Data. This is different from
# traditional approach with Runtime and Devel, but we need it to fix concurrent installation of
# debug and release builds in qmake-based build
set(CMAKE_INSTALL_DEFAULT_COMPONENT_NAME "Code")

# Override headers directories
set(bmalloc_FRAMEWORK_HEADERS_DIR ${CMAKE_BINARY_DIR}/bmalloc/Headers)
set(ANGLE_FRAMEWORK_HEADERS_DIR ${CMAKE_BINARY_DIR}/ANGLE/Headers)
set(WTF_FRAMEWORK_HEADERS_DIR ${CMAKE_BINARY_DIR}/WTF/Headers)
set(JavaScriptCore_FRAMEWORK_HEADERS_DIR ${CMAKE_BINARY_DIR}/JavaScriptCore/Headers)
set(JavaScriptCore_PRIVATE_FRAMEWORK_HEADERS_DIR ${CMAKE_BINARY_DIR}/JavaScriptCore/PrivateHeaders)
set(PAL_FRAMEWORK_HEADERS_DIR ${CMAKE_BINARY_DIR}/PAL/Headers)
set(WebCore_PRIVATE_FRAMEWORK_HEADERS_DIR ${CMAKE_BINARY_DIR}/WebCore/PrivateHeaders)
set(WebKit_FRAMEWORK_HEADERS_DIR ${CMAKE_BINARY_DIR}/WebKit/Headers)
set(WebKit_PRIVATE_FRAMEWORK_HEADERS_DIR ${CMAKE_BINARY_DIR}/WebKit/PrivateHeaders)
set(WebKitLegacy_FRAMEWORK_HEADERS_DIR ${CMAKE_BINARY_DIR}/WebKitLegacy/Headers)
set(WebKitWidgets_FRAMEWORK_HEADERS_DIR ${CMAKE_BINARY_DIR}/WebKitWidgets/Headers)

# Using DERIVED_SOURCES_DIR is deprecated
set(DERIVED_SOURCES_DIR "${CMAKE_BINARY_DIR}/DerivedSources")
set(DERIVED_SOURCES_WEBKIT_DIR ${DERIVED_SOURCES_DIR}/WebKit)

# Override derived sources directories
set(WTF_DERIVED_SOURCES_DIR ${CMAKE_BINARY_DIR}/WTF/DerivedSources)
set(JavaScriptCore_DERIVED_SOURCES_DIR ${CMAKE_BINARY_DIR}/JavaScriptCore/DerivedSources)
set(WebCore_DERIVED_SOURCES_DIR ${CMAKE_BINARY_DIR}/WebCore/DerivedSources)
set(WebKitLegacy_DERIVED_SOURCES_DIR ${CMAKE_BINARY_DIR}/WebKitLegacy/DerivedSources)
set(WebKit_DERIVED_SOURCES_DIR ${CMAKE_BINARY_DIR}/WebKit/DerivedSources)
set(WebInspectorUI_DERIVED_SOURCES_DIR ${CMAKE_BINARY_DIR}/WebInspectorUI/DerivedSources)

# Override scripts directories
set(WTF_SCRIPTS_DIR ${CMAKE_BINARY_DIR}/WTF/Scripts)
set(JavaScriptCore_SCRIPTS_DIR ${CMAKE_BINARY_DIR}/JavaScriptCore/Scripts)

# Public API
set(QtWebKit_FRAMEWORK_HEADERS_DIR "${CMAKE_BINARY_DIR}/include/QtWebKit")
set(QtWebKitWidgets_FRAMEWORK_HEADERS_DIR "${CMAKE_BINARY_DIR}/include/QtWebKitWidgets")
