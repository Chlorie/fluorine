#pragma once

#include "player.h"
#include "../evaluation/evaluator.h"
#include "../evaluation/midgame_searcher.h"
#include "../evaluation/endgame_solver.h"

FLUORINE_SUPPRESS_EXPORT_WARNING

namespace flr
{
    class FLUORINE_API SearchingPlayer final : public Player
    {
    public:
        SearchingPlayer(std::unique_ptr<const Evaluator> evaluator, int mid_depth, int end_depth);
        [[nodiscard]] Coords get_move(const GameState& game) override;
        [[nodiscard]] const Evaluator& get_evaluator() const noexcept { return *eval_; }
        [[nodiscard]] int midgame_depth() const noexcept { return midgame_depth_; }
        [[nodiscard]] int endgame_depth() const noexcept { return endgame_depth_; }

    private:
        std::unique_ptr<const Evaluator> eval_;
        int midgame_depth_;
        int endgame_depth_;
        MidgameSearcher searcher_;
        EndgameSolver solver_;
    };
} // namespace flr

FLUORINE_RESTORE_EXPORT_WARNING
