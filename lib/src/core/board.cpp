#include "fluorine/core/board.h"

#include <stdexcept>

#ifndef __AVX2__
#include <clu/static_for.h>
#else
#include <xsimd/xsimd.hpp>
#include <immintrin.h>
#endif

#include "../utils/bit.h"

namespace flr
{
    namespace
    {
#ifndef __AVX2__
        BitBoard find_legal_moves(const BitBoard self, const BitBoard opponent)
        {
            // Apply masks to opponent bit board to avoid row wrapping in the left/right shifts
            const BitBoard center = opponent & center_6x6; // Center 6x6
            const BitBoard columns = opponent & middle_6files; // Middle 6 columns
            // Initialize the result of the 8 directions, with the first iteration done
            BitBoard southeast = center & (self << 9), northwest = center & (self >> 9);
            BitBoard south = opponent & (self << 8), north = opponent & (self >> 8);
            BitBoard southwest = center & (self << 7), northeast = center & (self >> 7);
            BitBoard east = columns & (self << 1), west = columns & (self >> 1);
            // You can flip at most 6 opponent disks in one direction
            clu::static_for<0, 6>(
                [&](auto)
                {
                    southeast = (center & (southeast << 9)) | southeast;
                    northwest = (center & (northwest >> 9)) | northwest;
                    south = (opponent & (south << 8)) | south;
                    north = (opponent & (north >> 8)) | north;
                    southwest = (center & (southwest << 7)) | southwest;
                    northeast = (center & (northeast >> 7)) | northeast;
                    east = (columns & (east << 1)) | east;
                    west = (columns & (west >> 1)) | west;
                });
            // Get pseudo result (the spot with ones may not be empty) in each direction
            // clang-format off
            southeast <<= 9; northwest >>= 9;
            south <<= 8;     north >>= 8;
            southwest <<= 7; northeast >>= 7;
            east <<= 1;      west >>= 1;
            // clang-format on
            const BitBoard empty = ~(self | opponent); // Empty spots
            return (southeast | northwest | south | north | southwest | northeast | east | west) & empty;
        }

#else
        // Adapted from http://www.amy.hi-ho.ne.jp/okuhara/bitboard.htm
        BitBoard find_legal_moves(const BitBoard self, const BitBoard opponent)
        {
            using BitBoardx4 = xsimd::batch<BitBoard, xsimd::avx2>;
            using BitBoardx2 = xsimd::batch<BitBoard, xsimd::sse2>;
            const BitBoardx4 shifts{1, 8, 9, 7}, shifts2{2, 16, 18, 14};
            const BitBoardx4 mask{middle_6files, ~BitBoard{}, middle_6files, middle_6files};
            const BitBoardx4 selves(self), opponents = BitBoardx4(opponent) & mask; // Duplicated and masked
            BitBoardx4 flip_l = opponents & (selves << shifts);
            BitBoardx4 flip_r = opponents & (selves >> shifts);
            flip_l |= opponents & (flip_l << shifts);
            flip_r |= opponents & (flip_r >> shifts);
            const BitBoardx4 pre_l = opponents & (opponents << shifts);
            const BitBoardx4 pre_r = pre_l >> shifts;
            flip_l |= pre_l & (flip_l << shifts2);
            flip_r |= pre_r & (flip_r >> shifts2);
            flip_l |= pre_l & (flip_l << shifts2);
            flip_r |= pre_r & (flip_r >> shifts2);
            const BitBoardx4 moves4 = (flip_l << shifts) | (flip_r >> shifts);
            const BitBoardx2 moves_hi2 = _mm256_extracti128_si256(moves4, 1);
            const BitBoardx2 moves_lo2 = _mm256_castsi256_si128(moves4);
            BitBoardx2 moves2 = moves_lo2 | moves_hi2;
            moves2 |= swizzle(moves2, xsimd::batch_constant<BitBoard, xsimd::sse2, 1, 1>{});
            return _mm_cvtsi128_si64(moves2) & ~(self | opponent); // mask with empties
        }
#endif
    } // namespace

    std::string to_string(const Coords coords)
    {
        if (coords == Coords::none)
            return "Pass";
        char res[3]{};
        const int i = static_cast<int>(coords);
        const int r = i / static_cast<int>(board_length), c = i % static_cast<int>(board_length);
        res[0] = static_cast<char>('A' + c);
        res[1] = static_cast<char>('1' + r);
        return std::string(res);
    }

    Coords parse_coords(const std::string_view str)
    {
        if (str.size() != 2)
            return Coords::none;
        if (str[1] < '1' || str[1] > '8')
            return Coords::none;
        if ((str[0] < 'A' || str[0] > 'H') && (str[0] < 'a' || str[0] > 'h'))
            return Coords::none;
        const int r = str[1] - '1', c = str[0] >= 'a' ? str[0] - 'a' : str[0] - 'A';
        return static_cast<Coords>(r * static_cast<int>(board_length) + c);
    }

    Board Board::read(const std::string_view repr, const char black, const char white, const char space)
    {
        const auto error = [] { throw std::runtime_error("Invalid board representation"); };
        if (repr.size() != cell_count)
            error();
        Board board = empty;
        for (std::size_t i = 0; i < cell_count; i++)
        {
            if (const auto c = repr[i]; c == black)
                board.black |= 1ull << i;
            else if (c == white)
                board.white |= 1ull << i;
            else if (c != space)
                error();
        }
        return board;
    }

    BitBoard Board::find_legal_moves(const Color color) const noexcept
    {
        const BitBoard self = color == Color::black ? black : white;
        const BitBoard opponent = color == Color::black ? white : black;
        return flr::find_legal_moves(self, opponent);
    }
} // namespace flr
