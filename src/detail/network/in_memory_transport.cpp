#include <maelstrom/detail/network/in_memory_transport.hpp>
#include <maelstrom/log/logging.hpp>

namespace maelstrom {

void InMemoryTransport::Start() {
  is_running_.store(true);
}

void InMemoryTransport::Stop() {
  is_running_.store(false);
}

void InMemoryTransport::Send(Message message) {
  if (!is_running_.load()) {
    LOG_ERROR() << "Try to send message by not running transport!\n";
    return;
  }
  out_.Push(std::move(message));
}

std::optional<Message> InMemoryTransport::Recieve() {
  if (!is_running_.load()) {
    LOG_ERROR() << "Try to send message by not running transport!\n";
    return std::nullopt;
  }
  return in_.Pop();
}

[[nodiscard]] bool InMemoryTransport::IsRunning() const {
  return is_running_.load();
}

[[nodiscard]] bool InMemoryTransport::IsStreaming() const {
  return !in_.IsClosed();
}

void InMemoryTransport::StartStreaming() {
  end_of_stream_.store(false);
}

void InMemoryTransport::StopStreaming() {
  end_of_stream_.store(true);
  in_.Close();
}

void InMemoryTransport::Push(Message message) {
  if (!is_running_.load()) {
    LOG_ERROR() << "Try to push message to not running transport!\n";
    return;
  }
  in_.Push(std::move(message));
}

std::optional<Message> InMemoryTransport::Pop() {
  if (!is_running_.load()) {
    LOG_ERROR() << "Try to pop message from not running transport!\n";
    return std::nullopt;
  }
  return out_.Pop();
}

[[nodiscard]] std::size_t InMemoryTransport::InfligthResponses() const {
  return out_.Size();
}

[[nodiscard]] bool InMemoryTransport::HasInflightResponses() const {
  return !out_.IsEmpty();
}

[[nodiscard]] bool InMemoryTransport::HasNoInflightResponses() const {
  return out_.IsEmpty();
}

} // namespace maelstrom