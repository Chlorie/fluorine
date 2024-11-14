#include "fluorine/evaluation/transposition_table.h"

#include <clu/hash.h>

namespace flr
{
    std::size_t TranspositionTable::hash(const Board& board)
    {
        clu::fnv1a_hasher hasher;
        hasher.update(clu::trivial_buffer(board));
        return hasher.finalize() & (table_size - 1);
    }

    void TranspositionTable::store(const Board& board, const int depth, const Bounds bounds) noexcept
    {
        store(board, depth, bounds, hash(board));
    }

    void TranspositionTable::store(
        const Board& board, const int depth, const Bounds bounds, const std::size_t hash_hint) noexcept
    {
        data_[hash_hint] = {.state = {board, depth}, .bounds = bounds};
    }

    const Bounds* TranspositionTable::try_load(const Board& board, const int min_depth) const noexcept
    {
        return try_load(board, min_depth, hash(board));
    }

    const Bounds* TranspositionTable::try_load(
        const Board& board, const int min_depth, const std::size_t hash_hint) const noexcept
    {
        const auto& pair = data_[hash_hint];
        if (pair.state.board != board || pair.state.depth < min_depth)
            return nullptr;
        return &pair.bounds;
    }

    std::size_t TranspositionTable::size() const noexcept
    {
        static constexpr Board empty_board{.black = 0, .white = 0};
        return static_cast<std::size_t>(std::ranges::count_if(
            data_, [](const State& state) { return state.board != empty_board; }, &Entry::state));
    }
} // namespace flr
