#pragma once

#include "transposition_table.h"
#include "../core/game.h"

FLUORINE_SUPPRESS_EXPORT_WARNING

namespace flr
{
    class FLUORINE_API EndgameSolver final
    {
    public:
        struct EvalResult final
        {
            std::size_t traversed_nodes = 0;
            int score = 0;
        };

        struct SolveResult final
        {
            std::size_t traversed_nodes = 0;
            int score = -static_cast<int>(cell_count) - 1;
            Coords move = Coords::none;
        };

        [[nodiscard]] EvalResult evaluate(const GameState& state);
        [[nodiscard]] SolveResult solve(const GameState& state);
        [[nodiscard]] const TranspositionTable& transposition_table() const noexcept { return tt_; }
        void clear_transposition_table() noexcept { tt_.clear(); }

    private:
        std::size_t nodes_ = 0;
        TranspositionTable tt_;

        int negamax(const GameState& state, int alpha, int beta, int depth, bool passed);
        int negamax_last(const GameState& state, bool passed);
        int negascout(GameState state, int alpha, int beta, int depth, bool passed);
    };
} // namespace flr

FLUORINE_RESTORE_EXPORT_WARNING
