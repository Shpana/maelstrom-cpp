#pragma once

#include <atomic>

#include <maelstrom/detail/network/transport.hpp>

namespace maelstrom {

class ConsoleTransport final : public ITransport {
public:
  void Start() override;
  void Stop() override;

  void Send(Message message) override;
  std::optional<Message> Recieve() override;

  [[nodiscard]] bool IsRunning() const override;
  [[nodiscard]] bool IsStreaming() const override;

private:
  std::mutex mtx_{}; // Guards access to std cout
  std::atomic<bool> is_running_{false};
  std::atomic<bool> end_of_stream_{false};
};

} // namespace maelstrom