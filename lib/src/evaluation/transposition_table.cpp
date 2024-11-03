#include "fluorine/evaluation/transposition_table.h"

#include <algorithm>
#include <clu/hash.h>

namespace flr
{
    std::size_t TranspositionTable::hash(const Board& board, const int lookahead)
    {
        clu::fnv1a_hasher hasher;
        hasher.update(clu::trivial_buffer(board));
        hasher.update(clu::trivial_buffer(lookahead));
        return hasher.finalize() & (table_size - 1);
    }

    void TranspositionTable::store(const Board& board, const int lookahead, const Bounds bounds) noexcept
    {
        store(board, lookahead, bounds, hash(board, lookahead));
    }

    void TranspositionTable::store(
        const Board& board, const int lookahead, const Bounds bounds, const std::size_t hash_hint) noexcept
    {
        data_[hash_hint] = {.state = {board, lookahead}, .bounds = bounds};
    }

    const Bounds* TranspositionTable::try_load(const Board& board, const int lookahead) const noexcept
    {
        return try_load(board, lookahead, hash(board, lookahead));
    }

    const Bounds* TranspositionTable::try_load(
        const Board& board, const int lookahead, const std::size_t hash_hint) const noexcept
    {
        const auto& pair = data_[hash_hint];
        if (pair.state.board != board || pair.state.lookahead != lookahead)
            return nullptr;
        return &pair.bounds;
    }

    void TranspositionTable::clear() noexcept
    {
        static constexpr std::size_t size_bytes = sizeof(Pair) * table_size;
        std::memset(data_.data(), 0, size_bytes);
    }

    std::size_t TranspositionTable::size() noexcept
    {
        static constexpr Board empty_board{.black = 0, .white = 0};
        return std::ranges::count_if(
            data_, [](const State& state) { return state.board != empty_board; }, &Pair::state);
    }
} // namespace flr
