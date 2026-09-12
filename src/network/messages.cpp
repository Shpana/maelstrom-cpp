#include <maelstrom/log/logging.hpp>
#include <maelstrom/network/messages.hpp>

namespace maelstrom {

namespace {
constexpr std::string_view kErrorType = "error";
} // namespace

bool Message::IsRequest() const {
  return body.contains("type") && body.contains("msg_id");
}

std::optional<Request> Message::ToRequest() && {
  if (!IsRequest()) {
    return std::nullopt;
  }

  auto type = body["type"].get<std::string>();
  auto message_id = body["msg_id"].get<std::uint64_t>();

  return Request{.source = std::move(source),
                 .destination = std::move(destination),
                 .type = std::move(type),
                 .body = std::move(body),
                 .message_id = message_id};
}

bool Message::IsResponse() const {
  return body.contains("type") && body.contains("in_reply_to");
}

std::optional<Response> Message::ToResponse() && {
  if (!IsResponse()) {
    return std::nullopt;
  }

  auto type = body["type"].get<std::string>();
  auto in_reply_to = body["in_reply_to"].get<std::uint64_t>();

  return Response{.source = std::move(source),
                  .destination = std::move(destination),
                  .type = std::move(type),
                  .body = std::move(body),
                  .in_reply_to = in_reply_to};
}

nlohmann::json Message::ToJson() && {
  nlohmann::json json_message = nlohmann::json({});
  json_message["src"] = std::move(source);
  json_message["dest"] = std::move(destination);
  json_message["body"] = std::move(body);
  return json_message;
}

std::optional<Message> Message::Parse(const std::string &raw_message) {
  auto json = nlohmann::json::parse(raw_message);

  Message request;

  if (!json.contains("src") || !json.contains("dest") ||
      !json.contains("body")) {
    return std::nullopt;
  }

  return Message{.source = std::move(json["src"].get<std::string>()),
                 .destination = std::move(json["dest"].get<std::string>()),
                 .body = std::move(json["body"])};
}

Message Request::ToMessage() && {
  body["type"] = std::move(type);
  body["msg_id"] = message_id;

  return Message{.source = std::move(source),
                 .destination = std::move(destination),
                 .body = std::move(body)};
}

Response Request::ToResponse(nlohmann::json body) && {
  return Response{.source = std::move(destination),
                  .destination = std::move(source),
                  .type = type + "_ok",
                  .body = std::move(body),
                  .in_reply_to = message_id};
}

Response Request::ToResponse() && {
  return std::move(*this).ToResponse(nlohmann::json({}));
}

Error Request::ToError(ErrorCode code, std::string what) && {
  return Error{.source = std::move(destination),
               .destination = std::move(source),
               .code = code,
               .what = std::move(what),
               .body = nlohmann::json({}),
               .in_reply_to = message_id};
}

Message Response::ToMessage() && {
  body["type"] = std::move(type);
  body["in_reply_to"] = in_reply_to;

  return Message{.source = std::move(source),
                 .destination = std::move(destination),
                 .body = std::move(body)};
}

bool Response::IsError() const {
  return type == kErrorType && body.contains("code") && body.contains("text");
}

std::optional<Error> Response::ToError() && {
  if (!IsError()) {
    return std::nullopt;
  }

  return Error{.source = std::move(source),
               .destination = std::move(destination),
               .code = body["code"].get<ErrorCode>(),
               .what = std::move(body["text"].get<std::string>()),
               .body = std::move(body),
               .in_reply_to = in_reply_to};
}

Response Error::ToResponse() && {
  body["code"] = code;
  body["text"] = std::move(what);

  return Response{.source = std::move(source),
                  .destination = std::move(destination),
                  .type = std::string{kErrorType},
                  .body = std::move(body),
                  .in_reply_to = in_reply_to};
}

} // namespace maelstrom