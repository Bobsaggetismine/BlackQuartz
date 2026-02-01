#pragma once


#include <algorithm>
#include <atomic>
#include <chrono>
#include <cctype>
#include <cstdint>
#include <functional>
#include <iostream>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "surge.h"
#include "ChessAi.h"

namespace bq {

    class UciClient {
    public:

        explicit UciClient(Color defaultColor = WHITE)
            : m_defaultColor(defaultColor)
            , m_ai(defaultColor)
            , m_pos(kStartposFen)
        {
        }

        ~UciClient() {
            stopThinkingIfNeeded();
        }

        void run(std::istream& in = std::cin, std::ostream& out = std::cout) {
            m_in = &in;
            m_out = &out;

            std::string line;
            while (std::getline(*m_in, line)) {
                line = trim(line);
                if (line.empty()) continue;
                handleCommand(line);
                if (m_quit.load(std::memory_order_relaxed)) break;
            }

            stopThinkingIfNeeded();
        }

    private:
        static constexpr const char* kEngineName = "BlackQuartz";
        static constexpr const char* kEngineAuthor = "Brodie Quinlan";

        static constexpr const char* kStartposFen =
            "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

        std::istream* m_in = nullptr;
        std::ostream* m_out = nullptr;
        std::mutex m_outMx;

        void writeLine(const std::string& s) {
            std::lock_guard<std::mutex> lk(m_outMx);
            (*m_out) << s << "\n";
            m_out->flush();
        }

        Color   m_defaultColor = WHITE;
        ChessAi m_ai;
        Position m_pos;

        int m_hashMb = 16;
        int m_threads = 1;

        std::mutex m_thinkMx;
        std::thread m_thinkThread;
        std::atomic<bool> m_thinking{ false };
        std::atomic<bool> m_quit{ false };

        Color m_engineColorGuess = WHITE;

        static std::string trim(std::string s) {
            auto notSpace = [](unsigned char c) { return !std::isspace(c); };
            s.erase(s.begin(), std::find_if(s.begin(), s.end(), notSpace));
            s.erase(std::find_if(s.rbegin(), s.rend(), notSpace).base(), s.end());
            return s;
        }

        static std::vector<std::string> splitWS(const std::string& line) {
            std::istringstream iss(line);
            std::vector<std::string> out;
            std::string tok;
            while (iss >> tok) out.push_back(tok);
            return out;
        }

        static inline void applyMove(Position& p, Color stm, Move m) {
            if (stm == WHITE) p.play<WHITE>(m);
            else              p.play<BLACK>(m);
            
            
        }

        static inline bool hasSideToMove(const Position&) { return true; }

        static inline Color getSideToMove(Position& p) {
            return p.turn();
        }

        template <Color Us>
        static bool isLegal(Position& p, Move m) {
            MoveList<Us> moves(p);
            for (Move mv : moves) if (mv == m) return true;
            return false;
        }

        static std::optional<std::pair<Move, Color>> parseUciMoveToken(Position& p, const std::string& uciTok) {
            auto matchIn = [&](auto& ml) -> std::optional<Move> {
                for (Move m : ml) {
                    if (m.str() == uciTok) return m;
                }
                return std::nullopt;
                };

            if (hasSideToMove(p)) {
                Color stm = getSideToMove(p);
                if (stm == WHITE) {
                    MoveList<WHITE> ml(p);
                    if (auto mv = matchIn(ml)) return std::make_pair(*mv, WHITE);
                }
                else {
                    MoveList<BLACK> ml(p);
                    if (auto mv = matchIn(ml)) return std::make_pair(*mv, BLACK);
                }
                return std::nullopt;
            }
            else {
                {
                    MoveList<WHITE> ml(p);
                    if (auto mv = matchIn(ml)) return std::make_pair(*mv, WHITE);
                }
                {
                    MoveList<BLACK> ml(p);
                    if (auto mv = matchIn(ml)) return std::make_pair(*mv, BLACK);
                }
                return std::nullopt;
            }
        }

