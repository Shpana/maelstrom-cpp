#pragma once

#include <string>
#include <vector>

namespace maelstrom {

// TODO(shpana): precalculate useful info about environment
// 1. Index of current node as integer
// 2. Indecies of available nodes as integers
// 3. Count of nodes
struct Environment {
  std::string node_id;
  std::vector<std::string> available_node_ids;
};

} // namespace maelstrom