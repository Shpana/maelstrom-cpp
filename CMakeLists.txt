cmake_minimum_required(VERSION 3.22)
project(maelstrom-cpp CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(MAELSTROM_CPP_SOURCE_DIR ${CMAKE_SOURCE_DIR}/src)
set(MAELSTROM_CPP_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/include)

add_subdirectory(src)

if(MAELSTROM_CPP_TEST)
  enable_testing()
  add_subdirectory(tests)
endif()
