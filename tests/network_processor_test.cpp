#include <thread>

#include <maelstrom/detail/network/in_memory_transport.hpp>
#include <maelstrom/detail/processors/network.hpp>
#include <maelstrom/network/messages.hpp>

#include <gtest/gtest.h>

using namespace std::chrono_literals;

class NetworkProcessorTest : public ::testing::Test {
public:
  [[nodiscard]] std::shared_ptr<maelstrom::InMemoryTransport>
  GetTransport() const {
    return transport_;
  }

  [[nodiscard]] std::shared_ptr<maelstrom::detail::NetworkProcessor>
  GetProcessor() const {
    return processor_;
  }

protected:
  void SetUp() override {
    transport_ = std::make_shared<maelstrom::InMemoryTransport>();

    processor_ = std::make_shared<maelstrom::detail::NetworkProcessor>();
    processor_->UseTransport(transport_);

    transport_->Start();
  }

  void TearDown() override {
    transport_->Stop();

    transport_.reset();
    processor_.reset();
  }

private:
  std::shared_ptr<maelstrom::InMemoryTransport> transport_;
  std::shared_ptr<maelstrom::detail::NetworkProcessor> processor_;
};

TEST_F(NetworkProcessorTest, StartStopWithoutExceptions) {
  auto processor = GetProcessor();
  EXPECT_NO_THROW(processor->Start());
  EXPECT_NO_THROW(processor->Stop());
}

TEST_F(NetworkProcessorTest, SendOnce) {
  auto transport = GetTransport();

  auto processor = GetProcessor();
  processor->Start();

  {
    auto request =
      maelstrom::Request{.source = "n0",
                         .destination = "n1",
                         .type = "some_info_for_other_node",
                         .body = R"({"some_field_with_info": "67"})"_json,
                         .message_id = 10};

    std::ignore = processor->Send(std::move(request));
  }

  {
    auto message = transport->Pop();
    EXPECT_TRUE(message.has_value());
    EXPECT_TRUE(message.value().IsRequest());

    auto request = std::move(message.value()).ToRequest().value();
    EXPECT_EQ(request.source, "n0");
    EXPECT_EQ(request.destination, "n1");
    EXPECT_EQ(request.type, "some_info_for_other_node");
    EXPECT_TRUE(request.body.contains("some_field_with_info"));
    EXPECT_EQ(request.body["some_field_with_info"].get<std::string>(), "67");
    EXPECT_EQ(request.message_id, 10);
  }

  processor->Stop();

  EXPECT_TRUE(transport->HasNoInflightResponses());
}

TEST_F(NetworkProcessorTest, SendAtLeastOnceNoTimeout) {
  auto transport = GetTransport();

  auto processor = GetProcessor();
  processor->Start();

  {
    auto request =
      maelstrom::Request{.source = "n0",
                         .destination = "n1",
                         .type = "some_info_for_other_node",
                         .body = R"({"some_field_with_info": "67"})"_json,
                         .message_id = 10};

    std::ignore = processor->SendAtLeastOnce(std::move(request));
  }

  {
    std::this_thread::sleep_for(10s);

    auto response = maelstrom::Response{.source = "n1",
                                        .destination = "n0",
                                        .type = "some_info_for_other_node_ok",
                                        .body = nlohmann::json({}),
                                        .in_reply_to = 10};

    processor->Process(std::move(response));
  }

  processor->Stop();
}

TEST_F(NetworkProcessorTest, SendAtLeastOnceBeforeTimeout) {
  auto transport = GetTransport();

  auto processor = GetProcessor();
  processor->Start();

  {
    auto request =
      maelstrom::Request{.source = "n0",
                         .destination = "n1",
                         .type = "some_info_for_other_node",
                         .body = R"({"some_field_with_info": "67"})"_json,
                         .message_id = 10};

    std::ignore = processor->SendAtLeastOnce(std::move(request), 5s);
  }

  {
    std::this_thread::sleep_for(4s);

    EXPECT_GE(transport->InfligthResponses(), 4);
  }

  {
    auto response = maelstrom::Response{.source = "n1",
                                        .destination = "n0",
                                        .type = "some_info_for_other_node_ok",
                                        .body = nlohmann::json({}),
                                        .in_reply_to = 10};

    processor->Process(std::move(response));
  }

  processor->Stop();
}

TEST_F(NetworkProcessorTest, SendAtLeastOnceExpiredTimeout) {
  auto transport = GetTransport();

  auto processor = GetProcessor();
  processor->Start();

  {
    auto request =
      maelstrom::Request{.source = "n0",
                         .destination = "n1",
                         .type = "some_info_for_other_node",
                         .body = R"({"some_field_with_info": "67"})"_json,
                         .message_id = 10};

    std::ignore = processor->SendAtLeastOnce(std::move(request), 5s);
  }

  {
    std::this_thread::sleep_for(6s);

    EXPECT_GE(transport->InfligthResponses(), 4);
  }

  processor->Stop();
}

