#pragma once

#include <chrono>

#include <nlohmann/json.hpp>
#include <yaclib/async/future.hpp>

#include <maelstrom/detail/processors/network.hpp>
#include <maelstrom/environment.hpp>
#include <maelstrom/network/messages.hpp>

namespace maelstrom {

class Network {
public:
  class Session;

  using Clock = std::chrono::steady_clock;

public:
  explicit Network(detail::NetworkProcessor &processor);

  void Start(Environment env);
  void Stop();

  [[nodiscard]] Session MakeSession();

private:
  [[nodiscard]] std::uint64_t GenerateNextId();

private:
  detail::NetworkProcessor &processor_;
  Environment env_;

  std::atomic<std::uint64_t> previous_id_{0};
  std::uint64_t jitter_{};
};

class Network::Session {
public:
  explicit Session(Network &network);

  // Non-copyable
  Session(const Session &) = delete;
  Session &operator=(const Session &) = delete;
  // Movable
  Session(Session &&) = default;
  // Non move-assignable
  Session &operator=(Session &&) = delete;

  ~Session() = default;

  void SendDetached(std::string type, std::string destination,
                    nlohmann::json body);
  yaclib::Future<>
  Send(std::string type, std::string destination, nlohmann::json body,
       std::optional<Network::Clock::duration> timeout = std::nullopt);
  yaclib::Future<> SendAtLeastOnce(
    std::string type, std::string destination, nlohmann::json body,
    std::optional<Network::Clock::duration> timeout = std::nullopt);
  yaclib::Future<Response>
  Call(std::string type, std::string destination, nlohmann::json body,
       std::optional<Network::Clock::duration> timeout = std::nullopt);
  yaclib::Future<Response> CallAtLeastOnce(
    std::string type, std::string destination, nlohmann::json body,
    std::optional<Network::Clock::duration> timeout = std::nullopt);

  template <typename Handler>
  void SendDetached(std::string destination, nlohmann::json body);
  template <typename Handler>
  yaclib::Future<>
  Send(std::string destination, nlohmann::json body,
       std::optional<Network::Clock::duration> timeout = std::nullopt);
  template <typename Handler>
  yaclib::Future<> SendAtLeastOnce(
    std::string destination, nlohmann::json body,
    std::optional<Network::Clock::duration> timeout = std::nullopt);
  template <typename Handler>
  yaclib::Future<Response>
  Call(std::string destination, nlohmann::json body,
       std::optional<Network::Clock::duration> timeout = std::nullopt);
  template <typename Handler>
  yaclib::Future<Response> CallAtLeastOnce(
    std::string destination, nlohmann::json body,
    std::optional<Network::Clock::duration> timeout = std::nullopt);

private:
  [[nodiscard]] Request MakeRequest(std::string type, std::string destination,
                                    nlohmann::json body) const;

private:
  Network &network_;
};

} // namespace maelstrom

template <typename Handler>
void maelstrom::Network::Session::SendDetached(std::string destination,
                                               nlohmann::json body) {
  return SendDetached(std::string{Handler::kType}, std::move(destination),
                      std::move(body));
}

template <typename Handler>
yaclib::Future<> maelstrom::Network::Session::Send(
  std::string destination, nlohmann::json body,
  std::optional<Network::Clock::duration> timeout) {
  return Send(std::string{Handler::kType}, std::move(destination),
              std::move(body), timeout);
}

template <typename Handler>
yaclib::Future<> maelstrom::Network::Session::SendAtLeastOnce(
  std::string destination, nlohmann::json body,
  std::optional<Network::Clock::duration> timeout) {
  return SendAtLeastOnce(std::string{Handler::kType}, std::move(destination),
                         std::move(body));
}

template <typename Handler>
yaclib::Future<maelstrom::Response> maelstrom::Network::Session::Session::Call(
  std::string destination, nlohmann::json body,
  std::optional<Network::Clock::duration> timeout) {
  return Call(std::string{Handler::kType}, std::move(destination),
              std::move(body), timeout);
}

template <typename Handler>
yaclib::Future<maelstrom::Response>
maelstrom::Network::Session::Session::CallAtLeastOnce(
  std::string destination, nlohmann::json body,
  std::optional<Network::Clock::duration> timeout) {
  return CallAtLeastOnce(std::string{Handler::kType}, std::move(destination),
                         std::move(body), timeout);
}