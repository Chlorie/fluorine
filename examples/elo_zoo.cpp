#include <vector>
#include <fstream>
#include <filesystem>
#include <algorithm>
#include <unordered_map>
#include <optional>
#include <cmath>
#include <thread>

#include <clu/text/print.h>
#include <clu/random.h>

#include <fluorine/evaluation/linear_pattern_evaluator.h>
#include <fluorine/arena/player.h>
#include <fluorine/arena/random_player.h>
#include <fluorine/arena/searching_player.h>
#include <fluorine/utils/tui.h>

namespace
{
    namespace fs = std::filesystem;

    struct MatchStats
    {
        std::size_t wins = 0;
        std::size_t draws = 0;
        std::size_t losses = 0;

        static auto make_stats_matrix(const std::size_t n)
        {
            using Vec = std::vector<MatchStats>;
            using Mat = std::vector<Vec>;
            return Mat(n, Vec(n));
        }

        MatchStats& operator+=(const MatchStats& other) noexcept
        {
            wins += other.wins;
            draws += other.draws;
            losses += other.losses;
            return *this;
        }

        std::size_t total() const noexcept { return wins + draws + losses; }

        float score() const noexcept { return static_cast<float>(wins * 2 + draws) / 2.0f; }

        float delta_elo(const float my_elo, const float other_elo) const noexcept
        {
            const float expected_score =
                static_cast<float>(total()) / (1.0f + std::pow(10.0f, (other_elo - my_elo) / 400.0f));
            return score() - expected_score;
        }
    };

    class Results
    {
    public:
        Results() { load_openings(); }

        void run()
        {
            players_.clear();
            stats_.clear();
            load_players();
            load_stats();
            flr::clear_screen();
            show_table();
            run_tournament();
            sort_players();
            save_stats();
        }

    private:
        constexpr static float elo_floor = 100.0f;
        constexpr static float elo_ceiling = 9000.0f;

        struct Player
        {
            std::string name;
            std::unique_ptr<flr::Player> player;
            float elo = elo_floor;
        };

        std::vector<flr::GameState> openings_;
        std::vector<Player> players_;
        std::vector<std::vector<MatchStats>> stats_;
        std::vector<std::vector<MatchStats>> tournament_stats_;
        std::optional<flr::ProgressBar> bar_;

        void load_openings()
        {
            const fs::path path = "./openings.txt";
            if (!exists(path))
                return;
            std::ifstream file(path);
            std::string line;
            while (std::getline(file, line))
            {
                flr::GameState state;
                for (std::size_t i = 0; i < line.size(); i += 2)
                    state.play(flr::parse_coords(line.substr(i, 2)));
                openings_.push_back(state);
            }
        }

        void load_players()
        {
            players_.push_back(Player{.name = "rand", .player = std::make_unique<flr::RandomPlayer>()});
            for (const auto& entry : fs::directory_iterator(fs::path(".")))
            {
                const auto& path = entry.path();
                if (path.extension() != ".dat")
                    continue;
                auto lpe = flr::LinearPatternEvaluator::load(path);
                players_.push_back(Player{
                    .name = path.stem().string(), //
                    .player = std::make_unique<flr::SearchingPlayer>(std::move(lpe), 6, 8) //
                });
            }
        }

        void load_stats()
        {
            const fs::path path = "./stats.txt";
            if (!exists(path))
            {
                stats_ = MatchStats::make_stats_matrix(players_.size());
                return;
            }
            std::ifstream file(path);
            std::size_t size;
            file >> size;
            std::unordered_map<std::string, std::size_t> reverse_map;
            for (std::size_t i = 0; const auto& p : players_)
                reverse_map[p.name] = i++;
            std::vector<Player> new_players;
            for (std::size_t i = 0; i < size; i++)
            {
                std::string name;
                float elo;
                file >> name >> elo;
                if (const auto iter = reverse_map.find(name); iter != reverse_map.end())
                {
                    new_players.emplace_back(std::move(players_[iter->second])).elo = elo;
                    reverse_map.erase(iter);
                }
                else
                    new_players.push_back(Player{.name = name, .elo = elo});
            }
            for (const std::size_t idx : reverse_map | std::views::values)
                new_players.push_back(std::move(players_[idx]));
            players_ = std::move(new_players);
            stats_ = MatchStats::make_stats_matrix(players_.size());
            for (std::size_t i = 0; i < size; i++)
                for (std::size_t j = 0; j < size; j++)
                {
                    MatchStats cell;
                    file >> cell.wins >> cell.draws >> cell.losses;
                    stats_[i][j] = cell;
                }
        }

