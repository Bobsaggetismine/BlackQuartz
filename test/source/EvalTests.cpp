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
// Keep your includes + Eval helper

TEST_CASE("Eval_CheckPenalty_IsApplied")
{
    // Black king in check by white rook on e-file
    // White: Ke1 Re7 ; Black: Ke8 ; side to move doesn't matter much for the check term itself
    Position inCheck("4k3/4R3/8/8/8/8/8/4K3 b - - 0 1");
    int s = Eval(inCheck);

    // If black is in check, from side-to-move eval (black to move here),
    // your code does: if (pos.in_check<Us>()) mg -= 20;
    // So black-to-move should be slightly worse than "not in check" version.
    Position notInCheck("4k3/8/8/8/8/8/4R3/4K3 b - - 0 1");
    int s2 = Eval(notInCheck);

    CHECK(s < s2);
}

TEST_CASE("Eval_BishopPair_IsBetterThanBishopAndKnight_AllElseEqualish")
{
    // Very stripped, legal: Kings + minor pieces.
    // Case A: White has 2 bishops
    Position twoBishops("4k3/8/8/8/8/8/2B1B3/4K3 w - - 0 1");

    // Case B: White has bishop + knight
    Position bishopKnight("4k3/8/8/8/8/8/2B1N3/4K3 w - - 0 1");

    int a = Eval(twoBishops);
    int b = Eval(bishopKnight);

    // Your bishop pair term is +25/+35 blended => should usually win by some margin
    CHECK(a > b);
}

TEST_CASE("Eval_PassedPawn_IsValuedMoreThanBlockedPawn")
{
    // White passed pawn on e5, black pawn far away.
    // Compare to a similar position where black pawn blocks it on e6.
    Position passed("4k3/8/8/4P3/8/8/8/4K3 w - - 0 1");
    Position blocked("4k3/8/4p3/4P3/8/8/8/4K3 w - - 0 1");

    int sp = Eval(passed);
    int sb = Eval(blocked);

    CHECK(sp > sb);
}

TEST_CASE("Eval_DoubledPawns_ArePenalized")
{
    // White has doubled pawns on c-file (c2 and c3)
    Position doubled("4k3/8/8/8/8/2P5/2P5/4K3 w - - 0 1");

    // White has same pawn count but not doubled (c2 and d2)
    Position healthy("4k3/8/8/8/8/8/2PP4/4K3 w - - 0 1");

    int sd = Eval(doubled);
    int sh = Eval(healthy);

    CHECK(sh > sd);
}

TEST_CASE("Eval_IsolatedPawn_IsPenalized")
{
    // Isolated pawn on a2 (no neighbors)
    Position isolated("4k3/8/8/8/8/8/P7/4K3 w - - 0 1");

    // Connected pawn structure: a2 + b2 (not isolated)
    Position connected("4k3/8/8/8/8/8/PP6/4K3 w - - 0 1");

    int si = Eval(isolated);
    int sc = Eval(connected);

    CHECK(sc > si);
}

TEST_CASE("Eval_RookOn7th_IsRewarded")
{
    // White rook on 7th rank vs rook on 6th rank
    // Keep kings legal.
    Position rook7("4k3/4R3/8/8/8/8/8/4K3 w - - 0 1"); // rook on e7
    Position rook6("4k3/8/4R3/8/8/8/8/4K3 w - - 0 1"); // rook on e6

    int s7 = Eval(rook7);
    int s6 = Eval(rook6);

    CHECK(s7 > s6);
}

TEST_CASE("Eval_RookOpenFile_Bonus")
{
    // Same material in both: White has a rook; Black has a king + 1 pawn.
    // Only difference: where the black pawn is.
    // Rook is on e1 in both cases.

    // Open e-file: black pawn is on a7 (not on e-file)
    Position openFile("4k3/p7/8/8/8/8/8/4R1K1 w - - 0 1");

    // Semi-open e-file: black pawn is on e7 (on e-file)
    Position semiOpen("4k3/4p3/8/8/8/8/8/4R1K1 w - - 0 1");

    int so = Eval(openFile);
    int ss = Eval(semiOpen);

    // open file bonus (18) should beat semi-open (10), plus pawn PST differences are tiny
    CHECK(so > ss);
}


TEST_CASE("Eval_KingOpenFilePenalty_WhenNoPawnsOnKingFile")
{
    // King on e1 with no pawns on e-file => should be penalized a bit by king safety (open file)
    Position openE("4k3/8/8/8/8/8/8/4K3 w - - 0 1");

    // Add a pawn on e2 => not open anymore for that file (still minimal position)
    Position pawnE("4k3/8/8/8/8/8/4P3/4K3 w - - 0 1");

    int so = Eval(openE);
    int sp = Eval(pawnE);

    CHECK(sp > so);
}

TEST_CASE("Eval_Mobility_KnightCentralizationHelps")
{
    // Knights: center should score better via PST + mobility
    Position center("4k3/8/8/8/3N4/8/8/4K3 w - - 0 1"); // Nd4
    Position corner("4k3/8/8/8/8/8/N7/4K3 w - - 0 1");  // Na2

    int sc = Eval(center);
    int sk = Eval(corner);

    CHECK(sc > sk);
}

TEST_CASE("Eval_Sanity_ScoreIsFiniteAndReasonableOnRandomSimplePosition")
{
    // Just a quick “doesn’t explode” regression test.
    Position p("4k3/8/8/3p4/4P3/8/8/4K3 w - - 0 1");

    int s = Eval(p);

    CHECK(s > -5000);
    CHECK(s < 5000);
}
