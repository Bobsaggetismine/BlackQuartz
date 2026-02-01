#include "doctest.h"
#include "surge.h"
#include "tables.h"
#include "Evaluation.h"

static int Eval(Position& p) {
    return (p.turn() == WHITE) ? bq::Evaluation::ScoreBoard<WHITE>(p)
        : bq::Evaluation::ScoreBoard<BLACK>(p);
}

TEST_CASE("Eval_StartPositionNearZero")
{
    Position p(DEFAULT_FEN + " 0 1");
    int s = Eval(p);

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
    Position p("rnb1kbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKQNR w KQkq - 0 1");

    int s = Eval(p);
    CHECK(s > 600);
}

TEST_CASE("Eval_Material_PawnUpIsPositive")
{
    Position p("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    Position p2("rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq - 0 1");
    Position pawnUp("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    CHECK(Eval(p2) > -50);
}


TEST_CASE("Eval_Symmetry_MirroredPositionNegatesScore")
{
    Position p1("rnbqkbnr/pppp1ppp/4p3/8/4P3/5N2/PPPP1PPP/RNBQKB1R w KQkq - 0 2");
    Position p2("rnbqkb1r/pppp1ppp/5n2/4p3/8/4P3/PPPP1PPP/RNBQKBNR w KQkq - 0 2");

    int s1 = Eval(p1);
    int s2 = Eval(p2);

    CHECK((s1 + s2) > -80);
    CHECK((s1 + s2) < 80);
}

TEST_CASE("Eval_KingSafety_CastledUsuallyBetterThanUncastledInQuietSetup")
{
    Position castled("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQ1RK1 w kq - 0 1");
    Position uncastled("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQK2R w KQkq - 0 1");

    int sc = Eval(castled);
    int su = Eval(uncastled);

    CHECK(sc > su - 50);
}

TEST_CASE("Eval_CheckPenalty_IsApplied")
{
    Position inCheck("4k3/4R3/8/8/8/8/8/4K3 b - - 0 1");
    int s = Eval(inCheck);
    Position notInCheck("4k3/8/8/8/8/8/4R3/4K3 b - - 0 1");
    int s2 = Eval(notInCheck);

    CHECK(s < s2);
}

TEST_CASE("Eval_BishopPair_IsBetterThanBishopAndKnight_AllElseEqualish")
{
    Position twoBishops("4k3/8/8/8/8/8/2B1B3/4K3 w - - 0 1");
    Position bishopKnight("4k3/8/8/8/8/8/2B1N3/4K3 w - - 0 1");

    int a = Eval(twoBishops);
    int b = Eval(bishopKnight);

    CHECK(a > b);
}

TEST_CASE("Eval_PassedPawn_IsValuedMoreThanBlockedPawn")
{
    Position passed("4k3/8/8/4P3/8/8/8/4K3 w - - 0 1");
    Position blocked("4k3/8/4p3/4P3/8/8/8/4K3 w - - 0 1");

    int sp = Eval(passed);
    int sb = Eval(blocked);

    CHECK(sp > sb);
}

TEST_CASE("Eval_DoubledPawns_ArePenalized")
{
    Position doubled("4k3/8/8/8/8/2P5/2P5/4K3 w - - 0 1");

    Position healthy("4k3/8/8/8/8/8/2PP4/4K3 w - - 0 1");

    int sd = Eval(doubled);
    int sh = Eval(healthy);

    CHECK(sh > sd);
}

TEST_CASE("Eval_IsolatedPawn_IsPenalized")
{
    Position isolated("4k3/8/8/8/8/8/P7/4K3 w - - 0 1");
    Position connected("4k3/8/8/8/8/8/PP6/4K3 w - - 0 1");

    int si = Eval(isolated);
    int sc = Eval(connected);

    CHECK(sc > si);
}

TEST_CASE("Eval_RookOn7th_IsRewarded")
{
    Position rook7("4k3/4R3/8/8/8/8/8/4K3 w - - 0 1");
    Position rook6("4k3/8/4R3/8/8/8/8/4K3 w - - 0 1");

    int s7 = Eval(rook7);
    int s6 = Eval(rook6);

    CHECK(s7 > s6);
}

TEST_CASE("Eval_RookOpenFile_Bonus")
{
    Position openFile("4k3/p7/8/8/8/8/8/4R1K1 w - - 0 1");
    Position semiOpen("4k3/4p3/8/8/8/8/8/4R1K1 w - - 0 1");

    int so = Eval(openFile);
    int ss = Eval(semiOpen);
    CHECK(so > ss);
}


TEST_CASE("Eval_KingOpenFilePenalty_WhenNoPawnsOnKingFile")
{
    Position openE("4k3/8/8/8/8/8/8/4K3 w - - 0 1");
    Position pawnE("4k3/8/8/8/8/8/4P3/4K3 w - - 0 1");

    int so = Eval(openE);
    int sp = Eval(pawnE);

    CHECK(sp > so);
}

TEST_CASE("Eval_Mobility_KnightCentralizationHelps")
{
    Position center("4k3/8/8/8/3N4/8/8/4K3 w - - 0 1");
    Position corner("4k3/8/8/8/8/8/N7/4K3 w - - 0 1");

    int sc = Eval(center);
    int sk = Eval(corner);

    CHECK(sc > sk);
}

TEST_CASE("Eval_Sanity_ScoreIsFiniteAndReasonableOnRandomSimplePosition")
{
    Position p("4k3/8/8/3p4/4P3/8/8/4K3 w - - 0 1");

    int s = Eval(p);

    CHECK(s > -5000);
    CHECK(s < 5000);
}
