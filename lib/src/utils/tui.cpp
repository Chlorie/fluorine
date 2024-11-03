#include "fluorine/utils/tui.h"

#include <cassert>
#include <clu/text/print.h>

namespace flr
{
    void clear_screen() { clu::print_nonformatted("\033[H\033[J"); }

    void display_board(const Board& board, const BitBoard highlight)
    {
        static constexpr std::string_view numbers[]{"１", "２", "３", "４", "５", "６", "７", "８"};
        static constexpr std::string_view space = "　";
        static constexpr std::string_view black = "⚫";
        static constexpr std::string_view white = "⚪";
        clu::print_nonformatted("　ＡＢＣＤＥＦＧＨ\n");
        for (int i = 0; i < board_length; i++)
        {
            clu::print("{}\x1b[42m", numbers[i]);
            for (int j = 0; j < board_length; j++)
            {
                const BitBoard bit = 1ull << (i * board_length + j);
                if (bit & highlight)
                    clu::print_nonformatted("\x1b[41m");
                if (board.black & bit)
                    clu::print_nonformatted(black);
                else if (board.white & bit)
                    clu::print_nonformatted(white);
                else
                    clu::print_nonformatted(space);
                if (bit & highlight)
                    clu::print_nonformatted("\x1b[42m");
            }
            clu::print_nonformatted("\x1b[0m\n");
        }
    }

    void display_game(const GameState& state, const BitBoard highlight)
    {
        if (state.current == Color::black)
            clu::println("\x1b[7mBLACK {:2}\x1b[m  {:2} WHITE", //
                std::popcount(state.board.black), std::popcount(state.board.white));
        else
            clu::println("BLACK {:2}  \x1b[7m{:2} WHITE\x1b[m", //
                std::popcount(state.board.black), std::popcount(state.board.white));
        display_board(state.board, highlight);
    }

    using namespace std::literals;

    ProgressBar::ProgressBar(std::string name, const std::size_t total):
        name_(std::move(name)), total_(total), //
        start_(Clock::now()), prev_display_(start_), prev_update_(start_), now_(start_)
    {
        assert(total_ > 0);
        clu::print_nonformatted("\n\n\n\n");
        update_display(UpdateMode::forced_without_time_update);
    }

    void ProgressBar::set_current(const std::size_t current)
    {
        current_ = current;
        update_display(finished() ? UpdateMode::forced : UpdateMode::relaxed);
    }

    void ProgressBar::set_total(const std::size_t total)
    {
        total_ = total;
        update_display(UpdateMode::forced_without_time_update);
    }

    void ProgressBar::set_message(std::string msg) { msg_ = std::move(msg); }

    void ProgressBar::update_display(const UpdateMode mode)
    {
        if (mode != UpdateMode::forced_without_time_update)
            now_ = Clock::now();
        if (mode != UpdateMode::relaxed || now_ - prev_display_ >= 33ms) // Max 30fps
        {
            clu::print_nonformatted("\x1b[4F");
            display_title();
            display_time();
            display_bar();
            if (mode != UpdateMode::forced_without_time_update)
                prev_display_ = now_;
        }
        if (mode != UpdateMode::forced_without_time_update)
            prev_update_ = now_;
    }

    void ProgressBar::display_title() const
    {
        const std::size_t digit = static_cast<std::size_t>(std::floor(std::log10(total_))) + 1;
        clu::println("{} - {:{}}/{:{}}\n{}", name_, current_, digit, total_, digit, msg_);
    }

    void ProgressBar::display_time() const
    {
        using namespace std::literals;
        const auto all = now_ - start_;
        const auto one = now_ - prev_update_;
        clu::print("All: {:>8g}ms - This: {:>8g}ms - Avg: ", all / 1.0ms, one / 1.0ms);
        if (current_ == 0)
            clu::println("N/A");
        else
            clu::println("{:>8g}ms", (all / current_) / 1.0ms);
    }

    void ProgressBar::display_bar() const
    {
        static constexpr std::size_t bar_width = 50;
        static constexpr std::string_view block_chars[]{" ", "▏", "▎", "▍", "▌", "▋", "▊", "▉"};
        static constexpr std::size_t divisions = std::size(block_chars);
        static constexpr std::size_t total_ticks = bar_width * divisions;
        const std::size_t ticks = current_ * total_ticks / total_;
        const std::size_t full = ticks / divisions;
        if (const std::size_t partial = ticks % divisions; partial == 0)
            clu::println("[{:█>{}}{:{}}]", "", full, "", bar_width - full);
        else
            clu::println("[{:█>{}}{}{:{}}]", "", full, block_chars[partial], "", bar_width - full - 1);
    }
} // namespace flr
