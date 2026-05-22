cmake_minimum_required(VERSION 3.20)
project(runchart_cpp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Find required packages
find_package(Protobuf CONFIG REQUIRED)
find_package(gRPC CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

# Set up protobuf/gRPC generation
set(PROTO_DIR "${CMAKE_CURRENT_SOURCE_DIR}/proto")
set(PROTO_FILE "${PROTO_DIR}/runchart.proto")
set(GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")

# Create the generated directory
file(MAKE_DIRECTORY "${GENERATED_DIR}")

# Generated file paths
set(PROTO_SRC "${GENERATED_DIR}/runchart.pb.cc")
set(PROTO_HDR "${GENERATED_DIR}/runchart.pb.h")
set(GRPC_SRC "${GENERATED_DIR}/runchart.grpc.pb.cc")
set(GRPC_HDR "${GENERATED_DIR}/runchart.grpc.pb.h")

# Get the gRPC plugin executable path
get_target_property(GRPC_PLUGIN gRPC::grpc_cpp_plugin IMPORTED_LOCATION_RELEASE)
if(NOT GRPC_PLUGIN)
    get_target_property(GRPC_PLUGIN gRPC::grpc_cpp_plugin IMPORTED_LOCATION_DEBUG)
endif()
if(NOT GRPC_PLUGIN)
    get_target_property(GRPC_PLUGIN gRPC::grpc_cpp_plugin IMPORTED_LOCATION)
endif()

if(NOT GRPC_PLUGIN)
    message(FATAL_ERROR "Could not find gRPC plugin executable")
endif()

message(STATUS "gRPC plugin path: ${GRPC_PLUGIN}")

# Add custom command to generate protobuf/gRPC files when proto file changes
# This ensures generation only happens when needed, not on every build
add_custom_command(
    OUTPUT "${PROTO_SRC}" "${PROTO_HDR}" "${GRPC_SRC}" "${GRPC_HDR}"
    COMMAND ${Protobuf_PROTOC_EXECUTABLE}
        "--proto_path=${PROTO_DIR}"
        "--cpp_out=${GENERATED_DIR}"
        "--grpc_out=${GENERATED_DIR}"
        "--plugin=protoc-gen-grpc=${GRPC_PLUGIN}"
        "${PROTO_FILE}"
    DEPENDS "${PROTO_FILE}"
    COMMENT "Generating protobuf/gRPC files from runchart.proto"
    VERBATIM
)

# Create a library with common code (measurement, service, client)
add_library(runchart_lib
    src/measurement.cpp
    src/run_chart_service.cpp
    src/run_chart_client.cpp
    "${PROTO_SRC}"
    "${GRPC_SRC}"
)

target_include_directories(runchart_lib PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${GENERATED_DIR}"
)

target_link_libraries(runchart_lib PUBLIC
    gRPC::grpc++
    protobuf::libprotobuf
    nlohmann_json::nlohmann_json
)

if(MSVC)
    target_compile_options(runchart_lib PRIVATE /W3 /permissive-)
endif()

# Server executable
add_executable(runchart_server
    src/server_main.cpp
)

target_link_libraries(runchart_server PRIVATE runchart_lib)

if(MSVC)
    target_compile_options(runchart_server PRIVATE /W3 /permissive-)
endif()

# Client executable
add_executable(runchart_client
    src/client_main.cpp
)

target_link_libraries(runchart_client PRIVATE runchart_lib)

if(MSVC)
    target_compile_options(runchart_client PRIVATE /W3 /permissive-)
endif()
