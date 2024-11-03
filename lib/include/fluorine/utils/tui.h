#pragma once

#include <chrono>

#include "../core/game.h"

FLUORINE_SUPPRESS_EXPORT_WARNING

namespace flr
{
    FLUORINE_API void clear_screen();
    FLUORINE_API void display_board(const Board& board, BitBoard highlight = 0);
    FLUORINE_API void display_game(const GameState& state, BitBoard highlight = 0);

    class FLUORINE_API ProgressBar final
    {
    public:
        ProgressBar(std::string name, std::size_t total);

        void set_current(std::size_t current);
        void reset() { set_current(0); }
        void tick() { set_current(current_ + 1); }
        void set_total(std::size_t total);
        void set_message(std::string msg);

        [[nodiscard]] std::size_t current() const noexcept { return current_; }
        [[nodiscard]] std::size_t total() const noexcept { return total_; }
        [[nodiscard]] bool finished() const noexcept { return current_ == total_; }
        [[nodiscard]] auto elapsed() const noexcept { return now_ - start_; }

    private:
        using Clock = std::chrono::high_resolution_clock;
        using TimePoint = Clock::time_point;

        std::string name_;
        std::string msg_;
        std::size_t current_ = 0;
        std::size_t total_;
        TimePoint start_;
        TimePoint prev_display_;
        TimePoint prev_update_;
        TimePoint now_;

        enum class UpdateMode : std::uint8_t
        {
            relaxed,
            forced,
            forced_without_time_update
        };

        void update_display(UpdateMode mode = UpdateMode::relaxed);
        void display_title() const;
        void display_time() const;
        void display_bar() const;
    };
} // namespace flr

FLUORINE_RESTORE_EXPORT_WARNING
