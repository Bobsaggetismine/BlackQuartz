#pragma once

#include "surge.h"
#include "tables.h"

namespace bq {

    class Evaluation {
    public:
        template <Color Us>
        static int ScoreBoard(Position& pos) {
            constexpr Color Them = ~Us;

            const Bitboard occ = pos.all_pieces<WHITE>() | pos.all_pieces<BLACK>();
            const Bitboard usBB = pos.all_pieces<Us>();
            const Bitboard thBB = pos.all_pieces<Them>();

            const int phase = ComputePhase(pos);

            int mg = 0;
            int eg = 0;

            AddMaterial<Us>(pos, mg, eg);
            AddPieceSquareTables<Us>(pos, mg, eg);
            AddMobility<Us>(pos, occ, usBB, thBB, mg, eg);
            AddPawnStructure<Us>(pos, mg, eg);
            AddBishopPair<Us>(pos, mg, eg);
            AddRookTerms<Us>(pos, mg, eg);
            AddKingSafety<Us>(pos, occ, mg);
            AddCheckStatus<Us>(pos, mg);
            mg += (pos.turn() == Us) ? 10 : -10;
            return Blend(mg, eg, phase);
        }

    private:
        static constexpr int MirrorSq(int sq) { return sq ^ 56; } // rank flip

        static int Pst(const int* table, Color c, int sq) {
            return (c == WHITE) ? table[sq] : table[MirrorSq(sq)];
        }

        static constexpr int FileOfSq(int sq) { return sq & 7; }
        static constexpr int RankOfSq(int sq) { return sq >> 3; }

        static Bitboard AttackersFromColor(const Position& pos, Color attacker, Square s, Bitboard occ) {
            return (attacker == WHITE) ? pos.attackers_from<WHITE>(s, occ)
                : pos.attackers_from<BLACK>(s, occ);
        }

        static int Blend(int mg, int eg, int phase) {
            return (mg * phase + eg * (24 - phase)) / 24;
        }

        static int ComputePhase(const Position& pos) {
            static constexpr int PHASE_W[6] = { 0, 1, 1, 2, 4, 0 }; // P,N,B,R,Q,K
            int phase = 0;

            for (PieceType pt = PAWN; pt < KING; ++pt) {
                phase += pop_count(pos.bitboard_of(WHITE, pt)) * PHASE_W[pt];
                phase += pop_count(pos.bitboard_of(BLACK, pt)) * PHASE_W[pt];
            }

            if (phase > 24) phase = 24;
            return phase;
        }

        template <Color Us>
        static void AddMaterial(const Position& pos, int& mg, int& eg) {
            constexpr Color Them = ~Us;

            static constexpr int MG_VAL[6] = { 100, 320, 330, 500, 900, 0 };
            static constexpr int EG_VAL[6] = { 120, 300, 320, 520, 900, 0 };

            for (PieceType pt = PAWN; pt < KING; ++pt) {
                mg += pop_count(pos.bitboard_of(Us, pt)) * MG_VAL[pt];
                mg -= pop_count(pos.bitboard_of(Them, pt)) * MG_VAL[pt];

                eg += pop_count(pos.bitboard_of(Us, pt)) * EG_VAL[pt];
                eg -= pop_count(pos.bitboard_of(Them, pt)) * EG_VAL[pt];
            }
        }

