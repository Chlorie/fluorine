#include "fluorine/arena/random_player.h"

#include <clu/random.h>

#include "../utils/bit.h"

namespace flr
{
    Coords RandomPlayer::get_move(const GameState& game)
    {
        if (game.legal_moves == 0)
            return Coords::none;
        const int total_moves = std::popcount(game.legal_moves);
        const int move_idx = clu::randint(0, total_moves - 1);
        const BitBoard move_mask = bit_expandr(1ull << move_idx, game.legal_moves);
        return static_cast<Coords>(std::countr_zero(move_mask));
    }
} // namespace flr
