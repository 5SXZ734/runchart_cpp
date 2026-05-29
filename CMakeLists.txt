cmake_minimum_required(VERSION 3.20)

project(runchart_cpp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# -------------------- Dependencies --------------------

find_package(gRPC CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(SQLite3 REQUIRED)

# -------------------- Protobuf --------------------
# vcpkg usually provides Protobuf CONFIG mode.
# Ubuntu apt often provides only CMake module mode.

find_package(Protobuf CONFIG QUIET)

if(Protobuf_FOUND AND TARGET protobuf::libprotobuf)
    message(STATUS "Using Protobuf CONFIG package")

    set(RUNCHART_PROTOBUF_LIB protobuf::libprotobuf)

    if(TARGET protobuf::protoc)
        set(RUNCHART_PROTOC_EXE protobuf::protoc)
    elseif(DEFINED Protobuf_PROTOC_EXECUTABLE)
        set(RUNCHART_PROTOC_EXE "${Protobuf_PROTOC_EXECUTABLE}")
    else()
        message(FATAL_ERROR "Could not find protoc executable from Protobuf CONFIG package")
    endif()
else()
    message(STATUS "Using Protobuf module package")

    find_package(Protobuf REQUIRED)

    set(RUNCHART_PROTOBUF_LIB ${Protobuf_LIBRARIES})
    set(RUNCHART_PROTOC_EXE "${Protobuf_PROTOC_EXECUTABLE}")

    if(NOT RUNCHART_PROTOC_EXE)
        message(FATAL_ERROR "Could not find protoc executable")
    endif()
endif()

# -------------------- TagLib --------------------
# vcpkg may provide TagLibConfig.cmake.
# Ubuntu libtag1-dev usually does NOT, so use pkg-config fallback.

find_package(TagLib CONFIG QUIET)

if(TagLib_FOUND)
    message(STATUS "Using TagLib CONFIG package")

    if(TARGET TagLib::TagLib)
        set(RUNCHART_TAGLIB_LIB TagLib::TagLib)
    elseif(TARGET TagLib::tag)
        set(RUNCHART_TAGLIB_LIB TagLib::tag)
    elseif(TARGET tag)
        set(RUNCHART_TAGLIB_LIB tag)
    else()
        message(FATAL_ERROR "TagLib package found, but no known TagLib target exists")
    endif()
else()
    message(STATUS "Using TagLib via pkg-config")

    find_package(PkgConfig REQUIRED)
    pkg_check_modules(TAGLIB REQUIRED taglib)

    add_library(runchart_taglib INTERFACE)
    target_include_directories(runchart_taglib INTERFACE ${TAGLIB_INCLUDE_DIRS})
    target_compile_options(runchart_taglib INTERFACE ${TAGLIB_CFLAGS_OTHER})
    target_link_directories(runchart_taglib INTERFACE ${TAGLIB_LIBRARY_DIRS})
    target_link_libraries(runchart_taglib INTERFACE ${TAGLIB_LIBRARIES})

    set(RUNCHART_TAGLIB_LIB runchart_taglib)
endif()


# -------------------- Qt Widgets (optional desktop UI) --------------------

option(RUNCHART_ENABLE_QT_UI "Build the Qt Widgets desktop audio player when Qt is available" ON)
set(RUNCHART_QT_WIDGETS_TARGET "")

if(RUNCHART_ENABLE_QT_UI)
    find_package(Qt6 COMPONENTS Widgets QUIET)
    if(Qt6_FOUND)
        set(RUNCHART_QT_WIDGETS_TARGET Qt6::Widgets)
        message(STATUS "Qt Widgets desktop UI enabled with Qt6")
    else()
        find_package(Qt5 COMPONENTS Widgets QUIET)
        if(Qt5_FOUND)
            set(RUNCHART_QT_WIDGETS_TARGET Qt5::Widgets)
            message(STATUS "Qt Widgets desktop UI enabled with Qt5")
        else()
            message(WARNING "Qt Widgets was not found; runchart_desktop will not be built")
        endif()
    endif()
endif()

# -------------------- libVLC (optional client audio playback) --------------------

option(RUNCHART_ENABLE_VLC "Enable client-side audio playback via libVLC when available" ON)
set(RUNCHART_VLC_ROOT "D:/WORK/external/vlc-3.0.23" CACHE PATH "Root directory of a VLC/libVLC SDK installation")
set(RUNCHART_WITH_LIBVLC OFF)

if(RUNCHART_ENABLE_VLC)
    if(WIN32)
        find_path(RUNCHART_VLC_INCLUDE_DIR
            NAMES vlc/vlc.h
            PATHS "${RUNCHART_VLC_ROOT}/sdk/include"
            NO_DEFAULT_PATH
        )
        find_library(RUNCHART_VLC_LIBRARY
            NAMES libvlc vlc
            PATHS "${RUNCHART_VLC_ROOT}/sdk/lib"
            NO_DEFAULT_PATH
        )
    else()
        find_package(PkgConfig QUIET)
        if(PkgConfig_FOUND)
            pkg_check_modules(LIBVLC QUIET libvlc)
        endif()
        if(LIBVLC_FOUND)
            set(RUNCHART_VLC_INCLUDE_DIR ${LIBVLC_INCLUDE_DIRS})
            set(RUNCHART_VLC_LIBRARY ${LIBVLC_LIBRARIES})
            set(RUNCHART_VLC_LIBRARY_DIRS ${LIBVLC_LIBRARY_DIRS})
            set(RUNCHART_VLC_COMPILE_OPTIONS ${LIBVLC_CFLAGS_OTHER})
        else()
            find_path(RUNCHART_VLC_INCLUDE_DIR NAMES vlc/vlc.h)
            find_library(RUNCHART_VLC_LIBRARY NAMES vlc libvlc)
        endif()
    endif()

    if(RUNCHART_VLC_INCLUDE_DIR AND RUNCHART_VLC_LIBRARY)
        add_library(runchart_vlc INTERFACE)
        target_include_directories(runchart_vlc INTERFACE ${RUNCHART_VLC_INCLUDE_DIR})
        target_link_libraries(runchart_vlc INTERFACE ${RUNCHART_VLC_LIBRARY})
        if(DEFINED RUNCHART_VLC_LIBRARY_DIRS)
            target_link_directories(runchart_vlc INTERFACE ${RUNCHART_VLC_LIBRARY_DIRS})
        endif()
        if(DEFINED RUNCHART_VLC_COMPILE_OPTIONS)
            target_compile_options(runchart_vlc INTERFACE ${RUNCHART_VLC_COMPILE_OPTIONS})
        endif()
        set(RUNCHART_WITH_LIBVLC ON)
        message(STATUS "Client audio playback enabled with libVLC")
    else()
        message(WARNING "libVLC was not found; runchart_client play command will report audio playback as unavailable")
    endif()
endif()

# -------------------- gRPC plugin --------------------

if(NOT TARGET gRPC::grpc_cpp_plugin)
    message(FATAL_ERROR "gRPC::grpc_cpp_plugin target not found")
endif()

# -------------------- Proto generation --------------------

set(PROTO_DIR "${CMAKE_CURRENT_SOURCE_DIR}/proto")
set(PROTO_FILE "${PROTO_DIR}/runchart.proto")
set(GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")

file(MAKE_DIRECTORY "${GENERATED_DIR}")

set(PROTO_SRC "${GENERATED_DIR}/runchart.pb.cc")
set(PROTO_HDR "${GENERATED_DIR}/runchart.pb.h")
set(GRPC_SRC  "${GENERATED_DIR}/runchart.grpc.pb.cc")
set(GRPC_HDR  "${GENERATED_DIR}/runchart.grpc.pb.h")

add_custom_command(
    OUTPUT
        "${PROTO_SRC}"
        "${PROTO_HDR}"
        "${GRPC_SRC}"
        "${GRPC_HDR}"
    COMMAND "${RUNCHART_PROTOC_EXE}"
    ARGS
        "--proto_path=${PROTO_DIR}"
        "--cpp_out=${GENERATED_DIR}"
        "--grpc_out=${GENERATED_DIR}"
        "--plugin=protoc-gen-grpc=$<TARGET_FILE:gRPC::grpc_cpp_plugin>"
        "${PROTO_FILE}"
    DEPENDS
        "${PROTO_FILE}"
        gRPC::grpc_cpp_plugin
    COMMENT "Generating protobuf/gRPC files from runchart.proto"
    VERBATIM
)

# -------------------- Common library --------------------

add_library(runchart_lib
    src/measurement.cpp
    src/run_chart_service.cpp
    src/run_chart_client.cpp
    src/structured_logger.cpp
    src/config.cpp
    src/catalog.cpp
    src/session_auth.cpp
    src/metrics.cpp
    src/stream_http.cpp
    "${PROTO_SRC}"
    "${GRPC_SRC}"
)

target_include_directories(runchart_lib PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${GENERATED_DIR}"
)

if(DEFINED Protobuf_INCLUDE_DIRS)
    target_include_directories(runchart_lib PUBLIC "${Protobuf_INCLUDE_DIRS}")
endif()

target_link_libraries(runchart_lib PUBLIC
    gRPC::grpc++
    ${RUNCHART_PROTOBUF_LIB}
    nlohmann_json::nlohmann_json
    SQLite::SQLite3
    ${RUNCHART_TAGLIB_LIB}
)

if(WIN32)
    target_link_libraries(runchart_lib PUBLIC ws2_32)
endif()

# -------------------- Server executable --------------------

add_executable(runchart_server
    src/server_main.cpp
)

target_link_libraries(runchart_server PRIVATE runchart_lib)

# -------------------- Client executable --------------------

add_executable(runchart_client
    src/client_main.cpp
    src/audio_player.cpp
)

target_link_libraries(runchart_client PRIVATE runchart_lib)

if(RUNCHART_WITH_LIBVLC)
    target_compile_definitions(runchart_client PRIVATE RUNCHART_WITH_LIBVLC)
    target_link_libraries(runchart_client PRIVATE runchart_vlc)
endif()

# -------------------- Desktop UI executable --------------------

if(RUNCHART_QT_WIDGETS_TARGET)
    add_executable(runchart_desktop
        src/desktop_main.cpp
        src/playback_controller.cpp
        src/audio_player.cpp
    )

    target_link_libraries(runchart_desktop PRIVATE
        runchart_lib
        ${RUNCHART_QT_WIDGETS_TARGET}
    )

    if(RUNCHART_WITH_LIBVLC)
        target_compile_definitions(runchart_desktop PRIVATE RUNCHART_WITH_LIBVLC)
        target_link_libraries(runchart_desktop PRIVATE runchart_vlc)
    endif()
endif()

# -------------------- Warnings --------------------

if(MSVC)
    target_compile_options(runchart_lib PRIVATE /W3 /permissive-)
    target_compile_options(runchart_server PRIVATE /W3 /permissive-)
    target_compile_options(runchart_client PRIVATE /W3 /permissive-)
    if(TARGET runchart_desktop)
        target_compile_options(runchart_desktop PRIVATE /W3 /permissive-)
    endif()
else()
    target_compile_options(runchart_lib PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(runchart_server PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(runchart_client PRIVATE -Wall -Wextra -Wpedantic)
    if(TARGET runchart_desktop)
        target_compile_options(runchart_desktop PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endif()
