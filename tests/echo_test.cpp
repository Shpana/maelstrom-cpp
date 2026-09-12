#include <memory>

#include <yaclib/async/make.hpp>

#include <maelstrom/detail/network/in_memory_transport.hpp>
#include <maelstrom/network/messages.hpp>
#include <maelstrom/node.hpp>
#include <maelstrom/routines/handler.hpp>
#include <maelstrom/utils/unit.hpp>

#include <gtest/gtest.h>

namespace maelstrom::tests {

class EchoHandler final : public HandlerBase<Unit> {
public:
  static constexpr std::string_view kType = "echo";

  yaclib::Future<Response> Handle(Network::Session session,
                                  Request request) override {
    auto echo = nlohmann::json({});
    echo["message"] = request.body["message"].get<std::string>();
    return yaclib::MakeFuture(std::move(request).ToResponse(std::move(echo)));
  }
};

} // namespace maelstrom::tests

class EchoTest : public ::testing::Test {
public:
  std::shared_ptr<maelstrom::InMemoryTransport> GetTransport() {
    return transport_;
  }

  [[nodiscard]] bool HasNotCatchedExceptions() const {
    return has_not_catched_exceptions_.load();
  }

protected:
  void SetUp() override {
    transport_ = std::make_shared<maelstrom::InMemoryTransport>();

    node_ = std::make_shared<maelstrom::Node<maelstrom::Unit>>();
    node_->Add<maelstrom::tests::EchoHandler>();
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
  std::shared_ptr<maelstrom::InMemoryTransport> transport_;

  std::shared_ptr<maelstrom::Node<maelstrom::Unit>> node_;
  std::thread assistant_;

  std::atomic<bool> has_not_catched_exceptions_{false};
};

TEST_F(EchoTest, RequestOk) {
  auto transport = GetTransport();

  auto request = maelstrom::Request{.source = "c0",
                                    .destination = "n0",
                                    .type = "echo",
                                    .body = R"({"message": "42"})"_json,
                                    .message_id = 0};

  transport->Push(std::move(request).ToMessage());

  auto message = transport->Pop();
  EXPECT_TRUE(message.has_value());
  EXPECT_TRUE(message.value().IsResponse());

  auto response = std::move(message.value()).ToResponse().value();
  EXPECT_EQ(response.source, "n0");
  EXPECT_EQ(response.destination, "c0");
  EXPECT_EQ(response.type, "echo_ok");
  EXPECT_EQ(response.in_reply_to, 0);

  EXPECT_TRUE(response.body.contains("message"));
  EXPECT_EQ(response.body["message"].get<std::string>(), "42");
}

TEST_F(EchoTest, RequestFailWrongType) {
  auto transport = GetTransport();

  auto request = maelstrom::Request{.source = "c0",
                                    .destination = "n0",
                                    .type = "some_wrong_type",
                                    .body = R"({"message": "42"})"_json,
                                    .message_id = 0};

  transport->Push(std::move(request).ToMessage());
}

TEST_F(EchoTest, ManyRequests) {
  auto transport = GetTransport();

  constexpr std::size_t kCount = 10'000;

  for (std::size_t i = 0; i < kCount; ++i) {
    auto body = nlohmann::json({});
    body["message"] = std::to_string(i);

    auto request = maelstrom::Request{.source = "c0",
                                      .destination = "n0",
                                      .type = "echo",
                                      .body = std::move(body),
                                      .message_id = i};

    transport->Push(std::move(request).ToMessage());
  }

  std::vector<std::size_t> echos;
  echos.reserve(kCount);

  for (std::size_t i = 0; i < kCount; ++i) {
    auto message = transport->Pop();
    EXPECT_TRUE(message.has_value());
    EXPECT_TRUE(message.value().IsResponse());

    auto response = std::move(message.value()).ToResponse().value();
    echos.push_back(std::stoull(response.body["message"].get<std::string>()));
  }

  EXPECT_EQ(echos.size(), kCount);

  std::sort(echos.begin(), echos.end());
  for (std::size_t i = 0; i < kCount; ++i) {
    EXPECT_EQ(echos[i], i);
  }
}
