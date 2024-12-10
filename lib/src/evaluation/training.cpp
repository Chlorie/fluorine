#include "fluorine/evaluation/training.h"

#include <mutex>
#include <thread>
#include <array>
#include <clu/random.h>
#include <clu/text/print.h>

#include "fluorine/core/game.h"
#include "fluorine/arena/random_player.h"
#include "fluorine/utils/tui.h"
#include "fluorine/evaluation/midgame_searcher.h"
#include "fluorine/evaluation/endgame_solver.h"

namespace flr
{
    namespace
    {
        class DatasetGenerator
        {
        public:
            DatasetGenerator(const Evaluator& evaluator, const DataGenerationOptions& options):
                eval_(&evaluator), opt_(options)
            {
                assert(opt_.worker_count > 0);
                if (opt_.show_progress)
                    bar_.emplace("Generating dataset", opt_.total_games);
            }

            Dataset run()
            {
                std::vector<std::thread> workers;
                workers.reserve(opt_.worker_count - 1);
                const std::size_t games_per_worker = opt_.total_games / opt_.worker_count;
                const std::size_t remain = opt_.total_games % opt_.worker_count;
                for (std::size_t i = 1; i < opt_.worker_count; i++)
                    workers.emplace_back(&DatasetGenerator::work, this, i, games_per_worker + (i < remain));
                work(0, games_per_worker + (0 < remain));
                for (auto& t : workers)
                    t.join();
                return std::move(dataset_);
            }

        private:
            using Histogram = std::array<std::size_t, cell_count>;

            const Evaluator* eval_;
            DataGenerationOptions opt_;
            std::optional<ProgressBar> bar_;
            Dataset dataset_;
            std::size_t size_tracker_ = 0;
            std::mutex mutex_;

            void work(const std::size_t worker_id, const std::size_t total)
            {
                auto& rng = clu::thread_rng();
                if (opt_.seed)
                    rng.seed(*opt_.seed + worker_id);
                Dataset local;
                MidgameSearcher searcher;
                EndgameSolver solver;
                std::bernoulli_distribution dist(opt_.epsilon);
                for (std::size_t i = 0; i < total; i++)
                {
                    const std::size_t old_dataset_size = local.size();
                    GameState state;
                    while (true)
                    {
                        if (state.legal_moves == 0)
                        {
                            state.play(Coords::none);
                            if (state.legal_moves == 0)
                                break;
                            continue;
                        }
                        if (const auto totals = state.board.count_total();
                            static_cast<int>(cell_count) - totals <= opt_.endgame_solve_depth)
                        {
                            const std::size_t middle_size = local.size();
                            const auto [_, score, move] = solver.solve(state);
                            local.emplace_back(state.canonical_board(), static_cast<float>(score));
                            std::ranges::copy(solver.transposition_table().entries(), std::back_inserter(local));
                            solver.clear_transposition_table();
                            if (!opt_.balance_phases)
                                break;
                            const auto middle =
                                std::span(local).subspan(old_dataset_size, middle_size - old_dataset_size);
                            const auto end = std::span(local).subspan(middle_size);
                            auto hist = data_histogram(middle);
                            const auto target = balance_target(hist);
                            std::ranges::shuffle(end, rng);
                            const DataPoint* ptr = balance_phases(hist, target, end);
                            local.erase(ptr - local.data() + local.begin(), local.end());
                            break;
                        }
                        else
                        {
                            const auto [_, score, move] = searcher.search(state, *eval_, opt_.midgame_search_depth);
                            local.emplace_back(state.canonical_board(), score);
                            std::ranges::copy(searcher.transposition_table().entries(), std::back_inserter(local));
                            const auto use_rand = totals - 4 < opt_.initial_random_moves || dist(rng);
                            state.play(use_rand ? RandomPlayer{}.get_move(state) : move);
                        }
                    }
                    update_progress(worker_id, local.size() - old_dataset_size);
                }
                std::scoped_lock lock(mutex_);
                dataset_.insert(dataset_.end(), local.begin(), local.end());
            }

