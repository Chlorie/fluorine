#include "fluorine/utils/perft.h"
#include "fluorine/core/game.h"
#include "bit.h"

namespace flr
{
    namespace
    {
        std::uint64_t perft(GameState state, const int depth) noexcept
        {
            if (depth == 0)
                return 1;
            if (state.legal_moves == 0)
            {
                state.play(Coords::none);
                if (state.legal_moves == 0)
                    return 1;
            }
            std::uint64_t result = 0;
            for (const int move : SetBits{state.legal_moves})
            {
                GameState s = state;
                s.play(static_cast<Coords>(move));
                result += perft(s, depth - 1);
            }
            return result;
        }
    } // namespace

    std::uint64_t perft(const int depth) noexcept { return perft(GameState{}, depth); }
} // namespace flr
