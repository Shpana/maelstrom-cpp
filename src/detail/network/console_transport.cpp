#include <maelstrom/detail/network/console_transport.hpp>
#include <maelstrom/log/logging.hpp>

namespace maelstrom {

void ConsoleTransport::Start() {
  is_running_.store(true);
}

void ConsoleTransport::Stop() {
  is_running_.store(false);
}

void ConsoleTransport::Send(Message message) {
  if (!is_running_.load()) {
    LOG_ERROR() << "Try to send message by not running transport!\n";
    return;
  }

  auto json = std::move(message).ToJson();

  LOG_DEBUG() << "<- " << json << std::endl;

  {
    std::lock_guard guard{mtx_};
    std::cout << json << std::endl;
  }
}

std::optional<Message> ConsoleTransport::Recieve() {
  if (!is_running_.load()) {
    LOG_ERROR() << "Try to send message by not running transport!\n";
    return std::nullopt;
  }

  std::string input;
  std::getline(std::cin, input);
  if (input.empty()) {
    end_of_stream_.store(true);
    return std::nullopt;
  }

  LOG_DEBUG() << "-> " << input << std::endl;

  auto message = Message::Parse(input);
  if (!message.has_value()) {
    return std::nullopt;
  }

  return message;
}

[[nodiscard]] bool ConsoleTransport::IsRunning() const {
  return is_running_.load();
}

[[nodiscard]] bool ConsoleTransport::IsStreaming() const {
  return !end_of_stream_.load();
}

} // namespace maelstrom