include(FetchContent)
FetchContent_Declare(gtest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG main
  )
FetchContent_MakeAvailable(gtest)

set(MAELSTROM_CPP_TEST_SOURCES
  init_test.cpp
  echo_test.cpp
  echo_deferred_test.cpp
  network_processor_test.cpp
  dependencies_test.cpp
  )

foreach(TEST_SOURCE ${MAELSTROM_CPP_TEST_SOURCES})
  string(REPLACE "/" "_" TEST_NAME ${TEST_SOURCE})
  string(REPLACE "_" "-" TEST_NAME ${TEST_NAME})
  string(REGEX REPLACE "\\.cpp$" "" TEST_NAME ${TEST_NAME})

  add_executable(${TEST_NAME} ${TEST_SOURCE})
  target_link_directories(${TEST_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/include)
  target_link_libraries(${TEST_NAME} PRIVATE GTest::gtest_main maelstrom-cpp)

  add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
endforeach()
