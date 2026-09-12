#pragma once

#include <concepts>
#include <optional>

#include <yaclib/async/future.hpp>

#include <maelstrom/environment.hpp>
#include <maelstrom/log/logging.hpp>
#include <maelstrom/network/messages.hpp>
#include <maelstrom/network/network.hpp>

namespace maelstrom {

namespace detail {
template <typename State> class HandlersProcessor;
}

template <typename State> class HandlerBase {
public:
  virtual ~HandlerBase() = default;

  virtual void Start();
  virtual void Stop();

  virtual yaclib::Future<Response> Handle(Network::Session session,
                                          Request request) = 0;

protected:
  [[nodiscard]] State &GetState() const;
  [[nodiscard]] const Environment &GetEnvironment() const;

private:
  friend detail::HandlersProcessor<State>;

  void StartInternal(Environment env, std::shared_ptr<State> state);
  void StopInternal();

private:
  std::optional<const Environment> env_{std::nullopt};
  std::shared_ptr<State> state_;
};

template <typename Handler, typename State>
concept IsHandler =
  std::derived_from<Handler, HandlerBase<State>> && requires(Handler handler) {
    { Handler::kType };
  };

} // namespace maelstrom

template <typename State> void maelstrom::HandlerBase<State>::Start() {}

template <typename State> void maelstrom::HandlerBase<State>::Stop() {}

template <typename State>
void maelstrom::HandlerBase<State>::StartInternal(
  Environment env, std::shared_ptr<State> state) {
  env_.emplace(std::move(env));
  state_ = std::move(state);
}

template <typename State> void maelstrom::HandlerBase<State>::StopInternal() {}

template <typename State>
State &maelstrom::HandlerBase<State>::GetState() const {
  if (!state_) [[unlikely]] {
    LOG_ERROR() << "Cannot get state for uninitialized node!\n";
    throw std::runtime_error{"Cannot get state for uninitialized node!"};
  }

  return *state_;
}

template <typename State>
const maelstrom::Environment &
maelstrom::HandlerBase<State>::GetEnvironment() const {
  if (!env_.has_value()) [[unlikely]] {
    LOG_ERROR() << "Cannot get environment for uninitialized node!\n";
    throw std::runtime_error{"Cannot get environment for uninitialized node!"};
  }

  return env_.value();
}