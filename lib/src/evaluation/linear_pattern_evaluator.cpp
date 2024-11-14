#include "fluorine/evaluation/linear_pattern_evaluator.h"

#include <fstream>
#include <algorithm>
#include <numeric>
#include <array>
#include <clu/random.h>
#include <clu/static_vector.h>

#include "../utils/bit.h"

namespace flr
{
    namespace
    {
        constexpr std::size_t max_pattern_size = 10;
        using Symmetry = LinearPatternEvaluator::Symmetry;

        constexpr auto powers_of_3 = []
        {
            std::array<std::uint16_t, max_pattern_size + 1> res{1};
            for (std::size_t i = 1; i < res.size(); i++)
                res[i] = res[i - 1] * 3;
            return res;
        }();

        constexpr auto generate_2to3_table() noexcept
        {
            std::array<std::uint16_t, (1 << max_pattern_size)> table{};
            for (std::size_t i = 0; i < table.size(); i++)
                for (std::size_t j = 0; j < max_pattern_size; j++)
                    table[i] += (i & (1ull << j)) ? powers_of_3[j] : std::uint16_t{};
            return table;
        }

        // Reinterpret a binary number as a ternary number
        constexpr auto binary_to_ternary_table = generate_2to3_table();

        constexpr std::uint16_t extract_pattern(const Board board, const BitBoard pattern) noexcept
        {
            const auto black = bit_compressr(board.black, pattern);
            const auto white = bit_compressr(board.white, pattern);
            return static_cast<std::uint16_t>(binary_to_ternary_table[black] + binary_to_ternary_table[white] * 2);
        }

        // Get all the 8 possible rotoreflections of a board
        constexpr auto transform_d4(const BitBoard mask) noexcept
        {
            const BitBoard flip = mirror_main_diagonal(mask);
            return std::array{
                mask, rotate_90_ccw(mask), rotate_180(mask), rotate_90_cw(mask), //
                flip, rotate_90_ccw(flip), rotate_180(flip), rotate_90_cw(flip) //
            };
        }

        BitBoard find_pattern_canonical_form(const BitBoard mask) noexcept
        {
            return std::ranges::min(transform_d4(mask));
        }

        Symmetry find_pattern_symmetry(const BitBoard mask) noexcept
        {
            using enum Symmetry;
            if (mask == mirror_horizontal(mask))
                return axial;
            if (mask == mirror_main_diagonal(mask))
                return diagonal;
            return none;
        }

        std::vector<std::uint16_t> generate_pattern_index_map(const BitBoard mask, const Symmetry symmetry)
        {
            if (static_cast<std::size_t>(std::popcount(mask)) > max_pattern_size)
                throw std::runtime_error("Pattern is too large");

            using enum Symmetry;
            const auto pattern_size = static_cast<std::size_t>(std::popcount(mask));
            std::vector<std::uint16_t> map(powers_of_3[pattern_size]);
            std::iota(map.begin(), map.end(), std::uint16_t{});
            if (symmetry == none)
                return map;

            // Get reflect map
            const auto reflect = symmetry == diagonal ? &mirror_main_diagonal : &mirror_horizontal;
            for (std::size_t i = 0; i < 1ull << pattern_size; i++)
                for (std::size_t j = 0; j < 1ull << pattern_size; j++)
                {
                    if (i & j)
                        continue;
                    const auto first =
                        static_cast<std::uint16_t>(binary_to_ternary_table[i] + binary_to_ternary_table[j] * 2);
                    const BitBoard black = reflect(bit_expandr(i, mask));
                    const BitBoard white = reflect(bit_expandr(j, mask));
                    const auto second = extract_pattern({black, white}, mask);
                    map[first] = std::min(first, second);
                }

            // Compress unused indices
            std::vector<bool> occupied(map.size());
            for (const auto i : map)
                occupied[i] = true;
            std::vector<std::uint16_t> compressed(map.size());
            for (std::uint16_t i = 0, j = 0; const bool b : occupied)
            {
                if (b)
                    compressed[i] = j++;
                i++;
            }
            for (auto& i : map)
                i = compressed[i];
            return map;
        }

        template <typename T>
            requires std::is_trivial_v<T>
        T read(std::istream& stream)
        {
            T value;
            stream.read(reinterpret_cast<char*>(&value), sizeof(T));
            return value;
        }

        template <typename T>
            requires std::is_trivial_v<T>
        void write(std::ostream& stream, const T& value)
        {
            stream.write(reinterpret_cast<const char*>(&value), sizeof(T));
        }
    } // namespace

    LinearPatternEvaluator::LinearPatternEvaluator(const std::span<const BitBoard> patterns, const std::size_t stages):
        stages_(stages)
    {
        assert(stages > 0);
        patterns_.reserve(patterns.size());
        for (const auto& pattern : patterns)
            patterns_.emplace_back(pattern, stages_);
    }

    std::unique_ptr<Evaluator> LinearPatternEvaluator::clone() const
    {
        auto res = std::unique_ptr<LinearPatternEvaluator>(new LinearPatternEvaluator);
        res->patterns_ = patterns_;
        return res;
    }

    float LinearPatternEvaluator::evaluate(const Board& board) const
    {
        const std::size_t stage = stage_of(board);
        if (stage == stages_) [[unlikely]]
            return static_cast<float>(board.disk_difference());
        float res = 0.0f;
        const auto self_d4 = transform_d4(board.black);
        const auto opponent_d4 = transform_d4(board.white);
        for (const auto& pattern : patterns_)
        {
            const std::size_t n = pattern.symmetry == Symmetry::none ? 8 : 4;
            const auto weights = pattern.weights_at_stage(stage);
            for (std::size_t i = 0; i < n; i++)
            {
                const auto idx = extract_pattern({self_d4[i], opponent_d4[i]}, pattern.pattern);
                res += weights[pattern.index_map[idx]];
            }
        }
        return res;
    }

