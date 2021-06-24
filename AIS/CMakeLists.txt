cmake_minimum_required (VERSION 3.8)
project ("AIS")

set(CMAKE_CXX_STANDARD 14)

add_definitions(-D_GLIBCXX_USE_CXX11_ABI=0)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")

aux_source_directory(src SRC_LIST)

include_directories(include)
include_directories(include/utils)
include_directories(include/mysql-connector-c++/linux)
include_directories(include/utils)

link_directories(lib/linux)

add_executable(AIS ${SRC_LIST})

target_link_libraries(${PROJECT_NAME} libmysqlcppconn.so)