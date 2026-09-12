include(FetchContent)
FetchContent_Declare(fmt
  GIT_REPOSITORY https://github.com/fmtlib/fmt.git
  GIT_TAG origin/main
)
FetchContent_MakeAvailable(fmt)

FetchContent_Declare(json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
)
FetchContent_MakeAvailable(json)

FetchContent_Declare(yaclib
  GIT_REPOSITORY https://github.com/YACLib/YACLib.git
  GIT_TAG main
)
set(YACLIB_FLAGS "CORO" CACHE STRING "")
FetchContent_MakeAvailable(yaclib)

set(MAELSTROM_CPP_SOURCES
  network/messages.cpp
  network/network.cpp
  detail/network/console_transport.cpp
  detail/network/in_memory_transport.cpp

  detail/processors/network.cpp

  detail/sync/timer.cpp
  )
list(TRANSFORM MAELSTROM_CPP_SOURCES PREPEND ${MAELSTROM_CPP_SOURCE_DIR}/)

set(MAELSTROM_CPP_HEADERS
  log/logging.hpp

  network/services/kv_storage.hpp
  network/messages.hpp
  network/network.hpp
  detail/network/console_transport.hpp
  detail/network/in_memory_transport.hpp
  detail/network/transport.hpp

  routines/handler.hpp
  routines/worker.hpp

  utils/unit.hpp

  detail/processors/handlers.hpp
  detail/processors/network.hpp
  detail/processors/workers.hpp

  detail/sync/action.hpp
  detail/sync/queue.hpp
  detail/sync/timer.hpp

  node.hpp
  environment.hpp
  )
list(TRANSFORM MAELSTROM_CPP_HEADERS PREPEND ${MAELSTROM_CPP_INCLUDE_DIR}/maelstrom/)

add_library(maelstrom-cpp STATIC 
  ${MAELSTROM_CPP_SOURCES} 
  ${MAELSTROM_CPP_HEADERS}
  )
target_include_directories(maelstrom-cpp
  PUBLIC ${MAELSTROM_CPP_INCLUDE_DIR}

  PUBLIC ${fmt_SOURCE_DIR}/include
  PUBLIC ${json_SOURCE_DIR}
  PUBLIC ${yaclib_SOURCE_DIR}/include
  )
target_link_libraries(maelstrom-cpp
  PUBLIC fmt::fmt 
  PUBLIC nlohmann_json::nlohmann_json 
  PUBLIC yaclib
  )
