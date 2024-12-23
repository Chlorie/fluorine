#pragma once

#include <vector>

#include "board.h"

FLUORINE_SUPPRESS_EXPORT_WARNING

namespace flr
{
    struct FLUORINE_API GameState final
    {
        Color current = Color::black;
        Board board{};
        BitBoard legal_moves = 0x00001020'04080000ull;

        [[nodiscard]] static GameState read(
            std::string_view repr, char black = 'X', char white = 'O', char space = '-');
        [[nodiscard]] static GameState from_board_and_color(const Board& board, Color color) noexcept;

        [[nodiscard]] constexpr BitBoard self() const noexcept
        {
            return current == Color::black ? board.black : board.white;
        }

        [[nodiscard]] constexpr BitBoard opponent() const noexcept
        {
            return current == Color::white ? board.black : board.white;
        }

        constexpr void swap_colors() noexcept
        {
            board.swap_colors();
            current = opponent_of(current);
        }

        constexpr void canonicalize() noexcept
        {
            if (current == Color::white)
                swap_colors();
        }

        [[nodiscard]] constexpr GameState canonicalized() const noexcept
        {
            GameState res = *this;
            res.canonicalize();
            return res;
        }

        void mirror_main_diagonal() noexcept;
        void mirror_anti_diagonal() noexcept;
        void rotate_180() noexcept;

        constexpr Board canonical_board() const noexcept { return {self(), opponent()}; }

        [[nodiscard]] constexpr int disk_difference() const noexcept
        {
            return sign_of(current) * board.disk_difference();
        }

        /// \brief Calculate the final score of this game, assuming that the game is actually over.
        [[nodiscard]] constexpr int final_score() const noexcept
        {
            assert(legal_moves == 0);
            const int black = board.count_black(), white = board.count_white();
            const int empty = static_cast<int>(cell_count) - black - white;
            const int diff = black - white;
            // Empty cells are counted towards the winner when the game ends
            return sign_of(current) * (diff + (diff > 0 ? empty : diff < 0 ? -empty : 0));
        }

        void play(Coords coords) noexcept;

        [[nodiscard]] GameState play_copied(const Coords coords) const noexcept
        {
            GameState res = *this;
            res.play(coords);
            return res;
        }
    };

    class FLUORINE_API GameRecord final
    {
    public:
        GameRecord() noexcept: GameRecord(GameState{}) {}
        explicit GameRecord(const GameState& state): states_{state} {}

        void play(const Coords coords) { states_.emplace_back(states_.back()).play(coords); }

        void undo() noexcept { states_.pop_back(); }

        void reset() noexcept { states_.erase(states_.begin() + 1, states_.end()); }

        void reset(const GameState& state) noexcept
        {
            states_[0] = state;
            reset();
        }

        void canonicalize_all() noexcept
        {
            for (auto& state : states_)
                state.canonicalize();
        }

        [[nodiscard]] const GameState& current() const noexcept { return states_.back(); }

        [[nodiscard]] GameState current_canonical() const noexcept
        {
            GameState state = states_.back();
            state.canonicalize();
            return state;
        }

        [[nodiscard]] const auto& states() const noexcept { return states_; }

    private:
        std::vector<GameState> states_;
    };
} // namespace flr

FLUORINE_RESTORE_EXPORT_WARNING
