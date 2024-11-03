#pragma once

#include <vector>
#include <limits>

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
    };

    class FLUORINE_API TranspositionTable final
    {
    public:
        TranspositionTable(): data_(table_size) {}

        static std::size_t hash(const Board& board, int lookahead);
        void store(const Board& board, int lookahead, Bounds bounds) noexcept;
        void store(const Board& board, int lookahead, Bounds bounds, std::size_t hash_hint) noexcept;
        const Bounds* try_load(const Board& board, int lookahead) const noexcept;
        const Bounds* try_load(const Board& board, int lookahead, std::size_t hash_hint) const noexcept;
        void clear() noexcept;
        std::size_t size() noexcept;

    private:
        struct State final
        {
            Board board{.black = 0, .white = 0}; // Intended zero init
            int lookahead = 0;
        };

        struct Pair final
        {
            State state;
            Bounds bounds;
        };

        static constexpr std::size_t table_size = 1 << 20;
        std::vector<Pair> data_;
    };
} // namespace flr

FLUORINE_RESTORE_EXPORT_WARNING
