#pragma once

#include <cstdint>

#include "../core/macros.h"

FLUORINE_SUPPRESS_EXPORT_WARNING

namespace flr
{
    FLUORINE_API std::uint64_t perft(int depth) noexcept;
}

FLUORINE_RESTORE_EXPORT_WARNING