        void handleCommand(const std::string& line) {
            auto toks = splitWS(line);
            if (toks.empty()) return;

            const std::string& cmd = toks[0];

            if (cmd == "uci")             onUci();
            else if (cmd == "isready")    onIsReady();
            else if (cmd == "ucinewgame") onUciNewGame();
            else if (cmd == "position")   onPosition(toks);
            else if (cmd == "go")         onGo(toks);
            else if (cmd == "stop")       onStop();
            else if (cmd == "quit")       onQuit();
            else if (cmd == "setoption")  onSetOption(toks);
            else if (cmd == "ponderhit") {  }
            else {
            }
        }

        void onUci() {
            writeLine(std::string("id name ") + kEngineName);
            writeLine(std::string("id author ") + kEngineAuthor);
            writeLine("option name Hash type spin default 16 min 1 max 2048");
            writeLine("option name Threads type spin default 1 min 1 max 256");
            writeLine("option name Move Overhead type spin default 5 min 0 max 10000");
            writeLine("option name SyzygyPath type string default");
            writeLine("option name UCI_ShowWDL type check default false");
            writeLine("uciok");
        }

        void onIsReady() {
            writeLine("readyok");
        }

        void onUciNewGame() {
            stopThinkingIfNeeded();
            m_pos = Position(kStartposFen);
            m_ai.resetBook();
        }

        void onPosition(const std::vector<std::string>& toks) {
            stopThinkingIfNeeded();
            if (toks.size() < 2) return;

            std::size_t idx = 1;

            if (toks[idx] == "startpos") {
                m_pos = Position(kStartposFen);
                m_ai.resetBook();
                ++idx;
            }
            else if (toks[idx] == "fen") {
                ++idx;
                std::string fen;
                while (idx < toks.size() && toks[idx] != "moves") {
                    if (!fen.empty()) fen.push_back(' ');
                    fen += toks[idx];
                    ++idx;
                }
                if (fen.empty()) m_pos = Position(kStartposFen);
                else             m_pos = Position(fen);
            }
            else {
                m_pos = Position(kStartposFen);
            }

            if (idx < toks.size() && toks[idx] == "moves") {
                ++idx;
                for (; idx < toks.size(); ++idx) {
                    const std::string& mvTok = toks[idx];
                    auto parsed = parseUciMoveToken(m_pos, mvTok);
                    if (!parsed.has_value()) {
                        writeLine("info string illegal/unknown move in position: " + mvTok);
                        break;
                    }
                    applyMove(m_pos, parsed->second, parsed->first);
                    m_ai.addBookMove(parsed->first);
                }
            }
        }

        void onGo(const std::vector<std::string>& toks) {
            stopThinkingIfNeeded();

            TimeControl tc{};
            bool hasTime = false;
            bool infinite = false;
            int depthLimit = -1;
            long long moveTimeMs = -1;

            for (std::size_t i = 1; i < toks.size(); ++i) {
                const std::string& k = toks[i];

                auto readLL = [&](long long& out) {
                    if (i + 1 < toks.size()) out = std::stoll(toks[++i]);
                    };
                auto readI = [&](int& out) {
                    if (i + 1 < toks.size()) out = std::stoi(toks[++i]);
                    };

                if (k == "wtime") { long long ms = 0; readLL(ms); tc.wtimeUs = ms * 1000; hasTime = true; }
                else if (k == "btime") { long long ms = 0; readLL(ms); tc.btimeUs = ms * 1000; hasTime = true; }
                else if (k == "winc") { long long ms = 0; readLL(ms); tc.wincUs = ms * 1000; }
                else if (k == "binc") { long long ms = 0; readLL(ms); tc.bincUs = ms * 1000; }
                else if (k == "movestogo") { int v = 0; readI(v); tc.movestogo = v; }
                else if (k == "depth") { int v = 0; readI(v); depthLimit = v; }
                else if (k == "movetime") { long long v = 0; readLL(v); moveTimeMs = v; }
                else if (k == "infinite") { infinite = true; }
                else if (k == "ponder") {  }
            }

            Color stm = m_defaultColor;
            if (hasSideToMove(m_pos)) stm = getSideToMove(m_pos);
            m_ai.setColor(stm);
            m_engineColorGuess = stm;

            if (depthLimit > 0) m_ai.setMaxDepth(depthLimit);
            else                m_ai.setMaxDepth(64);

            if (moveTimeMs > 0) {
                if (stm == WHITE) tc.wtimeUs = moveTimeMs * 1000;
                else              tc.btimeUs = moveTimeMs * 1000;
                tc.movestogo = 1;
                hasTime = true;
                m_ai.setOverheadUs(0);
            }
            else if (!hasTime && !infinite) {
                if (stm == WHITE) tc.wtimeUs = 100 * 1000;
                else              tc.btimeUs = 100 * 1000;
                tc.movestogo = 1;
                hasTime = true;
            }

            if (infinite) {
                if (stm == WHITE) tc.wtimeUs = 24LL * 60 * 60 * 1'000'000;
                else              tc.btimeUs = 24LL * 60 * 60 * 1'000'000;
                tc.movestogo = 1;
            }

            startThinking(tc);
        }

