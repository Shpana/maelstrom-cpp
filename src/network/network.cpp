#include <maelstrom/network/network.hpp>

namespace maelstrom {

Network::Network(detail::NetworkProcessor &processor) : processor_{processor} {}

void Network::Start(Environment env) {
  env_ = std::move(env);

  auto raw_node_id = std::string{env_.node_id.begin() + 1, env_.node_id.end()};
  jitter_ = std::stoull(raw_node_id);
}

void Network::Stop() {}

Network::Session Network::MakeSession() {
  return Session{*this};
}

std::uint64_t Network::GenerateNextId() {
  return 1000 * previous_id_.fetch_add(1) + jitter_;
}

Network::Session::Session(Network &network) : network_{network} {}

void Network::Session::SendDetached(std::string type, std::string destination,
                                    nlohmann::json body) {
  return network_.processor_.SendDetached(
    MakeRequest(std::move(type), std::move(destination), std::move(body)));
}

yaclib::Future<>
Network::Session::Send(std::string type, std::string destination,
                       nlohmann::json body,
                       std::optional<Network::Clock::duration> timeout) {
  return network_.processor_.Send(
    MakeRequest(std::move(type), std::move(destination), std::move(body)),
    timeout);
}

yaclib::Future<> Network::Session::SendAtLeastOnce(
  std::string type, std::string destination, nlohmann::json body,
  std::optional<Network::Clock::duration> timeout) {
  return network_.processor_.SendAtLeastOnce(
    MakeRequest(std::move(type), std::move(destination), std::move(body)),
    timeout);
}

yaclib::Future<Response> Network::Session::Session::Call(
  std::string type, std::string destination, nlohmann::json body,
  std::optional<Network::Clock::duration> timeout) {
  return network_.processor_.Call(
    MakeRequest(std::move(type), std::move(destination), std::move(body)),
    timeout);
}

yaclib::Future<Response> Network::Session::Session::CallAtLeastOnce(
  std::string type, std::string destination, nlohmann::json body,
  std::optional<Network::Clock::duration> timeout) {
  return network_.processor_.CallAtLeastOnce(
    MakeRequest(std::move(type), std::move(destination), std::move(body)),
    timeout);
}

Request Network::Session::MakeRequest(std::string type, std::string destination,
                                      nlohmann::json body) const {
  return Request{.source = network_.env_.node_id,
                 .destination = std::move(destination),
                 .type = std::move(type),
                 .body = std::move(body),
                 .message_id = network_.GenerateNextId()};
}

} // namespace maelstrom