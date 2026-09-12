#pragma once

#include <memory>
#include <string>
#include <unordered_map>

#include <yaclib/async/make.hpp>
#include <yaclib/async/run.hpp>
#include <yaclib/coro/await.hpp>
#include <yaclib/coro/future.hpp>
#include <yaclib/exe/executor.hpp>

#include <maelstrom/detail/network/transport.hpp>
#include <maelstrom/environment.hpp>
#include <maelstrom/log/logging.hpp>
#include <maelstrom/network/network.hpp>
#include <maelstrom/routines/handler.hpp>

namespace maelstrom::detail {

template <typename State> class HandlersProcessor {
public:
  HandlersProcessor(yaclib::IExecutor &executor, Network &network);

  template <IsHandler<State> Handler, typename... Args>
  void Add(Args &&...args);

  void Start(Environment env, std::shared_ptr<State> state);
  void Stop();

  void UseTransport(std::shared_ptr<ITransport> transport);

  void Process(Request request);

private:
  bool is_running_{false};

  yaclib::IExecutor &executor_;

  std::shared_ptr<ITransport> transport_;
  Network &network_;

  std::unordered_map<std::string, std::unique_ptr<HandlerBase<State>>>
    handlers_{};
};

} // namespace maelstrom::detail

template <typename State>
maelstrom::detail::HandlersProcessor<State>::HandlersProcessor(
  yaclib::IExecutor &executor, Network &network)
  : executor_{executor}, network_{network} {}

template <typename State>
template <maelstrom::IsHandler<State> Handler, typename... Args>
void maelstrom::detail::HandlersProcessor<State>::Add(Args &&...args) {
  if (is_running_) {
    LOG_ERROR() << fmt::format(
      "Cannot add handler '{}', node already started!\n", Handler::kType);
    return;
  }

  auto handler = std::make_unique<Handler>(std::forward<Args>(args)...);
  handlers_[std::string{Handler::kType}] = std::move(handler);
  LOG_INFO() << fmt::format("Handler '{}' was added to node registry!\n",
                            Handler::kType);
}

template <typename State>
void maelstrom::detail::HandlersProcessor<State>::Start(
  Environment env, std::shared_ptr<State> state) {
  std::exchange(is_running_, true);

  for (auto &[type, handler] : handlers_) {
    handler->StartInternal(env, state);
    handler->Start();
  }
}

template <typename State>
void maelstrom::detail::HandlersProcessor<State>::Stop() {
  if (is_running_) {
    for (auto &[type, handler] : handlers_) {
      handler->Stop();
      handler->StopInternal();
    }

    std::exchange(is_running_, false);
  }
}

template <typename State>
void maelstrom::detail::HandlersProcessor<State>::UseTransport(
  std::shared_ptr<ITransport> transport) {
  transport_ = std::move(transport);
}

template <typename State>
void maelstrom::detail::HandlersProcessor<State>::Process(Request request) {
  auto type = request.type;

  auto it = handlers_.find(type);
  if (it == handlers_.end()) {
    LOG_ERROR() << fmt::format("No handler for message of type '{}'\n", type);
    return;
  }

  std::ignore = yaclib::Run(
    executor_,
    [this, type, it,
     request = std::move(request)]() mutable -> yaclib::Future<> {
      try {
        auto session = network_.MakeSession();

        const auto &handler = it->second;
        auto response =
          co_await handler->Handle(std::move(session), std::move(request));

        transport_->Send(std::move(response).ToMessage());
      } catch (const std::exception &ex) {
        LOG_ERROR() << fmt::format(
          "Exception occurs, when processing handler '{}', ex: {}\n", type,
          ex.what());
      }
    });
}