            void update_progress(const std::size_t worker_id, const std::size_t increment)
            {
                std::scoped_lock lock(mutex_);
                if (!bar_)
                    return;
                size_tracker_ += increment;
                bar_->set_message(std::format("[Worker {:3}] Accumulated dataset size: {}", worker_id, size_tracker_));
                bar_->tick();
            }

            std::size_t balance_target(const Histogram& hist) const noexcept
            {
                std::size_t total = 0;
                const std::size_t start = 4 + static_cast<std::size_t>(opt_.initial_random_moves);
                const std::size_t stop = cell_count - static_cast<std::size_t>(opt_.endgame_solve_depth);
                for (std::size_t i = start; i < stop; i++)
                    total += hist[i];
                return total / (stop - start);
            }

            const DataPoint* balance_phases(
                Histogram& hist, const std::size_t target_count, const std::span<DataPoint> data) const
            {
                DataPoint* last = data.data();
                for (DataPoint& dp : data)
                {
                    const auto disks = static_cast<std::size_t>(dp.first.count_total());
                    if (hist[disks] >= target_count)
                        continue;
                    hist[disks]++;
                    std::swap(dp, *last++);
                }
                return last;
            }

            static Histogram data_histogram(const std::span<const DataPoint> data)
            {
                Histogram hist{};
                for (const auto& board : data | std::views::keys)
                    hist[static_cast<std::size_t>(board.count_total())] += 1;
                return hist;
            }
        };

        float calculate_mse(const Dataset& dataset, const Evaluator& eval, const std::size_t batch_size)
        {
            const std::size_t size = dataset.size();
            float total_se = 0.0f;
            for (std::size_t i = 0; i < size; i += batch_size)
            {
                const std::size_t end = std::min(i + batch_size, size);
                float batch_se = 0.0f;
                for (std::size_t j = i; j < end; j++)
                {
                    const float predicted = eval.evaluate(dataset[j].first);
                    const float error = dataset[j].second.error(predicted);
                    batch_se += error * error;
                }
                total_se += batch_se;
            }
            return total_se / static_cast<float>(size);
        }
    } // namespace

    Dataset generate_dataset_via_self_play(const Evaluator& evaluator, const DataGenerationOptions& options)
    {
        return DatasetGenerator(evaluator, options).run();
    }

    void train_evaluator(LearnableEvaluator& evaluator, Dataset& dataset, const TrainOptions& options)
    {
        auto& rng = clu::thread_rng();
        if (options.seed)
            rng.seed(*options.seed);
        std::optional<ProgressBar> bar;
        if (options.show_progress)
            bar.emplace("Training", options.epochs);
        const float initial_mse = calculate_mse(dataset, evaluator, options.batch_size);
        for (std::size_t i = 0; i < options.epochs; i++)
        {
            std::ranges::shuffle(dataset, rng);
            const float mse = evaluator.optimize(dataset, options.batch_size, options.learning_rate);
            if (bar)
            {
                bar->set_message(std::format("MSE: {} -> {}", initial_mse, mse));
                bar->tick();
            }
        }
    }

    void training_loop(LearnableEvaluator& evaluator, TrainingLoopOptions options)
    {
        auto rng = clu::thread_rng();
        if (options.seed)
            rng.seed(*options.seed);
        const bool seed_data_gen = !options.data_generation_options.seed;
        const bool seed_train = !options.train_options.seed;
        for (std::size_t i = 0; i < options.iterations; i++)
        {
            if (options.show_progress)
                clu::println("=== Iteration {} ===", i + 1);
            if (seed_data_gen)
                options.data_generation_options.seed = rng();
            if (seed_train)
                options.train_options.seed = rng();
            Dataset dataset = generate_dataset_via_self_play(evaluator, options.data_generation_options);
            train_evaluator(evaluator, dataset, options.train_options);
            if (options.on_iteration_finished)
                options.on_iteration_finished(evaluator, i);
        }
    }
} // namespace flr
