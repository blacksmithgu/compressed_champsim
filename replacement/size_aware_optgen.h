#ifndef ___SIZE_AWARE_OPTGEN_H___
#define ___SIZE_AWARE_OPTGEN_H___

#include <iostream>
#include <math.h>
#include <set>
#include <vector>
#include <deque>
#include <inttypes.h>

/**
 * The structure which simulates what Belady's OPT would have done in
 * the past.
 */
struct OPTgen {
    /**
     * The liveness history, which records the number of used cache lines
     * at any given moment in the past.
     */
    std::vector<uint32_t> liveness_history;

    /**
     * The number of total cached lines in the past.
     */
    uint64_t num_cached;

    /**
     * The total number of lines which were attempted to be cached.
     */
    uint64_t num_attempted_cached;

    /**
     * The size of the cache, in cache lines.
     */
    uint64_t cache_size;

    /**
     * The time quanta of the START of the liveness vector; the vector is automatically
     * truncated whenever an entry is filled (i.e., equals cache size), and so this tracks
     * the first non-filled entry.
     */
    uint64_t liveness_start_quanta;

    /**
     * Construct an initially empty OPTgen instance. Useful only as
     * a temporary default constructor.
     */
    OPTgen(uint64_t cache_size) : liveness_history(), num_cached(0),
        num_attempted_cached(0), cache_size(cache_size), liveness_start_quanta(0) { }

    /**
     * Default optgen constructor; uses a default of 16 cache lines.
     */
    OPTgen() : OPTgen(16) {}

    /**
     * Attempts to cache a cache line which had a usage interval
     * between [last_quanta, curr_quanta].
     *
     * Returns true if it was successful, updating OPTgen's state
     * along the way, and false if it failed.
     */
    bool try_cache(uint64_t last_quanta, uint64_t curr_quanta) {
        // First things first, immediately record that an attempted
        // access occured on the interval [curr_quanta, last_quanta]
		num_attempted_cached++;

        // Figure out if OPTgen would cache the line; if it would, we
        // need to update the liveness history.
        if(!should_cache(last_quanta, curr_quanta)) return false;

        // We're resizing, so let's do things intelligently:
        // 1. Go through the existing part of the history, looking for entries which are full.
        //  find the index of the full entry if it exists.
        int64_t overflow_index = -1;
        for(int64_t i = liveness_index(last_quanta); i < std::min(int64_t(liveness_history.size()), liveness_index(curr_quanta) + 1); i++) {
            liveness_history[i]++;
            if(liveness_history[i] >= cache_size) overflow_index = i;
        }

        // Truncate the liveness history at overflow_index if it is positive,
        // and update the liveness history start.
        if(overflow_index >= 0) {
            liveness_history.erase(liveness_history.begin(), liveness_history.begin() + overflow_index + 1);
            liveness_start_quanta += uint64_t(overflow_index) + 1ull;
        }

        // Now, expand the new liveness history to the current quanta (inclusive), filling them in with '1's.
		while(int64_t(liveness_history.size()) <= liveness_index(curr_quanta))
			liveness_history.push_back(1);

        num_cached++;
		return true;
    }

    /**
     * Given a last access time and current access time, determines whether
     * OPTgen would have cached a line which had a liveness interval of
     * [last_quanta, curr_quanta].
     *
     * Does not mutate OPTgen's state.
     */
    bool should_cache(uint64_t last_quanta, uint64_t curr_quanta) {
        // If the last quanta is before the start, then the cache is already full.
        if(liveness_index(last_quanta) < 0) return false;

        // Obviously, we don't accept time traveling cache lines.
        if(curr_quanta < last_quanta) return false;

        // Otherwise, if the last quanta is after liveness_start_quanta and so is curr quanta, then we should definitely
        // cache - all entries are < cache_size.
        return true;
    }

    /**
     * Return the index of the given quanta in the liveness vector. If it's negative, this index is before
     * the beginning of the current index.
     */
    int64_t liveness_index(uint64_t quanta) { return int64_t(quanta) - int64_t(liveness_start_quanta); }

    /**
     * Obtain the total number of accesses recorded by OPTgen.
     */
    uint64_t num_accesses() { return num_attempted_cached; }

    /**
     * Obtain the total number of hits recorded by OPTgen.
     */
    uint64_t num_hits() { return num_cached; }
};

