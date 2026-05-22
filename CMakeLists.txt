cmake_minimum_required(VERSION 3.20)
project(runchart_cpp LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
find_package(Protobuf CONFIG REQUIRED)
find_package(gRPC CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

set(PROTO_DIR "${CMAKE_CURRENT_SOURCE_DIR}/proto")
set(PROTO_FILE "${PROTO_DIR}/runchart.proto")
set(GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(MAKE_DIRECTORY "${GENERATED_DIR}")

set(PROTO_SRC "${GENERATED_DIR}/runchart.pb.cc")
set(PROTO_HDR "${GENERATED_DIR}/runchart.pb.h")
set(GRPC_SRC  "${GENERATED_DIR}/runchart.grpc.pb.cc")
set(GRPC_HDR  "${GENERATED_DIR}/runchart.grpc.pb.h")

# Get the actual gRPC plugin executable path
get_target_property(GRPC_PLUGIN gRPC::grpc_cpp_plugin IMPORTED_LOCATION_RELEASE)
if(NOT GRPC_PLUGIN)
    get_target_property(GRPC_PLUGIN gRPC::grpc_cpp_plugin IMPORTED_LOCATION_DEBUG)
endif()
if(NOT GRPC_PLUGIN)
    get_target_property(GRPC_PLUGIN gRPC::grpc_cpp_plugin IMPORTED_LOCATION)
endif()

message(STATUS "gRPC plugin path: ${GRPC_PLUGIN}")

# Check if we need to regenerate
if(NOT EXISTS "${PROTO_SRC}" OR NOT EXISTS "${PROTO_HDR}" OR NOT EXISTS "${GRPC_SRC}" OR NOT EXISTS "${GRPC_HDR}")
    set(NEED_REGEN TRUE)
elseif("${PROTO_FILE}" IS_NEWER_THAN "${PROTO_SRC}")
    set(NEED_REGEN TRUE)
else()
    set(NEED_REGEN FALSE)
endif()

if(NEED_REGEN)
    message(STATUS "Regenerating protobuf/gRPC files from runchart.proto")
    execute_process(
        COMMAND ${Protobuf_PROTOC_EXECUTABLE}
            "--proto_path=${PROTO_DIR}"
            "--cpp_out=${GENERATED_DIR}"
            "--grpc_out=${GENERATED_DIR}"
            "--plugin=protoc-gen-grpc=${GRPC_PLUGIN}"
            "${PROTO_FILE}"
        RESULT_VARIABLE PROTOC_RESULT
        ERROR_VARIABLE PROTOC_ERROR
    )
    if(NOT PROTOC_RESULT EQUAL 0)
        message(FATAL_ERROR "protoc failed: ${PROTOC_ERROR}")
    endif()
    
    # Touch generated files to update their timestamps
    file(TOUCH "${PROTO_SRC}")
    file(TOUCH "${PROTO_HDR}")
    file(TOUCH "${GRPC_SRC}")
    file(TOUCH "${GRPC_HDR}")
else()
    message(STATUS "Protobuf/gRPC files are up-to-date, skipping generation")
endif()

add_executable(runchart_demo
    src/main.cpp
    src/measurement.cpp
    src/run_chart_service.cpp
    src/run_chart_client.cpp
    "${PROTO_SRC}"
    "${GRPC_SRC}"
)

target_include_directories(runchart_demo PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${GENERATED_DIR}"
)

target_link_libraries(runchart_demo PRIVATE
    gRPC::grpc++
    protobuf::libprotobuf
    nlohmann_json::nlohmann_json
)

if(MSVC)
    target_compile_options(runchart_demo PRIVATE /W3 /permissive-)
endif()
