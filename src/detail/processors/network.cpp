#include <fmt/format.h>
#include <yaclib/async/contract.hpp>
#include <yaclib/util/result.hpp>

#include <maelstrom/detail/processors/network.hpp>
#include <maelstrom/network/messages.hpp>

namespace maelstrom::detail {

namespace {

using namespace std::chrono_literals;
constexpr NetworkProcessor::Clock::duration kRepeatThreshold{1s};

} // namespace

void NetworkProcessor::Start() {
  std::exchange(is_running_, true);

  assistant_ = std::thread{[this]() { BackgroundUpdate(); }};
}

void NetworkProcessor::Stop() {
  std::exchange(is_running_, false);

  if (assistant_.joinable()) {
    assistant_.join();
  }
}

void NetworkProcessor::Process(Response response) {
  if (ProcessInternal(once_, response)) {
    return;
  }
  if (ProcessInternal(at_least_once_, response)) {
    return;
  }
}

void NetworkProcessor::UseTransport(std::shared_ptr<ITransport> transport) {
  transport_ = std::move(transport);
}

template <WaitPolicy P>
bool NetworkProcessor::ProcessInternal(Waiters<P> &waiters,
                                       Response &response) {
  yaclib::Promise<Response> p;

  {
    std::lock_guard guard{mtx_};

    auto it = waiters.find(response.in_reply_to);
    if (it == waiters.end()) {
      return false;
    }

    p = std::move(it->second.p);
    waiters.erase(it);
  }

  std::move(p).Set(std::move(response));
  return true;
}

void NetworkProcessor::SendDetached(Request request) {
  CallDetachedInternal(std::move(request));
}

yaclib::Future<>
NetworkProcessor::Send(Request request,
                       std::optional<Clock::duration> timeout) {
  return CallOnceInternal(std::move(request), timeout) //
    .ThenInline([](auto &&) {});
}

yaclib::Future<>
NetworkProcessor::SendAtLeastOnce(Request request,
                                  std::optional<Clock::duration> timeout) {
  return CallAtLeastOnceInternal(std::move(request), timeout)
    .ThenInline([](auto &&) {});
}

yaclib::Future<Response>
NetworkProcessor::Call(Request request,
                       std::optional<Clock::duration> timeout) {
  return CallOnceInternal(std::move(request), timeout);
}

yaclib::Future<Response>
NetworkProcessor::CallAtLeastOnce(Request request,
                                  std::optional<Clock::duration> timeout) {
  return CallAtLeastOnceInternal(std::move(request), timeout);
}

void NetworkProcessor::CallDetachedInternal(Request request) {
  transport_->Send(std::move(request).ToMessage());
}

yaclib::Future<Response>
NetworkProcessor::CallOnceInternal(Request request,
                                   std::optional<Clock::duration> timeout) {
  auto id = request.message_id;
  transport_->Send(std::move(request).ToMessage());

  auto [f, p] = yaclib::MakeContract<Response>();

  std::optional<Clock::time_point> deadline = std::nullopt;
  if (timeout.has_value()) {
    deadline.emplace(Clock::now() + timeout.value());
  }

  {
    std::lock_guard guard{mtx_};

    once_.emplace(
      id, Waiter<WaitPolicy::Once>{.p = std::move(p), .deadline = deadline});
  }

  return std::move(f);
}

yaclib::Future<Response> NetworkProcessor::CallAtLeastOnceInternal(
  Request request, std::optional<Clock::duration> timeout) {
  auto id = request.message_id;
  auto request_copy = request;
  transport_->Send(std::move(request).ToMessage());

  auto [f, p] = yaclib::MakeContract<Response>();

  auto now = Clock::now();

  std::optional<Clock::time_point> deadline = std::nullopt;
  if (timeout.has_value()) {
    deadline.emplace(now + timeout.value());
  }

  {
    std::lock_guard guard{mtx_};
    at_least_once_.emplace(id, Waiter<WaitPolicy::AtLeastOnce>{
                                 .p = std::move(p),
                                 .updated_at = now,
                                 .deadline = deadline,
                                 .request_copy = std::move(request_copy)});
  }

  return std::move(f);
}

void NetworkProcessor::BackgroundUpdate() {
  while (is_running_) {
    UpdateOnce();
    UpdateAtLeastOnce();
    // TODO(shpana): handle deadlines more precisely
    std::this_thread::sleep_for(kRepeatThreshold);
  }
}

void NetworkProcessor::UpdateOnce() {
  auto now = Clock::now();

  {
    std::lock_guard guard{mtx_};
    EraseExpiredDeadlines(once_, now);
  }
}

void NetworkProcessor::UpdateAtLeastOnce() {
  auto now = Clock::now();

  {
    std::lock_guard guard{mtx_};

    EraseExpiredDeadlines(at_least_once_, now);

    for (auto &[id, waiter] : at_least_once_) {
      if (waiter.updated_at + kRepeatThreshold < now) {
        auto request = waiter.request_copy;
        auto id = request.message_id;
        transport_->Send(std::move(request).ToMessage());

        waiter.updated_at = now;
      }
    }
  }
}

template <WaitPolicy P>
void NetworkProcessor::EraseExpiredDeadlines(Waiters<P> &waiters,
                                             Clock::time_point now) {
  for (auto &[id, waiter] : waiters) {
    if (waiter.deadline.has_value() && waiter.deadline.value() < now) {
      std::move(waiter.p).Set(Error{.source = "unknown",
                                    .destination = "unknown",
                                    .code = ErrorCode::Timeout,
                                    .what = "Timeout",
                                    .body = nlohmann::json({}),
                                    .in_reply_to = id}
                                .ToResponse());
    }
  }

  std::erase_if(waiters, [now](const auto &elem) {
    auto &deadline = elem.second.deadline;
    return deadline.has_value() && deadline.value() < now;
  });
}

} // namespace maelstrom::detail