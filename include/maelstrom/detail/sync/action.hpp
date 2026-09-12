#pragma once

#include <functional>

#include <yaclib/async/future.hpp>

namespace maelstrom::detail {

using Action = std::function<void()>;

} // namespace maelstrom::detail