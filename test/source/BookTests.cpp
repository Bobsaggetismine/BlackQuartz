#define DOCTEST_CONFIG_IMPLEMENT
#include "doctest.h"
#include "Book.h"
#include "surge.h"
int main(int argc, char** argv)
{
	zobrist::initialise_zobrist_keys();   
	initialise_all_databases();                

	doctest::Context ctx;
	ctx.applyCommandLine(argc, argv);
	return ctx.run();
}

TEST_CASE("TestNoFile")
{
	bq::Book book("folderdoesntexist/filedoesntexist.txt");
	CHECK(book.getSize() == 0);
}
TEST_CASE("TestAllLoaded")
{
	bq::Book book("res/books/mainbook.txt");
	CHECK(book.getSize() == 7748);
}
TEST_CASE("TestNoMoveFound")
{
	bq::Book book("folderdoesntexist/filedoesntexist.txt");
	Position p("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
	CHECK(book.getBookMove<WHITE>(p) == Move("a1a1"));
}
TEST_CASE("TestMoveFound")
{
	bq::Book book("res/books/mainbook.txt");
	Position p("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
	CHECK(book.getBookMove<WHITE>(p) != Move("a1a1"));
}
TEST_CASE("TestWrongColor")
{
	bq::Book book("res/books/mainbook.txt");
	Position p("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
	CHECK(book.getBookMove<BLACK>(p) == Move("a1a1"));
}
TEST_CASE("TestRightMove")
{
	bq::Book book("res/books/testbook.txt");
	Position p("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
	CHECK(book.getBookMove<WHITE>(p).str() == "e2e4");
}
TEST_CASE("TestAddMove")
{
	bq::Book book("res/books/testbook.txt");
	Position p("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
	Move m = book.getBookMove<WHITE>(p);
	CHECK(m.str() == "e2e4");
	book.addMove(m);
	p.play<WHITE>(m);
	m = book.getBookMove<WHITE>(p);
	CHECK(m.str() == "a1a1");
}

TEST_CASE("TestFollowsLine")
{
	bq::Book book("res/books/testbook.txt");
	Position p("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
	std::vector<std::string> moveSeq = {
		"e2e4",
		"e7e5",
		"g1f3",
		"b8c6",
		"f1c4",
		"f8c5",
		"c2c3",
		"g8f6",
	};
	for (int i = 0; i < 4; ++i)
	{
		Move m = book.getBookMove<WHITE>(p);
		book.addMove(m);
		p.play<WHITE>(m);
		CHECK(moveSeq[2 * i] == m.str());
		m = book.getBookMove<BLACK>(p);
		book.addMove(m);
		p.play<BLACK>(m);
		CHECK(moveSeq[2 * i + 1] == m.str());
	}
}
TEST_CASE("TestResetClearsHistory")
{
	bq::Book book("res/books/testbook.txt");
	Position p("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");

	Move m = book.getBookMove<WHITE>(p);
	CHECK(m.str() == "e2e4");

	book.addMove(m);
	p.play<WHITE>(m);

	book.Reset();
	Position p2("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
	Move m2 = book.getBookMove<WHITE>(p2);
	CHECK(m2.str() == "e2e4");
}

template<Color Us>
static bool is_legal(Position& p, Move m)
{
	MoveList<Us> moves(p);
	for (auto mv : moves)
		if (mv == m) return true;
	return false;
}

TEST_CASE("TestBookMoveIsLegal")
{
	bq::Book book("res/books/mainbook.txt");
	Position p("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");

	Move m = book.getBookMove<WHITE>(p);
	CHECK(m.is_null() == false);
	CHECK(is_legal<WHITE>(p, m));
}

TEST_CASE("TestHistoryMismatchReturnsNull")
{
	bq::Book book("res/books/testbook.txt");
	Position p("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");

	Move bogus("a2a3");
	book.addMove(bogus);
	p.play<WHITE>(bogus);

	Move m = book.getBookMove<BLACK>(p);
	CHECK(m.is_null());
}
TEST_CASE("TestReturnsOneOfCandidates")
{
	bq::Book book("res/books/testbook_twocandidates.txt");
	Position p("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");

	Move m = book.getBookMove<WHITE>(p);
	CHECK((m.str() == "e2e4" || m.str() == "d2d4"));
}
TEST_CASE("TestPositionHistoryOutOfSyncReturnsNull")
{
	bq::Book book("res/books/testbook.txt");
	Position p("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");

	Move m("e2e4");
	p.play<WHITE>(m);

	Move bm = book.getBookMove<BLACK>(p);
	CHECK(bm.is_null());
}
TEST_CASE("InvalidTokenSkipsEntireGame")
{
	bq::Book book("res/books/testbook_invalidtoken.txt");
	CHECK(book.getSize() == 2);
}