        template <Color Us>
        static void AddPieceSquareTables(const Position& pos, int& mg, int& eg) {
            constexpr Color Them = ~Us;

            static constexpr int MG_PAWN[64] = {
                 0,  0,  0,  0,  0,  0,  0,  0,
                10, 10, 10,-10,-10, 10, 10, 10,
                 5,  5, 10, 20, 20, 10,  5,  5,
                 0,  0,  0, 25, 25,  0,  0,  0,
                 5, -5,-10, 10, 10,-10, -5,  5,
                 5, 10, 10,-20,-20, 10, 10,  5,
                10, 10, 10,-10,-10, 10, 10, 10,
                 0,  0,  0,  0,  0,  0,  0,  0
            };
            static constexpr int EG_PAWN[64] = {
                 0,  0,  0,  0,  0,  0,  0,  0,
                20, 20, 20, 20, 20, 20, 20, 20,
                15, 15, 15, 15, 15, 15, 15, 15,
                10, 10, 10, 12, 12, 10, 10, 10,
                 6,  6,  6,  8,  8,  6,  6,  6,
                 3,  3,  3,  4,  4,  3,  3,  3,
                 1,  1,  1,  0,  0,  1,  1,  1,
                 0,  0,  0,  0,  0,  0,  0,  0
            };

            static constexpr int MG_KNIGHT[64] = {
               -50,-40,-30,-30,-30,-30,-40,-50,
               -40,-20,  0,  0,  0,  0,-20,-40,
               -30,  0, 10, 15, 15, 10,  0,-30,
               -30,  5, 15, 20, 20, 15,  5,-30,
               -30,  0, 15, 20, 20, 15,  0,-30,
               -30,  5, 10, 15, 15, 10,  5,-30,
               -40,-20,  0,  5,  5,  0,-20,-40,
               -50,-40,-30,-30,-30,-30,-40,-50
            };
            static constexpr int EG_KNIGHT[64] = {
               -40,-30,-20,-20,-20,-20,-30,-40,
               -30,-10,  0,  0,  0,  0,-10,-30,
               -20,  0, 10, 10, 10, 10,  0,-20,
               -20,  0, 10, 15, 15, 10,  0,-20,
               -20,  0, 10, 15, 15, 10,  0,-20,
               -20,  0, 10, 10, 10, 10,  0,-20,
               -30,-10,  0,  0,  0,  0,-10,-30,
               -40,-30,-20,-20,-20,-20,-30,-40
            };

            static constexpr int MG_BISHOP[64] = {
               -20,-10,-10,-10,-10,-10,-10,-20,
               -10,  0,  0,  0,  0,  0,  0,-10,
               -10,  0,  5, 10, 10,  5,  0,-10,
               -10,  5,  5, 10, 10,  5,  5,-10,
               -10,  0, 10, 10, 10, 10,  0,-10,
               -10, 10, 10, 10, 10, 10, 10,-10,
               -10,  5,  0,  0,  0,  0,  5,-10,
               -20,-10,-10,-10,-10,-10,-10,-20
            };
            static constexpr int EG_BISHOP[64] = {
               -20,-10,-10,-10,-10,-10,-10,-20,
               -10,  0,  0,  0,  0,  0,  0,-10,
               -10,  0, 10, 10, 10, 10,  0,-10,
               -10,  0, 10, 15, 15, 10,  0,-10,
               -10,  0, 10, 15, 15, 10,  0,-10,
               -10,  0, 10, 10, 10, 10,  0,-10,
               -10,  0,  0,  0,  0,  0,  0,-10,
               -20,-10,-10,-10,-10,-10,-10,-20
            };

            static constexpr int MG_ROOK[64] = {
                 0,  0,  0,  5,  5,  0,  0,  0,
                -5,  0,  0,  0,  0,  0,  0, -5,
                -5,  0,  0,  0,  0,  0,  0, -5,
                -5,  0,  0,  0,  0,  0,  0, -5,
                -5,  0,  0,  0,  0,  0,  0, -5,
                -5,  0,  0,  0,  0,  0,  0, -5,
                 5, 10, 10, 10, 10, 10, 10,  5,
                 0,  0,  0,  0,  0,  0,  0,  0
            };
            static constexpr int EG_ROOK[64] = {
                 0,  0,  0,  5,  5,  0,  0,  0,
                 0,  0,  0,  0,  0,  0,  0,  0,
                 0,  0,  0,  0,  0,  0,  0,  0,
                 0,  0,  0,  0,  0,  0,  0,  0,
                 0,  0,  0,  0,  0,  0,  0,  0,
                 0,  0,  0,  0,  0,  0,  0,  0,
                 5,  5,  5,  5,  5,  5,  5,  5,
                 0,  0,  0,  0,  0,  0,  0,  0
            };

            static constexpr int MG_QUEEN[64] = {
               -20,-10,-10, -5, -5,-10,-10,-20,
               -10,  0,  0,  0,  0,  0,  0,-10,
               -10,  0,  5,  5,  5,  5,  0,-10,
                -5,  0,  5,  5,  5,  5,  0, -5,
                 0,  0,  5,  5,  5,  5,  0, -5,
               -10,  5,  5,  5,  5,  5,  0,-10,
               -10,  0,  5,  0,  0,  0,  0,-10,
               -20,-10,-10, -5, -5,-10,-10,-20
            };
            static constexpr int EG_QUEEN[64] = {
               -20,-10,-10, -5, -5,-10,-10,-20,
               -10,  0,  0,  0,  0,  0,  0,-10,
               -10,  0,  5,  5,  5,  5,  0,-10,
                -5,  0,  5,  5,  5,  5,  0, -5,
                 0,  0,  5,  5,  5,  5,  0, -5,
               -10,  0,  5,  5,  5,  5,  0,-10,
               -10,  0,  0,  0,  0,  0,  0,-10,
               -20,-10,-10, -5, -5,-10,-10,-20
            };

            static constexpr int MG_KING[64] = {
                20, 30, 10,  0,  0, 10, 30, 20,
                20, 20,  0,  0,  0,  0, 20, 20,
               -10,-20,-20,-20,-20,-20,-20,-10,
               -20,-30,-30,-40,-40,-30,-30,-20,
               -30,-40,-40,-50,-50,-40,-40,-30,
               -30,-40,-40,-50,-50,-40,-40,-30,
               -30,-40,-40,-50,-50,-40,-40,-30,
               -30,-40,-40,-50,-50,-40,-40,-30
            };
            static constexpr int EG_KING[64] = {
               -50,-30,-30,-30,-30,-30,-30,-50,
               -30,-10,  0,  0,  0,  0,-10,-30,
               -30,  0, 10, 15, 15, 10,  0,-30,
               -30,  0, 15, 25, 25, 15,  0,-30,
               -30,  0, 15, 25, 25, 15,  0,-30,
               -30,  0, 10, 15, 15, 10,  0,-30,
               -30,-10,  0,  0,  0,  0,-10,-30,
               -50,-30,-30,-30,-30,-30,-30,-50
            };

            auto AddPst = [&](Color c, PieceType pt, const int* mgT, const int* egT, int sign) {
                Bitboard b = pos.bitboard_of(c, pt);
                while (b) {
                    int sq = int(pop_lsb(&b));
                    mg += sign * Pst(mgT, c, sq);
                    eg += sign * Pst(egT, c, sq);
                }
                };

            AddPst(Us, PAWN, MG_PAWN, EG_PAWN, +1);
            AddPst(Them, PAWN, MG_PAWN, EG_PAWN, -1);
            AddPst(Us, KNIGHT, MG_KNIGHT, EG_KNIGHT, +1);
            AddPst(Them, KNIGHT, MG_KNIGHT, EG_KNIGHT, -1);
            AddPst(Us, BISHOP, MG_BISHOP, EG_BISHOP, +1);
            AddPst(Them, BISHOP, MG_BISHOP, EG_BISHOP, -1);
            AddPst(Us, ROOK, MG_ROOK, EG_ROOK, +1);
            AddPst(Them, ROOK, MG_ROOK, EG_ROOK, -1);
            AddPst(Us, QUEEN, MG_QUEEN, EG_QUEEN, +1);
            AddPst(Them, QUEEN, MG_QUEEN, EG_QUEEN, -1);

            {
                int usK = int(bsf(pos.bitboard_of(Us, KING)));
                int thK = int(bsf(pos.bitboard_of(Them, KING)));
                mg += Pst(MG_KING, Us, usK);
                eg += Pst(EG_KING, Us, usK);
                mg -= Pst(MG_KING, Them, thK);
                eg -= Pst(EG_KING, Them, thK);
            }
        }

