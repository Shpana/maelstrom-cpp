#pragma once

#include <memory>
#include <string_view>
#include <utility>
#include <vector>

#include <yaclib/coro/future.hpp>
#include <yaclib/runtime/fair_thread_pool.hpp>

#include <maelstrom/detail/network/console_transport.hpp>
#include <maelstrom/detail/network/transport.hpp>
#include <maelstrom/detail/processors/handlers.hpp>
#include <maelstrom/detail/processors/network.hpp>
#include <maelstrom/detail/processors/workers.hpp>
#include <maelstrom/environment.hpp>
#include <maelstrom/log/logging.hpp>
#include <maelstrom/network/network.hpp>

namespace maelstrom {

template <typename State>
class Node final : private detail::NetworkProcessor,
                   private detail::HandlersProcessor<State>,
                   private detail::WorkersProcessor<State> {
public:
  explicit Node();
  void UseTransport(std::shared_ptr<ITransport> transport);

  void Run();

  using detail::HandlersProcessor<State>::Add;
  using detail::WorkersProcessor<State>::Add;

private:
  bool Start();
  bool LoadEnvironment();
  void Stop();

private:
  yaclib::FairThreadPool cpu_pool_;

  std::shared_ptr<ITransport> transport_ = std::make_shared<ConsoleTransport>();
  Network network_;

  std::optional<Environment> env_{std::nullopt};
  std::shared_ptr<State> state_{};
};

} // namespace maelstrom

template <typename State>
maelstrom::Node<State>::Node()
  : detail::NetworkProcessor{},
    detail::HandlersProcessor<State>{cpu_pool_, network_},
    detail::WorkersProcessor<State>{cpu_pool_, network_},
    cpu_pool_{yaclib::FairThreadPool{1}}, network_{*this} {}

template <typename State>
void maelstrom::Node<State>::UseTransport(
  std::shared_ptr<ITransport> transport) {
  transport_ = std::move(transport);
}

template <typename State> bool maelstrom::Node<State>::LoadEnvironment() {
  auto message = transport_->Recieve();
  if (!message.has_value()) {
    LOG_ERROR() << "Failed to load envrionment information! Cannot parse "
                   "initialization message!\n";
    return false;
  }

  auto init_request = std::move(message.value()).ToRequest();

  constexpr std::string_view kInitType = "init";

  if (!init_request.has_value() || init_request.value().type != kInitType) {
    LOG_ERROR() << "Failed to load envrionment information! There are errors "
                   "in initializaiton's request!\n";
    return false;
  }

  const auto &body = init_request.value().body;
  if (!body.contains("node_id") || !body.contains("node_ids")) {
    LOG_ERROR() << "Failed to load environment information! There are no "
                   "required fields in request!\n";
    return false;
  }

  env_.emplace(Environment{.node_id = body["node_id"].get<std::string>(),
                           .available_node_ids =
                             body["node_ids"].get<std::vector<std::string>>()});

  transport_->Send(std::move(init_request.value()).ToResponse().ToMessage());
  LOG_INFO() << "Successfully load environment!\n";
  return true;
}

template <typename State> bool maelstrom::Node<State>::Start() {
  LOG_INFO() << "Starting!\n";

  transport_->Start();

  if (!LoadEnvironment()) {
    return false;
  }

  network_.Start(env_.value());

  detail::NetworkProcessor::UseTransport(transport_);
  detail::HandlersProcessor<State>::UseTransport(transport_);

  state_ = std::make_shared<State>();
  detail::NetworkProcessor::Start();
  detail::HandlersProcessor<State>::Start(env_.value(), state_);
  detail::WorkersProcessor<State>::Start(env_.value(), state_);

  LOG_INFO() << "Successfully started!\n";
  return true;
}

template <typename State> void maelstrom::Node<State>::Run() {
  if (Start()) {
    while (transport_->IsStreaming()) {
      auto raw_message = transport_->Recieve();
      if (!raw_message.has_value()) {
        continue;
      }

      auto message = raw_message.value();

      if (message.IsRequest()) {
        detail::HandlersProcessor<State>::Process(
          std::move(std::move(message).ToRequest().value()));
        continue;
      } else if (message.IsResponse()) {
        detail::NetworkProcessor::Process(
          std::move(std::move(message).ToResponse().value()));
        continue;
      }

      LOG_ERROR() << "Unknown type of message!\n";
    }
  }

  Stop();
}

template <typename State> void maelstrom::Node<State>::Stop() {
  LOG_INFO() << "Stopping!\n";

  cpu_pool_.SoftStop();
  cpu_pool_.Wait();

  detail::HandlersProcessor<State>::Stop();
  detail::WorkersProcessor<State>::Stop();

  NetworkProcessor::Stop();

  network_.Stop();
  transport_->Stop();

  LOG_INFO() << "Successfully stopped!\n";
}