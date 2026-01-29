#pragma once

#include <cstdint>
#include <vector>

#include "surge.h" // Move
namespace bq
{
    enum class tt_flag : std::uint8_t {
        EXACT,
        UPPERBOUND,
        LOWERBOUND
    };

    struct tt_entry {
        int depth = -1;
        int score = 0;
        tt_flag flag = tt_flag::EXACT;
        bool valid = false;
        Move bestMove{}; // NEW
    };

    class TranspositionTable {
        struct Slot {
            std::uint64_t key = 0;
            tt_entry entry{};
        };

        struct Bucket {
            Slot a{};
            Slot b{};
        };

        Move m_topMove{};
        std::vector<Bucket> m_buckets{};
        std::size_t m_mask = 0;

    public:
        static constexpr std::size_t MAX_TT_SIZE = 44'800'000; // approximate number of entries desired

        TranspositionTable() { resize(MAX_TT_SIZE); }
        explicit TranspositionTable(std::size_t entries) { resize(entries); }

        size_t BucketCount() const { return m_buckets.size(); }
        size_t BucketIndex(uint64_t hash) const { return size_t(hash) & (m_buckets.size() - 1); }

        void clear()
        {
            for (auto& bk : m_buckets) {
                bk.a = Slot{};
                bk.b = Slot{};
            }
            m_topMove = Move{};
        }

        void insert(std::uint64_t hash, const tt_entry& newEntry)
        {
            if (!newEntry.valid) return;

            Bucket& bk = m_buckets[indexOf(hash)];

            // 1) If key already exists in either slot, replace if deeper/equal
            if (bk.a.entry.valid && bk.a.key == hash) {
                if (newEntry.depth >= bk.a.entry.depth) bk.a.entry = newEntry;
                return;
            }
            if (bk.b.entry.valid && bk.b.key == hash) {
                if (newEntry.depth >= bk.b.entry.depth) bk.b.entry = newEntry;
                return;
            }

            // 2) Empty slot? Use it.
            if (!bk.a.entry.valid) {
                bk.a.key = hash;
                bk.a.entry = newEntry;
                return;
            }
            if (!bk.b.entry.valid) {
                bk.b.key = hash;
                bk.b.entry = newEntry;
                return;
            }

            // 3) Collision: choose a victim.
            // Simple and effective: replace the shallower entry.
            // If equal depth, prefer replacing a non-EXACT entry first.
            Slot* victim = pickVictim(bk, newEntry);

            victim->key = hash;
            victim->entry = newEntry;
        }

        tt_entry lookup(std::uint64_t hash) const
        {
            const Bucket& bk = m_buckets[indexOf(hash)];

            if (bk.a.entry.valid && bk.a.key == hash) return bk.a.entry;
            if (bk.b.entry.valid && bk.b.key == hash) return bk.b.entry;

            return tt_entry{}; // invalid
        }

        void setTopMove(Move m) { m_topMove = m; }
        Move selectedMove() const { return m_topMove; }

        std::size_t bucketCount() const { return m_buckets.size(); }
        std::size_t approxEntryCapacity() const { return m_buckets.size() * 2; }

    private:
        static std::size_t nextPow2(std::size_t x)
        {
            if (x < 2) return 2;
            --x;
            x |= x >> 1;
            x |= x >> 2;
            x |= x >> 4;
            x |= x >> 8;
            x |= x >> 16;
            if constexpr (sizeof(std::size_t) == 8) x |= x >> 32;
            return x + 1;
        }

        void resize(std::size_t desiredEntries)
        {
            // 2-way: buckets = desiredEntries/2
            std::size_t desiredBuckets = (desiredEntries + 1) / 2;
            std::size_t bucketCap = nextPow2(desiredBuckets);

            m_buckets.assign(bucketCap, Bucket{});
            m_mask = bucketCap - 1;
        }

        std::size_t indexOf(std::uint64_t hash) const
        {
            return static_cast<std::size_t>(hash) & m_mask;
        }

        static Slot* pickVictim(Bucket& bk, const tt_entry& incoming)
        {
            // Replace shallower depth
            if (bk.a.entry.depth != bk.b.entry.depth) {
                return (bk.a.entry.depth < bk.b.entry.depth) ? &bk.a : &bk.b;
            }

            // Same depth: prefer replacing non-EXACT to keep strong info
            auto isExact = [](const Slot& s) { return s.entry.flag == tt_flag::EXACT; };

            const bool aExact = isExact(bk.a);
            const bool bExact = isExact(bk.b);

            if (aExact != bExact) {
                return aExact ? &bk.b : &bk.a;
            }

            // Same depth and both exactness same:
            // Replace one deterministically (or you could use hash bit to choose).
            // Using incoming depth doesn't help here; deterministic is fine.
            (void)incoming;
            return &bk.a;
        }
    };
}