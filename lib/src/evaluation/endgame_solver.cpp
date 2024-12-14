#include "fluorine/evaluation/endgame_solver.h"

#include <clu/static_vector.h>

#include "iterate_moves.h"
#include "../core/flip.h"
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
        const int depth = state.board.count_empty();
        const int res = negascout(state, -int_inf, int_inf, depth, false);
        return {.traversed_nodes = nodes_, .score = static_cast<int>(res)};
    }

    EndgameSolver::SolveResult EndgameSolver::solve(const GameState& state)
    {
        nodes_ = 0;
        const int depth = state.board.count_empty();
        if (state.legal_moves == 0)
        {
            const int score = -negascout(state.play_copied(Coords::none), -int_inf, int_inf, depth, true);
            return {.traversed_nodes = nodes_, .score = static_cast<int>(score), .move = Coords::none};
        }
        SolveResult res;
        for (const int move : SetBits{state.legal_moves})
        {
            const Coords move_coords = static_cast<Coords>(move);
            if (const auto score = -negascout(state.play_copied(move_coords), //
                    -int_inf, -res.score, depth - 1, false); //
                score > res.score)
            {
                res.score = score;
                res.move = move_coords;
            }
        }
        res.traversed_nodes = nodes_;
        return res;
    }

    int EndgameSolver::negamax(const GameState& state, int alpha, const int beta, const int depth, const bool passed)
    {
        switch (depth)
        {
            case 0: nodes_++; return state.disk_difference();
            case 1: return negamax_last(state, passed);
            default:;
        }
        nodes_++;
        const BitBoard moves = state.legal_moves;
        if (moves == 0)
        {
            if (passed)
                return state.final_score();
            const int score = -negamax(state.play_copied(Coords::none), -beta, -alpha, depth, true);
            return score;
        }
        for (const auto move : SetBits{moves})
        {
            if (const int score = -negamax(state.play_copied(static_cast<Coords>(move)), //
                    -beta, -alpha, depth - 1, false);
                score > alpha)
            {
                if (score >= beta)
                    return score;
                alpha = score;
            }
        }
        return alpha;
    }

    int EndgameSolver::negamax_last(const GameState& state, const bool passed)
    {
        nodes_++;
        const BitBoard moves = state.legal_moves;
        if (moves == 0)
        {
            if (passed)
                return state.final_score();
            const int score = -negamax_last(state.play_copied(Coords::none), true);
            return score;
        }
        const auto board = state.canonical_board();
        const int flips = count_flips(std::countr_zero(moves), board.black, board.white);
        return board.disk_difference() + 1 + 2 * flips;
    }

    int EndgameSolver::negascout(GameState state, int alpha, int beta, const int depth, const bool passed)
    {
        if (depth < min_negascout_depth)
            return negamax(state, alpha, beta, depth, passed);
        nodes_++;
        state.canonicalize();
        const int lookahead = static_cast<int>(state.current);
        const std::size_t hash = tt_.hash(state.board);
        Bounds<int> bounds{};
        if (const auto* ptr = tt_.try_load(state.board, lookahead, hash))
        {
            bounds = *ptr;
            const auto [lower, upper] = bounds;
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
                tt_.store(state.board, lookahead, {bounds.lower, score}, hash);
            else if (score >= beta)
                tt_.store(state.board, lookahead, {score, bounds.upper}, hash);
            else
                tt_.store(state.board, lookahead, score, hash);
        };
        if (moves == 0) // Pass
        {
            if (passed)
            {
                score = state.final_score();
                tt_.store(state.board, lookahead, score, hash);
                return score;
            }
            score = -negascout(state.play_copied(Coords::none), -beta, -alpha, depth, true);
            add_tt_entry();
            return score;
        }
        for (const Coords move : sort_moves_wrt_mobility(state))
        {
            const auto next = state.play_copied(move);
            const int lower = std::max(alpha, score);
            int new_score;
            if (lower == -int_inf)
                new_score = -negascout(next, -beta, int_inf, depth - 1, false);
            else
            {
                // Search with a null window
                new_score = -negascout(next, -lower - 1, -lower, depth - 1, false);
                if (lower < new_score && new_score < beta) // Re-search
                    new_score = -negascout(next, -beta, -lower, depth - 1, false);
            }
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
