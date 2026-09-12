#pragma once

#include <chrono>
#include <condition_variable>
#include <map>
#include <mutex>
#include <thread>

#include <yaclib/async/run.hpp>
#include <yaclib/exe/executor.hpp>

#include <maelstrom/detail/sync/action.hpp>
#include <maelstrom/log/logging.hpp>

namespace maelstrom::detail {

class Timer {
  using Clock = std::chrono::steady_clock;

public:
  explicit Timer(yaclib::IExecutor &executor);

  void Start();
  void Stop();

  void Submit(Action &&action);

  void Set(Clock::duration delay, Action &&action);
  void Set(Clock::time_point deadline, Action &&action);

private:
  void Process();

private:
  std::atomic<bool> is_running_{false};

  yaclib::IExecutor &executor_;
  std::thread assistant_;

  std::mutex mtx_{};
  std::map<Clock::time_point, Action> waiters_{}; // Guarded by mtx_

  std::condition_variable process_on_time_{};
  std::condition_variable is_empty_{};
};

} // namespace maelstrom::detail