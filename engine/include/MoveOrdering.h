#pragma once
#include <algorithm>
#include <vector>
#include <cstdint>

#include "surge.h"
#include "TranspositionTable.h"

namespace bq {

    template <Color Us>
    static inline int moveOrderingScore(const Position& p, const Move m, const Move ttMove, bool use_tt)
    {
        int score = 0;

        if (use_tt && m == ttMove)
            score += 1'000'000;

        if (m.is_capture())
            score += 100'000;

        if (m.flags() != QUIET)
            score += 10'000;

        return score;
    }

    template <Color Us>
    void orderMoves(Position& p, MoveList<Us>& moves, const bq::TranspositionTable& tt, bool use_tt)
    {
        Move ttMove{};
        if (use_tt) {
            auto e = tt.lookup(p.get_hash());
            if (e.valid) ttMove = e.bestMove;
        }

        std::vector<std::pair<int, Move>> scored;
        scored.reserve(moves.size());

        for (const Move m : moves) {
            int s = 0;

            if (use_tt && m == ttMove) s += 1'000'000;

            if (m.is_promotion()) s += 200'000;
            if (m.is_capture())   s += 100'000;
            if (m.flags() != QUIET) s += 10'000;

            scored.emplace_back(s, m);
        }

        std::stable_sort(scored.begin(), scored.end(),
            [](const auto& a, const auto& b) { return a.first > b.first; });

        Move* out = moves.begin();
        for (const auto& sm : scored) {
            *out++ = sm.second;
        }
    }

}

