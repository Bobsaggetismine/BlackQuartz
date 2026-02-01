#pragma once

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <thread>
#include <string>
#include <vector>
#include <cstdio>   
#include <cstdint>
#include <mutex>
#include <condition_variable>
#include "surge.h"
#include "Search.h"
#include "Logger.h"
#include "Book.h"
namespace bq {

    struct TimeControl {
        long long wtimeUs = 0;
        long long btimeUs = 0;
        long long wincUs = 0;
        long long bincUs = 0;
        int movestogo = 0;
    };

    class ChessAi {
        Color m_us;
        bq::Search m_search;
        bq::Book m_book;
        int m_maxDepth = 64;
        long long m_overheadUs = 5'000;
        long long m_minBudgetUs = 2'000;
        long long m_maxFrac = 3;

    public:
        explicit ChessAi(Color us, int maxSelDepth = 50)
            : m_us(us), m_search(maxSelDepth),m_book("res/books/mainbook.txt") {
        }

        void setColor(Color us) { m_us = us; }

        void setMaxDepth(int d) { m_maxDepth = std::max(1, d); }
        void setOverheadUs(long long us) { m_overheadUs = std::max(0LL, us); }
        void setMinBudgetUs(long long us) { m_minBudgetUs = std::max(0LL, us); }

        // Primary API
        inline Move think(Position& p, const TimeControl& tc) {

            Move bookMove;
            if (m_us == WHITE) {
                bookMove = m_book.getBookMove<WHITE>(p);
            }
            else{
                bookMove = m_book.getBookMove<BLACK>(p);
            }
            if (!bookMove.is_null()) {
                bq::SearchStats stats{};
                stats.depth = 0;
                stats.ellapsedTime = 0;
                stats.mateFound = false;
                stats.nodesSearched = 0;
                stats.qDepthReached = 0;
                stats.score = 0;
                stats.selectedMove = bookMove;
                logStats(stats);
                return bookMove;
            }
            const long long budgetUs = computeBudgetUs(tc);

            std::mutex mx;
            std::condition_variable cv;
            bool done = false;

            std::thread timer([&] {
                if (budgetUs <= 0) {
                    m_search.signalStop();
                    return;
                }
                std::unique_lock<std::mutex> lk(mx);
                if (!cv.wait_for(lk, std::chrono::microseconds(budgetUs), [&] { return done; })) {
                    m_search.signalStop();
                }
                });

            bq::SearchStats stats{};
            if (m_us == WHITE) stats = m_search.initiateIterativeSearch<WHITE>(p, m_maxDepth);
            else              stats = m_search.initiateIterativeSearch<BLACK>(p, m_maxDepth);

            {
                std::lock_guard<std::mutex> lk(mx);
                done = true;
            }
            cv.notify_one();
            timer.join();

            logStats(stats);
            return stats.selectedMove;
        }

        inline Move thinkFixedTime(Position& p, long long budgetUs) {
            TimeControl tc{};
            if (m_us == WHITE) tc.wtimeUs = budgetUs;
            else               tc.btimeUs = budgetUs;
            tc.movestogo = 1;
            setOverheadUs(0);
            return think(p, tc);
        }
        inline void resetBook() {
            m_book.Reset();
        }
        inline void stop() {
            m_search.signalStop();
        }
        inline void addBookMove(Move move) {
            m_book.addMove(move);
        }
    private:
        inline long long computeBudgetUs(const TimeControl& tc) const {
            long long timeUs = (m_us == WHITE) ? tc.wtimeUs : tc.btimeUs;
            long long incUs = (m_us == WHITE) ? tc.wincUs : tc.bincUs;

            timeUs = std::max(0LL, timeUs);
            incUs = std::max(0LL, incUs);

            long long budget = 0;

            if (tc.movestogo > 0) {
                const int mtg = std::max(1, tc.movestogo);
                budget = (timeUs / (mtg + 3)) + (incUs / 2);
            }
            else {
                budget = (timeUs / 30) + (incUs / 2);
            }

            if (m_maxFrac > 0) budget = std::min(budget, timeUs / m_maxFrac);
            budget = std::max(budget, m_minBudgetUs);

            budget = std::max(0LL, budget - m_overheadUs);

            return budget;
        }

        static constexpr const char* scoreUnit(bool mateFound) {
            return mateFound ? "mate" : "cp";
        }