        void save_stats() const
        {
            const fs::path path = "./stats.txt";
            std::ofstream file(path);
            const std::size_t size = stats_.size();
            file << std::format("{}\n", size);
            for (const auto& player : players_)
                file << std::format("{} {}\n", player.name, player.elo);
            for (std::size_t i = 0; i < size; i++)
            {
                for (std::size_t j = 0; j < size; j++)
                {
                    if (j != 0)
                        file << "  ";
                    const auto& cell = stats_[i][j];
                    file << std::format("{} {} {}", cell.wins, cell.draws, cell.losses);
                }
                file << '\n';
            }
        }

        void sort_players()
        {
            using Pair = std::pair<std::size_t, Player*>;
            std::vector<Pair> map;
            map.reserve(players_.size());
            for (std::size_t i = 0; i < players_.size(); i++)
                map.emplace_back(i, &players_[i]);
            const auto compare_players = [](const Pair lhs, const Pair rhs) noexcept
            {
                if (lhs.second->elo > rhs.second->elo)
                    return true;
                if (lhs.second->elo < rhs.second->elo)
                    return false;
                return lhs.second->name < rhs.second->name;
            };
            std::ranges::sort(map, compare_players);
            std::vector<Player> sorted;
            sorted.reserve(players_.size());
            for (const auto& p : map | std::views::values)
                sorted.push_back(std::move(*p));
            players_ = std::move(sorted);
            auto mat = MatchStats::make_stats_matrix(players_.size());
            for (std::size_t i = 0; i < players_.size(); i++)
                for (std::size_t j = 0; j < players_.size(); j++)
                    mat[i][j] = stats_[map[i].first][map[j].first];
            stats_ = std::move(mat);
        }

        void show_table() const
        {
            static constexpr std::size_t max_digits = 3;
            static constexpr std::size_t max_column_width = 3 * max_digits + 2;
            const auto& lengthiest_player =
                *std::ranges::max_element(players_, std::less{}, [](const Player& p) { return p.name.size(); });
            const std::size_t max_name_length = lengthiest_player.name.size();
            clu::print("{:{}}   \x1b[1;34mELO\x1b[0m  |", "", max_name_length);
            for (const auto& p : players_)
            {
                clu::print_nonformatted(p.player ? "\x1b[1;34m" : "\x1b[1;30m");
                if (p.name.size() > max_column_width - 1)
                    clu::print(" {}... ", p.name.substr(0, max_column_width - 5));
                else
                    clu::print("{:^{}}", p.name, max_column_width);
                clu::print_nonformatted("\x1b[0m");
            }
            clu::println("\n{:->{}}+{:->{}}", "", max_name_length + 8, "", max_column_width * players_.size());
            for (std::size_t i = 0; i < players_.size(); i++)
            {
                const auto& p = players_[i];
                clu::print("\x1b[1;{}m{:{}} \x1b[0m {:>4.0f}  |", p.player ? 34 : 30, p.name, max_name_length, p.elo);
                for (std::size_t j = 0; j < players_.size(); j++)
                {
                    if (i == j)
                    {
                        clu::print("\x1b[1;30m{:^{}}\x1b[0m", "-", max_column_width);
                        continue;
                    }
                    const auto cell = stats_[i][j];
                    if (cell.total() == 0)
                    {
                        clu::print("\x1b[1;30m {0:3}{0:3}{0:3} \x1b[0m", 0);
                        continue;
                    }
                    const auto total = cell.total();
                    if (cell.wins != cell.losses)
                    {
                        const float rate = //
                            (static_cast<float>(cell.wins) - static_cast<float>(cell.losses)) /
                            static_cast<float>(total);
                        const float curved = std::sqrt(std::abs(rate)) * (rate > 0 ? 1.0f : -1.0f);
                        const auto color_diff = static_cast<int>(std::round(curved * 48));
                        constexpr int base = 34;
                        const auto red = base - std::min(0, color_diff);
                        const auto green = base + std::max(0, color_diff);
                        clu::print("\x1b[48;2;{};{};{}m", red, green, base);
                    }
                    if (std::max({cell.wins, cell.draws, cell.losses}) < 1000)
                        clu::print(" \x1b[32m{:>3}\x1b[37m{:>3}\x1b[31m{:>3} ", cell.wins, cell.draws, cell.losses);
                    else
                    {
                        const auto wp = (cell.wins * 200 / total + 1) / 2;
                        const auto dp = (cell.draws * 200 / total + 1) / 2;
                        const auto lp = (cell.losses * 200 / total + 1) / 2;
                        if (wp == 100 || dp == 100 || lp == 100)
                            clu::print(
                                "\x1b[{}m{:^{}}", wp == 100 ? 32 : (dp == 100 ? 37 : 31), "100%", max_column_width);
                        else
                            clu::print(" \x1b[32m{:2}%\x1b[37m{:>2}%\x1b[31m{:>2}%\x1b[37m ", wp, dp, lp);
                    }
                    clu::print_nonformatted("\x1b[0m");
                }
                clu::print_nonformatted("\n");
            }
            clu::print_nonformatted("\n");
        }

