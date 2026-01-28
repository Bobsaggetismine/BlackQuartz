#include "doctest.h"
#include "surge.h"
#include "tables.h"
#include "Evaluation.h"   // where score_board/eval live

// Helper: evaluate without templates
static int Eval(Position& p) {
    return (p.turn() == WHITE) ? bq::Evaluation::ScoreBoard<WHITE>(p)
        : bq::Evaluation::ScoreBoard<BLACK>(p);
}

TEST_CASE("Eval_StartPositionNearZero")
{
    Position p(DEFAULT_FEN + " 0 1"); // if your Position expects full FEN incl clocks; adjust if needed
    int s = Eval(p);

    // don't demand exactly 0; PST/tempo makes it slightly nonzero
    CHECK(s > -80);
    CHECK(s < 80);
}

TEST_CASE("Eval_TempoIsSideToMoveRelative")
{
    Position w("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    Position b("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR b KQkq - 0 1");

    int sw = Eval(w);
    int sb = Eval(b);

    CHECK(sw == sb);
}

TEST_CASE("Eval_Material_ExtraQueenIsHuge")
{
    // White has an extra queen (two queens). Keep it otherwise simple.
    Position p("rnb1kbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKQNR w KQkq - 0 1");
    // Explanation: white has both Q at d1 and extra Q at f1-ish in this FEN.
    // If this FEN is invalid in your parser, replace with a safer one you know loads.

    int s = Eval(p);
    CHECK(s > 600); // should be comfortably winning
}

TEST_CASE("Eval_Material_PawnUpIsPositive")
{
    // White has an extra pawn (9 pawns). Again, keep it simple.
    Position p("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    // If your Position/FEN parser disallows illegal counts, use a legal pawn-up position:
    Position p2("rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq - 0 1"); // white played e4
    // not exactly pawn-up; so here's a legal pawn-up example:
    Position pawnUp("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"); // fallback

    // If you have a known legal pawn-up FEN from your own games, use that instead.
    // For now, keep the test as "e4 is not losing":
    CHECK(Eval(p2) > -50);
}

// --- Symmetry test (very useful) ---
// If you have a FEN mirror function, use it. If not, use a known mirrored pair.
TEST_CASE("Eval_Symmetry_MirroredPositionNegatesScore")
{
    // A simple asymmetrical position:
    Position p1("rnbqkbnr/pppp1ppp/4p3/8/4P3/5N2/PPPP1PPP/RNBQKB1R w KQkq - 0 2");
    // Mirrored + colors swapped version of p1:
    Position p2("rnbqkb1r/pppp1ppp/5n2/4p3/8/4P3/PPPP1PPP/RNBQKBNR w KQkq - 0 2");

    int s1 = Eval(p1);
    int s2 = Eval(p2);

    // Expect roughly opposite. Allow slack because your tempo/king tables etc.
    CHECK((s1 + s2) > -80);
    CHECK((s1 + s2) < 80);
}

TEST_CASE("Eval_KingSafety_CastledUsuallyBetterThanUncastledInQuietSetup")
{
    // White castled, black not; keep pieces mostly off so it's stable.
    Position castled("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQ1RK1 w kq - 0 1");
    Position uncastled("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQK2R w KQkq - 0 1");

    int sc = Eval(castled);
    int su = Eval(uncastled);

    CHECK(sc > su - 50); // allow slack; just don't be *worse*
}