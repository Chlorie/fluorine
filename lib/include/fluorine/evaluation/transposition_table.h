#pragma once

#include <vector>
#include <limits>
#include <algorithm>
#include <ranges>

#include "fluorine/core/board.h"

FLUORINE_SUPPRESS_EXPORT_WARNING

namespace flr
{
    inline constexpr float inf = std::numeric_limits<float>::infinity();

    struct FLUORINE_API Bounds final
    {
        float lower;
        float upper;

        constexpr Bounds() noexcept: lower(-inf), upper(inf) {}
        constexpr explicit(false) Bounds(const float value) noexcept: lower(value), upper(value) {}
        constexpr Bounds(const float l, const float u) noexcept: lower(l), upper(u) {}

        [[nodiscard]] constexpr float error(const float predicted) const noexcept
        {
            return lower > predicted ? predicted - lower //
                : upper < predicted  ? predicted - upper
                                     : 0.0f;
        }
    };

    class FLUORINE_API TranspositionTable final
    {
    public:
        TranspositionTable(): data_(table_size) {}

        [[nodiscard]] static std::size_t hash(const Board& board, int lookahead);
        void store(const Board& board, int lookahead, Bounds bounds) noexcept;
        void store(const Board& board, int lookahead, Bounds bounds, std::size_t hash_hint) noexcept;
        [[nodiscard]] const Bounds* try_load(const Board& board, int lookahead) const noexcept;
        [[nodiscard]] const Bounds* try_load(const Board& board, int lookahead, std::size_t hash_hint) const noexcept;
        void clear() noexcept { std::ranges::fill(data_, Entry{}); }
        [[nodiscard]] std::size_t size() const noexcept;

        [[nodiscard]] auto entries() const noexcept
        {
            return data_ //
                | std::views::filter([](const Entry& entry) { return entry.state.board != Board::empty; }) //
                | std::views::transform([](const Entry& entry) { return std::pair(entry.state.board, entry.bounds); });
        }

    private:
        struct State final
        {
            Board board{.black = 0, .white = 0}; // Intended zero init
            int lookahead = 0;
        };

        struct Entry final
        {
            State state;
            Bounds bounds;
        };

        static constexpr std::size_t table_size = 1 << 20;
        std::vector<Entry> data_;
    };
} // namespace flr

FLUORINE_RESTORE_EXPORT_WARNING
