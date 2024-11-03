#include "fluorine/core/board.h"

#include <stdexcept>
#include <clu/static_for.h>

#include "../utils/bit.h"

namespace flr
{
    namespace
    {
        BitBoard find_legal_moves(const BitBoard self, const BitBoard opponent)
        {
            const BitBoard empty = ~(self | opponent); // Empty spots
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
            return (southeast | northwest | south | north | southwest | northeast | east | west) & empty;
        }
    } // namespace

    std::string to_string(const Coords coords)
    {
        if (coords == Coords::none)
            return "Pass";
        char res[3]{};
        const int i = static_cast<int>(coords);
        const int r = static_cast<int>(i / board_length), c = static_cast<int>(i % board_length);
        res[0] = static_cast<char>('A' + c);
        res[1] = static_cast<char>('1' + r);
        return std::string(res);
    }

    Board Board::read(const std::string_view repr, const char black, const char white, const char space)
    {
        const auto error = [] { throw std::runtime_error("Invalid board representation"); };
        if (repr.size() != cell_count)
            error();
        Board board = empty;
        for (int i = 0; i < cell_count; i++)
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
