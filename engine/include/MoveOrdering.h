#pragma once
#include <array>
#include <algorithm>
#include <cstdint>

#include "surge.h"
#include "TranspositionTable.h"

namespace bq {

    struct ScoredMove {
        int  score;
        Move move;
    };

    template <Color Us>
    static inline int scoreMove(Move m, Move ttMove)
    {
        int s = 0;

        if (!ttMove.is_null() && m == ttMove)
            s += 1'000'000;

        if (m.is_promotion())
            s += 200'000;

        if (m.is_capture())
            s += 100'000;
        else if (m.flags() != QUIET)
            s += 10'000;

        return s;
    }

    template <Color Us>
    inline void orderMoves(MoveList<Us>& moves, Move ttMove = Move{})
    {
        Move* first = moves.begin();
        Move* last = moves.end();
        const int n = int(last - first);
        if (n <= 1) return;

        // MoveList max is 218 in your code
        std::array<ScoredMove, 218> scored;

        for (int i = 0; i < n; ++i) {
            const Move m = first[i];
            scored[i] = { scoreMove<Us>(m, ttMove), m };
        }

        std::sort(scored.begin(), scored.begin() + n,
            [](const ScoredMove& a, const ScoredMove& b) {
                return a.score > b.score;
            });

        for (int i = 0; i < n; ++i)
            first[i] = scored[i].move;
    }

} // namespace bq
