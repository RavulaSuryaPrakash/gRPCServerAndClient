cmake_minimum_required(VERSION 3.10)
project(ProcessorA_Server LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

cmake_policy(SET CMP0077 NEW)
set(protobuf_MODULE_COMPATIBLE TRUE CACHE BOOL "" FORCE)

# Set the prefix paths to use protobuf@21 and gRPC from Homebrew.
set(CMAKE_PREFIX_PATH "/opt/homebrew/opt/protobuf@21;/opt/homebrew/Cellar/grpc/1.71.0/lib/cmake/grpc;/opt/homebrew" CACHE STRING "Prefix path" FORCE)
set(gRPC_CPP_PLUGIN_EXECUTABLE "/opt/homebrew/bin/grpc_cpp_plugin")

find_package(Protobuf CONFIG REQUIRED)
find_package(gRPC CONFIG REQUIRED)
find_package(OpenMP REQUIRED)

# Force use of the protobuf@21 headers.
set(Protobuf_INCLUDE_DIRS "/opt/homebrew/opt/protobuf@21/include")

# Specify the generated proto files (assumed to be in the project root).
set(PROTO_SRCS 
    ${CMAKE_SOURCE_DIR}/data_transfer.pb.cc 
    ${CMAKE_SOURCE_DIR}/data_transfer.grpc.pb.cc
)
set(PROTO_HDRS 
    ${CMAKE_SOURCE_DIR}/data_transfer.pb.h 
    ${CMAKE_SOURCE_DIR}/data_transfer.grpc.pb.h
)

# Create the executable target.
add_executable(server
    ${CMAKE_SOURCE_DIR}/src/ProcessA_Server.cpp
    ${CMAKE_SOURCE_DIR}/src/data_classes.cpp
    ${PROTO_SRCS}
    ${PROTO_HDRS}
)

# Now specify include directories for the "server" target.
target_include_directories(server PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}
    ${Protobuf_INCLUDE_DIRS}
    ${gRPC_INCLUDE_DIRS}
)

target_link_libraries(server PRIVATE
    gRPC::grpc++
    protobuf::libprotobuf
    OpenMP::OpenMP_CXX
)
