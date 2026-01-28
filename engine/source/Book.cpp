#include "Book.h"

std::string bq::Book::loadGamesFromFile(const std::string& source)
{
    std::ifstream t(source);
    if (!t.is_open()) {
        return "";
    }
    std::stringstream buffer;
    buffer << t.rdbuf();
    std::string file_contents = buffer.str();
    
    return file_contents;
}

void bq::Book::parseGamesToGameEntries(std::vector<std::string>& games)
{
    for (std::string& game : games) {
        if (game.empty()) continue;
        bq::text::trim_inplace(game);
        Position p("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
        
        auto tokens = bq::text::split(game, " ");

        if (tokens.empty()) continue;

        // Parse game result from last token
        std::string last_token = tokens.back();
        auto result = parseLastToken(last_token);
        if (!result)
        {
            continue;
        }
        tokens.pop_back();

        auto moves = resolveTokensToMoves(p, tokens);

        if (!moves.empty()) {
            m_Games.push_back({ moves, result.value()});
        }
    }
}

void bq::Book::shuffleGames()
{
    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(m_Games.begin(), m_Games.end(), g);
}

std::optional<int> bq::Book::parseLastToken(const std::string& token)
{
    if (token == "1-0") return  1;
    if (token == "0-1") return -1;
    if (token == "1/2-1/2") return 0;
    return std::nullopt;
}

std::vector<std::string> bq::Book::resolveTokensToMoves(Position& p, std::vector<std::string> tokens)
{
    std::vector<std::string> updated_tokens;
    for (auto& token : tokens) {
        
        if (p.turn() == WHITE) {
            MoveList<WHITE> moves(p);
            for (Move move : moves) {
                if (get_notation(p, move) == token) {
                    updated_tokens.push_back(move.str());
                    p.play<WHITE>(move);
                    break;
                }
            }
        }
        else {
            MoveList<BLACK> moves(p);
            for (Move move : moves) {
                if (get_notation(p, move) == token) {
                    updated_tokens.push_back(move.str());
                    p.play<BLACK>(move);
                    break;
                }
            }
        }
    }
    return updated_tokens;
}

bq::Book::Book(const std::string& source)
{
    std::string fileContents = loadGamesFromFile(source);
    std::vector<std::string> games = bq::text::split(fileContents,"\n");
    parseGamesToGameEntries(games);
    shuffleGames();
}

void bq::Book::Reset()
{
    m_MoveHistory.clear();
    shuffleGames();
}

void bq::Book::addMove(Move move)
{
    m_MoveHistory.push_back(move);
}

size_t bq::Book::getSize()
{
    return m_Games.size();
}

