

#include <chrono>
#include "doctest.h"
#include "surge.h"
#include "ChessAi.h"

// Helpers
namespace {

    template <Color Us>
    bool is_legal_move(Position& p, Move m) {
        MoveList<Us> ml(p);
        for (Move x : ml)
            if (x == m) return true;
        return false;
    }

} // namespace

TEST_CASE("ChessAi: returns a legal move on startpos") {
    bq::ChessAi ai(WHITE, /*maxSelDepth=*/50);
    ai.setMaxDepth(10);

    Position p("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");

    bq::TimeControl tc{};
    tc.wtimeUs = 200'000; // 200ms "available"
    tc.wincUs = 0;

    Move m = ai.think(p, tc);
    CHECK(is_legal_move<WHITE>(p, m));
}

TEST_CASE("ChessAi: respects a fixed time budget (returns quickly)") {
    bq::ChessAi ai(WHITE, /*maxSelDepth=*/50);
    ai.setMaxDepth(64);
    ai.setOverheadUs(0);        // make the budget be the budget
    ai.setMinBudgetUs(0);

    Position p("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");

    // 30ms budget, allow jitter
    const long long budgetUs = 30'000;
    const long long slackUs = 30'000; // windows scheduling jitter + first-time cache effects

    auto t0 = std::chrono::steady_clock::now();
    Move m = ai.thinkFixedTime(p, budgetUs);
    auto t1 = std::chrono::steady_clock::now();

    auto elapsedUs = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();

    CHECK(is_legal_move<WHITE>(p, m));
    CHECK(elapsedUs <= budgetUs + slackUs);
}

TEST_CASE("ChessAi: handles tiny budgets (doesn't hang)") {
    bq::ChessAi ai(WHITE, /*maxSelDepth=*/50);
    ai.setMaxDepth(64);
    ai.setOverheadUs(0);
    ai.setMinBudgetUs(0);

    Position p("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");

    const long long budgetUs = 1'000;   // 1ms
    const long long slackUs = 50'000;  // allow coarse scheduling, but should still return fast

    auto t0 = std::chrono::steady_clock::now();
    Move m = ai.thinkFixedTime(p, budgetUs);
    auto t1 = std::chrono::steady_clock::now();

    auto elapsedUs = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();

    CHECK(is_legal_move<WHITE>(p, m));
    CHECK(elapsedUs <= budgetUs + slackUs);
}

TEST_CASE("ChessAi: does not mutate the root position") {
    bq::ChessAi ai(WHITE, /*maxSelDepth=*/50);
    ai.setMaxDepth(12);

    Position p("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    const auto h0 = p.get_hash();

    bq::TimeControl tc{};
    tc.wtimeUs = 50'000; // 50ms
    Move m = ai.think(p, tc);

    (void)m;
    CHECK(p.get_hash() == h0);
}

TEST_CASE("ChessAi: short budget tends to search less time than long budget") {
    bq::ChessAi ai(WHITE, /*maxSelDepth=*/50);
    ai.setMaxDepth(64);
    ai.setOverheadUs(0);
    ai.setMinBudgetUs(0);

    Position p1("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    Position p2("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");

    auto t0 = std::chrono::steady_clock::now();
    Move mShort = ai.thinkFixedTime(p1, 10'000); // 10ms
    auto t1 = std::chrono::steady_clock::now();

    auto t2 = std::chrono::steady_clock::now();
    Move mLong = ai.thinkFixedTime(p2, 80'000); // 80ms
    auto t3 = std::chrono::steady_clock::now();

    auto shortUs = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();
    auto longUs = std::chrono::duration_cast<std::chrono::microseconds>(t3 - t2).count();

    CHECK(is_legal_move<WHITE>(p1, mShort));
    CHECK(is_legal_move<WHITE>(p2, mLong));

    // Not exact, but should generally be smaller (allow huge slack if system is noisy)
    CHECK(shortUs < longUs);
}
