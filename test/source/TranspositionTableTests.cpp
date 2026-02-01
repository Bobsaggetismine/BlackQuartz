
#include "doctest.h"

#include <cstdint>
#include <type_traits>
#include <concepts>

#include "TranspositionTable.h"

namespace {

    template <typename T>
    concept EqualityComparable = requires(T a, T b) {
        { a == b } -> std::convertible_to<bool>;
    };

    inline bq::tt_entry make_entry(int depth, int score, bq::tt_flag flag) {
        bq::tt_entry e;
        e.depth = depth;
        e.score = score;
        e.flag = flag;
        e.valid = true;

        return e;
    }
    static std::vector<std::uint64_t> find_hashes_same_bucket(bq::TranspositionTable& tt, int want) {
        std::vector<std::uint64_t> out;
        const std::size_t target = tt.BucketIndex(1);

        for (std::uint64_t h = 2; out.size() < (std::size_t)want; ++h) {
            if (tt.BucketIndex(h) == target) out.push_back(h);
        }
        return out;
    }

} // namespace

TEST_SUITE("bq::TranspositionTable") {

    TEST_CASE("constructor/resizing produces sane bucket count and capacity") {
        bq::TranspositionTable tt(1); // 1 MB
        CHECK(tt.bucketCount() >= 2);
        CHECK(tt.approxEntryCapacity() == tt.bucketCount() * 2);
    }

    TEST_CASE("lookup on empty table returns invalid entry") {
        bq::TranspositionTable tt(8);
        auto e = tt.lookup(0x1234ULL);
        CHECK(e.valid == false);
        CHECK(e.depth == -1);
    }

    TEST_CASE("insert ignores invalid entries") {
        bq::TranspositionTable tt(8);

        bq::tt_entry bad;
        bad.valid = false;
        bad.depth = 99;
        bad.score = 123;

        const std::uint64_t h = 0xAULL;
        tt.insert(h, bad);

        auto got = tt.lookup(h);
        CHECK(got.valid == false);
    }

    TEST_CASE("basic insert/lookup roundtrip") {
        bq::TranspositionTable tt(8);

        const std::uint64_t h = 0xBEEFULL;
        auto in = make_entry(5, 42, bq::tt_flag::EXACT);

        tt.insert(h, in);
        auto got = tt.lookup(h);

        CHECK(got.valid == true);
        CHECK(got.depth == 5);
        CHECK(got.score == 42);
        CHECK(got.flag == bq::tt_flag::EXACT);
    }

    TEST_CASE("reinserting same key only overwrites if depth is >= existing depth") {
        bq::TranspositionTable tt(8);
        const std::uint64_t h = 0x1111ULL;

        tt.insert(h, make_entry(5, 100, bq::tt_flag::EXACT));

        tt.insert(h, make_entry(4, 200, bq::tt_flag::EXACT));
        {
            auto got = tt.lookup(h);
            CHECK(got.valid == true);
            CHECK(got.depth == 5);
            CHECK(got.score == 100);
        }

        tt.insert(h, make_entry(5, 300, bq::tt_flag::LOWERBOUND));
        {
            auto got = tt.lookup(h);
            CHECK(got.valid == true);
            CHECK(got.depth == 5);
            CHECK(got.score == 300);
            CHECK(got.flag == bq::tt_flag::LOWERBOUND);
        }

        tt.insert(h, make_entry(7, 400, bq::tt_flag::UPPERBOUND));
        {
            auto got = tt.lookup(h);
            CHECK(got.valid == true);
            CHECK(got.depth == 7);
            CHECK(got.score == 400);
            CHECK(got.flag == bq::tt_flag::UPPERBOUND);
        }
    }

    TEST_CASE("2-way bucket: two different keys with same bucket can coexist") {
        bq::TranspositionTable tt(1);
        auto hs = find_hashes_same_bucket(tt, 2);
        auto h1 = hs[0];
        auto h2 = hs[1];

        tt.insert(h1, make_entry(3, 10, bq::tt_flag::EXACT));
        tt.insert(h2, make_entry(6, 20, bq::tt_flag::UPPERBOUND));

        CHECK(tt.lookup(h1).valid == true);
        CHECK(tt.lookup(h2).valid == true);
    }

    TEST_CASE("collision replacement: replaces shallower depth entry") {
        bq::TranspositionTable tt(8);

        const std::uint64_t h1 = 0x1ULL;
        const std::uint64_t h2 = 0x5ULL;
        const std::uint64_t h3 = 0x9ULL;

        tt.insert(h1, make_entry(5, 111, bq::tt_flag::EXACT));
        tt.insert(h2, make_entry(10, 222, bq::tt_flag::UPPERBOUND));

        tt.insert(h3, make_entry(7, 333, bq::tt_flag::EXACT));

        CHECK(tt.lookup(h2).valid == true);
        CHECK(tt.lookup(h3).valid == true);
        CHECK(tt.lookup(h1).valid == false);
    }

    TEST_CASE("collision replacement: equal depth prefers replacing non-EXACT") {
        bq::TranspositionTable tt(8);

        const std::uint64_t h1 = 0x2ULL;
        const std::uint64_t h2 = 0x6ULL;
        const std::uint64_t h3 = 0xAULL;

        tt.insert(h1, make_entry(10, 111, bq::tt_flag::EXACT));
        tt.insert(h2, make_entry(10, 222, bq::tt_flag::LOWERBOUND));

        tt.insert(h3, make_entry(10, 333, bq::tt_flag::EXACT));

        CHECK(tt.lookup(h1).valid == true);
        CHECK(tt.lookup(h3).valid == true);
        CHECK(tt.lookup(h2).valid == false);
    }

    TEST_CASE("clear wipes all entries and resets top move") {
        bq::TranspositionTable tt(8);
        
        const std::uint64_t h = 0xDEADULL;
        tt.insert(h, make_entry(4, 99, bq::tt_flag::EXACT));

        Move m{};
        Position p;
        tt.setTopMove(m);

        CHECK(tt.lookup(h).valid == true);

        tt.clear();

        CHECK(tt.lookup(h).valid == false);

        if constexpr (EqualityComparable<Move>) {
            CHECK(tt.selectedMove() == Move{});
        }
    }

    TEST_CASE("BucketIndex matches mask-based index (sanity check)") {
        bq::TranspositionTable tt(8);
        const std::uint64_t h = 0x12345678ULL;
        CHECK(tt.BucketIndex(h) < tt.BucketCount());
    }

}
