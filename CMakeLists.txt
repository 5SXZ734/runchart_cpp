cmake_minimum_required(VERSION 3.20)

project(runchart_cpp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# -------------------- Dependencies --------------------

find_package(gRPC CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

# vcpkg provides Protobuf CONFIG mode.
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

# gRPC plugin target should exist from find_package(gRPC CONFIG REQUIRED)
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
    "${GENERATED_DIR}"
)

if(DEFINED Protobuf_INCLUDE_DIRS)
    target_include_directories(runchart_lib PUBLIC "${Protobuf_INCLUDE_DIRS}")
endif()

target_link_libraries(runchart_lib PUBLIC
    gRPC::grpc++
    ${RUNCHART_PROTOBUF_LIB}
    nlohmann_json::nlohmann_json
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
)

target_link_libraries(runchart_client PRIVATE runchart_lib)

# -------------------- Warnings --------------------

if(MSVC)
    target_compile_options(runchart_lib PRIVATE /W3 /permissive-)
    target_compile_options(runchart_server PRIVATE /W3 /permissive-)
    target_compile_options(runchart_client PRIVATE /W3 /permissive-)
else()
    target_compile_options(runchart_lib PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(runchart_server PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(runchart_client PRIVATE -Wall -Wextra -Wpedantic)
endif()