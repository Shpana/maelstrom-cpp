#pragma once

#include <memory>
#include <string>
#include <thread>
#include <unordered_map>

#include <yaclib/async/run.hpp>
#include <yaclib/coro/future.hpp>
#include <yaclib/exe/executor.hpp>

#include <maelstrom/environment.hpp>
#include <maelstrom/routines/worker.hpp>

namespace maelstrom::detail {

template <typename State> class WorkersProcessor {
public:
  explicit WorkersProcessor(yaclib::IExecutor &executor, Network &network);

  template <IsWorker<State> Handler, typename... Args> void Add(Args &&...args);

  void Start(Environment env, std::shared_ptr<State> state);
  void Stop();

private:
  void BackgroundProcess();

private:
  bool is_running_{false};

  yaclib::IExecutor &executor_;
  // TODO(shpana): use timers
  std::thread assistant_;

  Network &network_;

  std::unordered_map<std::string, std::unique_ptr<WorkerBase<State>>>
    workers_{};
};

} // namespace maelstrom::detail

template <typename State>
maelstrom::detail::WorkersProcessor<State>::WorkersProcessor(
  yaclib::IExecutor &executor, Network &network)
  : executor_{executor}, network_{network} {}

template <typename State>
template <maelstrom::IsWorker<State> Worker, typename... Args>
void maelstrom::detail::WorkersProcessor<State>::Add(Args &&...args) {
  if (is_running_) {
    LOG_ERROR() << fmt::format(
      "Cannot add worker '{}', node already started!\n", Worker::kType);
    return;
  }

  auto worker = std::make_unique<Worker>(std::forward<Args>(args)...);
  workers_[std::string{Worker::kType}] = std::move(worker);
  LOG_INFO() << fmt::format("Worker '{}' was added to node registry!\n",
                            Worker::kType);
}

template <typename State>
void maelstrom::detail::WorkersProcessor<State>::Start(
  Environment env, std::shared_ptr<State> state) {
  std::exchange(is_running_, true);

  for (auto &[type, worker] : workers_) {
    worker->StartInternal(env, state);
    worker->Start();
  }

  assistant_ = std::thread{[this]() {
    while (is_running_) {
      BackgroundProcess();
    }
  }};
}

template <typename State>
void maelstrom::detail::WorkersProcessor<State>::Stop() {
  if (is_running_) {
    std::exchange(is_running_, false);

    for (auto &[type, worker] : workers_) {
      worker->Stop();
      worker->StopInternal();
    }

    if (assistant_.joinable()) {
      assistant_.join();
    }
  }
}

template <typename State>
void maelstrom::detail::WorkersProcessor<State>::BackgroundProcess() {
  using Clock = WorkerBase<State>::Clock;
  using namespace std::chrono_literals;

  using ExecutionState = WorkerBase<State>::ExecutionState;

  if (!is_running_) {
    return;
  }

  auto now = Clock::now();

  for (auto &[type, worker] : workers_) {
    auto guess = ExecutionState::Idle;

    if (worker->next_deadline_.load() < now &&
        worker->exec_state_.compare_exchange_strong(
          guess, ExecutionState::InProgress)) {

      std::ignore = yaclib::Run(
        executor_, [this, type, &worker]() mutable -> yaclib::Future<> {
          try {
            auto session = network_.MakeSession();
            co_await worker->Process(std::move(session));
          } catch (const std::exception &ex) {
            LOG_ERROR() << fmt::format(
              "Exception occurs, when processing worker '{}', ex: {}\n", type,
              ex.what());
          }

          worker->exec_state_.store(ExecutionState::Idle);
          worker->next_deadline_.store(Clock::now() + worker->period_);
        });
    }
  }

  // TODO(shpana): wake up on stopping
  // TODO(shpana): use timers to wake up immedeatly
  std::this_thread::sleep_for(50ms);
}