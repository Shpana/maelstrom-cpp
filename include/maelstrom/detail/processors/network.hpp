#pragma once

#include <yaclib/async/future.hpp>
#include <yaclib/async/promise.hpp>
#include <yaclib/exe/executor.hpp>

#include <maelstrom/detail/network/transport.hpp>
#include <maelstrom/network/messages.hpp>

namespace maelstrom::detail {

enum struct WaitPolicy { Detached = 0, Once, AtLeastOnce };

class NetworkProcessor {
public:
  using Clock = std::chrono::steady_clock;

private:
  template <WaitPolicy P> struct Waiter;

  template <WaitPolicy P>
  using Waiters = std::unordered_map<std::uint64_t, Waiter<P>>;

public:
  void Start();
  void Stop();

  void UseTransport(std::shared_ptr<ITransport> transport);

  void Process(Response response);

  void SendDetached(Request request);
  yaclib::Future<> Send(Request request,
                        std::optional<Clock::duration> timeout = std::nullopt);
  yaclib::Future<>
  SendAtLeastOnce(Request request,
                  std::optional<Clock::duration> timeout = std::nullopt);
  yaclib::Future<Response>
  Call(Request request, std::optional<Clock::duration> timeout = std::nullopt);
  yaclib::Future<Response>
  CallAtLeastOnce(Request request,
                  std::optional<Clock::duration> timeout = std::nullopt);

private:
  template <WaitPolicy P>
  bool ProcessInternal(Waiters<P> &waiters, Response &response);

  // TODO(shpana): use cancellation tokens?
  void CallDetachedInternal(Request request);
  yaclib::Future<Response>
  CallOnceInternal(Request request, std::optional<Clock::duration> timeout);
  // TODO(shpana): make exponential backoffs strategy
  yaclib::Future<Response>
  CallAtLeastOnceInternal(Request request,
                          std::optional<Clock::duration> timeout);

  void BackgroundUpdate();

  void UpdateOnce();
  void UpdateAtLeastOnce();

  // Not thead-safe
  template <WaitPolicy P>
  void EraseExpiredDeadlines(Waiters<P> &waiters, Clock::time_point now);

private:
  bool is_running_{false};

  // TODO(shpana): use timers
  std::thread assistant_;

  std::shared_ptr<ITransport> transport_;

  std::mutex mtx_{};
  Waiters<WaitPolicy::Once> once_{};                 // Guarded by mtx_
  Waiters<WaitPolicy::AtLeastOnce> at_least_once_{}; // Guarded by mtx_
};

template <> struct NetworkProcessor::Waiter<WaitPolicy::Detached> {};

template <> struct NetworkProcessor::Waiter<WaitPolicy::Once> {
  yaclib::Promise<Response> p;
  std::optional<Clock::time_point> deadline;
};

template <> struct NetworkProcessor::Waiter<WaitPolicy::AtLeastOnce> {
  yaclib::Promise<Response> p;
  Clock::time_point updated_at;
  std::optional<Clock::time_point> deadline;
  Request request_copy;
};

} // namespace maelstrom::detail