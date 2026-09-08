cmake_minimum_required(VERSION 3.15)
project(SagaCoordinator CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_options(/permissive- /Zc:__cplusplus)
    add_link_options("/FORCE:MULTIPLE")
endif()

find_package(Protobuf REQUIRED)
find_package(gRPC CONFIG REQUIRED)

set(PROTO_FILE "${CMAKE_CURRENT_SOURCE_DIR}/proto/saga.proto")
set(PROTO_SRC_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(MAKE_DIRECTORY ${PROTO_SRC_DIR})

add_custom_command(
    OUTPUT "${PROTO_SRC_DIR}/saga.pb.cc"
           "${PROTO_SRC_DIR}/saga.pb.h"
           "${PROTO_SRC_DIR}/saga.grpc.pb.cc"
           "${PROTO_SRC_DIR}/saga.grpc.pb.h"
    COMMAND protobuf::protoc
    ARGS --grpc_out="${PROTO_SRC_DIR}"
         --cpp_out="${PROTO_SRC_DIR}"
         --plugin=protoc-gen-grpc="$<TARGET_FILE:gRPC::grpc_cpp_plugin>"
         -I "${CMAKE_CURRENT_SOURCE_DIR}/proto"
         "${PROTO_FILE}"
    DEPENDS "${PROTO_FILE}"
)

add_library(saga_grpc_proto
    "${PROTO_SRC_DIR}/saga.pb.cc"
    "${PROTO_SRC_DIR}/saga.grpc.pb.cc"
)
target_link_libraries(saga_grpc_proto PUBLIC gRPC::grpc++ protobuf::libprotobuf)
target_include_directories(saga_grpc_proto PUBLIC "${PROTO_SRC_DIR}")

# --- Microservices ---
add_executable(payment_server services/payment_server.cpp)
target_link_libraries(payment_server PRIVATE saga_grpc_proto)

add_executable(inventory_server services/inventory_server.cpp)
target_link_libraries(inventory_server PRIVATE saga_grpc_proto)

add_executable(order_server services/order_server.cpp)
target_link_libraries(order_server PRIVATE saga_grpc_proto)

# --- PostgreSQL Database ---
find_package(libpqxx CONFIG REQUIRED)

# --- Orchestrator ---
add_library(db_logger orchestrator/db_logger.cpp)
target_link_libraries(db_logger PUBLIC libpqxx::pqxx)

add_executable(saga_orchestrator orchestrator/main.cpp orchestrator/saga_client.cpp)
target_link_libraries(saga_orchestrator PRIVATE saga_grpc_proto db_logger)

# --- Test Client ---
add_executable(test_client test_client.cpp)
target_link_libraries(test_client PRIVATE saga_grpc_proto)