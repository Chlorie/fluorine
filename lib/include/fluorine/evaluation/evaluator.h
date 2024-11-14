#pragma once

#include <memory>
#include <iosfwd>
#include <filesystem>

#include "transposition_table.h"
#include "../core/game.h"

namespace flr
{
    class FLUORINE_API Evaluator
    {
    public:
        Evaluator() noexcept = default;
        virtual ~Evaluator() noexcept = default;
        Evaluator(const Evaluator&) = delete;
        Evaluator(Evaluator&&) = delete;
        Evaluator& operator=(const Evaluator&) = delete;
        Evaluator& operator=(Evaluator&&) = delete;

        [[nodiscard]] virtual std::unique_ptr<Evaluator> clone() const = 0;
        [[nodiscard]] virtual float evaluate(const Board& board) const = 0;
    };

    using DataPoint = std::pair<Board, Bounds>;
    using Dataset = std::vector<DataPoint>;

    class FLUORINE_API LearnableEvaluator : public Evaluator
    {
    public:
        virtual float optimize(std::span<const DataPoint> dataset, std::size_t batch_size, float lr) = 0;
        virtual void save(std::ostream& stream) const = 0;
        void save(const std::filesystem::path& path) const;
    };
} // namespace flr
