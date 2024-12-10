#include <clu/text/print.h>
#include <clu/chrono_utils.h>
#include <fluorine/utils/perft.h>
#include <fluorine/utils/humanize.h>

namespace
{
    // From https://www.egaroucid.nyanyan.dev/ja/technology/explanation
    // clang-format off
    constexpr std::size_t expected_values[]{
        1,            
        4,
        12,
        56,
        244,
        1396,
        8200,
        55092,
        390216,
        3005320,
        24571420,
        212260880,
        1939899208,
        18429791868,
        184043158384,
        1891845643044
    };
    // clang-format on
} // namespace

int main()
{
    clu::println("[Perft]");
    for (int i = 1; i <= 60; i++)
    {
        std::uint64_t res;
        const auto elapsed = clu::timeit([&] { res = flr::perft(i); });
        clu::println("Depth {:2}: {:20} nodes (elapsed {})", i, res, flr::humanize(elapsed));
        if (i < std::size(expected_values) && res != expected_values[i])
        {
            clu::println("ERROR! Expected: {} nodes", expected_values[i]);
            return 1;
        }
    }
}
