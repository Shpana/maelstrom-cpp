#pragma once

#include <maelstrom/detail/network/transport.hpp>
#include <maelstrom/detail/sync/queue.hpp>

namespace maelstrom {

class InMemoryTransport final : public ITransport {
public:
  void Start() override;
  void Stop() override;

  void Send(Message message) override;
  std::optional<Message> Recieve() override;

  [[nodiscard]] bool IsRunning() const override;
  [[nodiscard]] bool IsStreaming() const override;

  void StartStreaming();
  void StopStreaming();

  void Push(Message message);
  std::optional<Message> Pop();

  [[nodiscard]] std::size_t InfligthResponses() const;
  [[nodiscard]] bool HasInflightResponses() const;
  [[nodiscard]] bool HasNoInflightResponses() const;

private:
  std::atomic<bool> is_running_{false};
  std::atomic<bool> end_of_stream_{true};

  detail::UnboundedBlockingQueue<Message> in_;
  detail::UnboundedBlockingQueue<Message> out_;
};

} // namespace maelstrom