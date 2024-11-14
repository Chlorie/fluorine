#pragma once

#include <iosfwd>
#include <span>
#include <filesystem>

#include "evaluator.h"

FLUORINE_SUPPRESS_EXPORT_WARNING

namespace flr
{
    class FLUORINE_API LinearPatternEvaluator final : public LearnableEvaluator
    {
    public:
        enum class Symmetry : std::uint8_t
        {
            none,
            diagonal,
            axial
        };

        explicit LinearPatternEvaluator(std::span<const BitBoard> patterns, std::size_t stages = 1);

        [[nodiscard]] std::unique_ptr<Evaluator> clone() const override;
        [[nodiscard]] float evaluate(const Board& board) const override;
        float optimize(std::span<const DataPoint> dataset, std::size_t batch_size, float lr) override;

        void randomize_weights();
        [[nodiscard]] static std::unique_ptr<LinearPatternEvaluator> load(std::istream& stream);
        [[nodiscard]] static std::unique_ptr<LinearPatternEvaluator> load(const std::filesystem::path& path);
        void save(std::ostream& stream) const override;
        using LearnableEvaluator::save;

        void add_pattern(BitBoard pattern) { patterns_.emplace_back(pattern, stages_); }

    private:
        struct FLUORINE_API Pattern
        {
            BitBoard pattern;
            Symmetry symmetry;
            std::vector<std::uint16_t> index_map;
            std::size_t count;
            std::vector<float> weights;
            std::vector<float> gradients;

            explicit Pattern(BitBoard mask, std::size_t stages);
            static Pattern load(std::istream& stream, std::size_t stages);
            void save(std::ostream& stream) const;
            std::span<float> weights_at_stage(std::size_t stage) noexcept;
            std::span<const float> weights_at_stage(std::size_t stage) const noexcept;
            std::span<float> gradients_at_stage(std::size_t stage) noexcept;
            void apply_gradients() noexcept;
            void reset_gradients();
        };

        std::size_t stages_;
        std::vector<Pattern> patterns_;

        LinearPatternEvaluator() noexcept = default;
        std::size_t stage_of(const Board& board) const noexcept;
    };
} // namespace flr

FLUORINE_RESTORE_EXPORT_WARNING
