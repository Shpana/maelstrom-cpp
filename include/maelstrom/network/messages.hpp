#pragma once

#include <optional>

#include <nlohmann/json.hpp>

namespace maelstrom {

struct Request;
struct Response;

struct Error;

enum struct ErrorCode : std::uint8_t {
  Timeout = 0,
  NodeNotFound = 1,
  NotSupported = 10,
  TemporarilyUnavailable = 11,
  MalformedRequest = 12,
  Crash = 13,
  Abort = 14,
  KeyDoesNotExists = 20,
  KeyAlreadyExists = 21,
  PreconditionFailed = 22,
  TxnConflict = 30
};

struct Message {
  std::string source;
  std::string destination;
  nlohmann::json body;

  [[nodiscard]] bool IsRequest() const;
  std::optional<Request> ToRequest() &&;

  [[nodiscard]] bool IsResponse() const;
  std::optional<Response> ToResponse() &&;

  nlohmann::json ToJson() &&;

  static std::optional<Message> Parse(const std::string &raw_message);
};

struct Request {
  std::string source;
  std::string destination;
  std::string type;
  nlohmann::json body;
  std::uint64_t message_id;

  Message ToMessage() &&;

  Response ToResponse(nlohmann::json body) &&;
  Response ToResponse() &&;

  Error ToError(ErrorCode code, std::string what = {}) &&;
};

struct Response {
  std::string source;
  std::string destination;
  std::string type;
  nlohmann::json body;
  std::uint64_t in_reply_to;

  Message ToMessage() &&;

  [[nodiscard]] bool IsError() const;
  std::optional<Error> ToError() &&;
};

struct Error {
  std::string source;
  std::string destination;
  ErrorCode code;
  std::string what;
  nlohmann::json body;
  std::uint64_t in_reply_to;

  Response ToResponse() &&;
};

} // namespace maelstrom