#pragma once

#include <expected>

#include <yaclib/async/make.hpp>

#include <maelstrom/network/network.hpp>

namespace maelstrom {

enum struct Consistency : uint8_t {
  Linearizable = 0,
  SequentialConsistent,
  LastWriteWins
};

namespace detail {

template <Consistency C> struct StorageType;

template <> struct StorageType<Consistency::Linearizable> {
  static constexpr std::string_view kType = "lin-kv";
};

template <> struct StorageType<Consistency::SequentialConsistent> {
  static constexpr std::string_view kType = "seq-kv";
};

template <> struct StorageType<Consistency::LastWriteWins> {
  static constexpr std::string_view kType = "lww-kv";
};

} // namespace detail

template <typename V, Consistency C>
class KeyValueStorage : public detail::StorageType<C> {
public:
  struct ReadHandler;
  struct WriteHandler;
  struct CompareAndSwapHandler;

public:
  explicit KeyValueStorage(Network::Session &session);

  yaclib::Future<std::expected<V, Error>>
  Read(std::string key,
       std::optional<Network::Clock::duration> timeout = std::nullopt);
  yaclib::Future<std::optional<Error>>
  Write(std::string key, V value,
        std::optional<Network::Clock::duration> timeout = std::nullopt);
  yaclib::Future<std::optional<Error>> CompareAndSwap(
    std::string key, V from, V to, bool create_if_not_exists = true,
    std::optional<Network::Clock::duration> timeout = std::nullopt);

private:
  Network::Session &session_;
};

template <typename V, Consistency C> struct KeyValueStorage<V, C>::ReadHandler {
  static constexpr std::string_view kType = "read";
};

template <typename V, Consistency C>
struct KeyValueStorage<V, C>::WriteHandler {
  static constexpr std::string_view kType = "write";
};

template <typename V, Consistency C>
struct KeyValueStorage<V, C>::CompareAndSwapHandler {
  static constexpr std::string_view kType = "cas";
};

} // namespace maelstrom

template <typename V, maelstrom::Consistency C>
maelstrom::KeyValueStorage<V, C>::KeyValueStorage(Network::Session &session)
  : session_{session} {}

template <typename V, maelstrom::Consistency C>
yaclib::Future<std::expected<V, maelstrom::Error>>
maelstrom::KeyValueStorage<V, C>::Read(
  std::string key, std::optional<Network::Clock::duration> timeout) {
  auto body = nlohmann::json({});
  body["key"] = std::move(key);

  auto response = co_await session_.Call<ReadHandler>(
    std::string{KeyValueStorage::kType}, std::move(body), timeout);

  if (response.IsError()) {
    co_return std::unexpected{std::move(response).ToError().value()};
  }

  co_return response.body["value"].template get<V>();
}

template <typename V, maelstrom::Consistency C>
yaclib::Future<std::optional<maelstrom::Error>>
maelstrom::KeyValueStorage<V, C>::Write(
  std::string key, V value, std::optional<Network::Clock::duration> timeout) {
  auto body = nlohmann::json({});
  body["key"] = std::move(key);
  body["value"] = std::move(value);

  auto response = co_await session_.Call<WriteHandler>(
    std::string{KeyValueStorage::kType}, std::move(body), timeout);

  if (response.IsError()) {
    co_return std::move(response).ToError().value();
  }

  co_return std::nullopt;
}

template <typename V, maelstrom::Consistency C>
yaclib::Future<std::optional<maelstrom::Error>>
maelstrom::KeyValueStorage<V, C>::CompareAndSwap(
  std::string key, V from, V to, bool create_if_not_exists,
  std::optional<Network::Clock::duration> timeout) {
  auto body = nlohmann::json({});
  body["key"] = std::move(key);
  body["from"] = std::move(from);
  body["to"] = std::move(to);
  body["create_if_not_exists"] = create_if_not_exists;

  auto response = co_await session_.Call<CompareAndSwapHandler>(
    std::string{KeyValueStorage::kType}, std::move(body), timeout);

  if (response.IsError()) {
    co_return std::move(response).ToError().value();
  }

  co_return std::nullopt;
}