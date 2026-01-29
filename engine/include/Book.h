#pragma once

#include "surge.h"
#include "Text.h"
#include "Logger.h"
#include <string>
#include <fstream>
#include <print>
#include <random>
#include <optional>
namespace bq {
    
    struct GameEntry {
        std::vector<std::string> moves;
        int result; // 1 = white win, -1 = black win, 0 = draw
    };
    
    class Book {

        std::vector<Move> m_MoveHistory;
        std::vector<GameEntry> m_Games;
        std::string m_Delimeter = "\n";

        std::string loadGamesFromFile(const std::string& source);
        void parseGamesToGameEntries(std::vector<std::string>& games);
        void shuffleGames();
        std::optional<int> parseLastToken(const std::string& token);
        std::vector<std::string> resolveTokensToMoves(Position& p, const std::vector<std::string>& tokens);

    public:

        Book(const std::string& source);

        void Reset();
        void addMove(Move move);
        size_t getSize();

        template <Color Us>
        Move getBookMove(Position& p)
        {
            for (auto& game : m_Games) {

                // only use games where "our side" won
                if constexpr (Us == WHITE) {
                    if (game.result != 1) continue;
                }
                else {
                    if (game.result != -1) continue;
                }

                if (m_MoveHistory.size() >= game.moves.size()) continue;

                bool match = true;
                for (size_t i = 0; i < m_MoveHistory.size(); ++i) {
                    if (m_MoveHistory[i].str() != game.moves[i]) {
                        match = false;
                        break;
                    }
                }
                if (!match) continue;
             
                const auto& next = game.moves[m_MoveHistory.size()];
                MoveList<Us> moves(p);
                for (const auto& mv : moves) {
                    if (mv.str() == next) {
                        return mv;
                    }
                }
            }

            return Move(); // null
        }
    
        
    };
}
