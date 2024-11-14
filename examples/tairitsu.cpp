#include <iostream>
#include <string>
#include <string_view>
#include <format>
#include <tuple>
#include <optional>

#include <clu/parse.h>
#include <clu/type_traits.h>

#include <fluorine/core/game.h>
#include <fluorine/arena/searching_player.h>
#include <fluorine/evaluation/linear_pattern_evaluator.h>

namespace
{
    std::string_view split_first(std::string_view& sv) noexcept
    {
        const auto pos = sv.find(' ');
        if (pos == std::string_view::npos)
            return std::exchange(sv, {});
        const auto ret = sv.substr(0, pos);
        sv.remove_prefix(pos);
        while (!sv.empty() && sv.front() == ' ')
            sv.remove_prefix(1);
        return ret;
    }

    template <typename... Ts>
    std::optional<std::tuple<Ts...>> parse_command(std::string_view sv)
    {
        std::string_view parts[sizeof...(Ts)];
        for (std::size_t i = 0; i < sizeof...(Ts); i++)
            if ((parts[i] = split_first(sv)).empty())
                return std::nullopt;
        std::tuple<std::optional<Ts>...> res;
        const auto parse_nth = [&]<typename T>(clu::type_tag_t<T>, const std::string_view part)
        {
            if constexpr (std::is_same_v<T, std::string_view>)
                return std::make_optional(part);
            else
                return clu::parse<T>(part);
        };
        const bool all_parsed = [&]<std::size_t... i>(std::index_sequence<i...>) {
            return ((std::get<i>(res) = parse_nth(clu::type_tag<Ts>, parts[i])).has_value() && ...);
        }(std::index_sequence_for<Ts...>{});
        if (!all_parsed)
            return std::nullopt;
        return std::apply([](const auto&... vs) { return std::make_tuple(*vs...); }, res);
    }

    class Game
    {
    public:
        bool process_command()
        {
            std::string line;
            std::getline(std::cin, line);
            std::string_view line_sv = line;
            const std::string_view cmd = split_first(line_sv);
            try
            {
                if (cmd == "set")
                    set_state(line_sv);
                else if (cmd == "show")
                    report_state();
                else if (cmd == "load")
                    try_invoke_command(&Game::load_model, line_sv);
                else if (cmd == "play")
                    try_invoke_command(&Game::play, line_sv);
                else if (cmd == "suggest")
                    suggest();
                else if (cmd == "analyze")
                    analyze();
                else if (cmd == "quit")
                    return false;
            }
            catch (std::exception& e)
            {
                std::cout << e.what() << '\n' << std::flush;
            }
            return true;
        }

    private:
        flr::GameState state_;
        std::unique_ptr<flr::SearchingPlayer> model_;

        void set_state(const std::string_view state)
        {
            if (state.size() != 33 || (state.back() != 'b' && state.back() != 'w'))
                error();
            const auto black = clu::parse<flr::BitBoard>(state.substr(0, 16), 16);
            const auto white = clu::parse<flr::BitBoard>(state.substr(16, 16), 16);
            const flr::Color color = state.back() == 'b' ? flr::Color::black : flr::Color::white;
            if (!black || !white)
                error();
            state_ = flr::GameState::from_board_and_color({*black, *white}, color);
            report_state();
        }

        void report_state() const
        {
            bool game_ended = state_.legal_moves == 0;
            if (game_ended)
            {
                auto state = state_;
                state.play(flr::Coords::none);
                if (state.legal_moves != 0)
                    game_ended = false;
            }
            std::cout << std::format("{:016x}{:016x}{}{:016x}{}\n", //
                             state_.board.black, state_.board.white, //
                             state_.current == flr::Color::black ? 'b' : 'w', state_.legal_moves, //
                             game_ended ? "+" : "-")
                      << std::flush;
        }

        void load_model(const std::string_view path, const int midgame_depth, const int endgame_depth)
        {
            if (midgame_depth <= 0 || endgame_depth <= 0)
                error();
            const std::filesystem::path fspath(path);
            if (!exists(fspath))
                error();
            model_ = std::make_unique<flr::SearchingPlayer>(
                flr::LinearPatternEvaluator::load(fspath), midgame_depth, endgame_depth);
            std::cout << std::format("loaded {}\n", path) << std::flush;
        }

        void play(const std::string_view move_str)
        {
            if (state_.legal_moves == 0)
            {
                if (move_str == "pass")
                {
                    state_.play(flr::Coords::none);
                    report_state();
                    return;
                }
                error();
            }
            if (move_str.size() != 2)
                error();
            const int file = std::tolower(move_str[0]) - 'a';
            const int rank = move_str[1] - '1';
            if (file < 0 || file > 7 || rank < 0 || rank > 7)
                error();
            const auto move = static_cast<flr::Coords>(file + 8 * rank);
            if ((bit_of(move) & state_.legal_moves) == 0)
                error();
            state_.play(move);
            report_state();
        }

        void suggest() const
        {
            if (!model_)
                error();
            const auto move = model_->get_move(state_);
            std::cout << to_string(move) << '\n' << std::flush;
        }

        void analyze() const
        {
            if (!model_)
                error();
            const auto& eval = model_->get_evaluator();
            std::vector<std::pair<flr::Coords, float>> evals;
            const auto do_analyze = [&](const auto& evaluate)
            {
                if (state_.legal_moves == 0)
                {
                    auto state = state_;
                    state.play(flr::Coords::none);
                    evals.emplace_back(flr::Coords::none, -evaluate(state));
                    return;
                }
                for (int i = 0; i < 64; i++)
                {
                    if (((1ull << i) & state_.legal_moves) == 0)
                        continue;
                    auto state = state_;
                    const auto move = static_cast<flr::Coords>(i);
                    state.play(move);
                    evals.emplace_back(move, -evaluate(state));
                }
            };
            if (state_.board.count_empty() > model_->endgame_depth())
            {
                flr::MidgameSearcher searcher;
                do_analyze([&](const flr::GameState& state)
                    { return searcher.evaluate(state, eval, model_->midgame_depth() - 1).score; });
            }
            else
            {
                flr::EndgameSolver solver;
                do_analyze(
                    [&](const flr::GameState& state) { return static_cast<float>(solver.evaluate(state).score); });
            }
            std::ranges::sort(evals, std::greater(), &std::pair<flr::Coords, float>::second);
            for (const auto& [move, score] : evals)
                std::cout << std::format("{} {} ", to_string(move), score);
            std::cout << '\n' << std::flush;
        }

        template <typename... Ts>
        void try_invoke_command(void (Game::*fptr)(Ts...), const std::string_view params)
        {
            auto opt = parse_command<Ts...>(params);
            if (!opt)
                error();
            std::apply([this, fptr](const auto&... args) { (this->*fptr)(args...); }, *opt);
        }

        [[noreturn]] static void error() { throw std::runtime_error("error"); }
    };
} // namespace

int main()
{
    Game game;
    while (game.process_command()) {}
    return 0;
}
