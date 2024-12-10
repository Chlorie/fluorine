#pragma once

#include <optional>
#include <clu/function.h>

#include "transposition_table.h"
#include "evaluator.h"

FLUORINE_SUPPRESS_EXPORT_WARNING

namespace flr
{
    using DataPoint = std::pair<Board, Bounds>;
    using Dataset = std::vector<DataPoint>;

    struct DataGenerationOptions
    {
        std::size_t total_games = 100;
        int midgame_search_depth = 8;
        int endgame_solve_depth = 16;
        bool balance_phases = true;
        int initial_random_moves = 6;
        float epsilon = 0.01f; //< Probability of random move
        std::size_t worker_count = 1;
        std::optional<std::uint64_t> seed = std::nullopt;
        bool show_progress = true;
    };

    struct TrainOptions
    {
        std::size_t epochs = 20;
        std::size_t batch_size = 32;
        float learning_rate = 0.01f;
        std::optional<std::uint64_t> seed = std::nullopt;
        bool show_progress = true;
    };

    struct TrainingLoopOptions
    {
        std::size_t iterations = 10;
        DataGenerationOptions data_generation_options = {};
        TrainOptions train_options = {};
        clu::move_only_function<void(LearnableEvaluator&, std::size_t)> on_iteration_finished = {};
        std::optional<std::uint64_t> seed = std::nullopt;
        bool show_progress = true;
    };

    [[nodiscard]] FLUORINE_API Dataset generate_dataset_via_self_play(
        const Evaluator& evaluator, const DataGenerationOptions& options = {});

    FLUORINE_API void train_evaluator(
        LearnableEvaluator& evaluator, Dataset& dataset, const TrainOptions& options = {});

    FLUORINE_API void training_loop(LearnableEvaluator& evaluator, TrainingLoopOptions options = {});
} // namespace flr

FLUORINE_RESTORE_EXPORT_WARNING
