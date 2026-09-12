#include <maelstrom/detail/network/in_memory_transport.hpp>
#include <maelstrom/node.hpp>
#include <maelstrom/utils/unit.hpp>

#include <gtest/gtest.h>

class InitTest : public ::testing::Test {
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
    node_->UseTransport(transport_);

    assistant_ = std::thread{[this, node = node_]() {
      try {
        node->Run();
      } catch (...) {
        has_not_catched_exceptions_.store(true);
      }
    }};
  }

  void TearDown() override {
    if (assistant_.joinable()) {
      assistant_.join();
    }

    transport_.reset();
    node_.reset();
  }

private:
  std::shared_ptr<maelstrom::InMemoryTransport> transport_;

  std::shared_ptr<maelstrom::Node<maelstrom::Unit>> node_;
  std::thread assistant_;

  std::atomic<bool> has_not_catched_exceptions_{false};
};

TEST_F(InitTest, Ok) {
  auto transport = GetTransport();
  while (!transport->IsRunning()) {
    ;
  }

  auto request =
    maelstrom::Request{.source = "c0",
                       .destination = "n0",
                       .type = "init",
                       .body = R"({"node_id": "n0", "node_ids": ["n0"]})"_json,
                       .message_id = 0};

  transport->Push(std::move(request).ToMessage());

  auto message = transport->Pop();
  EXPECT_TRUE(message.has_value());
  EXPECT_TRUE(message.value().IsResponse());

  auto response = std::move(message.value()).ToResponse().value();
  EXPECT_EQ(response.source, "n0");
  EXPECT_EQ(response.destination, "c0");
  EXPECT_EQ(response.type, "init_ok");
  EXPECT_EQ(response.in_reply_to, 0);

  transport->StopStreaming();
  while (transport->IsRunning()) {
    ;
  }

  EXPECT_TRUE(transport->HasNoInflightResponses());
  EXPECT_FALSE(HasNotCatchedExceptions());
}

TEST_F(InitTest, FailsWrongType) {
  auto transport = GetTransport();
  while (!transport->IsRunning()) {
    ;
  }

  auto request =
    maelstrom::Request{.source = "c0",
                       .destination = "n0",
                       .type = "some_wrong_type",
                       .body = R"({"node_id": "n0", "node_ids": ["n0"]})"_json,
                       .message_id = 0};

  transport->Push(std::move(request).ToMessage());

  transport->StopStreaming();
  while (transport->IsRunning()) {
    ;
  }

  EXPECT_TRUE(transport->HasNoInflightResponses());
  EXPECT_FALSE(HasNotCatchedExceptions());
}

TEST_F(InitTest, FailsWrongBody) {
  auto transport = GetTransport();
  while (!transport->IsRunning()) {
    ;
  }

  auto request =
    maelstrom::Request{.source = "c0",
                       .destination = "n0",
                       .type = "init",
                       .body = R"({"some_wrong_fields": "wrong_value"})"_json,
                       .message_id = 0};

  transport->Push(std::move(request).ToMessage());

  transport->StopStreaming();
  while (transport->IsRunning()) {
    ;
  }

  EXPECT_TRUE(transport->HasNoInflightResponses());
  EXPECT_FALSE(HasNotCatchedExceptions());
}