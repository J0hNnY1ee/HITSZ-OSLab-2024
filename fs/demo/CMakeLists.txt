cmake_minimum_required(VERSION 3.0 FATAL_ERROR)
project(demo VERSION 0.0.1 LANGUAGES C)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D_FILE_OFFSET_BITS=64 -no-pie")
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -Wall --pedantic -g")

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/CMake" ${CMAKE_MODULE_PATH})

set(CMAKE_EXPORT_COMPILE_COMMANDS 1)

find_package(FUSE REQUIRED)
include_directories(${FUSE_INCLUDE_DIR} ./include)
aux_source_directory(./src DIR_SRCS)
add_executable(demo ${DIR_SRCS})
target_link_libraries(demo ${FUSE_LIBRARIES} $ENV{HOME}/lib/libddriver.a)


message("FUSE_INCLUDE_DIR ${FUSE_INCLUDE_DIR}")
message("FUSE_LIBRARIES ${FUSE_LIBRARIES}")
message("DIR_SRCS ${DIR_SRCS}")
message("!!!!!**CMAKE_GENERATOR** ${CMAKE_GENERATOR}")
