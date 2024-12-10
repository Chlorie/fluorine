#include "fluorine/core/game.h"

#include <array>
#include <stdexcept>
#include <clu/static_for.h>

#include "flip.h"
#include "../utils/bit.h"

namespace flr
{
    GameState GameState::read(const std::string_view repr, const char black, const char white, const char space)
    {
        const auto error = [] { throw std::runtime_error("Invalid game state representation"); };
        if (repr.size() != cell_count + 1)
            error();
        const char color_c = repr.back();
        if (color_c != black && color_c != white)
            error();
        const Color color = color_c == black ? Color::black : Color::white;
        const Board board = Board::read(repr.substr(0, cell_count), black, white, space);
        return from_board_and_color(board, color);
    }

    GameState GameState::from_board_and_color(const Board& board, const Color color) noexcept
    {
        GameState res{.current = color, .board = board, .legal_moves = 0};
        res.legal_moves = res.board.find_legal_moves(res.current);
        return res;
    }

    void GameState::mirror_main_diagonal() noexcept
    {
        board.black = flr::mirror_main_diagonal(board.black);
        board.white = flr::mirror_main_diagonal(board.white);
        legal_moves = flr::mirror_main_diagonal(legal_moves);
    }

    void GameState::mirror_anti_diagonal() noexcept
    {
        board.black = flr::mirror_anti_diagonal(board.black);
        board.white = flr::mirror_anti_diagonal(board.white);
        legal_moves = flr::mirror_anti_diagonal(legal_moves);
    }

    void GameState::rotate_180() noexcept
    {
        board.black = flr::rotate_180(board.black);
        board.white = flr::rotate_180(board.white);
        legal_moves = flr::rotate_180(legal_moves);
    }

    void GameState::play(const Coords coords) noexcept
    {
        if (coords == Coords::none)
        {
            assert(legal_moves == 0);
            current = opponent_of(current);
            legal_moves = board.find_legal_moves(current);
            return;
        }
        assert(legal_moves & (1ull << static_cast<int>(coords)));
        BitBoard& self = current == Color::black ? board.black : board.white;
        BitBoard& opponent = current == Color::black ? board.white : board.black;
        const BitBoard flips = find_flips(static_cast<int>(coords), self, opponent);
        self |= flips;
        opponent &= ~flips;
        current = opponent_of(current);
        legal_moves = board.find_legal_moves(current);
    }
} // namespace flr
