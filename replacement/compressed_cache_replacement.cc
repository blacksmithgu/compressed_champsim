#include "cache.h"
#ifdef COMPRESSED_CACHE
uint32_t CACHE::llc_find_victim_cc(uint32_t cpu, uint64_t instr_id, uint32_t set, const COMPRESSED_CACHE_BLOCK *current_set, uint64_t ip, uint64_t full_addr, uint32_t type, uint64_t incoming_cf, uint32_t& evicted_cf_index)
{
    //Step 1: Find line with same superblock and same CF
    for (uint32_t way=0; way<NUM_WAY; way++) 
    {
        //Superblock hit
        if (compressed_cache_block[set][way].sbTag == get_sb_tag(full_addr>>6))
        {
            //Same compression factor!
            if(compressed_cache_block[set][way].compressionFactor == incoming_cf) 
            {
                for (uint32_t cf = 0; cf < compressed_cache_block[set][way].compressionFactor; cf++) 
                {
                    //Found invalid block
                    if (compressed_cache_block[set][way].valid[cf] == 0) 
                    {
                        evicted_cf_index = cf;
//                        cout << "Small " << way << endl;
                        return way;
                    }
                }
            }
        }
    }

    //Step 2: Find invalid line with a CF=0
    uint32_t way = 0;

    for (way=0; way<NUM_WAY; way++) 
    {
        if(compressed_cache_block[set][way].compressionFactor == 0)
        {
            assert(compressed_cache_block[set][way].valid[0] == 0);
            assert(compressed_cache_block[set][way].valid[1] == 0);
            assert(compressed_cache_block[set][way].valid[2] == 0);
            assert(compressed_cache_block[set][way].valid[3] == 0);
            compressed_cache_block[set][way].compressionFactor = incoming_cf;
            evicted_cf_index = 0;
 //           cout << "Invalid: " << way << endl;
            return way;
        }
    }

    //Step 3: Evict a superblock in line with replacement policy
    // LRU victim
    //Maybe some lines are dirty! - We should handle this in controller but maybe let them know by setting evicted_cf_index to 4
    if (way == NUM_WAY) {
        for (way=0; way<NUM_WAY; way++) {
            if (compressed_cache_block[set][way].lru == NUM_WAY-1) {

                DP ( if (warmup_complete[cpu]) {
                        cout << "[" << NAME << "] " << __func__ << " instr_id: " << instr_id << " replace set: " << set << " way: " << way;
                        cout << hex << " address: " << (full_addr>>LOG2_BLOCK_SIZE) << " victim address: " << compressed_cache_block[set][way].address << " data: " << compressed_cache_block[set][way].data;
                        cout << dec << " lru: " << compressed_cache_block[set][way].lru << endl; });

            //    cout << "Victim " << set << " " << way << " LRU" << endl;
                break;
            }
        }
    }

    if (way == NUM_WAY) {
        cerr << "[" << NAME << "] " << __func__ << " no victim! set: " << set << endl;
        assert(0);
    }
  //  cout << "Victim end " << way << endl;
    evicted_cf_index = 4;
    return way;
}

void CACHE::llc_update_replacement_state_cc(uint32_t cpu, uint32_t set, uint32_t way, uint32_t cf, uint64_t full_addr, uint64_t ip, uint64_t victim_addr, uint32_t type, uint8_t hit, uint64_t latency, uint64_t effective_latency)
{
    //cout << "Update Start " << endl;
    if (type == WRITEBACK) {
        if (hit) // wrietback hit does not update LRU state
            return;
    }

    for (uint32_t i=0; i<NUM_WAY; i++) {
        if (compressed_cache_block[set][i].lru < compressed_cache_block[set][way].lru) {
            compressed_cache_block[set][i].lru++;
        }
    }
    compressed_cache_block[set][way].lru = 0; // promote to the MRU position

    //cout << "Update End " << endl;
}
#endif
