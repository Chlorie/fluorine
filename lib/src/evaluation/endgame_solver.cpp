#include "fluorine/evaluation/endgame_solver.h"

#include <clu/static_vector.h>

#include "iterate_moves.h"
#include "../utils/bit.h"

namespace flr
{
    namespace
    {
        constexpr int min_negascout_depth = 6;
        constexpr int int_inf = cell_count + 1;
    } // namespace

    EndgameSolver::EvalResult EndgameSolver::evaluate(const GameState& state)
    {
        nodes_ = 0;
        record_.reset(state); // No need to reset the transposition table
        const int depth = state.board.count_empty();
        const int res = negascout(-int_inf, int_inf, depth, false);
        return {.traversed_nodes = nodes_, .score = static_cast<int>(res)};
    }

    EndgameSolver::SolveResult EndgameSolver::solve(const GameState& state)
    {
        nodes_ = 0;
        record_.reset(state);
        const int depth = state.board.count_empty();
        if (state.legal_moves == 0)
        {
            record_.play(Coords::none);
            const int score = -negascout(-int_inf, int_inf, depth, true);
            return {.traversed_nodes = nodes_, .score = static_cast<int>(score), .move = Coords::none};
        }
        SolveResult res;
        for (const int move : SetBits{state.legal_moves})
        {
            const Coords move_coords = static_cast<Coords>(move);
            record_.play(move_coords);
            if (const auto score = -negascout(-int_inf, -res.score, depth - 1, false); //
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

    int EndgameSolver::negamax(int alpha, const int beta, const int depth, const bool passed)
    {
        switch (depth)
        {
            case 0: nodes_++; return record_.current().disk_difference();
            case 1: return negamax_last(passed);
            default:;
        }
        nodes_++;
        const GameState state = record_.current();
        const BitBoard moves = state.legal_moves;
        if (moves == 0)
        {
            if (passed)
                return state.final_score();
            record_.play(Coords::none);
            const int score = -negamax(-beta, -alpha, depth, true);
            record_.undo();
            return score;
        }
        for (const auto move : SetBits{moves})
        {
            const Coords move_coords = static_cast<Coords>(move);
            record_.play(move_coords);
            const int score = -negamax(-beta, -alpha, depth - 1, false);
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

    int EndgameSolver::negamax_last(const bool passed)
    {
        nodes_++;
        const GameState state = record_.current();
        const BitBoard moves = state.legal_moves;
        if (moves == 0)
        {
            if (passed)
                return state.final_score();
            record_.play(Coords::none);
            const int score = -negamax_last(true);
            record_.undo();
            return score;
        }
        record_.play(static_cast<Coords>(std::countr_zero(moves)));
        const int score = -record_.current().disk_difference();
        record_.undo();
        return score;
    }

    int EndgameSolver::negascout(int alpha, int beta, const int depth, const bool passed)
    {
        if (depth < min_negascout_depth)
            return negamax(alpha, beta, depth, passed);
        nodes_++;
        const GameState state = record_.current_canonical();
        const int lookahead = static_cast<int>(state.current);
        const std::size_t hash = TranspositionTable::hash(state.board);
        Bounds bounds{-static_cast<float>(int_inf), static_cast<float>(int_inf)};
        if (const Bounds* ptr = tt_.try_load(state.board, lookahead, hash))
        {
            bounds = *ptr;
            const int upper = static_cast<int>(bounds.upper), lower = static_cast<int>(bounds.lower);
            if (upper <= alpha) // alpha-cut
                return upper;
            if (lower >= beta) // beta-cut
                return lower;
            if (upper == lower) // Got the exact value
                return lower;
            alpha = std::max(alpha, lower);
            beta = std::min(beta, upper);
        }
        int score = -int_inf;
        const BitBoard moves = state.legal_moves;
        const auto add_tt_entry = [&]
        {
            if (score <= alpha)
                tt_.store(state.board, lookahead, {bounds.lower, static_cast<float>(score)}, hash);
            else if (score >= beta)
                tt_.store(state.board, lookahead, {static_cast<float>(score), bounds.upper}, hash);
            else
                tt_.store(state.board, lookahead, static_cast<float>(score), hash);
        };
        if (moves == 0) // Pass
        {
            if (passed)
            {
                score = state.final_score();
                tt_.store(state.board, lookahead, static_cast<float>(score), hash);
                return score;
            }
            record_.play(Coords::none);
            score = -negascout(-beta, -alpha, depth, true);
            record_.undo();
            add_tt_entry();
            return score;
        }
        for (const Coords move : sort_moves_wrt_mobility(state))
        {
            record_.play(move);
            const int lower = std::max(alpha, score);
            int new_score;
            if (lower == -int_inf)
                new_score = -negascout(-beta, int_inf, depth - 1, false);
            else
            {
                // Search with a null window
                new_score = -negascout(-lower - 1, -lower, depth - 1, false);
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
