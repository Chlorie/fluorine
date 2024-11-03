#pragma once

#include "../core/game.h"

FLUORINE_SUPPRESS_EXPORT_WARNING

namespace flr
{
    class FLUORINE_API Player
    {
    public:
        Player() noexcept = default;
        virtual ~Player() noexcept = default;
        Player(const Player&) = delete;
        Player(Player&&) = delete;
        Player& operator=(const Player&) = delete;
        Player& operator=(Player&&) = delete;

        [[nodiscard]] virtual Coords get_move(const GameState& game) = 0;
    };
} // namespace flr

FLUORINE_RESTORE_EXPORT_WARNING