        template <Color Us>
        static void AddMobility(const Position& pos, Bitboard occ, Bitboard usBB, Bitboard thBB, int& mg, int& eg) {
            constexpr Color Them = ~Us;

            auto AddMobForPiece = [&](PieceType pt, int mgW, int egW) {
                auto EvalSide = [&](Color c, int sign) {
                    Bitboard own = (c == Us) ? usBB : thBB;
                    Bitboard b = pos.bitboard_of(c, pt);
                    while (b) {
                        Square s = pop_lsb(&b);
                        Bitboard moves = attacks(pt, s, occ) & ~own;
                        int m = pop_count(moves);
                        mg += sign * m * mgW;
                        eg += sign * m * egW;
                    }
                    };
                EvalSide(Us, +1);
                EvalSide(Them, -1);
                };

            AddMobForPiece(KNIGHT, 4, 4);
            AddMobForPiece(BISHOP, 4, 4);
            AddMobForPiece(ROOK, 2, 3);
            AddMobForPiece(QUEEN, 1, 2);
        }

        template <Color Us>
        static void AddPawnStructure(const Position& pos, int& mg, int& eg) {
            constexpr Color Them = ~Us;

            const Bitboard usP = pos.bitboard_of(Us, PAWN);
            const Bitboard thP = pos.bitboard_of(Them, PAWN);

            auto PawnFileCounts = [&](Bitboard pawns, int out[8]) {
                for (int i = 0; i < 8; ++i) out[i] = 0;
                Bitboard t = pawns;
                while (t) {
                    int sq = int(pop_lsb(&t));
                    out[FileOfSq(sq)]++;
                }
                };

            int usFileCnt[8], thFileCnt[8];
            PawnFileCounts(usP, usFileCnt);
            PawnFileCounts(thP, thFileCnt);

            // doubled
            for (int f = 0; f < 8; ++f) {
                if (usFileCnt[f] > 1) { mg -= 12 * (usFileCnt[f] - 1); eg -= 10 * (usFileCnt[f] - 1); }
                if (thFileCnt[f] > 1) { mg += 12 * (thFileCnt[f] - 1); eg += 10 * (thFileCnt[f] - 1); }
            }

            auto IsIsolated = [&](int f, const int cnt[8]) -> bool {
                bool left = (f > 0) && cnt[f - 1] > 0;
                bool right = (f < 7) && cnt[f + 1] > 0;
                return !left && !right;
                };

            auto IsConnected = [&](Bitboard pawns, int sq) -> bool {
                int f = FileOfSq(sq);
                int r = RankOfSq(sq);
                if (f > 0) {
                    int idx = r * 8 + (f - 1);
                    if (pawns & (Bitboard(1) << idx)) return true;
                }
                if (f < 7) {
                    int idx = r * 8 + (f + 1);
                    if (pawns & (Bitboard(1) << idx)) return true;
                }
                return false;
                };

            auto IsPassed = [&](Color c, int sq) -> bool {
                int f = FileOfSq(sq);
                int r = RankOfSq(sq);
                Bitboard enemyP = pos.bitboard_of(~c, PAWN);

                for (int df = -1; df <= 1; ++df) {
                    int ff = f + df;
                    if (ff < 0 || ff > 7) continue;

                    if (c == WHITE) {
                        for (int rr = r + 1; rr <= 7; ++rr) {
                            int idx = rr * 8 + ff;
                            if (enemyP & (Bitboard(1) << idx)) return false;
                        }
                    }
                    else {
                        for (int rr = r - 1; rr >= 0; --rr) {
                            int idx = rr * 8 + ff;
                            if (enemyP & (Bitboard(1) << idx)) return false;
                        }
                    }
                }
                return true;
                };

            auto EvalPawnSide = [&](Color c, Bitboard pawns, const int fileCnt[8], int sign) {
                Bitboard b = pawns;
                while (b) {
                    int sq = int(pop_lsb(&b));
                    int f = FileOfSq(sq);
                    int r = RankOfSq(sq);

                    if (IsIsolated(f, fileCnt)) { mg += sign * -10; eg += sign * -8; }
                    if (IsConnected(pawns, sq)) { mg += sign * 4;  eg += sign * 6; }

                    if (IsPassed(c, sq)) {
                        int adv = (c == WHITE) ? r : (7 - r);
                        mg += sign * (8 + adv * 2);
                        eg += sign * (18 + adv * 6);
                    }
                }
                };

            EvalPawnSide(Us, usP, usFileCnt, +1);
            EvalPawnSide(Them, thP, thFileCnt, -1);
        }

