cmake_minimum_required(VERSION 3.8)

project(helloworld C CXX)

set (CMAKE_EXE_LINKER_FLAGS)
set (CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -w")
set(CMAKE_BUILD_TYPE Release)
set(CMAKE_CXX_FLAGS_RELEASE "-O3")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/build)

include(${CMAKE_CURRENT_LIST_DIR}/cmake/common.cmake)

# generate proto file c/c++ interface
set(PROTO_PATH "${CMAKE_CURRENT_LIST_DIR}/protos")
file(GLOB PROTO_FILES "${PROTO_PATH}/*.proto")

foreach(PROTO_FILE ${PROTO_FILES})
    get_filename_component(FILE_NAME ${PROTO_FILE} NAME_WE)
    list(APPEND PROTONAMES ${FILE_NAME})
    message(${FILE_NAME})
endforeach()

foreach(PROTONAME ${PROTONAMES})
    add_custom_command(
      OUTPUT "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${PROTONAME}.pb.cc"
      "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${PROTONAME}.pb.h"
      COMMAND ${_PROTOBUF_PROTOC}
        --cpp_out "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}"
        -I "${PROTO_PATH}"
        "${PROTO_PATH}/${PROTONAME}.proto"
      DEPENDS "${PROTO_PATH}/${PROTONAME}.proto")

    add_library("${PROTONAME}_proto"
    "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${PROTONAME}.pb.cc"
    "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${PROTONAME}.pb.h")
    target_link_libraries("${PROTONAME}_proto"
      ${_REFLECTION}
      ${_GRPC_GRPCPP}
      ${_PROTOBUF_LIBPROTOBUF})
endforeach()
# Include generated *.pb.h files
include_directories("${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")

add_executable(NodeTestPub "NodeTestPub.cpp")
target_link_libraries(NodeTestPub
  ${_CORE_LIBRARIES}
  hello_proto)
add_executable(NodeTestSub "NodeTestSub.cpp")
target_link_libraries(NodeTestSub
  ${_CORE_LIBRARIES}
  hello_proto)