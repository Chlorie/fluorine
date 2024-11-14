#include "fluorine/arena/searching_player.h"

namespace flr
{
    SearchingPlayer::SearchingPlayer(
        std::unique_ptr<const Evaluator> evaluator, const int mid_depth, const int end_depth):
        eval_(std::move(evaluator)),
        midgame_depth_(mid_depth), endgame_depth_(end_depth)
    {
        assert(eval_ != nullptr);
    }

    Coords SearchingPlayer::get_move(const GameState& game)
    {
        if (game.legal_moves == 0)
            return Coords::none;
        if (game.board.count_empty() <= endgame_depth_)
            return solver_.solve(game).move;
        return searcher_.search(game, *eval_, midgame_depth_).move;
    }
} // namespace flr
