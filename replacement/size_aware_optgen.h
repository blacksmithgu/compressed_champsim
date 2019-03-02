#ifndef SIZE_AWARE_OPTGEN_H
#define SIZE_AWARE_OPTGEN_H

#include <iostream>
#include <math.h>
#include <set>
#include <vector>
#include <deque>
#include <inttypes.h>
#include <array>
#include <map>

/**
 * A generic interface for a cache model which we can query to check if a given usage interval could/could not be
 * cached.
 */
struct CacheGen {
    /**
     * Attempt to cache the given usage interval, returning true if it can be cached and false otherwise.
     */
    virtual bool try_cache(uint64_t start_quanta, uint64_t end_quanta, uint64_t address, uint32_t compression_factor) = 0;

    /**
     * Return true if the line can be cached, and false otherwise.
     */
    virtual bool can_cache(uint64_t start_quanta, uint64_t end_quanta, uint64_t address, uint32_t compression_factor) const = 0;

    /**
     * Obtain the total number of accesses recorded by the oracle.
     */
    virtual uint64_t num_accesses() const = 0;

    /**
     * Obtain the total number of hits recorded by the oracle.
     */
    virtual uint64_t num_hits() const = 0;
};

/**
 * An OPTgen-specific implementation of a ring buffer; keeps track of up to N elements and efficently supports adding
 * elements (automatically removing old elements when the capacity is exceeded). Tracks the quanta at the start of the
 * ring buffer automatically.
 */
template<typename T, uint32_t _capacity> class OptgenRingBuffer {
    // The buffer of actual elements.
    std::array<T, _capacity> buffer = {0};

    // The current size of the buffer.
    size_t _size = 0;

    // The index of the current head of the buffer.
    size_t _head = 0;

    // The quanta of the head of the ring buffer.
    size_t _head_quanta = 0;

    // Convert a virtual index into an index in the ring buffer.
    size_t buffer_index(size_t index) const {
        return (_head + index) % _capacity;
    }

    // Convert a quanta into an index in the ring buffer.
    size_t quanta_index(size_t quanta) const {
        return buffer_index(quanta - _head_quanta);
    }

public:
    OptgenRingBuffer() : buffer(), _size(0), _head(0), _head_quanta(0) {}

    // Push a new element onto the ring buffer.
    void push(T&& element) {
        buffer[buffer_index(_size)] = element;
        _size++;

        if(_size > _capacity) {
            _size--;
            _head = (_head + 1) % _capacity;

            _head_quanta++;
        }
    }

    // Return true if the given quanta is in the bounds of the ring buffer.
    bool in_bounds(size_t quanta) const {
        return quanta >= _head_quanta && quanta < _head_quanta + _size;
    }

    // Return true if the given quanta is before the start of the buffer.
    bool before_start(size_t quanta) const {
        return quanta < _head_quanta;
    }

    // Return true if the given quanta is after the end of the buffer.
    bool after_end(size_t quanta) const {
        return quanta >= _head_quanta + _size;
    }

    // Clamp the quanta to be in the bounds of the buffer.
    size_t clamp(size_t quanta) const {
        // If there's nothing in the buffer, there's no way to clamp, so just return 0.
        if(_head_quanta + _size == 0) return 0;

        return std::max(std::min(quanta, _head_quanta + _size - 1), _head_quanta);
    }

    // Operator [] overrides.
    const T& operator[](size_t quanta) const { return buffer[quanta_index(quanta)]; }
    T& operator[](size_t quanta) { return buffer[quanta_index(quanta)]; }
};

template<size_t capacity> struct OPTgen : public CacheGen {
    // Liveness vector; capacity limited.
    OptgenRingBuffer<uint32_t, capacity> liveness;

    // The number of total cached lines in the past.
    uint64_t num_cached = 0;

    // The total number of lines which were attempted to be cached.
    uint64_t num_attempted_cached = 0;

    // The size of the cache, in cache lines.
    uint64_t cache_size;

    OPTgen(uint32_t cache_size) : cache_size(cache_size) {}
    OPTgen(const OPTgen& other) : liveness(other.liveness), num_cached(other.num_cached), num_attempted_cached(other.num_attempted_cached),
        cache_size(other.cache_size) {}
    OPTgen() : OPTgen(16) {}

    /**
     * Attempt to cache the given usage interval, returning true if it can be cached and false otherwise.
     * Note that end_quanta > start_quanta (strictly), and both are *inclusive*.
     */
    virtual bool try_cache(uint64_t start_quanta, uint64_t end_quanta, uint64_t address, uint32_t compression_factor) override {
        num_attempted_cached++;
        if(!can_cache(start_quanta, end_quanta, address, compression_factor)) return false;

        // start_quanta is in or after the buffer (via can_cache), and end_quanta is after start_quanta, so shift up
        // until it's in the buffer.
        while(liveness.after_end(end_quanta)) liveness.push(0);

        // Increment all entries in the buffer from clamp(start_quanta) to end_quanta.
        for(uint64_t quanta = liveness.clamp(start_quanta); quanta <= end_quanta; quanta++) liveness[quanta]++;

        num_cached++;
        return true;
    }

    /**
     * Return true if the line would be cached, and false otherwise. Note both start and end quanta are inclusive.
     */
    virtual bool can_cache(uint64_t start_quanta, uint64_t end_quanta, uint64_t address, uint32_t compression_factor) const override {
        if(liveness.before_start(start_quanta)) return false;
        if(liveness.after_end(start_quanta)) return true;

        // Start quanta is in bounds, check if all entries are < max cache size.
        for(uint64_t quanta = start_quanta; quanta <= liveness.clamp(end_quanta); quanta++)
            if(liveness[quanta] >= cache_size) return false;

        return true;
    }

    /**
     * Obtain the total number of accesses recorded by the oracle.
     */
    virtual uint64_t num_accesses() const override { return num_attempted_cached; }

    /**
     * Obtain the total number of hits recorded by the oracle.
     */
    virtual uint64_t num_hits() const override { return num_cached; }
};

