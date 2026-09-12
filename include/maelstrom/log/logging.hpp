#include <chrono>
#include <iostream>

#include <fmt/chrono.h>
#include <fmt/format.h>

// TODO(shpana): use glog
#ifdef MAELSTROM_DEBUG
#  define LOG_DEBUG()                                                          \
    std::cerr << fmt::format("[DEBUG][{}]: ", std::chrono::system_clock::now())
#else
#  define LOG_DEBUG()                                                          \
    std::stringstream {}
#endif
#define LOG_INFO()                                                             \
  std::cerr << fmt::format("[INFO][{}]: ", std::chrono::system_clock::now())
#define LOG_ERROR()                                                            \
  std::cerr << fmt::format("[ERROR][{}]: ", std::chrono::system_clock::now())
