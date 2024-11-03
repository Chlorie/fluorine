#pragma once

#include "player.h"

FLUORINE_SUPPRESS_EXPORT_WARNING

namespace flr
{
    class FLUORINE_API RandomPlayer final : public Player
    {
    public:
        [[nodiscard]] Coords get_move(const GameState& game) override;
    };
} // namespace flr

FLUORINE_RESTORE_EXPORT_WARNING