/**
 * Superblock in YACC; has a compression factor, current capacity.
 */
struct YACCSuperblock {
    // The address of the superblock.
    uint64_t address;

    // The compression factor of this superblock.
    uint32_t compression_factor;

    // The number of entries currently in the superblock.
    uint32_t entries;

    YACCSuperblock(uint64_t addr, uint32_t cf, uint32_t entries) : address(addr), compression_factor(cf), entries(entries) {}
    YACCSuperblock() : address(0x0), compression_factor(1), entries(0) {}
};

/**
 * A time quanta in a YACC-based cache; supports trying adding new cache lines.
 */
struct YACCQuanta {
    // The map of superblock address -> superblock.
    std::map<uint64_t, YACCSuperblock> superblocks;

    // The max number of allowed entries.
    uint32_t cache_size;

    YACCQuanta(uint32_t cache_size) : cache_size(cache_size) {}
    YACCQuanta() : YACCQuanta(16) {}

    bool try_cache(uint64_t address, uint32_t compression_factor) {
        uint64_t superblock_addr = YACCQuanta::superblock_for(address);
        auto block = superblocks.find(superblock_addr);

        // Two cases: if the super block is not in the cache, then we can cache if there's still cache lines.
        // If the super block is in the cache, we can cache if the lines are the same compression factor and there is
        // still space left within the superblock.
        if(block == superblocks.end()) {
            if(superblocks.size() >= cache_size) return false;
            superblocks[superblock_addr] = YACCSuperblock(superblock_addr, compression_factor, 1);
        } else {
            if(block->second.compression_factor != compression_factor || block->second.entries >= block->second.compression_factor) return false;
            block->second.entries++;
        }

        return true;
    }

    // Return true if this quanta can store the given address with the given compression factor, and false otherwise.
    bool can_cache(uint64_t address, uint32_t compression_factor) const {
        uint64_t superblock_addr = YACCQuanta::superblock_for(address);
        auto block = superblocks.find(superblock_addr);

        // Two cases: if the super block is not in the cache, then we can cache if there's still cache lines.
        // If the super block is in the cache, we can cache if the lines are the same compression factor and there is
        // still space left within the superblock.
        if(block == superblocks.end()) {
            return superblocks.size() < cache_size;
        } else {
            return block->second.compression_factor == compression_factor && block->second.entries < block->second.compression_factor;
        }
    }

    // Obtain the super block address given a full address; this is just dropping 6 bits (the offset bits in a cache
    // line) and an additional 2 bits (for the 4 lines per superblock).
    static uint64_t superblock_for(uint64_t address) {
        return address & ~0xff;
    }
};

/**
 * A cache model which checks if YACC would be able to cache given cache accesses.
 */
template<uint32_t capacity> struct YACCgen : public CacheGen {
    // Liveness vector (with the superblocks in the cache at the time); capacity limited.
    OptgenRingBuffer<YACCQuanta, capacity> liveness;

    // The number of total cached lines in the past.
    uint64_t num_cached = 0;

    // The total number of lines which were attempted to be cached.
    uint64_t num_attempted_cached = 0;

    // The size of the cache, in cache lines.
    uint64_t cache_size;

    YACCgen(uint32_t cache_size) : cache_size(cache_size) {}
    YACCgen() : YACCgen(16) {}

    /**
     * Attempt to cache the given usage interval, returning true if it can be cached and false otherwise.
     * Note that end_quanta > start_quanta (strictly), and both are *inclusive*.
     */
    virtual bool try_cache(uint64_t start_quanta, uint64_t end_quanta, uint64_t address, uint32_t compression_factor) override {
        num_attempted_cached++;
        if(!can_cache(start_quanta, end_quanta, address, compression_factor)) return false;

        // start_quanta is in or after the buffer (via can_cache), and end_quanta is after start_quanta, so shift up
        // until it's in the buffer.
        while(liveness.after_end(end_quanta)) liveness.push(YACCQuanta(cache_size));

        // Increment all entries in the buffer from clamp(start_quanta) to end_quanta.
        for(uint64_t quanta = liveness.clamp(start_quanta); quanta <= end_quanta; quanta++) liveness[quanta].try_cache(address, compression_factor);

        num_cached++;
        return true;
    }

    /**
     * Return true if the line would be cached, and false otherwise. Note both start and end quanta are inclusive.
     */
    virtual bool can_cache(uint64_t start_quanta, uint64_t end_quanta, uint64_t address, uint32_t compression_factor) const override {
        if(!liveness.before_start(start_quanta)) return false;
        if(liveness.after_end(start_quanta)) return true;

        // Start quanta is in bounds, check if this usage interval fits at every quanta.
        for(uint64_t quanta = start_quanta; quanta <= liveness.clamp(end_quanta); quanta++)
            if(!liveness[quanta].can_cache(address, compression_factor)) return false;

        return true;
    }

    /**
     * Obtain the total number of accesses recorded by the oracle.
     */
    virtual uint64_t num_accesses() const override { return num_attempted_cached; }

    /**
     * Obtain the total number of hits recorded by the oracle.
     */
    virtual uint64_t num_hits() const override { return num_cached; }
};

