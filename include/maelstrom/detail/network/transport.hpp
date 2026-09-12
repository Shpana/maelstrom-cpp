#pragma once

#include <fmt/format.h>

#include <maelstrom/network/messages.hpp>

namespace maelstrom {

class ITransport {
public:
  virtual ~ITransport() = default;

  virtual void Start() = 0;
  virtual void Stop() = 0;

  virtual void Send(Message message) = 0;
  virtual std::optional<Message> Recieve() = 0;

  [[nodiscard]] virtual bool IsRunning() const = 0;
  [[nodiscard]] virtual bool IsStreaming() const = 0;
};

} // namespace maelstrom