        flr::GameState random_opening() const noexcept
        {
            if (openings_.empty())
            {
                flr::GameState state;
                flr::RandomPlayer rand;
                for (std::size_t i = 0; i < 6; i++)
                    state.play(rand.get_move(state));
                return state;
            }
            else
            {
                std::uniform_int_distribution<std::size_t> sample_dist(0, openings_.size() - 1);
                flr::GameState state = openings_[sample_dist(clu::thread_rng())];
                std::uniform_int_distribution flip_dist(0, 0b11);
                const int flip = flip_dist(clu::thread_rng());
                if (flip & 1)
                    state.mirror_main_diagonal();
                if (flip & 2)
                    state.mirror_anti_diagonal();
                return state;
            }
        }

        void run_tournament()
        {
            static constexpr std::size_t max_matches = 200;
            std::vector<std::pair<std::size_t, std::size_t>> match_pairs;
            for (std::size_t i = 0; i < players_.size(); i++)
            {
                if (!players_[i].player)
                    continue;
                for (std::size_t j = i + 1; j < players_.size(); j++)
                    if (players_[j].player && stats_[i][j].total() < max_matches)
                        match_pairs.emplace_back(i, j);
            }
            if (match_pairs.empty())
            {
                clu::print_nonformatted("Waiting for new contestants...\n");
                std::this_thread::sleep_for(std::chrono::seconds(20));
                return;
            }
            bar_.emplace("Tournament", match_pairs.size());
            tournament_stats_ = MatchStats::make_stats_matrix(players_.size());
            for (const auto [i, j] : match_pairs)
                match_players(i, j);
            merge_stats();
            update_elo();
        }

        void update_elo()
        {
            static constexpr float eps = 1e-5f;
            static constexpr float lr = 1e-3f;
            const std::size_t n = players_.size();
            for (std::size_t iter = 0; iter < 1'000'000; iter++)
            {
                std::vector<float> delta_elo(n);
                std::vector<bool> clamped(n);
                for (std::size_t i = 0; i < n; i++)
                    for (std::size_t j = 0; j < n; j++)
                        delta_elo[i] += lr * stats_[i][j].delta_elo(players_[i].elo, players_[j].elo);
                for (std::size_t i = 0; i < n; i++)
                {
                    const auto [cl, elo] = clamp_elo(players_[i].elo + delta_elo[i]);
                    clamped[i] = cl;
                    players_[i].elo = elo;
                }
                for (std::size_t i = 0; i < n; i++)
                    if (!clamped[i] && std::abs(delta_elo[i]) > eps)
                        goto next_iter;
                break;
            next_iter:;
            }
        }

        static std::pair<bool, float> clamp_elo(const float elo) noexcept
        {
            if (elo <= elo_floor)
                return {true, elo_floor};
            if (elo >= elo_ceiling)
                return {true, elo_ceiling};
            return {false, elo};
        }

        void merge_stats() noexcept
        {
            for (std::size_t i = 0; i < players_.size(); i++)
                for (std::size_t j = 0; j < players_.size(); j++)
                    stats_[i][j] += tournament_stats_[i][j];
        }

        void match_players(const std::size_t p1, const std::size_t p2) noexcept
        {
            const auto start = random_opening();
            for (const bool p1_first : {true, false})
            {
                auto state = start;
                while (true)
                {
                    if (state.legal_moves == 0)
                    {
                        state.play(flr::Coords::none);
                        if (state.legal_moves == 0)
                            break;
                        continue;
                    }
                    const auto& idx = p1_first == (state.current == flr::Color::black) ? p1 : p2;
                    const auto move = players_[idx].player->get_move(state);
                    state.play(move);
                }
                if (const int diff = state.board.disk_difference(); diff == 0)
                {
                    tournament_stats_[p1][p2].draws++;
                    tournament_stats_[p2][p1].draws++;
                }
                else if ((diff > 0) == p1_first)
                {
                    tournament_stats_[p1][p2].wins++;
                    tournament_stats_[p2][p1].losses++;
                }
                else
                {
                    tournament_stats_[p1][p2].losses++;
                    tournament_stats_[p2][p1].wins++;
                }
            }
            if (!bar_)
                return;
            bar_->set_message(std::format("Match between \x1b[1;34m{}\x1b[0m and \x1b[1;34m{}\x1b[0m: "
                                          "\x1b[32m{} \x1b[37m{} \x1b[31m{}\x1b[0m",
                players_[p1].name, players_[p2].name, //
                tournament_stats_[p1][p2].wins, tournament_stats_[p1][p2].draws, tournament_stats_[p1][p2].losses));
            bar_->tick();
        }
    };
} // namespace

int main()
{
    Results res;
    while (true)
        res.run();
}
