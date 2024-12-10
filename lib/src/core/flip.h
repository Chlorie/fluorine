#pragma once

#include "fluorine/core/board.h"

namespace flr
{
    BitBoard find_flips(int placed, BitBoard self, BitBoard opponent) noexcept;
    int count_flips(int placed, BitBoard self, BitBoard opponent) noexcept;
}