        template <Color Us>
        static void AddBishopPair(const Position& pos, int& mg, int& eg) {
            constexpr Color Them = ~Us;

            if (pop_count(pos.bitboard_of(Us, BISHOP)) >= 2) { mg += 25; eg += 35; }
            if (pop_count(pos.bitboard_of(Them, BISHOP)) >= 2) { mg -= 25; eg -= 35; }
        }

        template <Color Us>
        static void AddRookTerms(const Position& pos, int& mg, int& eg) {
            constexpr Color Them = ~Us;

            auto RookTermsFor = [&](Color c) -> int {
                Bitboard rooks = pos.bitboard_of(c, ROOK);
                Bitboard ourP = pos.bitboard_of(c, PAWN);
                Bitboard thP = pos.bitboard_of(~c, PAWN);

                int s = 0;
                while (rooks) {
                    int sq = int(pop_lsb(&rooks));
                    int f = FileOfSq(sq);
                    int r = RankOfSq(sq);

                    Bitboard fileMask = MASK_FILE[f];
                    bool ourPawnOnFile = (ourP & fileMask) != 0;
                    bool theirPawnOnFile = (thP & fileMask) != 0;

                    if (!ourPawnOnFile && !theirPawnOnFile) s += 18;
                    else if (!ourPawnOnFile && theirPawnOnFile) s += 10;

                    if ((c == WHITE && r == 6) || (c == BLACK && r == 1)) s += 15;
                }
                return s;
                };

            int rt = RookTermsFor(Us) - RookTermsFor(Them);
            mg += rt;
            eg += rt / 2;
        }

