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

    private:
        std::size_t nodes_ = 0;
        GameRecord record_;
        TranspositionTable tt_;

        float negamax(float alpha, float beta, int depth, bool passed);
        float negascout(float alpha, float beta, int depth, bool passed);
    };
} // namespace flr

FLUORINE_RESTORE_EXPORT_WARNING