TEST_F(NetworkProcessorTest, CallOnceNoTimeout) {
  auto transport = GetTransport();

  auto processor = GetProcessor();
  processor->Start();

  yaclib::Future<maelstrom::Response> f;

  {
    auto request =
      maelstrom::Request{.source = "n0",
                         .destination = "n1",
                         .type = "some_info_for_other_node",
                         .body = R"({"some_field_with_info": "67"})"_json,
                         .message_id = 10};

    f = processor->Call(std::move(request));
  }

  {
    std::this_thread::sleep_for(10s);

    auto response = maelstrom::Response{.source = "n1",
                                        .destination = "n0",
                                        .type = "some_info_for_other_node_ok",
                                        .body = nlohmann::json({}),
                                        .in_reply_to = 10};

    processor->Process(std::move(response));
  }

  {
    EXPECT_TRUE(f.Ready());
    auto result = std::move(f).Get();
    EXPECT_TRUE(result);

    auto response = std::move(result).Ok();
    EXPECT_EQ(response.type, "some_info_for_other_node_ok");
    EXPECT_EQ(response.in_reply_to, 10);
  }

  processor->Stop();
}

TEST_F(NetworkProcessorTest, CallOnceBeforeTimeout) {
  auto transport = GetTransport();

  auto processor = GetProcessor();
  processor->Start();

  yaclib::Future<maelstrom::Response> f;

  {
    auto request =
      maelstrom::Request{.source = "n0",
                         .destination = "n1",
                         .type = "some_info_for_other_node",
                         .body = R"({"some_field_with_info": "67"})"_json,
                         .message_id = 10};

    f = processor->Call(std::move(request), 5s);
  }

  {
    std::this_thread::sleep_for(4s);
  }

  {
    auto response = maelstrom::Response{.source = "n1",
                                        .destination = "n0",
                                        .type = "some_info_for_other_node_ok",
                                        .body = nlohmann::json({}),
                                        .in_reply_to = 10};

    processor->Process(std::move(response));
  }

  {
    EXPECT_TRUE(f.Ready());
    auto result = std::move(f).Get();
    EXPECT_TRUE(result);

    auto response = std::move(result).Ok();
    EXPECT_EQ(response.type, "some_info_for_other_node_ok");
    EXPECT_EQ(response.in_reply_to, 10);
  }

  processor->Stop();
}

TEST_F(NetworkProcessorTest, CallOnceExpiredTimeout) {
  auto transport = GetTransport();

  auto processor = GetProcessor();
  processor->Start();

  yaclib::Future<maelstrom::Response> f;

  {
    auto request =
      maelstrom::Request{.source = "n0",
                         .destination = "n1",
                         .type = "some_info_for_other_node",
                         .body = R"({"some_field_with_info": "67"})"_json,
                         .message_id = 10};

    f = processor->Call(std::move(request), 5s);
  }

  {
    std::this_thread::sleep_for(6s);
  }

  {
    EXPECT_TRUE(f.Ready());
    auto result = std::move(f).Get();
    EXPECT_TRUE(result);
    auto response = std::move(result).Ok();
    EXPECT_TRUE(response.IsError());
    auto error = std::move(response).ToError().value();
    EXPECT_EQ(error.code, maelstrom::ErrorCode::Timeout);
  }

  processor->Stop();
}

TEST_F(NetworkProcessorTest, CallAtLeastOnceNoTimeout) {
  auto transport = GetTransport();

  auto processor = GetProcessor();
  processor->Start();

  yaclib::Future<maelstrom::Response> f;

  {
    auto request =
      maelstrom::Request{.source = "n0",
                         .destination = "n1",
                         .type = "some_info_for_other_node",
                         .body = R"({"some_field_with_info": "67"})"_json,
                         .message_id = 10};

    f = processor->CallAtLeastOnce(std::move(request));
  }

  {
    std::this_thread::sleep_for(10s);
    EXPECT_GE(transport->InfligthResponses(), 8);

    auto response = maelstrom::Response{.source = "n1",
                                        .destination = "n0",
                                        .type = "some_info_for_other_node_ok",
                                        .body = nlohmann::json({}),
                                        .in_reply_to = 10};

    processor->Process(std::move(response));
  }

  {
    EXPECT_TRUE(f.Ready());
    auto result = std::move(f).Get();
    EXPECT_TRUE(result);

    auto response = std::move(result).Ok();
    EXPECT_EQ(response.type, "some_info_for_other_node_ok");
    EXPECT_EQ(response.in_reply_to, 10);
  }

  processor->Stop();
}