/**
 * The structure which simulates what Belady's OPT would have done in the past.
 */
struct UnboundedOPTgen : public CacheGen {
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
     * The size of the cache, in cache lines.  */
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
    UnboundedOPTgen(uint64_t cache_size) : liveness_history(), num_cached(0),
        num_attempted_cached(0), cache_size(cache_size), liveness_start_quanta(0) { }

    /**
     * Default optgen constructor; uses a default of 16 cache lines.
     */
    UnboundedOPTgen() : UnboundedOPTgen(16) {}

    /**
     * Attempts to cache a cache line which had a usage interval
     * between [last_quanta, curr_quanta].
     *
     * Returns true if it was successful, updating OPTgen's state
     * along the way, and false if it failed.
     */
    virtual bool try_cache(uint64_t last_quanta, uint64_t curr_quanta, uint64_t address, uint32_t compression_factor) override {
        // First things first, immediately record that an attempted
        // access occured on the interval [curr_quanta, last_quanta]
		num_attempted_cached++;

        // Figure out if OPTgen would cache the line; if it would, we
        // need to update the liveness history.
        if(!can_cache(last_quanta, curr_quanta, address, compression_factor)) return false;

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
    virtual bool can_cache(uint64_t last_quanta, uint64_t curr_quanta, uint64_t address, uint32_t compression_factor) const override {
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
    int64_t liveness_index(uint64_t quanta) const { return int64_t(quanta) - int64_t(liveness_start_quanta); }

    /**
     * Obtain the total number of accesses recorded by OPTgen.
     */
    virtual uint64_t num_accesses() const override { return num_attempted_cached; }

    /**
     * Obtain the total number of hits recorded by OPTgen.
     */
    virtual uint64_t num_hits() const override { return num_cached; }
};

/**
 * A slower variant of OPTgen which is dimly "size aware" - instead of all cache lines
 * taking up a single cache line in the cache, this implementation tracks their exact
 * size in bytes and allows lines to be inserted until the cache's total BYTE capacity
 * is exceeded.
 */
struct UnboundedSizeAwareOPTgen : public CacheGen {
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
    UnboundedSizeAwareOPTgen(uint64_t cache_size) : liveness_history(), num_cached(0),
        num_attempted_cached(0), cache_size(cache_size * 64), liveness_start_quanta(0) { }

    /**
     * The default constructor for the size aware OPTgen - assumes 16 cache lines.
     */
    UnboundedSizeAwareOPTgen() : UnboundedSizeAwareOPTgen(16) {}

    /**
     * Attempts to cache a cache line which had a usage interval
     * between [last_quanta, curr_quanta] with the given size in bytes.
     *
     * Returns true if it was successful, updating OPTgen's state
     * along the way, and false if it failed.
     */
    virtual bool try_cache(uint64_t last_quanta, uint64_t curr_quanta, uint64_t address, uint32_t compression_factor) override {
        // First things first, immediately record that an attempted
        // access occured on the interval [curr_quanta, last_quanta]
		num_attempted_cached++;

        // Figure out if OPTgen would cache the line; if it would, we
        // need to update the liveness history.
        if(!can_cache(last_quanta, curr_quanta, address, compression_factor)) return false;

        uint32_t size = 64 / compression_factor;

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
    virtual bool can_cache(uint64_t last_quanta, uint64_t curr_quanta, uint64_t address, uint32_t compression_factor) const override {
        // If the last quanta is before the start, then the cache is already full.
        if(liveness_index(last_quanta) < 0) return false;

        // Obviously, we don't accept time traveling cache lines.
        if(curr_quanta < last_quanta) return false;

        uint32_t size = 64 / compression_factor;

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
    int64_t liveness_index(uint64_t quanta) const { return int64_t(quanta) - int64_t(liveness_start_quanta); }

    /**
     * Obtain the total number of accesses recorded by OPTgen.
     */
    virtual uint64_t num_accesses() const override { return num_attempted_cached; }

    /**
     * Obtain the total number of hits recorded by OPTgen.
     */
    virtual uint64_t num_hits() const override { return num_cached; }
};

#endif
