cmake_minimum_required(VERSION 3.8)

project(Test C CXX)
set (CMAKE_EXE_LINKER_FLAGS)
set (CMAKE_CXX_STANDARD 17)
set(CMAKE_BUILD_TYPE Release)
set(CMAKE_CXX_FLAGS_RELEASE "-O3")
set(CMAKE_CXX_FLAGS_DEBUG "-g")

include(./cmake/common.cmake)

find_package(Boost REQUIRED COMPONENTS thread)

# Proto file
get_filename_component(big_proto "./protos/big.proto" ABSOLUTE)
get_filename_component(big_proto_path "${big_proto}" PATH)


# Generated sources
set(big_proto_srcs "${CMAKE_CURRENT_BINARY_DIR}/big.pb.cc")
set(big_proto_hdrs "${CMAKE_CURRENT_BINARY_DIR}/big.pb.h")

add_custom_command(
  OUTPUT "${big_proto_srcs}" "${big_proto_hdrs}"
  COMMAND ${_PROTOBUF_PROTOC}
  ARGS
    --cpp_out "${CMAKE_CURRENT_BINARY_DIR}"
    -I "${big_proto_path}"
    --plugin=protoc-gen-grpc="${_GRPC_CPP_PLUGIN_EXECUTABLE}"
    "${big_proto}"
  DEPENDS "${big_proto}")
# Include generated *.pb.h files
include_directories("${CMAKE_CURRENT_BINARY_DIR}")

add_library(big_proto
  ${big_proto_srcs}
  ${big_proto_hdrs})
target_link_libraries(big_proto
  ${_REFLECTION}
  ${_PROTOBUF_LIBPROTOBUF})

add_executable(Test Test.cc)
target_include_directories(Test PRIVATE
${CMAKE_PREFIX_PATH}/include
)
target_link_directories(Test PRIVATE
    ${CMAKE_PREFIX_PATH}/lib
)
target_link_libraries(Test
    big_proto
    registration_grpc_proto
    topic_message_grpc_proto
    service_message_grpc_proto
    NodeHandler
    ${_REFLECTION}
    ${_GRPC_GRPCPP}
    ${_PROTOBUF_LIBPROTOBUF}
    ${Boost_LIBRARIES})

add_executable(TestService TestService.cc)
target_include_directories(TestService PRIVATE
${CMAKE_PREFIX_PATH}/include
)
target_link_directories(TestService PRIVATE
    ${CMAKE_PREFIX_PATH}/lib
)
target_link_libraries(TestService
    big_proto
    registration_grpc_proto
    topic_message_grpc_proto
    service_message_grpc_proto
    NodeHandler
    ${_REFLECTION}
    ${_GRPC_GRPCPP}
    ${_PROTOBUF_LIBPROTOBUF}
    ${Boost_LIBRARIES})
