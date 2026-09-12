#include <chrono>
#include <list>
#include <mutex>

#include <yaclib/async/make.hpp>

#include <maelstrom/detail/network/in_memory_transport.hpp>
#include <maelstrom/network/messages.hpp>
#include <maelstrom/node.hpp>
#include <maelstrom/routines/handler.hpp>
#include <maelstrom/routines/worker.hpp>

#include <gtest/gtest.h>

using namespace std::chrono_literals;

// Parse time
namespace nlohmann {

template <typename Rep, typename Period>
struct adl_serializer<std::chrono::duration<Rep, Period>> {
  static void from_json(const json &j,
                        std::chrono::duration<Rep, Period> &duration) {
    duration = std::chrono::duration<Rep, Period>{j.get<std::int64_t>()};
  }
};

} // namespace nlohmann

namespace maelstrom::tests {

struct EchoState {
  using Clock = std::chrono::steady_clock;

  std::mutex mtx{};

  struct Deferred {
    std::string source;
    std::string message;
    Clock::time_point deadline;
  };

  std::list<Deferred> deferreds{}; // Guarded by mtx
};

class EchoHandler final : public HandlerBase<EchoState> {
public:
  static constexpr std::string_view kType = "echo";

  yaclib::Future<Response> Handle(Network::Session session,
                                  Request request) override {
    using Clock = EchoState::Clock;

    auto &state = GetState();

    auto deferred = EchoState::Deferred{
      .source = request.source,
      .message = request.body["message"].get<std::string>(),
      .deadline = Clock::now() +
                  std::chrono::duration_cast<Clock::duration>(
                    request.body["delay_ms"].get<std::chrono::milliseconds>())};

    {
      std::lock_guard guard{state.mtx};
      state.deferreds.push_back(std::move(deferred));
    }

    return yaclib::MakeFuture(std::move(request).ToResponse());
  }
};

class EchoWorker final : public WorkerBase<EchoState> {
public:
  using WorkerBase<EchoState>::WorkerBase;

  static constexpr std::string_view kType = "echo";

  yaclib::Future<> Process(Network::Session session) override {
    using Clock = EchoState::Clock;

    auto &state = GetState();

    auto now = Clock::now();

    std::list<EchoState::Deferred> local_deferreds;
    {
      std::lock_guard guard{state.mtx};
      local_deferreds = std::move(state.deferreds);
    }

    for (auto &deferred : local_deferreds) {
      if (deferred.deadline < now) {
        auto body = nlohmann::json({});
        body["message"] = std::move(deferred.message);
        std::ignore = session.Send("echo_deferred", std::move(deferred.source),
                                   std::move(body));
      }
    }

    std::erase_if(local_deferreds,
                  [now](const auto &elem) { return elem.deadline < now; });

    if (local_deferreds.size() > 0) {
      std::lock_guard guard{state.mtx};
      state.deferreds.insert(state.deferreds.begin(), local_deferreds.begin(),
                             local_deferreds.end());
    }

    return yaclib::MakeFuture();
  }
};

} // namespace maelstrom::tests

class EchoDeferredTest : public ::testing::Test {
public:
  std::shared_ptr<maelstrom::InMemoryTransport> GetTransport() {
    return transport_;
  }

protected:
  void SetUp() override {
    transport_ = std::make_shared<maelstrom::InMemoryTransport>();

    node_ = std::make_shared<maelstrom::Node<maelstrom::tests::EchoState>>();
    node_->Add<maelstrom::tests::EchoHandler>();
    node_->Add<maelstrom::tests::EchoWorker>(50ms);

    node_->UseTransport(transport_);

    assistant_ = std::thread{[this, node = node_]() {
      try {
        node->Run();
      } catch (...) {
        has_not_catched_exceptions_.store(true);
      }
    }};

    while (!transport_->IsRunning()) {
      ;
    }

    // Send inital message
    {
      auto request = maelstrom::Request{
        .source = "c0",
        .destination = "n0",
        .type = "init",
        .body = R"({"node_id": "n0", "node_ids": ["n0"]})"_json,
        .message_id = 0};

      transport_->Push(std::move(request).ToMessage());
      std::ignore = transport_->Pop();
    }
  }

  void TearDown() override {
    transport_->StopStreaming();

    if (assistant_.joinable()) {
      assistant_.join();
    }

    EXPECT_TRUE(transport_->HasNoInflightResponses());
    EXPECT_FALSE(HasNotCatchedExceptions());

    transport_.reset();
    node_.reset();
  }

private:
  [[nodiscard]] bool HasNotCatchedExceptions() const {
    return has_not_catched_exceptions_.load();
  }

private:
  std::shared_ptr<maelstrom::InMemoryTransport> transport_;

  std::shared_ptr<maelstrom::Node<maelstrom::tests::EchoState>> node_;
  std::thread assistant_;

  std::atomic<bool> has_not_catched_exceptions_{false};
};

TEST_F(EchoDeferredTest, ValidDelay) {
  using Clock = std::chrono::steady_clock;

  auto transport = GetTransport();

  Clock::time_point start, end;

  {
    auto request = maelstrom::Request{
      .source = "c0",
      .destination = "n0",
      .type = "echo",
      .body = R"({"message": "some_message", "delay_ms": 5000})"_json,
      .message_id = 0};

    start = Clock::now();
    transport->Push(std::move(request).ToMessage());
    std::ignore = transport->Pop();
  }

  {
    auto message = transport->Pop();
    end = Clock::now();

    EXPECT_TRUE(message.has_value());
    EXPECT_TRUE(message.value().IsRequest());
    auto request = std::move(message.value()).ToRequest().value();
    EXPECT_EQ(request.source, "n0");
    EXPECT_EQ(request.destination, "c0");
    EXPECT_EQ(request.type, "echo_deferred");
    EXPECT_EQ(request.body["message"].get<std::string>(), "some_message");
  }

  EXPECT_GE(end - start, 5000ms);
  EXPECT_LE(end - start, 5100ms);

  transport->StopStreaming();
}

TEST_F(EchoDeferredTest, ValidDelaySequential) {
  using Clock = std::chrono::steady_clock;

  auto transport = GetTransport();

  constexpr std::size_t kTimes = 10;

  for (std::size_t i = 0; i < kTimes; ++i) {
    auto request = maelstrom::Request{
      .source = "c0",
      .destination = "n0",
      .type = "echo",
      .body = R"({"message": "some_message", "delay_ms": 500})"_json,
      .message_id = 0};

    auto start = Clock::now();
    transport->Push(std::move(request).ToMessage());
    std::ignore = transport->Pop();

    auto message = transport->Pop();
    auto end = Clock::now();
    EXPECT_GE(end - start, 500ms);
    EXPECT_LE(end - start, 600ms);
  }

  transport->StopStreaming();
}