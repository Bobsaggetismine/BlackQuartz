
#include "doctest.h"

#include <cstdint>
#include <type_traits>
#include <concepts>

// Change this include to your actual header file name/path:
#include "TranspositionTable.h" // contains namespace bq::TranspositionTable, tt_entry, tt_flag

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

} // namespace

TEST_SUITE("bq::TranspositionTable") {

    TEST_CASE("constructor/resizing produces power-of-two bucket count and sane capacity") {
        // Small table so we can reason about indices/collisions.
        bq::TranspositionTable tt(8); // desiredEntries=8 -> desiredBuckets=4 -> bucketCap=4
        CHECK(tt.bucketCount() >= 2);

        // bucketCount is always a power of two (due to nextPow2).
        const auto bc = tt.bucketCount();
        CHECK((bc & (bc - 1)) == 0);

        // 2-way buckets => approxEntryCapacity = buckets * 2
        CHECK(tt.approxEntryCapacity() == bc * 2);
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
        auto in = make_entry(/*depth*/5, /*score*/42, bq::tt_flag::EXACT);

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

        // Shallower should NOT overwrite
        tt.insert(h, make_entry(4, 200, bq::tt_flag::EXACT));
        {
            auto got = tt.lookup(h);
            CHECK(got.valid == true);
            CHECK(got.depth == 5);
            CHECK(got.score == 100);
        }

        // Equal depth SHOULD overwrite (>=)
        tt.insert(h, make_entry(5, 300, bq::tt_flag::LOWERBOUND));
        {
            auto got = tt.lookup(h);
            CHECK(got.valid == true);
            CHECK(got.depth == 5);
            CHECK(got.score == 300);
            CHECK(got.flag == bq::tt_flag::LOWERBOUND);
        }

        // Deeper SHOULD overwrite
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
        bq::TranspositionTable tt(8); // bucketCount=4 => mask low 2 bits

        // Force same bucket by matching low bits.
        // With mask=3, hashes that differ by 4 collide: x and x+4.
        const std::uint64_t h1 = 0x0ULL;
        const std::uint64_t h2 = 0x4ULL; // same bucket as h1

        CHECK(tt.lookup(h1).valid == false);
        CHECK(tt.lookup(h2).valid == false);

        tt.insert(h1, make_entry(3, 10, bq::tt_flag::EXACT));
        tt.insert(h2, make_entry(6, 20, bq::tt_flag::UPPERBOUND));

        auto e1 = tt.lookup(h1);
        auto e2 = tt.lookup(h2);

        CHECK(e1.valid == true);
        CHECK(e2.valid == true);
        CHECK(e1.score == 10);
        CHECK(e2.score == 20);
    }

    TEST_CASE("collision replacement: replaces shallower depth entry") {
        bq::TranspositionTable tt(8);

        // Same bucket:
        const std::uint64_t h1 = 0x1ULL;
        const std::uint64_t h2 = 0x5ULL; // +4 => same bucket
        const std::uint64_t h3 = 0x9ULL; // +8 => same bucket

        // Fill both slots
        tt.insert(h1, make_entry(5, 111, bq::tt_flag::EXACT));
        tt.insert(h2, make_entry(10, 222, bq::tt_flag::UPPERBOUND));

        // Incoming depth 7 should replace the shallower one (depth 5 => h1)
        tt.insert(h3, make_entry(7, 333, bq::tt_flag::EXACT));

        CHECK(tt.lookup(h2).valid == true);  // depth 10 survives
        CHECK(tt.lookup(h3).valid == true);  // inserted

        // The shallow (h1) should be gone
        CHECK(tt.lookup(h1).valid == false);
    }

    TEST_CASE("collision replacement: equal depth prefers replacing non-EXACT") {
        bq::TranspositionTable tt(8);

        // Same bucket:
        const std::uint64_t h1 = 0x2ULL;
        const std::uint64_t h2 = 0x6ULL; // +4 => same bucket
        const std::uint64_t h3 = 0xAULL; // +8 => same bucket

        // Fill with same depth, one EXACT and one non-EXACT
        tt.insert(h1, make_entry(10, 111, bq::tt_flag::EXACT));
        tt.insert(h2, make_entry(10, 222, bq::tt_flag::LOWERBOUND));

        // Incoming with same depth: should evict the non-EXACT one (h2)
        tt.insert(h3, make_entry(10, 333, bq::tt_flag::EXACT));

        CHECK(tt.lookup(h1).valid == true);   // exact should be kept
        CHECK(tt.lookup(h3).valid == true);   // inserted

        CHECK(tt.lookup(h2).valid == false);  // non-exact evicted
    }

    TEST_CASE("clear wipes all entries and resets top move") {
        bq::TranspositionTable tt(8);
        
        const std::uint64_t h = 0xDEADULL;
        tt.insert(h, make_entry(4, 99, bq::tt_flag::EXACT));

        // Set some top move (we don't assume anything about Move other than it exists)
        Move m{};
        Position p;
        tt.setTopMove(m);

        CHECK(tt.lookup(h).valid == true);

        tt.clear();

        CHECK(tt.lookup(h).valid == false);

        // If Move supports operator==, verify top move reset.
        if constexpr (EqualityComparable<Move>) {
            CHECK(tt.selectedMove() == Move{});
        }
    }

    TEST_CASE("BucketIndex matches mask-based index (sanity check)") {
        bq::TranspositionTable tt(8);
        // For power-of-two bucket count, BucketIndex uses size-1 masking.
        const std::uint64_t h = 0x12345678ULL;
        CHECK(tt.BucketIndex(h) < tt.BucketCount());
    }

} // TEST_SUITE
