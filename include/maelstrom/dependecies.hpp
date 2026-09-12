#pragma once

#include <cstddef>
#include <memory>
#include <unordered_map>

namespace maelstrom {

namespace detail {

template <std::size_t i>
struct Counter {
  friend auto Count(Counter<i>);
};

template <typename T, std::size_t i, typename = void>
struct CountedTo {
  static constexpr bool kValue = false;
};
template <typename T, std::size_t i>
struct CountedTo<T, i, decltype(Count(Counter<i>{}))> {
  static constexpr bool kValue = true;
};

template <typename T, std::size_t i = 0>
struct Increase {
  static constexpr std::size_t kValue = Increase<T, i + 1>::kValue;
};
template <typename T, std::size_t i>
  requires(!CountedTo<T, i>::kValue)
struct Increase<T, i> {
  static constexpr std::size_t kValue = i;

  friend auto Count(Counter<i> counter) {};
};

template <typename T>
struct CountThrough {
  static constexpr std::size_t kValue = Increase<T>::kValue;
};

template <typename D>
struct Entry;

struct EntryBase {
  virtual ~EntryBase() = default;

  template <typename D>
  D &Get() {
    return dynamic_cast<Entry<D> &>(*this).content;
  }
};

template <typename D>
struct Entry final : EntryBase {
  D content;

  template <typename... Args>
  explicit Entry(Args &&...args) : content{D(std::forward<Args>(args)...)} {}

  ~Entry() override = default;
};

using Entries = std::unordered_map<std::size_t, std::unique_ptr<EntryBase>>;

} // namespace detail

class DependenciesStore;

class DependenciesProvider {
  friend class DependenciesStore;

public:
  template <typename D>
  D &Get() const {
    return entries_.at(detail::CountThrough<D>::kValue)->template Get<D>();
  }

private:
  explicit DependenciesProvider(detail::Entries &&entries)
    : entries_{std::move(entries)} {}

private:
  detail::Entries entries_;
};

class DependenciesStore {
public:
  template <typename D, typename... Args>
  void Add(Args &&...args) {
    auto hash = detail::CountThrough<D>::kValue;
    auto entry =
      std::make_unique<detail::Entry<D>>(std::forward<Args>(args)...);
    entries_.insert(
      {hash, std::unique_ptr<detail::EntryBase>{std::move(entry)}});
  }

  DependenciesProvider Build() && {
    return DependenciesProvider{std::move(entries_)};
  }

private:
  detail::Entries entries_;
};

} // namespace maelstrom