/**
 * A slower variant of OPTgen which is dimly "size aware" - instead of all cache lines
 * taking up a single cache line in the cache, this implementation tracks their exact
 * size in bytes and allows lines to be inserted until the cache's total BYTE capacity
 * is exceeded.
 */
struct SizeAwareOPTgen {
    /**
     * The liveness history, which records the total size in bytes of active cache lines
     * at any given moment in the past.
     */
    std::vector<uint64_t> liveness_history;

    /**
     * The number of total cached lines in the past.
     */
    uint64_t num_cached;

    /**
     * The total number of lines which were attempted to be cached.
     */
    uint64_t num_attempted_cached;

    /**
     * The size of the cache, in cache lines.
     */
    uint64_t cache_size;

    /**
     * The time quanta of the START of the liveness vector; the vector is automatically
     * truncated whenever an entry is filled (i.e., equals cache size), and so this tracks
     * the first non-filled entry.
     */
    uint64_t liveness_start_quanta;

    /**
     * Construct an initially empty OPTgen instance. Useful only as
     * a temporary default constructor.
     */
    SizeAwareOPTgen(uint64_t cache_size) : liveness_history(), num_cached(0),
        num_attempted_cached(0), cache_size(cache_size), liveness_start_quanta(0) { }

    /**
     * The default constructor for OPTgen - assumes 16 cache lines * 64 bytes of space.
     */
    SizeAwareOPTgen() : SizeAwareOPTgen(64) {}

    /**
     * Attempts to cache a cache line which had a usage interval
     * between [last_quanta, curr_quanta] with the given size in bytes.
     *
     * Returns true if it was successful, updating OPTgen's state
     * along the way, and false if it failed.
     */
    bool try_cache(uint64_t last_quanta, uint64_t curr_quanta, uint32_t size) {
        // First things first, immediately record that an attempted
        // access occured on the interval [curr_quanta, last_quanta]
		num_attempted_cached++;

        // Figure out if OPTgen would cache the line; if it would, we
        // need to update the liveness history.
        if(!should_cache(last_quanta, curr_quanta, size)) return false;

        // We're resizing, so let's do things intelligently:
        // 1. Go through the existing part of the history, looking for entries which are full.
        //  find the index of the full entry if it exists.
        int64_t overflow_index = -1;
        for(int64_t i = liveness_index(last_quanta); i < std::min(int64_t(liveness_history.size()), liveness_index(curr_quanta) + 1); i++) {
            liveness_history[i] += size;
            if(liveness_history[i] >= cache_size) overflow_index = i;
        }

        // Truncate the liveness history at overflow_index if it is positive,
        // and update the liveness history start.
        if(overflow_index >= 0) {
            liveness_history.erase(liveness_history.begin(), liveness_history.begin() + overflow_index + 1);
            liveness_start_quanta += uint64_t(overflow_index) + 1ull;
        }

        // Now, expand the new liveness history to the current quanta (inclusive), filling them in with '1's.
		while(int64_t(liveness_history.size()) <= liveness_index(curr_quanta))
			liveness_history.push_back(size);

        num_cached++;
		return true;
    }

    /**
     * Given a last access time and current access time, determines whether
     * OPTgen would have cached a line which had a liveness interval of
     * [last_quanta, curr_quanta].
     *
     * Does not mutate OPTgen's state.
     */
    bool should_cache(uint64_t last_quanta, uint64_t curr_quanta, uint32_t size) {
        // If the last quanta is before the start, then the cache is already full.
        if(liveness_index(last_quanta) < 0) return false;

        // Obviously, we don't accept time traveling cache lines.
        if(curr_quanta < last_quanta) return false;

        // Otherwise, in this size-aware case, we need to check that all of the cache lines in the liveness history
        // can fit <size> more bytes.
        for(int64_t index = std::max(liveness_index(last_quanta), int64_t(0));
            index < std::min(int64_t(liveness_history.size()), liveness_index(curr_quanta) + 1); index++) {
            if(liveness_history[index] + size > cache_size) return false;
        }

        return true;
    }

    /**
     * Return the index of the given quanta in the liveness vector. If it's negative, this index is before
     * the beginning of the current index.
     */
    int64_t liveness_index(uint64_t quanta) { return int64_t(quanta) - int64_t(liveness_start_quanta); }

    /**
     * Obtain the total number of accesses recorded by OPTgen.
     */
    uint64_t num_accesses() { return num_attempted_cached; }

    /**
     * Obtain the total number of hits recorded by OPTgen.
     */
    uint64_t num_hits() { return num_cached; }
};

#endif
