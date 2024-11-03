#pragma once

#include <cassert>
#include <cstdint>
#include <limits>
#include <utility>
#include <bit>
#include <string>
#include <string_view>

#include "macros.h"

FLUORINE_SUPPRESS_EXPORT_WARNING

namespace flr
{
    using BitBoard = std::uint64_t;
    using BitRow = std::uint8_t;

    inline constexpr std::size_t board_length = 8;
    inline constexpr std::size_t cell_count = board_length * board_length;

    enum class Coords : std::uint8_t // NOLINT(readability-enum-initial-value)
    {
        // clang-format off
        a1, b1, c1, d1, e1, f1, g1, h1,
        a2, b2, c2, d2, e2, f2, g2, h2,
        a3, b3, c3, d3, e3, f3, g3, h3,
        a4, b4, c4, d4, e4, f4, g4, h4,
        a5, b5, c5, d5, e5, f5, g5, h5,
        a6, b6, c6, d6, e6, f6, g6, h6,
        a7, b7, c7, d7, e7, f7, g7, h7,
        a8, b8, c8, d8, e8, f8, g8, h8,
        none = std::numeric_limits<std::uint8_t>::max()
        // clang-format on
    };

    [[nodiscard]] FLUORINE_API std::string to_string(Coords coords);

    enum class Color : bool
    {
        black,
        white
    };

    /// \brief Converts a Coords to a BitBoard.
    /// \param coords The Coords to convert. It must not be Coords::none.
    [[nodiscard]] constexpr BitBoard bit_of(const Coords coords) noexcept
    {
        assert(coords != Coords::none);
        return 1ull << static_cast<int>(coords);
    }

    [[nodiscard]] constexpr Color opponent_of(const Color color)
    {
        return color == Color::black ? Color::white : Color::black;
    }

    [[nodiscard]] constexpr int sign_of(const Color color) noexcept { return color == Color::black ? 1 : -1; }

    struct FLUORINE_API Board final
    {
        BitBoard black = 0x00000008'10000000ull;
        BitBoard white = 0x00000010'08000000ull;

        FLUORINE_API static const Board empty;
        [[nodiscard]] static Board read(std::string_view repr, char black = 'X', char white = 'O', char space = '-');

        [[nodiscard]] constexpr friend bool operator==(Board, Board) noexcept = default;
        [[nodiscard]] constexpr bool is_black(const Coords coords) const noexcept { return black & bit_of(coords); }
        [[nodiscard]] constexpr bool is_white(const Coords coords) const noexcept { return white & bit_of(coords); }

        [[nodiscard]] constexpr int count_black() const noexcept { return std::popcount(black); }
        [[nodiscard]] constexpr int count_white() const noexcept { return std::popcount(white); }
        [[nodiscard]] constexpr int count_total() const noexcept { return std::popcount(black | white); }
        [[nodiscard]] constexpr int count_empty() const noexcept { return std::popcount(~(black | white)); }
        [[nodiscard]] constexpr int disk_difference() const noexcept { return count_black() - count_white(); }
        [[nodiscard]] BitBoard find_legal_moves(Color color) const noexcept;

        constexpr void swap_colors() noexcept { std::swap(black, white); }
    };

    // ReSharper disable once CppRedundantInlineSpecifier
    inline constexpr Board Board::empty{.black = 0, .white = 0};
} // namespace flr

FLUORINE_RESTORE_EXPORT_WARNING
