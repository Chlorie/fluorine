#pragma once

#include <vector>
#include <limits>
#include <algorithm>
#include <ranges>
#include <bit>
#include <clu/concepts.h>
#include <clu/hash.h>

#include "fluorine/core/board.h"

namespace flr
{
    inline constexpr float inf = std::numeric_limits<float>::infinity();

    template <clu::arithmetic T>
    struct Bounds final
    {
        static constexpr T inf = []
        {
            if constexpr (std::is_floating_point_v<T>)
                return std::numeric_limits<T>::infinity();
            else
                return static_cast<T>(cell_count) + 1;
        }();

        T lower;
        T upper;

        constexpr Bounds() noexcept: lower(-inf), upper(inf) {}
        constexpr explicit(false) Bounds(const T value) noexcept: lower(value), upper(value) {}
        constexpr Bounds(const T l, const T u) noexcept: lower(l), upper(u) {}

        [[nodiscard]] constexpr T error(const T predicted) const noexcept
        {
            return lower > predicted ? predicted - lower //
                : upper < predicted  ? predicted - upper
                                     : T{};
        }
    };

    template <clu::arithmetic T>
    class TranspositionTable final
    {
    public:
        explicit TranspositionTable(const std::size_t table_size = 1 << 22): table_size_(table_size), data_(table_size)
        {
            if (!std::has_single_bit(table_size_))
                throw std::runtime_error("Table size must be a power of two");
        }

        [[nodiscard]] std::size_t hash(const Board& board) const noexcept
        {
            clu::fnv1a_hasher hasher;
            hasher.update(clu::trivial_buffer(board));
            return hasher.finalize() & (table_size_ - 1);
        }

        void store(const Board& board, const int depth, const Bounds<T> bounds) noexcept
        {
            this->store(board, depth, bounds, hash(board));
        }

        void store(const Board& board, const int depth, const Bounds<T> bounds, const std::size_t hash_hint) noexcept
        {
            data_[hash_hint] = {.state = {board, depth}, .bounds = bounds};
        }

        [[nodiscard]] const Bounds<T>* try_load(const Board& board, const int min_depth) const noexcept
        {
            return try_load(board, min_depth, hash(board));
        }

        [[nodiscard]] const Bounds<T>* try_load(
            const Board& board, const int min_depth, const std::size_t hash_hint) const noexcept
        {
            const auto& pair = data_[hash_hint];
            if (pair.state.board != board || pair.state.depth < min_depth)
                return nullptr;
            return &pair.bounds;
        }

        void clear() noexcept { std::ranges::fill(data_, Entry{}); }

        [[nodiscard]] std::size_t size() const noexcept
        {
            static constexpr Board empty_board{.black = 0, .white = 0};
            return static_cast<std::size_t>(std::ranges::count_if(
                data_, [](const State& state) { return state.board != empty_board; }, &Entry::state));
        }

        [[nodiscard]] auto entries() const noexcept
        {
            return data_ //
                | std::views::filter([](const Entry& entry) { return entry.state.board != Board::empty; }) //
                | std::views::transform([](const Entry& entry) { return std::pair(entry.state.board, entry.bounds); });
        }

    private:
        struct State final
        {
            Board board = Board::empty; // Intended zero init
            int depth = 0;
        };

        struct Entry final
        {
            State state;
            Bounds<T> bounds;
        };

        std::size_t table_size_;
        std::vector<Entry> data_;
    };
} // namespace flr