        inline void logStats(const bq::SearchStats& s) const {
            auto commas = [](long long v) -> std::string {
                bool neg = v < 0;
                unsigned long long x = neg ? (unsigned long long)(-v) : (unsigned long long)v;
                std::string out;
                int group = 0;
                do {
                    if (group == 3) { out.push_back(','); group = 0; }
                    out.push_back(char('0' + (x % 10)));
                    x /= 10;
                    ++group;
                } while (x);
                if (neg) out.push_back('-');
                std::reverse(out.begin(), out.end());
                return out;
                };

            auto padR = [](std::string s, std::size_t w) {
                if (s.size() < w) s.append(w - s.size(), ' ');
                return s;
                };

            auto trunc = [](std::string s, std::size_t w) {
                if (s.size() <= w) return s;
                if (w <= 3) return s.substr(0, w);
                return s.substr(0, w - 3) + "...";
                };

            const long long us = s.ellapsedTime;
            const long long ms = us / 1000;

            const long long npsRaw = (us > 0) ? (s.nodesSearched * 1'000'000LL) / us : 0;
            const double mnps = double(npsRaw) / 1'000'000.0;

            char scoreBuf[32]{};
            std::snprintf(scoreBuf, sizeof(scoreBuf), "%+d", s.score);

            char mnpsBuf[32]{};
            std::snprintf(mnpsBuf, sizeof(mnpsBuf), "%.2f", mnps);

            const char* unit = s.mateFound ? "mate" : "cp";

            constexpr std::size_t W1 = 18;
            constexpr std::size_t W2 = 22;
            constexpr std::size_t W3 = 28;

            std::string f1a = "depth: " + std::to_string(s.depth);
            if (s.depth == 0) f1a = "depth: 0(book)";

            std::string f2a = std::string("score: ") + scoreBuf + " " + unit;
            std::string f3a = "time: " + std::to_string(ms) + "ms";

            std::string f1b = "nodes: " + commas(s.nodesSearched);
            std::string f2b = std::string("nps: ") + mnpsBuf + " Mnps";
            std::string f3b = std::string("best: ") + s.selectedMove.str() + (s.mateFound ? " [MATE]" : "");

            f1a = padR(trunc(f1a, W1), W1);
            f2a = padR(trunc(f2a, W2), W2);
            f3a = padR(trunc(f3a, W3), W3);

            f1b = padR(trunc(f1b, W1), W1);
            f2b = padR(trunc(f2b, W2), W2);
            f3b = padR(trunc(f3b, W3), W3);

            auto row = [&](const std::string& a, const std::string& b, const std::string& c) {
                return a + " | " + b + " | " + c;
                };

            std::string r1 = row(f1a, f2a, f3a);
            std::string r2 = row(f1b, f2b, f3b);

            const std::size_t innerW = r1.size();

            auto border = [&](char L, char M, char R) {
                std::string b;
                b += L;
                b.append(innerW + 2, M);
                b += R;
                return b;
                };

            auto line = [&](const std::string& inner) {
                return std::string("| ") + padR(inner, innerW) + " |";
                };

            std::vector<std::string> pvLines;
            if (s.pvLen <= 0) {
                pvLines.push_back("pv: (none)");
            }
            else {
                const std::string prefix = "pv: ";
                const std::string indent(prefix.size(), ' ');

                std::string cur = prefix;
                for (int i = 0; i < s.pvLen; ++i) {
                    std::string tok = s.pv[i].str();
                    if (tok.empty()) continue;

                    std::size_t extra = (cur.size() > prefix.size() ? 1 : 0) + tok.size();

                    if (cur.size() + extra > innerW) {
                        pvLines.push_back(cur);
                        cur = indent + tok;
                    }
                    else {
                        if (cur.size() > prefix.size()) cur.push_back(' ');
                        cur += tok;
                    }
                }
                if (!cur.empty()) pvLines.push_back(cur);
            }

            bq::Logger::Info("{}", border('+', '-', '+'));
            bq::Logger::Info("{}", line(r1));
            bq::Logger::Info("{}", line(r2));

            for (auto& pl : pvLines) {
                bq::Logger::Info("{}", line(trunc(pl, innerW)));
            }

            bq::Logger::Info("{}", border('+', '-', '+'));
        }


        template <Color Us>
        inline bool isLegalMoveFor(Position& p, Move m) const {
            MoveList<Us> ml(p);
            for (Move x : ml) if (x == m) return true;
            return false;
        }

        inline bool isLegalSelectedMove(Position& p, Move m) const {
            if (m_us == WHITE) return isLegalMoveFor<WHITE>(p, m);
            else              return isLegalMoveFor<BLACK>(p, m);
        }

        inline bool pickFirstLegal(Position& p, Move& out) const {
            if (m_us == WHITE) {
                MoveList<WHITE> ml(p);
                if (ml.size() == 0) return false;
                out = *ml.begin();
                return true;
            }
            else {
                MoveList<BLACK> ml(p);
                if (ml.size() == 0) return false;
                out = *ml.begin();
                return true;
            }
        }
        
    };

}
