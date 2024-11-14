#include <vector>
#include <string>
#include <filesystem>
#include <iostream>

#include <clu/text/print.h>
#include <clu/parse.h>

#include <fluorine/arena/searching_player.h>
#include <fluorine/arena/random_player.h>
#include <fluorine/evaluation/linear_pattern_evaluator.h>
#include <fluorine/utils/tui.h>

namespace
{
    struct Config
    {
        std::unique_ptr<flr::Player> p1;
        std::unique_ptr<flr::Player> p2;
        std::size_t random_moves = 0;
    };

    const std::string help = //
        R"(Usage: match <depth> <p1> <p2> [rand]
    <depth>     search depth, must be a positive integer
    <p1> <p2>   model paths for the black player and the white player
                can be a single dash "-" for manual input
    [rand]      play n random moves at the start of the game)";

    std::unique_ptr<flr::Player> load_player(const std::string& path, const int depth)
    {
        if (path == "-")
            return nullptr;
        const std::filesystem::path fspath(path);
        if (!exists(fspath))
            throw std::runtime_error(std::format("File does not exist: {}", path));
        auto eval = flr::LinearPatternEvaluator::load(fspath);
        return std::make_unique<flr::SearchingPlayer>(std::move(eval), depth, 0);
    }

    flr::Coords get_user_move(const flr::BitBoard legal_moves)
    {
        while (true)
        {
            clu::print("Your move: ");
            std::string input;
            std::getline(std::cin, input);
            if (input.size() != 2 || input[1] < '1' || input[1] > '8')
            {
                clu::println("Please enter a valid position on the board");
                continue;
            }
            input[0] = static_cast<char>(std::tolower(input[0]));
            if (input[0] < 'a' || input[0] > 'h')
            {
                clu::println("Please enter a valid position on the board");
                continue;
            }
            const int idx = (input[1] - '1') * 8 + (input[0] - 'a');
            const auto move = static_cast<flr::Coords>(idx);
            if ((bit_of(move) & legal_moves) == 0)
            {
                clu::println("That move is illegal");
                continue;
            }
            return move;
        }
    }

    auto process_args(const int argc, const char* argv[])
    {
        if (argc != 4 && argc != 5)
            throw std::runtime_error(help);
        std::vector<std::string> args(argc);
        for (std::size_t i = 0; i < argc; i++)
            args[i] = argv[i];
        const auto depth = clu::parse<int>(args[1]);
        if (!depth || *depth == 0)
            throw std::runtime_error(help);
        Config config{.p1 = load_player(args[2], *depth), .p2 = load_player(args[3], *depth)};
        if (!config.p1 && !config.p2)
            throw std::runtime_error("At least one player must be computer controlled");
        if (argc == 5)
        {
            const auto rand_moves = clu::parse<std::size_t>(args[4]);
            if (!rand_moves)
                throw std::runtime_error(help);
            config.random_moves = *rand_moves;
        }
        return config;
    }

    void run_match(const Config& config)
    {
        flr::GameState state;
        std::size_t move_count = 0;
        while (true)
        {
            flr::clear_screen();
            display_game(state);
            if (state.legal_moves == 0)
            {
                state.play(flr::Coords::none);
                if (state.legal_moves == 0)
                    break;
                continue;
            }
            move_count++;
            if (move_count <= config.random_moves)
            {
                state.play(flr::RandomPlayer().get_move(state));
                continue;
            }
            const auto& player = state.current == flr::Color::black ? config.p1 : config.p2;
            const flr::Coords move = player ? player->get_move(state) : get_user_move(state.legal_moves);
            state.play(move);
            if (config.p1 && config.p2)
                (void)std::getchar();
        }
        clu::println("Game ended");
    }
} // namespace

int main(const int argc, const char* argv[])
try
{
    const auto config = process_args(argc, argv);
    run_match(config);
    return 0;
}
catch (const std::exception& e)
{
    clu::println("Error due to exception:\n{}", e.what());
    return 1;
}