TEST_F(NetworkProcessorTest, CallAtLeastOnceBeforeTimeout) {
  auto transport = GetTransport();

  auto processor = GetProcessor();
  processor->Start();

  yaclib::Future<maelstrom::Response> f;

  {
    auto request =
      maelstrom::Request{.source = "n0",
                         .destination = "n1",
                         .type = "some_info_for_other_node",
                         .body = R"({"some_field_with_info": "67"})"_json,
                         .message_id = 10};

    f = processor->CallAtLeastOnce(std::move(request), 5s);
  }

  {
    std::this_thread::sleep_for(4s);
    EXPECT_GE(transport->InfligthResponses(), 4);

    auto response = maelstrom::Response{.source = "n1",
                                        .destination = "n0",
                                        .type = "some_info_for_other_node_ok",
                                        .body = nlohmann::json({}),
                                        .in_reply_to = 10};

    processor->Process(std::move(response));
  }

  {
    EXPECT_TRUE(f.Ready());
    auto result = std::move(f).Get();
    EXPECT_TRUE(result);

    auto response = std::move(result).Ok();
    EXPECT_EQ(response.type, "some_info_for_other_node_ok");
    EXPECT_EQ(response.in_reply_to, 10);
  }

  processor->Stop();
}

TEST_F(NetworkProcessorTest, CallAtLeastOnceExpiredTimeout) {
  auto transport = GetTransport();

  auto processor = GetProcessor();
  processor->Start();

  yaclib::Future<maelstrom::Response> f;

  {
    auto request =
      maelstrom::Request{.source = "n0",
                         .destination = "n1",
                         .type = "some_info_for_other_node",
                         .body = R"({"some_field_with_info": "67"})"_json,
                         .message_id = 10};

    f = processor->CallAtLeastOnce(std::move(request), 5s);
  }

  {
    std::this_thread::sleep_for(6s);

    EXPECT_GE(transport->InfligthResponses(), 4);
  }

  {
    EXPECT_TRUE(f.Ready());
    auto result = std::move(f).Get();
    EXPECT_TRUE(result);
    auto response = std::move(result).Ok();
    EXPECT_TRUE(response.IsError());
    auto error = std::move(response).ToError().value();
    EXPECT_EQ(error.code, maelstrom::ErrorCode::Timeout);
  }

  processor->Stop();
}

TEST_F(NetworkProcessorTest, ManyCallOnceWithTimeout) {
  auto transport = GetTransport();

  auto processor = GetProcessor();
  processor->Start();

  constexpr std::size_t kCount = 5'000;
  constexpr std::size_t kCountExpired = 5'000;

  std::vector<yaclib::Future<maelstrom::Response>> fs;
  fs.reserve(kCount + kCountExpired);

  for (std::size_t i = 0; i < kCount; ++i) {
    auto request =
      maelstrom::Request{.source = "n0",
                         .destination = "n1",
                         .type = "some_info_for_other_node",
                         .body = R"({"some_field_with_info": "67"})"_json,
                         .message_id = i};

    fs.push_back(processor->Call(std::move(request), 5s));
  }

  for (std::size_t i = 0; i < kCountExpired; ++i) {
    auto request =
      maelstrom::Request{.source = "n0",
                         .destination = "n1",
                         .type = "some_info_for_other_node",
                         .body = R"({"some_field_with_info": "67"})"_json,
                         .message_id = i + kCountExpired};

    fs.push_back(processor->Call(std::move(request), 5s));
  }

  std::this_thread::sleep_for(4s);

  for (std::size_t i = 0; i < kCount; ++i) {
    auto response = maelstrom::Response{.source = "n1",
                                        .destination = "n0",
                                        .type = "some_info_for_other_node_ok",
                                        .body = nlohmann::json({}),
                                        .in_reply_to = i};

    processor->Process(std::move(response));
  }

  std::this_thread::sleep_for(2s);

  for (auto &&f : fs) {
    EXPECT_TRUE(f.Ready());
  }

  processor->Stop();
}

TEST_F(NetworkProcessorTest, CallOncePingPong) {
  auto transport = GetTransport();

  auto processor = GetProcessor();
  processor->Start();

  constexpr std::size_t kIterations = 10'000;

  yaclib::Future<maelstrom::Response> f;

  std::size_t message = 0;
  for (std::size_t i = 0; i < kIterations; ++i) {
    {
      auto body = nlohmann::json({});
      body["message"] = message;

      auto request = maelstrom::Request{.source = "n0",
                                        .destination = "n1",
                                        .type = "ping_pong",
                                        .body = std::move(body),
                                        .message_id = i};

      f = processor->Call(std::move(request));
    }

    {
      auto message = transport->Pop();
      EXPECT_TRUE(message.has_value());
      EXPECT_TRUE(message.value().IsRequest());

      auto request = std::move(message.value()).ToRequest().value();

      auto body = nlohmann::json({});
      body["message"] = request.body["message"].get<std::size_t>() + 1;

      auto response = std::move(request).ToResponse(std::move(body));
      processor->Process(std::move(response));
    }

    while (!f.Ready()) {
      ;
    }

    {
      EXPECT_TRUE(f.Ready());
      auto result = std::move(f).Get();
      EXPECT_TRUE(result);

      auto response = std::move(result).Ok();
      message = response.body["message"].get<std::size_t>();
    }
  }

  EXPECT_EQ(message, 10'000);

  processor->Stop();
}