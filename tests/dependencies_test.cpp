#include <maelstrom/dependecies.hpp>

#include <gtest/gtest.h>

TEST(Dependencies, Trivial) {
  maelstrom::DependenciesStore store;
  store.Add<int>(42);
  store.Add<float>(67.0f);

  auto provider = std::move(store).Build();
  EXPECT_EQ(provider.Get<int>(), 42);
  EXPECT_EQ(provider.Get<float>(), 67.0f);
}

TEST(Dependencies, Custom) {
  struct S1 {
    int value;
  };

  struct S2 {
    float value;
  };

  maelstrom::DependenciesStore store;
  store.Add<S1>(42);
  store.Add<S2>(67.0f);

  auto provider = std::move(store).Build();
  EXPECT_EQ(provider.Get<S1>().value, 42);
  EXPECT_EQ(provider.Get<S2>().value, 67.0f);
}

TEST(Dependencies, MoveOnly) {
  struct S {
    int value;

    explicit S(int value) : value(value) {}

    // Non-copyable
    S(const S &other) = delete;
    S &operator=(const S &other) = delete;

    // Movable
    S(S &&other) = default;
    S &operator=(S &&other) = default;
  };

  maelstrom::DependenciesStore store;
  store.Add<S>(42);

  auto provider = std::move(store).Build();
  EXPECT_EQ(provider.Get<S>().value, 42);
}