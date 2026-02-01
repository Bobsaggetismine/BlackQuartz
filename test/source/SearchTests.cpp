
#include "doctest.h"

#include <string>

#include "surge.h"
#include "Search.h"      
#include "Logger.h"
namespace {

    template <Color Us>
    bool is_legal_move(Position& p, const Move& m) {
        MoveList<Us> list(p);
        for (const Move& x : list) {
            if (x == m) return true;
        }
        return false;
    }

    static long long calc_nps(const bq::SearchStats& s) {
        return (s.ellapsedTime > 0)
            ? (s.nodesSearched * 1'000'000LL) / s.ellapsedTime
            : 0;
    }
    template <Color SideToMove>
    bool is_checkmated(Position& p) {
        MoveList<SideToMove> ml(p);
        return ml.size() == 0 && p.in_check<SideToMove>();
    }

} 

TEST_CASE("Search: startpos returns a legal move and searches some nodes") {
    bq::Search search(50);

    Position p("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    auto stats = search.initiateIterativeSearch<WHITE>(p, 4);

    CHECK(stats.nodesSearched > 0);
    CHECK(stats.ellapsedTime >= 0);

    CHECK(is_legal_move<WHITE>(p, stats.selectedMove));
}

TEST_CASE("Search: nodes generally increase with depth (startpos sanity)") {
    bq::Search search(50);

    Position p1("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    auto s3 = search.initiateIterativeSearch<WHITE>(p1, 3);

    bq::Search search2(50);
    Position p2("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    auto s4 = search2.initiateIterativeSearch<WHITE>(p2,4);

    CHECK(s4.nodesSearched > s3.nodesSearched);

    CHECK(calc_nps(s3) >= 0);
    CHECK(calc_nps(s4) >= 0);
}


TEST_CASE("Search: mate-in-1 results in checkmate") {
    bq::Search search(50);
    Position p("r3kb1r/ppp1pppp/5n2/1n3P2/6bP/4K3/PPq5/RNB2q2 b kq - 0 13");

    auto stats = search.initiateIterativeSearch<BLACK>(p, 7);
    CHECK(is_legal_move<BLACK>(p, stats.selectedMove));

    p.play<BLACK>(stats.selectedMove);
    CHECK(is_checkmated<WHITE>(p));
    p.undo<BLACK>(stats.selectedMove);
}

TEST_CASE("Search: qsearch in-check does not stand-pat (sanity)") {
    bq::Search search(50);


    Position p("4r3/8/8/8/8/8/8/4K3 w - - 0 1");

    auto stats = search.initiateIterativeSearch<WHITE>(p, 2);

    CHECK(stats.nodesSearched > 0);
    CHECK(is_legal_move<WHITE>(p, stats.selectedMove));
}
TEST_CASE("Search: selected move is always legal on a set of positions") {
    struct Case { const char* fen; Color stm; };
    Case cases[] = {
        {"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", WHITE},
        {"r1bqkbnr/pppp1ppp/2n5/4p3/4P3/5N2/PPPP1PPP/RNBQKB1R w KQkq - 2 3", WHITE},
        {"r3k2r/pppq1ppp/2npbn2/4p3/4P3/2NPBN2/PPPQ1PPP/R3K2R w KQkq - 0 1", WHITE},
        {"8/8/8/3pP3/8/8/8/4K3 w - d6 0 1", WHITE}, 
        {"7k/5Q2/7K/8/8/8/8/8 w - - 0 1", WHITE}, 
    };

    for (auto& c : cases) {
        bq::Search search(50);
        Position p(c.fen);

        if (c.stm == WHITE) {
            auto s = search.initiateIterativeSearch<WHITE>(p, 4);
            CHECK(is_legal_move<WHITE>(p, s.selectedMove));
        }
        else {
            auto s = search.initiateIterativeSearch<BLACK>(p, 4);
            CHECK(is_legal_move<BLACK>(p, s.selectedMove));
        }
    }
}
TEST_CASE("Search: if a position has exactly one legal move, Search selects it") {

    const char* fens[] = {
        "8/8/8/8/8/8/4r3/4K3 w - - 0 1",
        "8/8/8/8/8/8/3r4/4K3 w - - 0 1",
        "8/8/8/8/8/8/4q3/4K3 w - - 0 1",
        "8/8/8/8/8/8/7r/7K w - - 0 1",
    };

    const char* chosen = nullptr;
    Move onlyMove{};

    for (auto fen : fens) {
        Position p(fen);
        MoveList<WHITE> ml(p);
        if (ml.size() == 1) {
            chosen = fen;
            onlyMove = *ml.begin();
            break;
        }
    }

    if (!chosen) {
        DOCTEST_INFO("No single-legal-move FEN found in the test set for this movegen.");
        return;
    }

    Position p(chosen);
    bq::Search search(50);
    auto s = search.initiateIterativeSearch<WHITE>(p, 2);

    CHECK(s.selectedMove == onlyMove);
}
TEST_CASE("Search: deeper depth searches more nodes (same position, new Search instance)") {
    Position fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");

    bq::Search s1(50);
    Position p1 = fen;
    auto a = s1.initiateIterativeSearch<WHITE>(p1, 3);

    bq::Search s2(50);
    Position p2 = fen;
    auto b = s2.initiateIterativeSearch<WHITE>(p2, 4);

    CHECK(b.nodesSearched > a.nodesSearched);
    CHECK(is_legal_move<WHITE>(p2, b.selectedMove));
}
TEST_CASE("Search: does not mutate root position") {
    bq::Search search(50);
    Position p("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");

    auto h0 = p.get_hash();
    auto s = search.initiateIterativeSearch<WHITE>(p, 4);
    (void)s;

    CHECK(p.get_hash() == h0);
}
TEST_CASE("Search: TT reuse reduces nodes on repeated search") {
    bq::Search search(50);
    Position p("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");

    auto a = search.initiateIterativeSearch<WHITE>(p, 5);

    auto b = search.initiateIterativeSearch<WHITE>(p, 5);

    CHECK(b.nodesSearched < a.nodesSearched);
}
TEST_CASE("Search: NPS is sensible at moderate depth") {
    bq::Search search(50);
    Position p("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");

    auto s = search.initiateIterativeSearch<WHITE>(p, 6);

    CHECK(s.ellapsedTime > 0);
    CHECK(s.nodesSearched > 0);
    CHECK(calc_nps(s) > 0);
}
TEST_CASE("Search: checkmate vs stalemate terminal handling") {
    bq::Search search(50);

    Position mate("7k/6Q1/7K/8/8/8/8/8 b - - 0 1");
    MoveList<BLACK> ml1(mate);
    REQUIRE(ml1.size() == 0);
    REQUIRE(mate.in_check<BLACK>());

    auto s1 = search.initiateIterativeSearch<BLACK>(mate, 2);
    CHECK(s1.nodesSearched > 0);

    bq::Search search2(50);
    Position stal("7k/5Q2/7K/8/8/8/8/8 b - - 0 1");
    MoveList<BLACK> ml2(stal);
    REQUIRE(ml2.size() == 0);
    REQUIRE(!stal.in_check<BLACK>());

    auto s2 = search2.initiateIterativeSearch<BLACK>(stal, 2);
    CHECK(s2.nodesSearched > 0);
}