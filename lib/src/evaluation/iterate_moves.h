#pragma once

#include <clu/static_vector.h>

#include "fluorine/core/game.h"
#include "../utils/bit.h"

namespace flr
{
    using MoveVec = clu::static_vector<Coords, cell_count>;

    inline MoveVec iterate_moves(const BitBoard moves)
    {
        MoveVec res;
        for (const auto move : SetBits{moves})
            res.push_back(static_cast<Coords>(move));
        return res;
    }

    inline MoveVec sort_moves_wrt_mobility(const GameState& state) noexcept
    {
        if (std::has_single_bit(state.legal_moves))
            return {static_cast<Coords>(std::countr_zero(state.legal_moves))};
        clu::static_vector<std::pair<Coords, int>, cell_count> weighted_moves;
        for (const int bit : SetBits{state.legal_moves})
        {
            GameState s = state;
            const auto move = static_cast<Coords>(bit);
            s.play(move);
            const int weight = std::popcount(s.legal_moves);
            weighted_moves.emplace_back(move, weight);
        }
        std::ranges::sort(weighted_moves, std::less{}, &std::pair<Coords, int>::second);
        MoveVec res;
        for (const auto move : weighted_moves | std::views::keys)
            res.emplace_back(static_cast<Coords>(move));
        return res;
    }
} // namespace flr
