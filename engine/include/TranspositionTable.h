#pragma once
#include <cstdint>
#include <vector>

#if defined(_MSC_VER) && defined(_M_X64)
#include <intrin.h> // _umul128
#endif

#include "surge.h"

namespace bq {

    enum class tt_flag : std::uint8_t { EXACT, UPPERBOUND, LOWERBOUND };

    struct tt_entry {
        int depth = -1;
        int score = 0;
        tt_flag flag = tt_flag::EXACT;
        bool valid = false;
        Move bestMove{};
    };

    class TranspositionTable {
    public:
        struct Slot {
            std::uint64_t key = 0;
            tt_entry entry{};
        };

        struct Bucket {
            Slot a{};
            Slot b{};
        };

    private:
        std::vector<Bucket> m_buckets{};
        Move m_topMove{};
        static constexpr std::size_t kMinBuckets = 2;

        static inline std::size_t fastIndex(std::uint64_t h, std::size_t n) {
#if defined(_MSC_VER) && defined(_M_X64)
            // index = (h * n) >> 64
            unsigned __int64 hi = 0;
            _umul128((unsigned __int64)h, (unsigned __int64)n, &hi);
            return (std::size_t)hi;
#elif defined(__SIZEOF_INT128__)
            return (std::size_t)(((__uint128_t)h * (__uint128_t)n) >> 64);
#else
            return (std::size_t)(h % n);
#endif
        }

        std::size_t indexOf(std::uint64_t hash) const {
            return fastIndex(hash, m_buckets.size());
        }

    public:
        static constexpr std::size_t defaultSizeMb = 1024;

        TranspositionTable() { resizeMB(defaultSizeMb); }
        explicit TranspositionTable(std::size_t sizeMb) { resizeMB(sizeMb); }

        void resizeMB(std::size_t mb) {
            const std::size_t bytes = mb * 1024ULL * 1024ULL;
            std::size_t buckets = bytes / sizeof(Bucket);
            if (buckets < kMinBuckets) buckets = kMinBuckets;
            m_buckets.assign(buckets, Bucket{});
        }

        std::size_t bucketCount() const { return m_buckets.size(); }
        std::size_t approxEntryCapacity() const { return m_buckets.size() * 2; }

        // NOTE: now this is the TRUE bucket index (not mask-based)
        std::size_t BucketIndex(std::uint64_t hash) const { return indexOf(hash); }
        std::size_t BucketCount() const { return m_buckets.size(); }

        void clear() {
            for (auto& bk : m_buckets) {
                bk.a = Slot{};
                bk.b = Slot{};
            }
            m_topMove = Move{};
        }

        void insert(std::uint64_t hash, const tt_entry& newEntry) {
            if (!newEntry.valid) return;

            Bucket& bk = m_buckets[indexOf(hash)];

            if (bk.a.entry.valid && bk.a.key == hash) {
                if (newEntry.depth >= bk.a.entry.depth) bk.a.entry = newEntry;
                return;
            }
            if (bk.b.entry.valid && bk.b.key == hash) {
                if (newEntry.depth >= bk.b.entry.depth) bk.b.entry = newEntry;
                return;
            }

            if (!bk.a.entry.valid) { bk.a.key = hash; bk.a.entry = newEntry; return; }
            if (!bk.b.entry.valid) { bk.b.key = hash; bk.b.entry = newEntry; return; }

            Slot* victim = pickVictim(bk);
            victim->key = hash;
            victim->entry = newEntry;
        }

        tt_entry lookup(std::uint64_t hash) const {
            const Bucket& bk = m_buckets[indexOf(hash)];
            if (bk.a.entry.valid && bk.a.key == hash) return bk.a.entry;
            if (bk.b.entry.valid && bk.b.key == hash) return bk.b.entry;
            return tt_entry{};
        }

        void setTopMove(Move m) { m_topMove = m; }
        Move selectedMove() const { return m_topMove; }

    private:
        static Slot* pickVictim(Bucket& bk) {
            if (bk.a.entry.depth != bk.b.entry.depth)
                return (bk.a.entry.depth < bk.b.entry.depth) ? &bk.a : &bk.b;

            const bool aExact = (bk.a.entry.flag == tt_flag::EXACT);
            const bool bExact = (bk.b.entry.flag == tt_flag::EXACT);
            if (aExact != bExact) return aExact ? &bk.b : &bk.a;

            return &bk.a;
        }
    };
}
