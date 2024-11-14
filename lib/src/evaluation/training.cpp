#include "fluorine/evaluation/training.h"

#include <mutex>
#include <thread>
#include <clu/random.h>
#include <clu/text/print.h>

#include "fluorine/core/game.h"
#include "fluorine/arena/random_player.h"
#include "fluorine/utils/tui.h"
#include "fluorine/evaluation/midgame_searcher.h"

namespace flr
{
    namespace
    {
        GameState random_initial_state(const std::size_t moves)
        {
            GameState state;
            RandomPlayer rand;
            for (std::size_t i = 0; i < moves; i++)
                state.play(rand.get_move(state));
            return state;
        }

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
                std::bernoulli_distribution dist(opt_.epsilon);
                for (std::size_t i = 0; i < total; i++)
                {
                    const std::size_t old_dataset_size = local.size();
                    auto state = random_initial_state(opt_.initial_random_moves);
                    while (true)
                    {
                        if (state.legal_moves == 0)
                        {
                            state.play(Coords::none);
                            if (state.legal_moves == 0)
                                break;
                            continue;
                        }
                        const auto [_, score, move] = searcher.search(state, *eval_, opt_.tree_search_depth);
                        local.emplace_back(state.canonical_board(), score);
                        std::ranges::copy(searcher.transposition_table().entries(), std::back_inserter(local));
                        state.play(dist(rng) ? RandomPlayer{}.get_move(state) : move);
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
