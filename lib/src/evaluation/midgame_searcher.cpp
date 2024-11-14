#include "fluorine/evaluation/midgame_searcher.h"

#include <cmath>
#include <clu/static_vector.h>

#include "../utils/bit.h"

namespace flr
{
    namespace
    {
        constexpr int max_move_ordering_depth = 4;
        constexpr int min_negascout_depth = 4;

        auto generate_moves(GameRecord& record, const BitBoard move_mask, const bool sort_moves) noexcept
        {
            using MoveVec = clu::static_vector<Coords, cell_count>;
            if (!sort_moves)
            {
                MoveVec moves;
                for (const int move : SetBits{move_mask})
                    moves.emplace_back(static_cast<Coords>(move));
                return moves;
            }
            clu::static_vector<std::pair<Coords, int>, cell_count> weighted_moves;
            for (const int bit : SetBits{move_mask})
            {
                const auto move = static_cast<Coords>(bit);
                record.play(move);
                const int weight = std::popcount(record.current().legal_moves);
                weighted_moves.emplace_back(move, weight);
                record.undo();
            }
            std::ranges::sort(weighted_moves, std::less{}, &std::pair<Coords, int>::second);
            MoveVec res;
            for (const auto move : weighted_moves | std::views::keys)
                res.emplace_back(static_cast<Coords>(move));
            return res;
        }
    } // namespace

    MidgameSearcher::EvalResult MidgameSearcher::evaluate( //
        const GameState& state, const Evaluator& evaluator, const int depth)
    {
        nodes_ = 0;
        eval_ = &evaluator;
        record_.reset(state);
        tt_.clear();
        const float res = negascout(-inf, inf, depth, false);
        return {.traversed_nodes = nodes_, .score = res};
    }

    MidgameSearcher::SolveResult MidgameSearcher::search( //
        const GameState& state, const Evaluator& evaluator, const int depth)
    {
        nodes_ = 0;
        eval_ = &evaluator;
        record_.reset(state);
        tt_.clear();
        if (state.legal_moves == 0)
        {
            record_.play(Coords::none);
            const float score = -negascout(-inf, inf, depth, false);
            return {.traversed_nodes = nodes_, .score = score, .move = Coords::none};
        }
        SolveResult res{};
        for (const int move : SetBits{state.legal_moves})
        {
            const Coords move_coords = static_cast<Coords>(move);
            record_.play(move_coords);
            if (const float score = -negascout(-inf, -res.score, depth - 1, false); //
                score > res.score)
            {
                res.score = score;
                res.move = move_coords;
            }
            record_.undo();
        }
        res.traversed_nodes = nodes_;
        return res;
    }

    float MidgameSearcher::negamax(float alpha, const float beta, const int depth, const bool passed)
    {
        nodes_++;
        const GameState state = record_.current_canonical();
        if (depth == 0)
            return eval_->evaluate(state.board);
        const BitBoard moves = state.legal_moves;
        if (moves == 0)
        {
            if (passed)
                return static_cast<float>(state.final_score());
            record_.play(Coords::none);
            const float score = -negamax(-beta, -alpha, depth, true);
            record_.undo();
            return score;
        }
        for (const auto move : generate_moves(record_, moves, false))
        {
            record_.play(move);
            const float score = -negamax(-beta, -alpha, depth - 1, false);
            record_.undo();
            if (score > alpha)
            {
                if (score >= beta)
                    return score;
                alpha = score;
            }
        }
        return alpha;
    }

    float MidgameSearcher::negascout(float alpha, float beta, const int depth, const bool passed)
    {
        if (depth < min_negascout_depth)
            return negamax(alpha, beta, depth, passed);
        nodes_++;
        const GameState state = record_.current_canonical();
        const std::size_t hash = TranspositionTable::hash(state.board, depth);
        Bounds bounds{};
        if (const Bounds* ptr = tt_.try_load(state.board, depth, hash))
        {
            bounds = *ptr;
            if (bounds.upper <= alpha) // alpha-cut
                return bounds.upper;
            if (bounds.lower >= beta) // beta-cut
                return bounds.lower;
            if (bounds.upper == bounds.lower) // Got the exact value
                return bounds.lower;
            alpha = std::max(alpha, bounds.lower);
            beta = std::min(beta, bounds.upper);
        }
        float score = -inf;
        const BitBoard moves = state.legal_moves;
        const auto add_tt_entry = [&]
        {
            if (score <= alpha)
                tt_.store(state.board, depth, {bounds.lower, score}, hash);
            else if (score >= beta)
                tt_.store(state.board, depth, {score, bounds.upper}, hash);
            else
                tt_.store(state.board, depth, score, hash);
        };
        if (moves == 0) // Pass
        {
            if (passed)
            {
                score = static_cast<float>(state.final_score());
                tt_.store(state.board, depth, score, hash);
                return score;
            }
            record_.play(Coords::none);
            score = -negascout(-beta, -alpha, depth, true);
            record_.undo();
            add_tt_entry();
            return score;
        }
        for (const Coords move : generate_moves(record_, moves, true))
        {
            record_.play(move);
            const float lower = std::max(alpha, score);
            float new_score;
            if (lower == -inf)
                new_score = -negascout(-beta, inf, depth - 1, false);
            else
            {
                // Search with a null window
                new_score = -negascout(-std::nextafter(lower, inf), -lower, depth - 1, false);
                if (lower < new_score && new_score < beta) // Re-search
                    new_score = -negascout(-beta, -lower, depth - 1, false);
            }
            record_.undo();
            if (new_score > score)
            {
                score = new_score;
                if (score >= beta) // beta-cut
                    break;
            }
        }
        add_tt_entry();
        return score;
    }
} // namespace flr
