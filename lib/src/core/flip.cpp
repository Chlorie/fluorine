#include "flip.h"

#include <array>
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
        constexpr auto line_table = generate_lines();

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
        constexpr auto pos_in_line_table = generate_pos_in_lines();

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
        constexpr auto outflanks_table = generate_outflanks();

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
        constexpr auto flips_table = generate_flips();

        // Flip count doesn't include the newly placed disk
        consteval auto generate_flip_counts() noexcept
        {
            std::array<std::array<std::uint8_t, (1 << board_length)>, board_length> res{};
            for (std::size_t i = 0; i < board_length; i++)
                for (std::size_t j = 0; j < (1 << board_length); j++)
                {
                    const BitRow flips = flips_table[i][j];
                    res[i][j] = flips == 0 ? 0 : static_cast<std::uint8_t>(std::popcount(flips) - 1);
                }
            return res;
        }
        constexpr auto flip_counts_table = generate_flip_counts();
    } // namespace

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

    int count_flips(const int placed, const BitBoard self, const BitBoard opponent) noexcept
    {
        std::uint8_t flips = 0;
        const auto& lines = line_table[static_cast<std::size_t>(placed)];
        const auto& pos = pos_in_line_table[static_cast<std::size_t>(placed)];
        clu::static_for<0, 4>(
            [&](const std::size_t i)
            {
                const auto self_line = static_cast<BitRow>(bit_compressr(self, lines[i]));
                const auto opp_line = static_cast<BitRow>(bit_compressr(opponent, lines[i]));
                const auto outflank = static_cast<BitRow>(outflanks_table[pos[i]][opp_line] & self_line);
                flips += flip_counts_table[pos[i]][outflank];
            });
        return flips;
    }
} // namespace flr
