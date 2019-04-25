#ifndef __AUXILIARY_TAG_ARRAY_H__
#define __AUXILIARY_TAG_ARRAY_H__

#include "cache.h"
#include "block.h"

#include <memory>
#include <stdint.h>

// Abstraction for an auxiliary tag array; has it's own RRPV counters and tags,
// as well as a specific block size which it prioritizes.
struct AuxiliaryTagArray {
    std::array<std::array<uint32_t, MAX_COMPRESSIBILITY>, LLC_WAY> rrpv;
    std::unique_ptr<COMPRESSED_CACHE_BLOCK[]> block;

    // The 'index' of the prioritized size: 0 = 1-8b, 1 = 9-16b, and so on, up to 7 = 57-64b.
    uint32_t prioritized_size_index;

    inline AuxiliaryTagArray() : rrpv(), block(), prioritized_size_index(-1) {}

    inline AuxiliaryTagArray(uint32_t prioritized_size_index, uint32_t rrpv_max) : rrpv(),
         block(new COMPRESSED_CACHE_BLOCK[LLC_WAY]), prioritized_size_index(prioritized_size_index) {
        for(int way = 0; way < LLC_WAY; way++)
            for(int cf = 0; cf < MAX_COMPRESSIBILITY; cf++)
                rrpv[way][cf] = rrpv_max;
    }

    // Copy the current state of a different cache block.
    inline void copy(COMPRESSED_CACHE_BLOCK* existing) {
        for(int way = 0; way < LLC_WAY; way++) {
            block[way].sbTag = existing[way].sbTag;
            block[way].compressionFactor = existing[way].compressionFactor;

            for(uint32_t ci = 0; ci < existing[way].compressionFactor; ci++) {
                block[way].valid[ci] = existing[way].valid[ci];
                block[way].dirty[ci] = existing[way].dirty[ci];
                block[way].prefetch[ci] = existing[way].prefetch[ci];
                block[way].used[ci] = existing[way].used[ci];

                block[way].compressed_size[ci] = existing[way].compressed_size[ci];
                block[way].blkId[ci] = existing[way].blkId[ci];
                block[way].tag[ci] = existing[way].tag[ci];
                block[way].address[ci] = existing[way].address[ci];
                block[way].full_addr[ci] = existing[way].full_addr[ci];
            }
        }
    }

    // Add a cache line to the auxiliary cache.
    inline void fill(uint32_t way, uint32_t compressed_index, uint64_t full_addr, uint32_t compressed_size, CACHE* cache) {
        // Reset the cache fields - set the position to valid, clean and unused.
        block[way].valid[compressed_index] = 1;
        block[way].dirty[compressed_index] = 0;
        block[way].prefetch[compressed_index] = 0;
        block[way].used[compressed_index] = 0;

        // Set the identifying information - tag, compression factor, and block ID.
        uint64_t address = full_addr >> LOG2_BLOCK_SIZE;
        block[way].sbTag = cache->get_sb_tag(address);
        block[way].compressed_size[compressed_index] = compressed_size;
        block[way].compressionFactor = cache->get_compression_factor(compressed_size);
        block[way].blkId[compressed_index] = cache->get_blkid_cc(address);

        block[way].tag[compressed_index] = address;
        block[way].address[compressed_index] = address;
        block[way].full_addr[compressed_index] = full_addr;
    }

    // Evict a way from the auxiliary cache; if compressed_index = 4, evicts all lines in a way.
    inline void evict(uint32_t way, uint32_t compressed_index) {
        if(compressed_index == MAX_COMPRESSIBILITY) {
            // If evicting line, set all valid = 0, and compression factor to 0.
            for(int cf = 0; cf < MAX_COMPRESSIBILITY; cf++) block[way].valid[cf] = 0;
            block[way].compressionFactor = 0;
        } else {
            // Otherwise, set just the given cf to invalid, and then check if any ways are valid. If not, set the entire
            // line to be invalid.
            block[way].valid[compressed_index] = 0;
            bool any_valid = false;
            for(int cf = 0; cf < MAX_COMPRESSIBILITY; cf++) any_valid = any_valid || (block[way].valid[cf] == 1);

            if(!any_valid) block[way].compressionFactor = 0;
        }
    }

    // Attempts to find the way/compressed index which contains the given address. Returns false if the address cannot be
    // found; otherwise, returns true.
    inline bool find(uint64_t full_addr, CACHE* cache, uint32_t& found_way, uint32_t& found_compressed_index) {
        uint64_t sb_tag = cache->get_sb_tag(full_addr >> LOG2_BLOCK_SIZE);
        uint64_t blk_id = cache->get_blkid_cc(full_addr >> LOG2_BLOCK_SIZE);

        for(int way = 0; way < LLC_WAY; way++) {
            for(int cf = 0; cf < MAX_COMPRESSIBILITY; cf++) {
                if(block[way].valid[cf] && block[way].sbTag == sb_tag && block[way].blkId[cf] == blk_id) {
                    found_compressed_index = cf;
                    found_way = way;
                    return true;
                }
            }
        }

        return false;
    }
};
#endif