    float LinearPatternEvaluator::optimize(
        const std::span<const DataPoint> dataset, const std::size_t batch_size, const float lr)
    {
        float total_se = 0.0f;
        std::vector<float*> updated_params;
        updated_params.reserve(patterns_.size() * 8);
        for (std::size_t i = 0; i < dataset.size(); i += batch_size)
        {
            for (auto& pattern : patterns_)
                pattern.reset_gradients();
            const std::size_t end = std::min(dataset.size(), i + batch_size);
            const float mult = 2.0f * lr / static_cast<float>(end - i);
            float batch_se = 0.0f;
            for (std::size_t j = i; j < end; j++)
            {
                updated_params.clear();
                const auto& [board, bounds] = dataset[j];
                const std::size_t stage = stage_of(board);
                if (stage == stages_) [[unlikely]]
                    continue;
                const auto self_d4 = transform_d4(board.black);
                const auto opponent_d4 = transform_d4(board.white);
                float predicted = 0.0f;
                for (auto& pattern : patterns_)
                {
                    const std::size_t sym = pattern.symmetry == Symmetry::none ? 8 : 4;
                    const auto weights = pattern.weights_at_stage(stage);
                    const auto grads = pattern.gradients_at_stage(stage);
                    for (std::size_t k = 0; k < sym; k++)
                    {
                        const auto idx = extract_pattern({self_d4[k], opponent_d4[k]}, pattern.pattern);
                        const auto mapped = pattern.index_map[idx];
                        updated_params.push_back(&grads[mapped]);
                        predicted += weights[mapped];
                    }
                }
                const float error = bounds.error(predicted);
                if (error == 0.0f)
                    continue;
                batch_se += error * error;
                const float grad = std::clamp(mult * error, -2.0f, 2.0f); // Clip gradient
                for (float* g : updated_params)
                    *g += grad;
            }
            total_se += batch_se;
            for (auto& pattern : patterns_)
                pattern.apply_gradients();
        }
        return total_se / static_cast<float>(dataset.size());
    }

    void LinearPatternEvaluator::randomize_weights()
    {
        // Some arbitrary distribution just to get the weights to be non-zero
        const float stddev = 1.0f / static_cast<float>(patterns_.size());
        auto& rng = clu::thread_rng();
        std::normal_distribution dist(0.0f, stddev);
        for (auto& pattern : patterns_)
            for (auto& weight : pattern.weights)
                weight = dist(rng);
    }

    std::unique_ptr<LinearPatternEvaluator> LinearPatternEvaluator::load(std::istream& stream)
    {
        auto res = std::unique_ptr<LinearPatternEvaluator>(new LinearPatternEvaluator);
        res->stages_ = read<std::size_t>(stream);
        while (true)
        {
            Pattern pattern = Pattern::load(stream, res->stages_);
            if (pattern.pattern == 0)
                return res;
            res->patterns_.push_back(std::move(pattern));
        }
    }

    std::unique_ptr<LinearPatternEvaluator> LinearPatternEvaluator::load(const std::filesystem::path& path)
    {
        std::ifstream stream(path, std::ios::binary);
        return load(stream);
    }

    void LinearPatternEvaluator::save(std::ostream& stream) const
    {
        write(stream, stages_);
        for (const auto& p : patterns_)
            p.save(stream);
        write(stream, BitBoard{});
    }

    LinearPatternEvaluator::Pattern::Pattern(const BitBoard mask, const std::size_t stages):
        pattern(find_pattern_canonical_form(mask)), symmetry(find_pattern_symmetry(pattern)),
        index_map(generate_pattern_index_map(pattern, symmetry)), count(std::ranges::max(index_map) + 1),
        weights(stages * count)
    {
    }

    LinearPatternEvaluator::Pattern LinearPatternEvaluator::Pattern::load(
        std::istream& stream, const std::size_t stages)
    {
        Pattern res(read<BitBoard>(stream), stages);
        stream.read(reinterpret_cast<char*>(res.weights.data()),
            static_cast<std::streamsize>(sizeof(float) * res.weights.size()));
        return res;
    }

    void LinearPatternEvaluator::Pattern::save(std::ostream& stream) const
    {
        write(stream, pattern);
        stream.write(reinterpret_cast<const char*>(weights.data()),
            static_cast<std::streamsize>(sizeof(float) * weights.size()));
    }

    std::span<float> LinearPatternEvaluator::Pattern::weights_at_stage(const std::size_t stage) noexcept
    {
        return {weights.data() + stage * count, count};
    }

    std::span<const float> LinearPatternEvaluator::Pattern::weights_at_stage(const std::size_t stage) const noexcept
    {
        return {weights.data() + stage * count, count};
    }

    std::span<float> LinearPatternEvaluator::Pattern::gradients_at_stage(const std::size_t stage) noexcept
    {
        return {gradients.data() + stage * count, count};
    }

    void LinearPatternEvaluator::Pattern::apply_gradients() noexcept
    {
        assert(gradients.size() == weights.size());
        const std::size_t size = weights.size();
        for (std::size_t i = 0; i < size; i++)
            weights[i] -= gradients[i];
    }

    void LinearPatternEvaluator::Pattern::reset_gradients()
    {
        if (gradients.empty())
        {
            gradients.resize(weights.size());
            return;
        }
        assert(gradients.size() == weights.size());
        std::ranges::fill(gradients, 0.0f);
    }

    std::size_t LinearPatternEvaluator::stage_of(const Board& board) const noexcept
    {
        return static_cast<std::size_t>(board.count_total() - 4) * stages_ / (cell_count - 4);
    }
} // namespace flr