        void onStop() {
            stopThinkingIfNeeded();
        }

        void onQuit() {
            m_quit.store(true, std::memory_order_relaxed);
        }

        void onSetOption(const std::vector<std::string>& toks) {
            std::string name, value;

            enum class Mode { None, Name, Value };
            Mode mode = Mode::None;

            for (std::size_t i = 1; i < toks.size(); ++i) {
                if (toks[i] == "name") { mode = Mode::Name;  continue; }
                if (toks[i] == "value") { mode = Mode::Value; continue; }

                if (mode == Mode::Name) {
                    if (!name.empty()) name.push_back(' ');
                    name += toks[i];
                }
                else if (mode == Mode::Value) {
                    if (!value.empty()) value.push_back(' ');
                    value += toks[i];
                }

            }

            name = trim(name);
            value = trim(value);
            if (name == "Hash" && !value.empty()) {
                m_hashMb = std::clamp(std::stoi(value), 1, 2048);
                //todo
            }
            else if (name == "Threads" && !value.empty()) {
                //todo
            }
            else if (name == "Move Overhead" && !value.empty()) {
                int ms = std::max(0, std::stoi(value));
                m_ai.setOverheadUs((long long)ms * 1000);
            }
            else if (name == "SyzygyPath" && !value.empty()) {
                //todo
            }
            else if (name == "UCI_ShowWDL") {
                //todo
            }
        }

        void startThinking(const TimeControl& tc) {
            m_thinking.store(true, std::memory_order_relaxed);

            std::lock_guard<std::mutex> lk(m_thinkMx);
            m_thinkThread = std::thread([this, tc]() {
                writeLine("info string thinking");

                Move best = m_ai.think(m_pos, tc);

                if (best.is_null()) {
                    Move fallback{};
                    bool ok = false;
                    if (m_engineColorGuess == WHITE) {
                        MoveList<WHITE> ml(m_pos);
                        if (ml.size() > 0) { fallback = *ml.begin(); ok = true; }
                    }
                    else {
                        MoveList<BLACK> ml(m_pos);
                        if (ml.size() > 0) { fallback = *ml.begin(); ok = true; }
                    }
                    if (ok) best = fallback;
                }

                if (!best.is_null()) writeLine(std::string("bestmove ") + best.str());
                else                 writeLine("bestmove 0000");

                m_thinking.store(false, std::memory_order_relaxed);
                });
        }

        void stopThinkingIfNeeded() {
            if (!m_thinking.load(std::memory_order_relaxed)) {
                std::lock_guard<std::mutex> lk(m_thinkMx);
                if (m_thinkThread.joinable()) m_thinkThread.join();
                return;
            }

            m_ai.stop();

            std::lock_guard<std::mutex> lk(m_thinkMx);
            if (m_thinkThread.joinable()) m_thinkThread.join();

            m_thinking.store(false, std::memory_order_relaxed);
        }
    };
     
}