        template <Color Us>
        static void AddKingSafety(const Position& pos, Bitboard occ, int& mg) {
            constexpr Color Them = ~Us;

            auto KingSafetyMg = [&](Color c) -> int {
                Square ksq = bsf(pos.bitboard_of(c, KING));
                int kf = int(file_of(ksq));
                int kr = int(rank_of(ksq));

                Bitboard ourPawns = pos.bitboard_of(c, PAWN);
                Bitboard allPawns = pos.bitboard_of(WHITE, PAWN) | pos.bitboard_of(BLACK, PAWN);

                int dir = (c == WHITE) ? 1 : -1;

                auto OnBoard = [](int rr, int ff) -> bool {
                    return rr >= 0 && rr <= 7 && ff >= 0 && ff <= 7;
                    };

                int shield = 0;
                for (int df = -1; df <= 1; ++df) {
                    int ff = kf + df;
                    int rr1 = kr + dir;
                    int rr2 = kr + 2 * dir;

                    if (OnBoard(rr1, ff)) shield += (ourPawns >> (rr1 * 8 + ff)) & 1ULL;
                    if (OnBoard(rr2, ff)) shield += (ourPawns >> (rr2 * 8 + ff)) & 1ULL;
                }

                int score = 0;
                score -= (6 - shield) * 10;

                if ((allPawns & MASK_FILE[kf]) == 0) score -= 14;

                Bitboard zone = attacks<KING>(ksq, occ) | SQUARE_BB[ksq];

                int pressure = 0;
                Bitboard z = zone;
                while (z) {
                    Square s = pop_lsb(&z);
                    Bitboard at = AttackersFromColor(pos, Color(~c), s, occ);
                    pressure += sparse_pop_count(at);
                }

                score -= pressure * 2;
                return score;
                };

            int ksUs = KingSafetyMg(Us);
            int ksThem = KingSafetyMg(Them);
            mg += (ksUs - ksThem);
        }

        template <Color Us>
        static void AddCheckStatus(const Position& pos, int& mg) {
            constexpr Color Them = ~Us;

            if (pos.in_check<Us>())   mg -= 20;
            if (pos.in_check<Them>()) mg += 20;
        }
    };

}
