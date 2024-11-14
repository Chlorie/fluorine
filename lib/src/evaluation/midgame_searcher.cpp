#include "fluorine/evaluation/midgame_searcher.h"

#include <cmath>
#include <clu/static_vector.h>

#include "iterate_moves.h"
#include "../utils/bit.h"

namespace flr
{
    namespace
    {
        constexpr int min_negascout_depth = 4;
        constexpr int min_shallow_search_required_depth = 10;
    } // namespace

    MidgameSearcher::EvalResult MidgameSearcher::evaluate( //
        const GameState& state, const Evaluator& evaluator, const int depth)
    {
        nodes_ = 0;
        eval_ = &evaluator;
        record_.reset(state);
        tt_.clear();
        const float res = negascout(-inf, inf, depth, false, true);
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
            const float score = -negascout(-inf, inf, depth, true, true);
            return {.traversed_nodes = nodes_, .score = score, .move = Coords::none};
        }
        SolveResult res{};
        const auto sorted_moves = depth >= min_shallow_search_required_depth //
            ? sort_moves(state, -inf, inf, depth / 2)
            : sort_moves_wrt_mobility(state);
        for (const Coords move : sorted_moves)
        {
            record_.play(move);
            if (const float score = -negascout(-inf, -res.score, depth - 1, false, true); //
                score > res.score)
            {
                res.score = score;
                res.move = move;
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
        for (const auto move : SetBits{moves})
        {
            const Coords move_coords = static_cast<Coords>(move);
            record_.play(move_coords);
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

    float MidgameSearcher::negascout(
        float alpha, float beta, const int depth, const bool passed, const bool needs_shallow)
    {
        if (depth < min_negascout_depth)
            return negamax(alpha, beta, depth, passed);
        nodes_++;
        const GameState state = record_.current_canonical();
        const std::size_t hash = TranspositionTable::hash(state.board);
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
            score = -negascout(-beta, -alpha, depth, true, needs_shallow);
            record_.undo();
            add_tt_entry();
            return score;
        }
        const auto sorted_moves = needs_shallow && depth >= min_shallow_search_required_depth
            ? sort_moves(state, alpha, beta, depth / 2)
            : sort_moves_wrt_mobility(state);
        for (const Coords move : sorted_moves)
        {
            record_.play(move);
            const float lower = std::max(alpha, score);
            float new_score;
            if (lower == -inf)
                new_score = -negascout(-beta, inf, depth - 1, false, needs_shallow);
            else
            {
                // Search with a null window
                new_score = -negascout(-std::nextafter(lower, inf), -lower, depth - 1, false, needs_shallow);
                if (lower < new_score && new_score < beta) // Re-search
                    new_score = -negascout(-beta, -lower, depth - 1, false, needs_shallow);
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

    clu::static_vector<Coords, cell_count> MidgameSearcher::sort_moves(
        const GameState& state, const float alpha, const float beta, const int depth)
    {
        if (std::has_single_bit(state.legal_moves))
            return {static_cast<Coords>(std::countr_zero(state.legal_moves))};
        clu::static_vector<std::pair<Coords, float>, cell_count> weighted_moves;
        std::swap(record_, temp_record_);
        for (const int bit : SetBits{state.legal_moves})
        {
            GameState s = state;
            const auto move = static_cast<Coords>(bit);
            s.play(move);
            record_.reset(s);
            const float weight = -negascout(-beta, -alpha, depth, false, false);
            weighted_moves.emplace_back(move, weight);
        }
        std::swap(record_, temp_record_);
        std::ranges::sort(weighted_moves, std::greater{}, &std::pair<Coords, float>::second);
        clu::static_vector<Coords, cell_count> res;
        for (const auto move : weighted_moves | std::views::keys)
            res.emplace_back(static_cast<Coords>(move));
        return res;
    }
} // namespace flr
