#pragma once

#include <clu/static_vector.h>

#include "transposition_table.h"
#include "evaluator.h"
#include "../core/game.h"

FLUORINE_SUPPRESS_EXPORT_WARNING

namespace flr
{
    class FLUORINE_API MidgameSearcher final
    {
    public:
        struct EvalResult final
        {
            std::size_t traversed_nodes = 0;
            float score = 0;
        };

        struct SolveResult final
        {
            std::size_t traversed_nodes = 0;
            float score = -inf;
            Coords move = Coords::none;
        };

        [[nodiscard]] EvalResult evaluate(const GameState& state, const Evaluator& evaluator, int depth);
        [[nodiscard]] SolveResult search(const GameState& state, const Evaluator& evaluator, int depth);
        [[nodiscard]] const auto& transposition_table() const noexcept { return tt_; }

    private:
        std::size_t nodes_ = 0;
        GameRecord record_;
        GameRecord temp_record_;
        TranspositionTable<float> tt_;
        const Evaluator* eval_ = nullptr;

        float negamax(float alpha, float beta, int depth, bool passed);
        float negascout(float alpha, float beta, int depth, bool passed, bool needs_shallow);
        clu::static_vector<Coords, cell_count> sort_moves(const GameState& state, float alpha, float beta, int depth);
    };
} // namespace flr

FLUORINE_RESTORE_EXPORT_WARNING
