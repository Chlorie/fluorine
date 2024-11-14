#include "fluorine/core/game.h"

#include <array>
#include <stdexcept>
#include <clu/static_for.h>

#include "../utils/bit.h"

namespace flr
{
    namespace
    {
        // Generate bitmasks for all the four lines crossing a specific cell
        // Example for a 4x4 bitmask:
        // In    Out0  Out1  Out2  Out3
        // ....  ....  ..*.  .*..  ...*
        // ..*.  ****  ..*.  ..*.  ..*.
        // ....  ....  ..*.  ...*  .*..
        // ....  ....  ..*.  ....  *...
        consteval auto generate_lines() noexcept
        {
            std::array<std::array<BitBoard, 4>, cell_count> res;
            for (std::size_t i = 0; i < cell_count; i++)
            {
                auto& arr = res[i];
                arr[0] = arr[1] = arr[2] = arr[3] = 1ull << i;
                for (std::size_t j = 0; j < board_length - 1; j++)
                {
                    arr[0] |= shift_west(arr[0]) | shift_east(arr[0]);
                    arr[1] |= (arr[1] << 8) | (arr[1] >> 8);
                    arr[2] |= shift_northwest(arr[2]) | shift_southeast(arr[2]);
                    arr[3] |= shift_southwest(arr[3]) | shift_northeast(arr[3]);
                }
            }
            return res;
        }

        // Generate the positions of a specific cell in the four lines crossing it
        // Example for a 4x4 bitmask:
        // In   Out [2, 1, 1, 1]
        // ....
        // ..*.
        // ....
        // ....
        consteval auto generate_pos_in_lines() noexcept
        {
            std::array<std::array<std::uint8_t, 4>, cell_count> res;
            for (std::size_t i = 0; i < cell_count; i++)
            {
                const auto r = static_cast<std::uint8_t>(i / 8);
                const auto c = static_cast<std::uint8_t>(i % 8);
                res[i][0] = c;
                res[i][1] = r;
                res[i][2] = std::min(r, c);
                res[i][3] = std::min(r, static_cast<std::uint8_t>(7 - c));
            }
            return res;
        }

        // Given the position of the newly placed disk, find the outside self-disks
        // that'd be needed to flip the given opponent disk pattern
        // Example:
        // Self:      ...*.... (as the bit index 3)
        // Opponent:  .**.*.**
        // Outflanks: *....*..
        consteval auto generate_outflanks() noexcept
        {
            std::array<std::array<BitRow, (1 << board_length)>, board_length> res{};
            for (unsigned i = 0; i < board_length; i++)
            {
                const BitRow bit = 1 << i;
                for (unsigned j = 0; j < (1 << board_length); j++)
                {
                    const auto opponent = static_cast<BitRow>(j | bit);
                    if ((bit << 1) & opponent)
                    {
                        BitRow self = bit;
                        while (self & opponent)
                            self <<= 1;
                        res[i][j] |= self;
                    }
                    if ((bit >> 1) & opponent)
                    {
                        BitRow self = bit;
                        while (self & opponent)
                            self >>= 1;
                        res[i][j] |= self;
                    }
                }
            }
            return res;
        }

        // Given the position of the newly placed disk and the outflank pattern,
        // output the flipped disks together with the newly placed disk
        // Example:
        // Self:      ...*.... (as the bit index 3)
        // Outflanks: *....*..
        // Flips:     .****...
        consteval auto generate_flips() noexcept
        {
            std::array<std::array<BitRow, (1 << board_length)>, board_length> res{};
            for (unsigned i = 0; i < board_length; i++)
            {
                const BitRow bit = 1 << i;
                for (unsigned j = 0; j < (1 << board_length); j++)
                {
                    if ((j & bit) || std::popcount(j) > 2)
                        continue;
                    if (j > bit)
                    {
                        BitRow self = bit;
                        while (!(self & j))
                        {
                            res[i][j] |= self;
                            self <<= 1;
                        }
                    }
                    if (j & (bit - 1))
                    {
                        BitRow self = bit;
                        while (!(self & j))
                        {
                            res[i][j] |= self;
                            self >>= 1;
                        }
                    }
                }
            }
            return res;
        }

        constexpr auto line_table = generate_lines();
        constexpr auto pos_in_line_table = generate_pos_in_lines();
        constexpr auto outflanks_table = generate_outflanks();
        constexpr auto flips_table = generate_flips();

        BitBoard find_flips(const int placed, const BitBoard self, const BitBoard opponent) noexcept
        {
            BitBoard flips = 0;
            const auto& lines = line_table[static_cast<std::size_t>(placed)];
            const auto& pos = pos_in_line_table[static_cast<std::size_t>(placed)];
            clu::static_for<0, 4>(
                [&](const std::size_t i)
                {
                    const auto self_line = static_cast<BitRow>(bit_compressr(self, lines[i]));
                    const auto opp_line = static_cast<BitRow>(bit_compressr(opponent, lines[i]));
                    const auto outflank = static_cast<BitRow>(outflanks_table[pos[i]][opp_line] & self_line);
                    const auto flip_line = static_cast<BitRow>(flips_table[pos[i]][outflank]);
                    flips |= bit_expandr(flip_line, lines[i]);
                });
            return flips;
        }
    } // namespace

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
