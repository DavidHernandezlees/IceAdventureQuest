#pragma once
#include "Puzzle.h"

namespace ice {
// Throws std::invalid_argument for mechanics the editable board cannot simulate.
Level loadOriginalLevel(const std::string &json);
}
