#include "fluorine/evaluation/evaluator.h"

#include <fstream>

namespace flr
{
    void LearnableEvaluator::save(const std::filesystem::path& path) const
    {
        std::ofstream stream(path, std::ios::binary);
        save(stream);        
    }
} // namespace flr
