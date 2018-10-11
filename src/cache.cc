#include "cache.h"
#include "set.h"

uint64_t l2pf_access = 0;
#ifdef COMPRESSED_CACHE
void CACHE::configure_compressed_cache()
{
    is_compressed = true;
    compressed_cache_block = new COMPRESSED_CACHE_BLOCK* [NUM_SET];
    for (uint32_t i=0; i<NUM_SET; i++) {
        compressed_cache_block[i] = new COMPRESSED_CACHE_BLOCK[NUM_WAY]; 

        for (uint32_t j=0; j<NUM_WAY; j++) {
            compressed_cache_block[i][j].lru = j;
            compressed_cache_block[i][j].compressionFactor = 0;
        }
    }
}
#endif

void CACHE::handle_fill()
{
    // handle fill
    uint32_t fill_cpu = (MSHR.next_fill_index == MSHR_SIZE) ? NUM_CPUS : MSHR.entry[MSHR.next_fill_index].cpu;
    if (fill_cpu == NUM_CPUS)
        return;

    if (MSHR.next_fill_cycle <= current_core_cycle[fill_cpu]) {

#ifdef SANITY_CHECK
        if (MSHR.next_fill_index >= MSHR.SIZE)
            assert(0);
#endif

        uint32_t mshr_index = MSHR.next_fill_index;

        // find victim
        uint32_t set = get_set(MSHR.entry[mshr_index].address), way;

#ifdef COMPRESSED_CACHE
        uint32_t evicted_cf = 0;
        uint32_t compression_factor = getCF(MSHR.entry[mshr_index].program_data);
#endif

        if (cache_type == IS_LLC) {
#ifdef COMPRESSED_CACHE
            if(is_compressed)
            {
                set = get_set_cc(MSHR.entry[mshr_index].address);
                way = llc_find_victim_cc(fill_cpu, MSHR.entry[mshr_index].instr_id, set, compressed_cache_block[set], MSHR.entry[mshr_index].ip, MSHR.entry[mshr_index].full_addr, MSHR.entry[mshr_index].type, compression_factor, evicted_cf);
            }
            else
#endif
                way = llc_find_victim(fill_cpu, MSHR.entry[mshr_index].instr_id, set, block[set], MSHR.entry[mshr_index].ip, MSHR.entry[mshr_index].full_addr, MSHR.entry[mshr_index].type);
        }
        else
            way = find_victim(fill_cpu, MSHR.entry[mshr_index].instr_id, set, block[set], MSHR.entry[mshr_index].ip, MSHR.entry[mshr_index].full_addr, MSHR.entry[mshr_index].type);

#ifdef LLC_BYPASS
        if ((cache_type == IS_LLC) && (way == LLC_WAY)) { // this is a bypass that does not fill the LLC

            // update replacement policy
            if (cache_type == IS_LLC) {
#ifdef COMPRESSED_CACHE
                if(is_compressed)
                    llc_update_replacement_state_cc(fill_cpu, set, way, evicted_cf, MSHR.entry[mshr_index].full_addr, MSHR.entry[mshr_index].ip, 0, MSHR.entry[mshr_index].type, compression_factor, 0, MSHR.entry[mshr_index].latency, MSHR.entry[mshr_index].effective_latency);
                else
#endif
                    llc_update_replacement_state(fill_cpu, set, way, MSHR.entry[mshr_index].full_addr, MSHR.entry[mshr_index].ip, 0, MSHR.entry[mshr_index].type, 0, MSHR.entry[mshr_index].latency, MSHR.entry[mshr_index].effective_latency);

            }
            else
                update_replacement_state(fill_cpu, set, way, MSHR.entry[mshr_index].full_addr, MSHR.entry[mshr_index].ip, 0, MSHR.entry[mshr_index].type, 0);

            // COLLECT STATS
            sim_miss[fill_cpu][MSHR.entry[mshr_index].type]++;
            sim_access[fill_cpu][MSHR.entry[mshr_index].type]++;

            // check fill level
            if (MSHR.entry[mshr_index].fill_level < fill_level) {

                if (MSHR.entry[mshr_index].instruction) 
                    upper_level_icache[fill_cpu]->return_data(&MSHR.entry[mshr_index]);
                else // data
                    upper_level_dcache[fill_cpu]->return_data(&MSHR.entry[mshr_index]);
            }

            if(MSHR.entry[mshr_index].type == LOAD)
                MSHR.read_occupancy--;
            MSHR.remove_queue(&MSHR.entry[mshr_index]);
            MSHR.num_returned--;

            update_fill_cycle();

            return; // return here, no need to process further in this function
        }
#endif

        uint8_t  do_fill = 1;
#ifdef COMPRESSED_CACHE
        if(is_compressed)
            do_fill = evict_compressed_line(set, way, MSHR.entry[mshr_index], evicted_cf);
#else
        bool evicted_block_dirty = block[set][way].dirty;
        uint64_t evicted_block_addr = block[set][way].address;

        // is this dirty?
        if (evicted_block_dirty) {

            // check if the lower level WQ has enough room to keep this writeback request
            if (lower_level) {
                if (lower_level->get_occupancy(2, evicted_block_addr) == lower_level->get_size(2, evicted_block_addr)) {

                    // lower level WQ is full, cannot replace this victim
                    do_fill = 0;
                    lower_level->increment_WQ_FULL(evicted_block_addr);
                    STALL[MSHR.entry[mshr_index].type]++;

                    DP ( if (warmup_complete[fill_cpu]) {
                            cout << "[" << NAME << "] " << __func__ << "do_fill: " << +do_fill;
                            cout << " lower level wq is full!" << " fill_addr: " << hex << MSHR.entry[mshr_index].address;
                            cout << " victim_addr: " << evicted_block_addr << dec << endl; });
                }
                else {
                    PACKET writeback_packet;

                    writeback_packet.fill_level = fill_level << 1;
                    writeback_packet.cpu = fill_cpu;
                    writeback_packet.address = block[set][way].address;
                    writeback_packet.full_addr = block[set][way].full_addr;
                    writeback_packet.data = block[set][way].data;
                    memcpy( writeback_packet.program_data, block[set][way].program_data, CACHE_LINE_BYTES);
                    writeback_packet.instr_id = MSHR.entry[mshr_index].instr_id;
                    writeback_packet.ip = 0; // writeback does not have ip
                    writeback_packet.type = WRITEBACK;
                    writeback_packet.event_cycle = current_core_cycle[fill_cpu];

                    lower_level->add_wq(&writeback_packet);
                }
            }
#ifdef SANITY_CHECK
            else {
                // sanity check
                if (cache_type != IS_STLB)
                    assert(0);
            }
#endif
        }
#endif

        if (do_fill) {
            // update prefetcher
            if (cache_type == IS_L1D)
                l1d_prefetcher_cache_fill(MSHR.entry[mshr_index].full_addr, set, way, (MSHR.entry[mshr_index].type == PREFETCH) ? 1 : 0, block[set][way].full_addr);
            if  (cache_type == IS_L2C)
                l2c_prefetcher_cache_fill(MSHR.entry[mshr_index].full_addr, set, way, (MSHR.entry[mshr_index].type == PREFETCH) ? 1 : 0, block[set][way].full_addr);

            // update replacement policy
            if (cache_type == IS_LLC) {
#ifdef COMPRESSED_CACHE
                if(is_compressed)
                    llc_update_replacement_state_cc(fill_cpu, set, way, evicted_cf, MSHR.entry[mshr_index].full_addr, MSHR.entry[mshr_index].ip, compressed_cache_block[set][way].full_addr[evicted_cf], MSHR.entry[mshr_index].type, compression_factor, 0, MSHR.entry[mshr_index].latency, MSHR.entry[mshr_index].effective_latency);
                else
#endif
                llc_update_replacement_state(fill_cpu, set, way, MSHR.entry[mshr_index].full_addr, MSHR.entry[mshr_index].ip, block[set][way].full_addr, MSHR.entry[mshr_index].type, 0, MSHR.entry[mshr_index].latency, MSHR.entry[mshr_index].effective_latency);

            }
            else
                update_replacement_state(fill_cpu, set, way, MSHR.entry[mshr_index].full_addr, MSHR.entry[mshr_index].ip, block[set][way].full_addr, MSHR.entry[mshr_index].type, 0);


            if((cache_type == IS_STLB) && (prefetcher_level_dcache != NULL)) {
                prefetcher_level_dcache->inform_tlb_eviction(MSHR.entry[mshr_index].data, block[set][way].data);
            }


            // COLLECT STATS
            sim_miss[fill_cpu][MSHR.entry[mshr_index].type]++;
            sim_access[fill_cpu][MSHR.entry[mshr_index].type]++;

#ifdef COMPRESSED_CACHE
            if(is_compressed)
                fill_cache_cc(set, way, evicted_cf, &MSHR.entry[mshr_index]);
            else
#endif
            fill_cache(set, way, &MSHR.entry[mshr_index]);

            // RFO marks cache line dirty
            if (cache_type == IS_L1D) {
                if (MSHR.entry[mshr_index].type == RFO)
                    block[set][way].dirty = 1;
            }

            // check fill level
            if (MSHR.entry[mshr_index].fill_level < fill_level) {

                if (MSHR.entry[mshr_index].instruction) 
                    upper_level_icache[fill_cpu]->return_data(&MSHR.entry[mshr_index]);
                else // data
                    upper_level_dcache[fill_cpu]->return_data(&MSHR.entry[mshr_index]);
            }

            // update processed packets
            if (cache_type == IS_ITLB) { 
                MSHR.entry[mshr_index].instruction_pa = block[set][way].data;
                if (PROCESSED.occupancy < PROCESSED.SIZE)
                    PROCESSED.add_queue(&MSHR.entry[mshr_index]);
            }
            else if (cache_type == IS_DTLB) {
                MSHR.entry[mshr_index].data_pa = block[set][way].data;
                if (PROCESSED.occupancy < PROCESSED.SIZE)
                    PROCESSED.add_queue(&MSHR.entry[mshr_index]);
            }
            else if (cache_type == IS_L1I) {
                if (PROCESSED.occupancy < PROCESSED.SIZE)
                    PROCESSED.add_queue(&MSHR.entry[mshr_index]);
            }
            else if ((cache_type == IS_L1D) && (MSHR.entry[mshr_index].type != PREFETCH)) {
                if (PROCESSED.occupancy < PROCESSED.SIZE)
                    PROCESSED.add_queue(&MSHR.entry[mshr_index]);
            }

            if(MSHR.entry[mshr_index].type == LOAD)
                MSHR.read_occupancy--;
            MSHR.remove_queue(&MSHR.entry[mshr_index]);
            MSHR.num_returned--;

            update_fill_cycle();
        }
    }
}

void CACHE::handle_writeback()
{
    // handle write
    uint32_t writeback_cpu = WQ.entry[WQ.head].cpu;
    if (writeback_cpu == NUM_CPUS)
        return;

    // handle the oldest entry
    if ((WQ.entry[WQ.head].event_cycle <= current_core_cycle[writeback_cpu]) && (WQ.occupancy > 0)) {
        int index = WQ.head;

        // access cache
        uint32_t set = get_set(WQ.entry[index].address);
        int way = check_hit(&WQ.entry[index]);
        bool force_victim = false;
#ifdef COMPRESSED_CACHE
        uint32_t compression_factor = getCF(WQ.entry[index].program_data);
        uint32_t myCF = 0;

        if(is_compressed)
        {
            set = get_set_cc(WQ.entry[index].address);
            way = check_hit_cc(&WQ.entry[index]);
            //assert(check_hit(&WQ.entry[index]) == check_hit_cc(&WQ.entry[index]));
            //Hit
            if(way >= 0) {
                if(compressed_cache_block[set][way].compressionFactor != compression_factor)
                {
                    //What if this line is dirty? We are writing the same line, so even it was dirty, the value is being overwritten.
                    invalidate_entry_cc(WQ.entry[index].address); 
                    force_victim = true;
                }
            }
        }
#endif

        if ((way >= 0) && (!force_victim)) { // writeback hit (or RFO hit for L1D)

            if (cache_type == IS_LLC) {
#ifdef COMPRESSED_CACHE
                    if(is_compressed)
                    {
                        bool found = false;
                        uint32_t myBlkId = get_blkid_cc(WQ.entry[index].address);
                        for (uint32_t cf = 0; cf < compressed_cache_block[set][way].compressionFactor; cf++) 
                        {
                            if ((compressed_cache_block[set][way].valid[cf] == 1) && (compressed_cache_block[set][way].blkId[cf] == myBlkId)) {
                                found = true;
                                myCF = cf;
                                if(compressed_cache_block[set][way].compressionFactor != compression_factor)
                                {
                                    //It's a writeback hit but it can force an eviction, so we made this a WB miss.
                                    assert(0);
                                }
                                llc_update_replacement_state_cc(writeback_cpu, set, way, cf, compressed_cache_block[set][way].full_addr[cf], WQ.entry[index].ip, 0, WQ.entry[index].type, compression_factor, 1, 0, 0);

                            }
                        }
                        assert(found);
                    }
                    else
#endif

                llc_update_replacement_state(writeback_cpu, set, way, block[set][way].full_addr, WQ.entry[index].ip, 0, WQ.entry[index].type, 1, 0, 0);

            }
            else
                update_replacement_state(writeback_cpu, set, way, block[set][way].full_addr, WQ.entry[index].ip, 0, WQ.entry[index].type, 1);

            // COLLECT STATS
            sim_hit[writeback_cpu][WQ.entry[index].type]++;
            sim_access[writeback_cpu][WQ.entry[index].type]++;

            // mark dirty
            block[set][way].dirty = 1;
#ifdef COMPRESSED_CACHE
            if(is_compressed)
                compressed_cache_block[set][way].dirty[myCF] = 1;
#endif
            if (cache_type == IS_ITLB)
                WQ.entry[index].instruction_pa = block[set][way].data;
            else if (cache_type == IS_DTLB)
                WQ.entry[index].data_pa = block[set][way].data;
            else if (cache_type == IS_STLB)
                WQ.entry[index].data = block[set][way].data;

            // check fill level
            if (WQ.entry[index].fill_level < fill_level) {

                if (WQ.entry[index].instruction) 
                    upper_level_icache[writeback_cpu]->return_data(&WQ.entry[index]);
                else // data
                    upper_level_dcache[writeback_cpu]->return_data(&WQ.entry[index]);
            }

            HIT[WQ.entry[index].type]++;
            ACCESS[WQ.entry[index].type]++;

            // remove this entry from WQ
            WQ.remove_queue(&WQ.entry[index]);
        }
        else { // writeback miss (or RFO miss for L1D)
            
            DP ( if (warmup_complete[writeback_cpu]) {
            cout << "[" << NAME << "] " << __func__ << " type: " << +WQ.entry[index].type << " miss";
            cout << " instr_id: " << WQ.entry[index].instr_id << " address: " << hex << WQ.entry[index].address;
            cout << " full_addr: " << WQ.entry[index].full_addr << dec;
            cout << " cycle: " << WQ.entry[index].event_cycle << endl; });

            if (cache_type == IS_L1D) { // RFO miss

                // check mshr
                uint8_t miss_handled = 1;
                int mshr_index = check_mshr(&WQ.entry[index]);

                if ((mshr_index == -1) && (MSHR.occupancy < MSHR_SIZE)) { // this is a new miss

                    // add it to mshr (RFO miss)
                    add_mshr(&WQ.entry[index]);

                    // add it to the next level's read queue
                    //if (lower_level) // L1D always has a lower level cache
                        lower_level->add_rq(&WQ.entry[index]);
                }
                else {
                    if ((mshr_index == -1) && (MSHR.occupancy == MSHR_SIZE)) { // not enough MSHR resource
                        
                        // cannot handle miss request until one of MSHRs is available
                        miss_handled = 0;
                        STALL[WQ.entry[index].type]++;
                    }
                    else if (mshr_index != -1) { // already in-flight miss

                        // update fill_level
                        if (WQ.entry[index].fill_level < MSHR.entry[mshr_index].fill_level)
                            MSHR.entry[mshr_index].fill_level = WQ.entry[index].fill_level;

                        // update request
                        if (MSHR.entry[mshr_index].type == PREFETCH) {
                            uint8_t  prior_returned = MSHR.entry[mshr_index].returned;
                            uint64_t prior_event_cycle = MSHR.entry[mshr_index].event_cycle;
			    MSHR.entry[mshr_index] = WQ.entry[index];

                            // in case request is already returned, we should keep event_cycle and retunred variables
                            MSHR.entry[mshr_index].returned = prior_returned;
                            MSHR.entry[mshr_index].event_cycle = prior_event_cycle;
                        }

                        MSHR_MERGED[WQ.entry[index].type]++;

                        DP ( if (warmup_complete[writeback_cpu]) {
                        cout << "[" << NAME << "] " << __func__ << " mshr merged";
                        cout << " instr_id: " << WQ.entry[index].instr_id << " prior_id: " << MSHR.entry[mshr_index].instr_id; 
                        cout << " address: " << hex << WQ.entry[index].address;
                        cout << " full_addr: " << WQ.entry[index].full_addr << dec;
                        cout << " cycle: " << WQ.entry[index].event_cycle << endl; });
                    }
                    else { // WE SHOULD NOT REACH HERE
                        cerr << "[" << NAME << "] MSHR errors" << endl;
                        assert(0);
                    }
                }

                if (miss_handled) {

                    MISS[WQ.entry[index].type]++;
                    ACCESS[WQ.entry[index].type]++;

                    // remove this entry from WQ
                    WQ.remove_queue(&WQ.entry[index]);
                }

            }
            else {
                // find victim
                uint32_t set = get_set(WQ.entry[index].address), way;
                uint32_t evicted_cf = 0;
                if (cache_type == IS_LLC) {
#ifdef COMPRESSED_CACHE 
                    if(is_compressed)
                    {
                        set = get_set_cc(WQ.entry[index].address);
                        way = llc_find_victim_cc(writeback_cpu, WQ.entry[index].instr_id, set, compressed_cache_block[set], WQ.entry[index].ip, WQ.entry[index].full_addr, WQ.entry[index].type, compression_factor, evicted_cf);
                    }
                    else
#endif
                    way = llc_find_victim(writeback_cpu, WQ.entry[index].instr_id, set, block[set], WQ.entry[index].ip, WQ.entry[index].full_addr, WQ.entry[index].type);
                }
                else
                    way = find_victim(writeback_cpu, WQ.entry[index].instr_id, set, block[set], WQ.entry[index].ip, WQ.entry[index].full_addr, WQ.entry[index].type);

#ifdef LLC_BYPASS
                if ((cache_type == IS_LLC) && (way == LLC_WAY)) {
                    cerr << "LLC bypassing for writebacks is not allowed!" << endl;
                    assert(0);
                }
#endif

                uint8_t  do_fill = 1;
#ifdef COMPRESSED_CACHE
                if(is_compressed)
                    do_fill = evict_compressed_line(set, way, WQ.entry[index], evicted_cf);
#else
                bool evicted_block_dirty = block[set][way].dirty;
                uint64_t evicted_block_addr = block[set][way].address;

                // is this dirty?
                if (evicted_block_dirty) {

                    // check if the lower level WQ has enough room to keep this writeback request
                    if (lower_level) { 
                        if (lower_level->get_occupancy(2, evicted_block_addr) == lower_level->get_size(2, evicted_block_addr)) {

                            // lower level WQ is full, cannot replace this victim
                            do_fill = 0;
                            lower_level->increment_WQ_FULL(evicted_block_addr);
                            STALL[WQ.entry[index].type]++;

                            DP ( if (warmup_complete[writeback_cpu]) {
                            cout << "[" << NAME << "] " << __func__ << "do_fill: " << +do_fill;
                            cout << " lower level wq is full!" << " fill_addr: " << hex << WQ.entry[index].address;
                            cout << " victim_addr: " << evicted_block_addr << dec << endl; });
                        }
                        else { 
                            PACKET writeback_packet;

                            writeback_packet.fill_level = fill_level << 1;
                            writeback_packet.cpu = writeback_cpu;
                            writeback_packet.address = block[set][way].address;
                            writeback_packet.full_addr = block[set][way].full_addr;
                            writeback_packet.data = block[set][way].data;
                            memcpy( writeback_packet.program_data, block[set][way].program_data, CACHE_LINE_BYTES);
                            writeback_packet.instr_id = WQ.entry[index].instr_id;
                            writeback_packet.ip = 0;
                            writeback_packet.type = WRITEBACK;
                            writeback_packet.event_cycle = current_core_cycle[writeback_cpu];

                            lower_level->add_wq(&writeback_packet);
                        }
                    }
#ifdef SANITY_CHECK
                    else {
                        // sanity check
                        if (cache_type != IS_STLB)
                            assert(0);
                    }
#endif
                }
#endif

                if (do_fill) {
                    // update prefetcher
                    if (cache_type == IS_L1D)
                        l1d_prefetcher_cache_fill(WQ.entry[index].full_addr, set, way, 0, block[set][way].full_addr);
                    else if (cache_type == IS_L2C)
                        l2c_prefetcher_cache_fill(WQ.entry[index].full_addr, set, way, 0, block[set][way].full_addr);

                    // update replacement policy
                    if (cache_type == IS_LLC) {
#ifdef COMPRESSED_CACHE
                    if(is_compressed)
                        llc_update_replacement_state_cc(writeback_cpu, set, way, evicted_cf, WQ.entry[index].full_addr, WQ.entry[index].ip, compressed_cache_block[set][way].full_addr[evicted_cf], WQ.entry[index].type, compression_factor, 0, 0, 0);
                    else
#endif

                        llc_update_replacement_state(writeback_cpu, set, way, WQ.entry[index].full_addr, WQ.entry[index].ip, block[set][way].full_addr, WQ.entry[index].type, 0, 0, 0);

                    }
                    else
                        update_replacement_state(writeback_cpu, set, way, WQ.entry[index].full_addr, WQ.entry[index].ip, block[set][way].full_addr, WQ.entry[index].type, 0);

                    // COLLECT STATS
                    sim_miss[writeback_cpu][WQ.entry[index].type]++;
                    sim_access[writeback_cpu][WQ.entry[index].type]++;

                    #ifdef COMPRESSED_CACHE
                    if(is_compressed)
                        fill_cache_cc(set, way, evicted_cf, &WQ.entry[index]);
                    else
                    #endif
                    fill_cache(set, way, &WQ.entry[index]);

                    // mark dirty
                    block[set][way].dirty = 1; 
#ifdef COMPRESSED_CACHE
                    if(is_compressed)
                        compressed_cache_block[set][way].dirty[evicted_cf] = 1;
#endif
                    // check fill level
                    if (WQ.entry[index].fill_level < fill_level) {

                        if (WQ.entry[index].instruction) 
                            upper_level_icache[writeback_cpu]->return_data(&WQ.entry[index]);
                        else // data
                            upper_level_dcache[writeback_cpu]->return_data(&WQ.entry[index]);
                    }

                    MISS[WQ.entry[index].type]++;
                    ACCESS[WQ.entry[index].type]++;

                    // remove this entry from WQ
                    WQ.remove_queue(&WQ.entry[index]);
                }
            }
        }
    }
}

void CACHE::handle_read()
{
    // handle read
    uint32_t read_cpu = RQ.entry[RQ.head].cpu;
    if (read_cpu == NUM_CPUS)
        return;

    for (uint32_t i=0; i<MAX_READ; i++) {

        // handle the oldest entry
        if ((RQ.entry[RQ.head].event_cycle <= current_core_cycle[read_cpu]) && (RQ.occupancy > 0)) {
            int index = RQ.head;

            // access cache
            uint32_t set = get_set(RQ.entry[index].address);
            int way = check_hit(&RQ.entry[index]);
#ifdef COMPRESSED_CACHE
            if(is_compressed)
            {
                set = get_set_cc(RQ.entry[index].address);
                way = check_hit_cc(&RQ.entry[index]);
                //assert(check_hit(&RQ.entry[index]) == check_hit_cc(&RQ.entry[index]));
            }
#endif        

            
            if (way >= 0) { // read hit

                if (cache_type == IS_ITLB) {
                    RQ.entry[index].instruction_pa = block[set][way].data;
                    if (PROCESSED.occupancy < PROCESSED.SIZE)
                        PROCESSED.add_queue(&RQ.entry[index]);
                }
                else if (cache_type == IS_DTLB) {
                    RQ.entry[index].data_pa = block[set][way].data;
                    if (PROCESSED.occupancy < PROCESSED.SIZE)
                        PROCESSED.add_queue(&RQ.entry[index]);
                }
                else if (cache_type == IS_STLB) 
                    RQ.entry[index].data = block[set][way].data;
                else if (cache_type == IS_L1I) {
                    if (PROCESSED.occupancy < PROCESSED.SIZE)
                        PROCESSED.add_queue(&RQ.entry[index]);
                }
                //else if (cache_type == IS_L1D) {
                else if ((cache_type == IS_L1D) && (RQ.entry[index].type != PREFETCH)) {
                    if (PROCESSED.occupancy < PROCESSED.SIZE)
                        PROCESSED.add_queue(&RQ.entry[index]);
                }

                // update prefetcher on load instruction
                if (RQ.entry[index].type == LOAD) {
                    if (cache_type == IS_L1D) 
                        l1d_prefetcher_operate(block[set][way].full_addr, RQ.entry[index].ip, 1, RQ.entry[index].type);
                    else if (cache_type == IS_L2C)
                        l2c_prefetcher_operate(block[set][way].full_addr, RQ.entry[index].ip, 1, RQ.entry[index].type);
                }

                // update replacement policy
                if (cache_type == IS_LLC) {
#ifdef COMPRESSED_CACHE
                    if(is_compressed)
                    {
                        bool found = false;
                        uint32_t myBlkId = get_blkid_cc(RQ.entry[index].address);
                        for (uint32_t cf = 0; cf < compressed_cache_block[set][way].compressionFactor; cf++) {
                            if ((compressed_cache_block[set][way].valid[cf] == 1) && (compressed_cache_block[set][way].blkId[cf] == myBlkId)) {
                                found = true;
                                //Compression factor should not change because this is a read
                                llc_update_replacement_state_cc(read_cpu, set, way, cf, compressed_cache_block[set][way].full_addr[cf], RQ.entry[index].ip, 0, RQ.entry[index].type, compressed_cache_block[set][way].compressionFactor, 1, 0, 0);
                                
                            }
                        }
                        assert(found);
                    }
                    else
#endif
                    llc_update_replacement_state(read_cpu, set, way, block[set][way].full_addr, RQ.entry[index].ip, 0, RQ.entry[index].type, 1, 0, 0);

                }
                else
                    update_replacement_state(read_cpu, set, way, block[set][way].full_addr, RQ.entry[index].ip, 0, RQ.entry[index].type, 1);

                // COLLECT STATS
                sim_hit[read_cpu][RQ.entry[index].type]++;
                sim_access[read_cpu][RQ.entry[index].type]++;

                // check fill level
                if (RQ.entry[index].fill_level < fill_level) {

                    if (RQ.entry[index].instruction) 
                        upper_level_icache[read_cpu]->return_data(&RQ.entry[index]);
                    else // data
                        upper_level_dcache[read_cpu]->return_data(&RQ.entry[index]);
                }

                // update prefetch stats and reset prefetch bit
                if (block[set][way].prefetch) {
                    pf_useful++;
                    block[set][way].prefetch = 0;
                }
                block[set][way].used = 1;

                HIT[RQ.entry[index].type]++;
                ACCESS[RQ.entry[index].type]++;
                
                // remove this entry from RQ
                RQ.remove_queue(&RQ.entry[index]);
            }
            else { // read miss

                DP ( if (warmup_complete[read_cpu]) {
                cout << "[" << NAME << "] " << __func__ << " read miss";
                cout << " instr_id: " << RQ.entry[index].instr_id << " address: " << hex << RQ.entry[index].address;
                cout << " full_addr: " << RQ.entry[index].full_addr << dec;
                cout << " cycle: " << RQ.entry[index].event_cycle << endl; });

                // check mshr
                uint8_t miss_handled = 1;
                int mshr_index = check_mshr(&RQ.entry[index]);

                if ((mshr_index == -1) && (MSHR.occupancy < MSHR_SIZE)) { // this is a new miss

                    // add it to mshr (read miss)
                    add_mshr(&RQ.entry[index]);

                    // add it to the next level's read queue
                    if (lower_level)
                        lower_level->add_rq(&RQ.entry[index]);
                    else { // this is the last level
                        if (cache_type == IS_STLB) {
                            // TODO: need to differentiate page table walk and actual swap

                            // emulate page table walk
                            uint64_t pa = va_to_pa(read_cpu, RQ.entry[index].instr_id, RQ.entry[index].full_addr, RQ.entry[index].address);

                            RQ.entry[index].data = pa >> LOG2_PAGE_SIZE; 
                            RQ.entry[index].event_cycle = current_core_cycle[read_cpu];

                            return_data(&RQ.entry[index]);
                        }
                    }
                }
                else {
                    if ((mshr_index == -1) && (MSHR.occupancy == MSHR_SIZE)) { // not enough MSHR resource
                        
                        // cannot handle miss request until one of MSHRs is available
                        miss_handled = 0;
                        STALL[RQ.entry[index].type]++;
                    }
                    else if (mshr_index != -1) { // already in-flight miss

                        // mark merged consumer
                        if (RQ.entry[index].type == RFO) {

                            if (RQ.entry[index].tlb_access) {
                                uint32_t sq_index = RQ.entry[index].sq_index;
                                MSHR.entry[mshr_index].store_merged = 1;
                                MSHR.entry[mshr_index].sq_index_depend_on_me.insert (sq_index);
				MSHR.entry[mshr_index].sq_index_depend_on_me.join (RQ.entry[index].sq_index_depend_on_me, SQ_SIZE);
                            }

                            if (RQ.entry[index].load_merged) {
                                //uint32_t lq_index = RQ.entry[index].lq_index; 
                                MSHR.entry[mshr_index].load_merged = 1;
                                //MSHR.entry[mshr_index].lq_index_depend_on_me[lq_index] = 1;
				MSHR.entry[mshr_index].lq_index_depend_on_me.join (RQ.entry[index].lq_index_depend_on_me, LQ_SIZE);
                            }
                        }
                        else {
                            if (RQ.entry[index].instruction) {
                                uint32_t rob_index = RQ.entry[index].rob_index;
                                MSHR.entry[mshr_index].instr_merged = 1;
                                MSHR.entry[mshr_index].rob_index_depend_on_me.insert (rob_index);

                                DP (if (warmup_complete[MSHR.entry[mshr_index].cpu]) {
                                cout << "[INSTR_MERGED] " << __func__ << " cpu: " << MSHR.entry[mshr_index].cpu << " instr_id: " << MSHR.entry[mshr_index].instr_id;
                                cout << " merged rob_index: " << rob_index << " instr_id: " << RQ.entry[index].instr_id << endl; });

                                if (RQ.entry[index].instr_merged) {
				    MSHR.entry[mshr_index].rob_index_depend_on_me.join (RQ.entry[index].rob_index_depend_on_me, ROB_SIZE);
                                    DP (if (warmup_complete[MSHR.entry[mshr_index].cpu]) {
                                    cout << "[INSTR_MERGED] " << __func__ << " cpu: " << MSHR.entry[mshr_index].cpu << " instr_id: " << MSHR.entry[mshr_index].instr_id;
                                    cout << " merged rob_index: " << i << " instr_id: N/A" << endl; });
                                }
                            }
                            else 
                            {
                                uint32_t lq_index = RQ.entry[index].lq_index;
                                MSHR.entry[mshr_index].load_merged = 1;
                                MSHR.entry[mshr_index].lq_index_depend_on_me.insert (lq_index);

                                DP (if (warmup_complete[read_cpu]) {
                                cout << "[DATA_MERGED] " << __func__ << " cpu: " << read_cpu << " instr_id: " << RQ.entry[index].instr_id;
                                cout << " merged rob_index: " << RQ.entry[index].rob_index << " instr_id: " << RQ.entry[index].instr_id << " lq_index: " << RQ.entry[index].lq_index << endl; });
				MSHR.entry[mshr_index].lq_index_depend_on_me.join (RQ.entry[index].lq_index_depend_on_me, LQ_SIZE);
                                if (RQ.entry[index].store_merged) {
                                    MSHR.entry[mshr_index].store_merged = 1;
				    MSHR.entry[mshr_index].sq_index_depend_on_me.join (RQ.entry[index].sq_index_depend_on_me, SQ_SIZE);
                                }
                            }
                        }

                        // update fill_level
                        if (RQ.entry[index].fill_level < MSHR.entry[mshr_index].fill_level)
                            MSHR.entry[mshr_index].fill_level = RQ.entry[index].fill_level;

                        // update request
                        if (MSHR.entry[mshr_index].type == PREFETCH) {
                            uint8_t  prior_returned = MSHR.entry[mshr_index].returned;
                            uint64_t prior_event_cycle = MSHR.entry[mshr_index].event_cycle;
                            MSHR.entry[mshr_index] = RQ.entry[index];
                            
                            // in case request is already returned, we should keep event_cycle and retunred variables
                            MSHR.entry[mshr_index].returned = prior_returned;
                            MSHR.entry[mshr_index].event_cycle = prior_event_cycle;
                        }

                        MSHR_MERGED[RQ.entry[index].type]++;

                        DP ( if (warmup_complete[read_cpu]) {
                        cout << "[" << NAME << "] " << __func__ << " mshr merged";
                        cout << " instr_id: " << RQ.entry[index].instr_id << " prior_id: " << MSHR.entry[mshr_index].instr_id; 
                        cout << " address: " << hex << RQ.entry[index].address;
                        cout << " full_addr: " << RQ.entry[index].full_addr << dec;
                        cout << " cycle: " << RQ.entry[index].event_cycle << endl; });
                    }
                    else { // WE SHOULD NOT REACH HERE
                        cerr << "[" << NAME << "] MSHR errors" << endl;
                        assert(0);
                    }
                }

                if (miss_handled) {
                    // update prefetcher on load instruction
                    if (RQ.entry[index].type == LOAD) {
                        if (cache_type == IS_L1D) 
                            l1d_prefetcher_operate(RQ.entry[index].full_addr, RQ.entry[index].ip, 0, RQ.entry[index].type);
                        if (cache_type == IS_L2C)
                            l2c_prefetcher_operate(RQ.entry[index].full_addr, RQ.entry[index].ip, 0, RQ.entry[index].type);
                    }

                    MISS[RQ.entry[index].type]++;
                    ACCESS[RQ.entry[index].type]++;

                    // remove this entry from RQ
                    RQ.remove_queue(&RQ.entry[index]);
                }
            }
        }
    }
}

void CACHE::handle_prefetch()
{
    // handle prefetch
    uint32_t prefetch_cpu = PQ.entry[PQ.head].cpu;
    if (prefetch_cpu == NUM_CPUS)
        return;

    for (uint32_t i=0; i<MAX_READ; i++) {

        // handle the oldest entry
        if ((PQ.entry[PQ.head].event_cycle <= current_core_cycle[prefetch_cpu]) && (PQ.occupancy > 0)) {
            int index = PQ.head;

            // access cache
            uint32_t set = get_set(PQ.entry[index].address);
            int way = check_hit(&PQ.entry[index]);
#ifdef COMPRESSED_CACHE
        if(is_compressed)
        {
            set = get_set_cc(PQ.entry[index].address);
            way = check_hit_cc(&PQ.entry[index]);
            //assert(check_hit(&PQ.entry[index]) == check_hit_cc(&PQ.entry[index]));
        }
#endif        

            bool fake_hit = false; //only llc
            if (cache_type == IS_LLC) 
                fake_hit = is_fake_hit(PQ.entry[index].full_addr);
            
            if ((way >= 0) || fake_hit) { // prefetch hit

                // update replacement policy
                if (cache_type == IS_LLC) {
                    if(way >= 0) {
#ifdef COMPRESSED_CACHE
                    if(is_compressed)
                    {
                        bool found = false;
                        uint32_t myBlkId = get_blkid_cc(RQ.entry[index].address);
                        for (uint32_t cf = 0; cf < compressed_cache_block[set][way].compressionFactor; cf++) {
                            if ((compressed_cache_block[set][way].valid[cf] == 1) && (compressed_cache_block[set][way].blkId[cf] == myBlkId)) {
                                found = true;
                                llc_update_replacement_state_cc(prefetch_cpu, set, way, cf, compressed_cache_block[set][way].full_addr[cf], PQ.entry[index].ip, 0, PQ.entry[index].type, compressed_cache_block[set][way].compressionFactor, 1, 0, 0);
                                
                            }
                        }
                        assert(found);
                    }
                    else
#endif
                        llc_update_replacement_state(prefetch_cpu, set, way, block[set][way].full_addr, PQ.entry[index].ip, 0, PQ.entry[index].type, 1, 0, 0);
                    }
                }
                else {
                    update_replacement_state(prefetch_cpu, set, way, block[set][way].full_addr, PQ.entry[index].ip, 0, PQ.entry[index].type, 1);
                    assert(!fake_hit);
                }

                // COLLECT STATS
                sim_hit[prefetch_cpu][PQ.entry[index].type]++;
                sim_access[prefetch_cpu][PQ.entry[index].type]++;

                // check fill level
                if (PQ.entry[index].fill_level < fill_level) {

                    if (PQ.entry[index].instruction) 
                        upper_level_icache[prefetch_cpu]->return_data(&PQ.entry[index]);
                    else // data
                        upper_level_dcache[prefetch_cpu]->return_data(&PQ.entry[index]);
                }

                HIT[PQ.entry[index].type]++;
                ACCESS[PQ.entry[index].type]++;
                
                // remove this entry from PQ
                PQ.remove_queue(&PQ.entry[index]);
            }
            else { // prefetch miss

                DP ( if (warmup_complete[prefetch_cpu]) {
                cout << "[" << NAME << "] " << __func__ << " prefetch miss";
                cout << " instr_id: " << PQ.entry[index].instr_id << " address: " << hex << PQ.entry[index].address;
                cout << " full_addr: " << PQ.entry[index].full_addr << dec << " fill_level: " << PQ.entry[index].fill_level;
                cout << " cycle: " << PQ.entry[index].event_cycle << endl; });

                // check mshr
                uint8_t miss_handled = 1;
                int mshr_index = check_mshr(&PQ.entry[index]);

                if ((mshr_index == -1) && (MSHR.occupancy < MSHR_SIZE)) { // this is a new miss

                    DP ( if (warmup_complete[PQ.entry[index].cpu]) {
                    cout << "[" << NAME << "_PQ] " <<  __func__ << " want to add instr_id: " << PQ.entry[index].instr_id << " address: " << hex << PQ.entry[index].address;
                    cout << " full_addr: " << PQ.entry[index].full_addr << dec;
                    cout << " occupancy: " << lower_level->get_occupancy(3, PQ.entry[index].address) << " SIZE: " << lower_level->get_size(3, PQ.entry[index].address) << endl; });

                    // first check if the lower level PQ is full or not
                    // this is possible since multiple prefetchers can exist at each level of caches
                    if (lower_level) {
                        if (cache_type == IS_LLC) {
                            if (lower_level->get_occupancy(1, PQ.entry[index].address) == lower_level->get_size(1, PQ.entry[index].address))
                                miss_handled = 0;
                            else {
                                // add it to MSHRs if this prefetch miss will be filled to this cache level
                                if (PQ.entry[index].fill_level <= fill_level)
                                    add_mshr(&PQ.entry[index]);
                                
                                lower_level->add_rq(&PQ.entry[index]); // add it to the DRAM RQ
                            }
                        }
                        else {
                            if (lower_level->get_occupancy(3, PQ.entry[index].address) == lower_level->get_size(3, PQ.entry[index].address))
                                miss_handled = 0;
                            else {
                                // add it to MSHRs if this prefetch miss will be filled to this cache level
                                if (PQ.entry[index].fill_level <= fill_level)
                                    add_mshr(&PQ.entry[index]);

                                lower_level->add_pq(&PQ.entry[index]); // add it to the DRAM RQ
                            }
                        }
                    }
                }
                else {
                    if ((mshr_index == -1) && (MSHR.occupancy == MSHR_SIZE)) { // not enough MSHR resource

                        // TODO: should we allow prefetching with lower fill level at this case?
                        
                        // cannot handle miss request until one of MSHRs is available
                        miss_handled = 0;
                        STALL[PQ.entry[index].type]++;
                    }
                    else if (mshr_index != -1) { // already in-flight miss

                        // no need to update request except fill_level
                        // update fill_level
                        if (PQ.entry[index].fill_level < MSHR.entry[mshr_index].fill_level)
                            MSHR.entry[mshr_index].fill_level = PQ.entry[index].fill_level;

                        MSHR_MERGED[PQ.entry[index].type]++;

                        DP ( if (warmup_complete[prefetch_cpu]) {
                        cout << "[" << NAME << "] " << __func__ << " mshr merged";
                        cout << " instr_id: " << PQ.entry[index].instr_id << " prior_id: " << MSHR.entry[mshr_index].instr_id; 
                        cout << " address: " << hex << PQ.entry[index].address;
                        cout << " full_addr: " << PQ.entry[index].full_addr << dec << " fill_level: " << MSHR.entry[mshr_index].fill_level;
                        cout << " cycle: " << MSHR.entry[mshr_index].event_cycle << endl; });
                    }
                    else { // WE SHOULD NOT REACH HERE
                        cerr << "[" << NAME << "] MSHR errors" << endl;
                        assert(0);
                    }
                }

                if (miss_handled) {

                    DP ( if (warmup_complete[prefetch_cpu]) {
                    cout << "[" << NAME << "] " << __func__ << " prefetch miss handled";
                    cout << " instr_id: " << PQ.entry[index].instr_id << " address: " << hex << PQ.entry[index].address;
                    cout << " full_addr: " << PQ.entry[index].full_addr << dec << " fill_level: " << PQ.entry[index].fill_level;
                    cout << " cycle: " << PQ.entry[index].event_cycle << endl; });

                    MISS[PQ.entry[index].type]++;
                    ACCESS[PQ.entry[index].type]++;

                    // remove this entry from PQ
                    PQ.remove_queue(&PQ.entry[index]);
                }
            }
        }
    }
}

void CACHE::operate()
{
    handle_fill();
    handle_writeback();
    handle_read();

    if (PQ.occupancy && (RQ.occupancy == 0))
        handle_prefetch();
}

uint32_t CACHE::get_set(uint64_t address)
{
    return (uint32_t) (address & ((1 << lg2(NUM_SET)) - 1)); 
}

#ifdef COMPRESSED_CACHE

//Note addresses passed to these methods already drop off the last 6 bits.
uint32_t CACHE::get_set_cc(uint64_t address)
{
    //Drop last 2 bits because they are included in block ID;
    return (uint32_t) ((address >> lg2(MAX_COMPRESSIBILITY)) & ((1 << lg2(NUM_SET)) - 1)); 
}

uint32_t CACHE::get_blkid_cc(uint64_t address)
{
    uint32_t blkId = (address % MAX_COMPRESSIBILITY);
    return blkId;
}

uint64_t CACHE::get_sb_tag(uint64_t address)
{
    return (address >> (lg2(MAX_COMPRESSIBILITY)+lg2(NUM_SET)));
}

static unsigned long long my_llabs ( long long x )
{
   unsigned long long t = x >> 63;
   return (x ^ t) - t;
}

long long unsigned * convertBuffer2Array (char * buffer, unsigned size, unsigned step)
{
      long long unsigned * values = (long long unsigned *) malloc(sizeof(long long unsigned) * size/step);
     //init
     unsigned int i,j; 
     for (i = 0; i < size / step; i++) {
          values[i] = 0;    // Initialize all elements to zero.
      }
      for (i = 0; i < size; i += step ){
          for (j = 0; j < step; j++){
              values[i / step] += (long long unsigned)((unsigned char)buffer[i + j]) << (8*j);
          }
      }
      return values;
}

///
/// Check if the cache line consists of only zero values
///
int isZeroPackable ( long long unsigned * values, unsigned size){
  int nonZero = 0;
  unsigned int i;
  for (i = 0; i < size; i++) {
      if( values[i] != 0){
          nonZero = 1;
          break;
      }
  }
  return !nonZero;
}

///
/// Check if the cache line consists of only same values
///
int isSameValuePackable ( long long unsigned * values, unsigned size){
  int notSame = 0;
  unsigned int i;
  for (i = 0; i < size; i++) {
      if( values[0] != values[i]){
          notSame = 1;
          break;
      }
  }
  return !notSame;
}


unsigned multBaseCompression ( long long unsigned * values, unsigned size, unsigned blimit, unsigned bsize){
    unsigned long long limit = 0;
    unsigned BASES = 3;
    //define the appropriate size for the mask
    switch(blimit){
        case 1:
            limit = 0xFF;
            break;
        case 2:
            limit = 0xFFFF;
            break;
        case 4:
            limit = 0xFFFFFFFF;
            break;
        default:
            std::cerr << "Wrong blimit value = " <<  blimit << std::endl;
            exit(1);
    }
    // finding bases: # BASES
    //std::vector<unsigned long long> mbases;
    //mbases.push_back(values[0]); //add the first base
    unsigned long long mbases [64];
    unsigned baseCount = 1;
    mbases[0] = 0;
    unsigned int i,j;
    for (i = 0; i < size; i++) {
        for(j = 0; j <  baseCount; j++){
            if( my_llabs((long long int)(mbases[j] -  values[i])) > limit ){
                //mbases.push_back(values[i]); // add new base
                bool new_base = true;
                for(uint32_t k = 0; k <  baseCount; k++){
                    if(mbases[k] == values[i])
                    {
                        new_base = false;
                        break;
                    }
                }
                if(new_base)
                    mbases[baseCount++] = values[i];  
            }
        }
        if(baseCount >= BASES) //we don't have more bases
            break;
    }
    // find how many elements can be compressed with mbases
    unsigned compCount = 0;
    for (i = 0; i < size; i++) {
        //ol covered = 0;
        for(j = 0; j <  baseCount; j++){
            if( my_llabs((long long int)(mbases[j] -  values[i])) <= limit ){
                compCount++;
                break;
            }
        }
    }

    unsigned mCompSize = blimit * compCount + bsize * (BASES-1) + (size - compCount) * bsize;
    if(compCount < size)
        return size * bsize;

    return mCompSize;
}

unsigned BDICompress (char * buffer, unsigned _blockSize)
{
    long long unsigned * values = convertBuffer2Array( buffer, _blockSize, 8);
    unsigned bestCSize = _blockSize;
    unsigned currCSize = _blockSize;
    if( isZeroPackable( values, _blockSize / 8))
        bestCSize = 1;
    if( isSameValuePackable( values, _blockSize / 8))
        currCSize = 8;

    bestCSize = bestCSize > currCSize ? currCSize: bestCSize;
    currCSize = multBaseCompression( values, _blockSize / 8, 1, 8);
    bestCSize = bestCSize > currCSize ? currCSize: bestCSize;
    currCSize = multBaseCompression( values, _blockSize / 8, 2, 8);
    bestCSize = bestCSize > currCSize ? currCSize: bestCSize;
    currCSize = multBaseCompression( values, _blockSize / 8, 4, 8);
    bestCSize = bestCSize > currCSize ? currCSize: bestCSize;
    free(values);

    values = convertBuffer2Array( buffer, _blockSize, 4);
    if( isSameValuePackable( values, _blockSize / 4))
        currCSize = 4;
    bestCSize = bestCSize > currCSize ? currCSize: bestCSize;
    currCSize = multBaseCompression( values, _blockSize / 4, 1, 4);
    bestCSize = bestCSize > currCSize ? currCSize: bestCSize;
    currCSize = multBaseCompression( values, _blockSize / 4, 2, 4);
    bestCSize = bestCSize > currCSize ? currCSize: bestCSize;
    free(values);
    values = convertBuffer2Array( buffer, _blockSize, 2);
    currCSize = multBaseCompression( values, _blockSize / 2, 1, 2);
    bestCSize = bestCSize > currCSize ? currCSize: bestCSize;
    free(values);

    //delete [] buffer;
    buffer = NULL;
    values = NULL;
    return bestCSize;
}

uint64_t CACHE::getCF(char* data, bool count)
{
    unsigned int CF = 64 / BDICompress(data, 64);
    return CF;
}

#endif

uint32_t CACHE::get_way(uint64_t address, uint32_t set)
{
    for (uint32_t way=0; way<NUM_WAY; way++) {
        if (block[set][way].valid && (block[set][way].tag == address)) 
            return way;
    }

    return NUM_WAY;
}

void CACHE::fill_cache(uint32_t set, uint32_t way, PACKET *packet)
{
#ifdef SANITY_CHECK
    if (cache_type == IS_ITLB) {
        if (packet->data == 0)
            assert(0);
    }

    if (cache_type == IS_DTLB) {
        if (packet->data == 0)
            assert(0);
    }

    if (cache_type == IS_STLB) {
        if (packet->data == 0)
            assert(0);
    }
#endif
    if (block[set][way].prefetch && (block[set][way].used == 0))
        pf_useless++;

    if (block[set][way].valid == 0)
        block[set][way].valid = 1;
    block[set][way].dirty = 0;
    block[set][way].prefetch = (packet->type == PREFETCH) ? 1 : 0;
    block[set][way].used = 0;

    if (block[set][way].prefetch)
        pf_fill++;

    block[set][way].delta = packet->delta;
    block[set][way].depth = packet->depth;
    block[set][way].signature = packet->signature;
    block[set][way].confidence = packet->confidence;

    block[set][way].tag = packet->address;
    block[set][way].address = packet->address;
    block[set][way].full_addr = packet->full_addr;
    block[set][way].data = packet->data;
    memcpy(block[set][way].program_data, packet->program_data, CACHE_LINE_BYTES);
    block[set][way].cpu = packet->cpu;
    block[set][way].instr_id = packet->instr_id;

    DP ( if (warmup_complete[packet->cpu]) {
    cout << "[" << NAME << "] " << __func__ << " set: " << set << " way: " << way;
    cout << " lru: " << block[set][way].lru << " tag: " << hex << block[set][way].tag << " full_addr: " << block[set][way].full_addr;
    cout << " data: " << block[set][way].data << dec << endl; });
}

int CACHE::check_hit(PACKET *packet)
{
    uint32_t set = get_set(packet->address);
    int match_way = -1;

    if (NUM_SET < set) {
        cerr << "[" << NAME << "_ERROR] " << __func__ << " invalid set index: " << set << " NUM_SET: " << NUM_SET;
        cerr << " address: " << hex << packet->address << " full_addr: " << packet->full_addr << dec;
        cerr << " event: " << packet->event_cycle << endl;
        assert(0);
    }

    // hit
    for (uint32_t way=0; way<NUM_WAY; way++) {
        if (block[set][way].valid && (block[set][way].tag == packet->address)) {

            match_way = way;

            DP ( if (warmup_complete[packet->cpu]) {
            cout << "[" << NAME << "] " << __func__ << " instr_id: " << packet->instr_id << " type: " << +packet->type << hex << " addr: " << packet->address;
            cout << " full_addr: " << packet->full_addr << " tag: " << block[set][way].tag << " data: " << block[set][way].data << dec;
            cout << " set: " << set << " way: " << way << " lru: " << block[set][way].lru;
            cout << " event: " << packet->event_cycle << " cycle: " << current_core_cycle[cpu] << endl; });

            break;
        }
    }

    return match_way;
}

int CACHE::invalidate_entry(uint64_t inval_addr)
{
    uint32_t set = get_set(inval_addr);
    int match_way = -1;

    if (NUM_SET < set) {
        cerr << "[" << NAME << "_ERROR] " << __func__ << " invalid set index: " << set << " NUM_SET: " << NUM_SET;
        cerr << " inval_addr: " << hex << inval_addr << dec << endl;
        assert(0);
    }

    // invalidate
    for (uint32_t way=0; way<NUM_WAY; way++) {
        if (block[set][way].valid && (block[set][way].tag == inval_addr)) {

            block[set][way].valid = 0;

            match_way = way;

            DP ( if (warmup_complete[cpu]) {
            cout << "[" << NAME << "] " << __func__ << " inval_addr: " << hex << inval_addr;  
            cout << " tag: " << block[set][way].tag << " data: " << block[set][way].data << dec;
            cout << " set: " << set << " way: " << way << " lru: " << block[set][way].lru << " cycle: " << current_core_cycle[cpu] << endl; });

            break;
        }
    }

    return match_way;
}

#ifdef COMPRESSED_CACHE

void CACHE::fill_cache_cc(uint32_t set, uint32_t way, uint32_t cf, PACKET *packet)
{
    assert(cache_type == IS_LLC);
    //assert(cf == 0);

    if (compressed_cache_block[set][way].prefetch[cf] && (compressed_cache_block[set][way].used[cf] == 0))
        pf_useless++;

    if (compressed_cache_block[set][way].valid[cf] == 0)
        compressed_cache_block[set][way].valid[cf] = 1;
    compressed_cache_block[set][way].dirty[cf] = 0;
    compressed_cache_block[set][way].prefetch[cf] = (packet->type == PREFETCH) ? 1 : 0;
    compressed_cache_block[set][way].used[cf] = 0;

    if (compressed_cache_block[set][way].prefetch[cf])
        pf_fill++;

    compressed_cache_block[set][way].sbTag = get_sb_tag(packet->address);
    compressed_cache_block[set][way].compressionFactor = getCF(packet->program_data, true);
    compressed_cache_block[set][way].blkId[cf] = get_blkid_cc(packet->address);

    compressed_cache_block[set][way].delta = packet->delta;
    compressed_cache_block[set][way].depth = packet->depth;
    compressed_cache_block[set][way].signature = packet->signature;
    compressed_cache_block[set][way].confidence = packet->confidence;

    compressed_cache_block[set][way].tag[cf] = packet->address;
    compressed_cache_block[set][way].address[cf] = packet->address;
    compressed_cache_block[set][way].full_addr[cf] = packet->full_addr;
    compressed_cache_block[set][way].data[cf] = packet->data;
    memcpy(compressed_cache_block[set][way].program_data[cf], packet->program_data, CACHE_LINE_BYTES);
    compressed_cache_block[set][way].cpu[cf] = packet->cpu;
    compressed_cache_block[set][way].instr_id[cf] = packet->instr_id;

    DP ( if (warmup_complete[packet->cpu]) {
    cout << "[" << NAME << "] " << __func__ << " set: " << set << " way: " << way;
    cout << " lru: " << compressed_cache_block[set][way].lru << " tag: " << hex << compressed_cache_block[set][way].tag[cf] << " full_addr: " << compressed_cache_block[set][way].full_addr[cf];
    cout << " CF: " << compressed_cache_block[set][way].compressionFactor;
    cout << " data: " << compressed_cache_block[set][way].data[cf] << dec << endl; });
}

int CACHE::check_hit_cc(PACKET *packet)
{
    assert(cache_type == IS_LLC);
    uint32_t set = get_set_cc(packet->address);
    //assert(set == get_set(packet->address));
    uint32_t myBlkId = get_blkid_cc(packet->address);
    int match_way = -1;

    if (NUM_SET < set) {
        cerr << "[" << NAME << "_ERROR] " << __func__ << " invalid set index: " << set << " NUM_SET: " << NUM_SET;
        cerr << " address: " << hex << packet->address << " full_addr: " << packet->full_addr << dec;
        cerr << " event: " << packet->event_cycle << endl;
        assert(0);
    }

    // hit
    for (uint32_t way=0; way<NUM_WAY; way++) {
        if (compressed_cache_block[set][way].sbTag == get_sb_tag(packet->address)) {
            for (uint32_t cf = 0; cf < compressed_cache_block[set][way].compressionFactor; cf++) {
                if ((compressed_cache_block[set][way].valid[cf] == 1) && (compressed_cache_block[set][way].blkId[cf] == myBlkId)) {

                    match_way = way;

                    DP ( if (warmup_complete[packet->cpu]) {
                        cout << "[" << NAME << "] " << __func__ << " instr_id: " << packet->instr_id << " type: " << +packet->type << hex << " addr: " << packet->address;
                        cout << " full_addr: " << packet->full_addr << " tag: " << compressed_cache_block[set][way].tag << " data: " << compressed_cache_block[set][way].data << dec;
                        cout << " set: " << set << " way: " << way << " lru: " << compressed_cache_block[set][way].lru;
                        cout << " event: " << packet->event_cycle << " cycle: " << current_core_cycle[cpu] << endl; });

                    break;
                }
            }
        }
    }

    return match_way;
}

uint8_t CACHE::evict_compressed_line(uint32_t set, uint32_t way, PACKET pkt, uint32_t& evicted_cf)
{
    assert(is_compressed);
    uint32_t num_dirty = 1;
    uint8_t do_fill = 1;

    //TODO: What if multiple blocks are dirty? Then evicted index should be 4
    if(evicted_cf >= 4)
    {
        num_dirty = 0;
        for (uint32_t cf = 0; cf < compressed_cache_block[set][way].compressionFactor; cf++) 
        {
            if ((compressed_cache_block[set][way].valid[cf] == 1) && (compressed_cache_block[set][way].dirty[cf] == 1))
                num_dirty++; 
        }
    }

    bool evicted_block_dirty = 0;
    uint64_t evicted_block_addr = 0;

    for(uint32_t i=0; i<num_dirty; i++)
    {
        uint32_t index = evicted_cf;
        if(evicted_cf < 4)
        {
            evicted_block_dirty = compressed_cache_block[set][way].dirty[evicted_cf];
            evicted_block_addr = compressed_cache_block[set][way].address[evicted_cf];
        }
        else
        {
            for (uint32_t cf = 0; cf < compressed_cache_block[set][way].compressionFactor; cf++) 
            {
                if ((compressed_cache_block[set][way].valid[cf] == 1) && (compressed_cache_block[set][way].dirty[cf] == 1))
                {
                    evicted_block_dirty = compressed_cache_block[set][way].dirty[cf];
                    evicted_block_addr = compressed_cache_block[set][way].address[cf];
                    index = cf;
                    break;
                }
            }
            assert(index < 4);
        }

        if(evicted_block_dirty)
        {
            // check if the lower level WQ has enough room to keep this writeback request
            if (lower_level) 
            {
                if (lower_level->get_occupancy(2, evicted_block_addr) == lower_level->get_size(2, evicted_block_addr)) {

                    // lower level WQ is full, cannot replace this victim
                    do_fill = 0;
                    lower_level->increment_WQ_FULL(evicted_block_addr);
                    STALL[pkt.type]++;

                    DP ( if (warmup_complete[fill_cpu]) {
                            cout << "[" << NAME << "] " << __func__ << "do_fill: " << +do_fill;
                            cout << " lower level wq is full!" << " fill_addr: " << hex << MSHR.entry[mshr_index].address;
                            cout << " victim_addr: " << evicted_block_addr << dec << endl; });
                }
                else {
                    PACKET writeback_packet;

                    writeback_packet.fill_level = fill_level << 1;
                    writeback_packet.cpu = pkt.cpu;
                    writeback_packet.address = compressed_cache_block[set][way].address[index];
                    writeback_packet.full_addr = compressed_cache_block[set][way].full_addr[index];
                    writeback_packet.data = compressed_cache_block[set][way].data[index]; 
                    memcpy(writeback_packet.program_data, compressed_cache_block[set][way].program_data[index], CACHE_LINE_BYTES); 
                    writeback_packet.instr_id = pkt.instr_id;
                    writeback_packet.ip = 0; // writeback does not have ip
                    writeback_packet.type = WRITEBACK;
                    writeback_packet.event_cycle = current_core_cycle[pkt.cpu];

                    invalidate_entry_cc(compressed_cache_block[set][way].address[index]);
                    lower_level->add_wq(&writeback_packet);
                }
            }
#ifdef SANITY_CHECK
            else {
                // sanity check
                if (cache_type != IS_STLB)
                    assert(0);
            }
#endif
        }
    }

    if(evicted_cf >= 4) 
        evicted_cf = 0;

    return do_fill;
}

int CACHE::invalidate_entry_cc(uint64_t inval_addr)
{
    uint32_t set = get_set_cc(inval_addr);
    uint32_t myBlkId = get_blkid_cc(inval_addr);
    int match_way = -1;

    if (NUM_SET < set) {
        cerr << "[" << NAME << "_ERROR] " << __func__ << " invalid set index: " << set << " NUM_SET: " << NUM_SET;
        cerr << " inval_addr: " << hex << inval_addr << dec << endl;
        assert(0);
    }

    // invalidate
    for (uint32_t way=0; way<NUM_WAY; way++) {
        if (compressed_cache_block[set][way].sbTag == get_sb_tag(inval_addr)) {
            for (uint32_t cf = 0; cf < compressed_cache_block[set][way].compressionFactor; cf++) {
                if ((compressed_cache_block[set][way].valid[cf] == 1) && (compressed_cache_block[set][way].blkId[cf] == myBlkId)) {

                    compressed_cache_block[set][way].valid[cf] = 0;

                    match_way = way;

                    DP ( if (warmup_complete[cpu]) {
                            cout << "[" << NAME << "] " << __func__ << " inval_addr: " << hex << inval_addr;  
                            cout << " tag: " << compressed_cache_block[set][way].tag << " data: " << compressed_cache_block[set][way].data << dec;
                            cout << " set: " << set << " way: " << way << " lru: " << compressed_cache_block[set][way].lru << " cycle: " << current_core_cycle[cpu] << endl; });

                    break;
                }
            }
        }
    }

    bool is_valid = false;
    for (uint32_t cf = 0; cf < compressed_cache_block[set][match_way].compressionFactor; cf++) 
    {
        if (compressed_cache_block[set][match_way].valid[cf] == 1)
        {
            is_valid = true;
            break;
        }
    }
    if(!is_valid)
        compressed_cache_block[set][match_way].compressionFactor = 0;

    return match_way;
}
#endif

int CACHE::add_rq(PACKET *packet)
{
    // check for the latest wirtebacks in the write queue
    int wq_index = WQ.check_queue(packet);
    if (wq_index != -1) {
        
        // check fill level
        if (packet->fill_level < fill_level) {
            packet->data = WQ.entry[wq_index].data;
            memcpy(packet->program_data, WQ.entry[wq_index].program_data, CACHE_LINE_BYTES);
            if (packet->instruction) 
                upper_level_icache[packet->cpu]->return_data(packet);
            else // data
                upper_level_dcache[packet->cpu]->return_data(packet);
        }

#ifdef SANITY_CHECK
        if (cache_type == IS_ITLB)
            assert(0);
        else if (cache_type == IS_DTLB)
            assert(0);
        else if (cache_type == IS_L1I)
            assert(0);
#endif
        // update processed packets
        if ((cache_type == IS_L1D) && (packet->type != PREFETCH)) {
            if (PROCESSED.occupancy < PROCESSED.SIZE)
                PROCESSED.add_queue(packet);

            DP ( if (warmup_complete[packet->cpu]) {
            cout << "[" << NAME << "_RQ] " << __func__ << " instr_id: " << packet->instr_id << " found recent writebacks";
            cout << hex << " read: " << packet->address << " writeback: " << WQ.entry[wq_index].address << dec;
            cout << " index: " << MAX_READ << " rob_signal: " << packet->rob_signal << endl; });
        }

        HIT[packet->type]++;
        ACCESS[packet->type]++;

        WQ.FORWARD++;
        RQ.ACCESS++;

        return -1;
    }

    // check for duplicates in the read queue
    int index = RQ.check_queue(packet);
    if (index != -1) {
        
        if (packet->instruction) {
            uint32_t rob_index = packet->rob_index;
            RQ.entry[index].rob_index_depend_on_me.insert (rob_index);
            RQ.entry[index].instr_merged = 1;

            DP (if (warmup_complete[packet->cpu]) {
            cout << "[INSTR_MERGED] " << __func__ << " cpu: " << packet->cpu << " instr_id: " << RQ.entry[index].instr_id;
            cout << " merged rob_index: " << rob_index << " instr_id: " << packet->instr_id << endl; });
        }
        else 
        {
            // mark merged consumer
            if (packet->type == RFO) {

                uint32_t sq_index = packet->sq_index;
                RQ.entry[index].sq_index_depend_on_me.insert (sq_index);
                RQ.entry[index].store_merged = 1;
            }
            else {
                uint32_t lq_index = packet->lq_index; 
                RQ.entry[index].lq_index_depend_on_me.insert (lq_index);
                RQ.entry[index].load_merged = 1;

                DP (if (warmup_complete[packet->cpu]) {
                cout << "[DATA_MERGED] " << __func__ << " cpu: " << packet->cpu << " instr_id: " << RQ.entry[index].instr_id;
                cout << " merged rob_index: " << packet->rob_index << " instr_id: " << packet->instr_id << " lq_index: " << packet->lq_index << endl; });
            }
        }

        RQ.MERGED++;
        RQ.ACCESS++;

        return index; // merged index
    }

    // check occupancy
    if (RQ.occupancy == RQ_SIZE) {
        RQ.FULL++;

        return -2; // cannot handle this request
    }

    // if there is no duplicate, add it to RQ
    index = RQ.tail;

#ifdef SANITY_CHECK
    if (RQ.entry[index].address != 0) {
        cerr << "[" << NAME << "_ERROR] " << __func__ << " is not empty index: " << index;
        cerr << " address: " << hex << RQ.entry[index].address;
        cerr << " full_addr: " << RQ.entry[index].full_addr << dec << endl;
        assert(0);
    }
#endif

    RQ.entry[index] = *packet;

    // ADD LATENCY
    if (RQ.entry[index].event_cycle < current_core_cycle[packet->cpu])
        RQ.entry[index].event_cycle = current_core_cycle[packet->cpu] + LATENCY;
    else
        RQ.entry[index].event_cycle += LATENCY;

    RQ.occupancy++;
    RQ.tail++;
    if (RQ.tail >= RQ.SIZE)
        RQ.tail = 0;

    DP ( if (warmup_complete[RQ.entry[index].cpu]) {
    cout << "[" << NAME << "_RQ] " <<  __func__ << " instr_id: " << RQ.entry[index].instr_id << " address: " << hex << RQ.entry[index].address;
    cout << " full_addr: " << RQ.entry[index].full_addr << dec;
    cout << " type: " << +RQ.entry[index].type << " head: " << RQ.head << " tail: " << RQ.tail << " occupancy: " << RQ.occupancy;
    cout << " event: " << RQ.entry[index].event_cycle << " current: " << current_core_cycle[RQ.entry[index].cpu] << endl; });

    if (packet->address == 0)
        assert(0);

    RQ.TO_CACHE++;
    RQ.ACCESS++;

    return -1;
}

int CACHE::add_wq(PACKET *packet)
{
    // check for duplicates in the write queue
    int index = WQ.check_queue(packet);
    if (index != -1) {

        WQ.MERGED++;
        WQ.ACCESS++;

        return index; // merged index
    }

    // sanity check
    if (WQ.occupancy >= WQ.SIZE)
        assert(0);

    // if there is no duplicate, add it to the write queue
    index = WQ.tail;
    if (WQ.entry[index].address != 0) {
        cerr << "[" << NAME << "_ERROR] " << __func__ << " is not empty index: " << index;
        cerr << " address: " << hex << WQ.entry[index].address;
        cerr << " full_addr: " << WQ.entry[index].full_addr << dec << endl;
        assert(0);
    }

    WQ.entry[index] = *packet;

    // ADD LATENCY
    if (WQ.entry[index].event_cycle < current_core_cycle[packet->cpu])
        WQ.entry[index].event_cycle = current_core_cycle[packet->cpu] + LATENCY;
    else
        WQ.entry[index].event_cycle += LATENCY;

    WQ.occupancy++;
    WQ.tail++;
    if (WQ.tail >= WQ.SIZE)
        WQ.tail = 0;

    DP (if (warmup_complete[WQ.entry[index].cpu]) {
    cout << "[" << NAME << "_WQ] " <<  __func__ << " instr_id: " << WQ.entry[index].instr_id << " address: " << hex << WQ.entry[index].address;
    cout << " full_addr: " << WQ.entry[index].full_addr << dec;
    cout << " head: " << WQ.head << " tail: " << WQ.tail << " occupancy: " << WQ.occupancy;
    cout << " data: " << hex << WQ.entry[index].data << dec;
    cout << " event: " << WQ.entry[index].event_cycle << " current: " << current_core_cycle[WQ.entry[index].cpu] << endl; });

    WQ.TO_CACHE++;
    WQ.ACCESS++;

    return -1;
}

int CACHE::prefetch_line(uint64_t ip, uint64_t base_addr, uint64_t pf_addr, int fill_level)
{
    pf_requested++;

    if (PQ.occupancy < PQ.SIZE) {
        //if ((base_addr>>LOG2_PAGE_SIZE) == (pf_addr>>LOG2_PAGE_SIZE)) {
            
            PACKET pf_packet;
            pf_packet.fill_level = fill_level;
            pf_packet.cpu = cpu;
            //pf_packet.data_index = LQ.entry[lq_index].data_index;
            //pf_packet.lq_index = lq_index;
            pf_packet.address = pf_addr >> LOG2_BLOCK_SIZE;
            pf_packet.full_addr = pf_addr;
            //pf_packet.instr_id = LQ.entry[lq_index].instr_id;
            //pf_packet.rob_index = LQ.entry[lq_index].rob_index;
            pf_packet.ip = ip;
            pf_packet.type = PREFETCH;
            pf_packet.event_cycle = current_core_cycle[cpu];

            // give a dummy 0 as the IP of a prefetch
            add_pq(&pf_packet);

            pf_issued++;

            return 1;
    //    }
    }

    return 0;
}

int CACHE::kpc_prefetch_line(uint64_t base_addr, uint64_t pf_addr, int fill_level, int delta, int depth, int signature, int confidence)
{
    if (PQ.occupancy < PQ.SIZE) {
        if ((base_addr>>LOG2_PAGE_SIZE) == (pf_addr>>LOG2_PAGE_SIZE)) {
            
            PACKET pf_packet;
            pf_packet.fill_level = fill_level;
            pf_packet.cpu = cpu;
            //pf_packet.data_index = LQ.entry[lq_index].data_index;
            //pf_packet.lq_index = lq_index;
            pf_packet.address = pf_addr >> LOG2_BLOCK_SIZE;
            pf_packet.full_addr = pf_addr;
            //pf_packet.instr_id = LQ.entry[lq_index].instr_id;
            //pf_packet.rob_index = LQ.entry[lq_index].rob_index;
            pf_packet.ip = 0;
            pf_packet.type = PREFETCH;
            pf_packet.delta = delta;
            pf_packet.depth = depth;
            pf_packet.signature = signature;
            pf_packet.confidence = confidence;
            pf_packet.event_cycle = current_core_cycle[cpu];

            // give a dummy 0 as the IP of a prefetch
            add_pq(&pf_packet);

            pf_issued++;

            return 1;
        }
    }

    return 0;
}

int CACHE::add_pq(PACKET *packet)
{
    // check for the latest wirtebacks in the write queue
    int wq_index = WQ.check_queue(packet);
    if (wq_index != -1) {
        
        // check fill level
        if (packet->fill_level < fill_level) {

            packet->data = WQ.entry[wq_index].data;
            memcpy(packet->program_data, WQ.entry[wq_index].program_data, CACHE_LINE_BYTES);
            if (packet->instruction) 
                upper_level_icache[packet->cpu]->return_data(packet);
            else // data
                upper_level_dcache[packet->cpu]->return_data(packet);
        }

        HIT[packet->type]++;
        ACCESS[packet->type]++;

        WQ.FORWARD++;
        PQ.ACCESS++;

        return -1;
    }

    // check for duplicates in the PQ
    int index = PQ.check_queue(packet);
    if (index != -1) {
        if (packet->fill_level < PQ.entry[index].fill_level)
            PQ.entry[index].fill_level = packet->fill_level;

        PQ.MERGED++;
        PQ.ACCESS++;

        return index; // merged index
    }

    // check occupancy
    if (PQ.occupancy == PQ_SIZE) {
        PQ.FULL++;

        DP ( if (warmup_complete[packet->cpu]) {
        cout << "[" << NAME << "] cannot process add_pq since it is full" << endl; });
        return -2; // cannot handle this request
    }

    // if there is no duplicate, add it to PQ
    index = PQ.tail;

#ifdef SANITY_CHECK
    if (PQ.entry[index].address != 0) {
        cerr << "[" << NAME << "_ERROR] " << __func__ << " is not empty index: " << index;
        cerr << " address: " << hex << PQ.entry[index].address;
        cerr << " full_addr: " << PQ.entry[index].full_addr << dec << endl;
        assert(0);
    }
#endif

    PQ.entry[index] = *packet;

    // ADD LATENCY
    if (PQ.entry[index].event_cycle < current_core_cycle[packet->cpu])
        PQ.entry[index].event_cycle = current_core_cycle[packet->cpu] + LATENCY;
    else
        PQ.entry[index].event_cycle += LATENCY;

    PQ.occupancy++;
    PQ.tail++;
    if (PQ.tail >= PQ.SIZE)
        PQ.tail = 0;

    DP ( if (warmup_complete[PQ.entry[index].cpu]) {
    cout << "[" << NAME << "_PQ] " <<  __func__ << " instr_id: " << PQ.entry[index].instr_id << " address: " << hex << PQ.entry[index].address;
    cout << " full_addr: " << PQ.entry[index].full_addr << dec;
    cout << " type: " << +PQ.entry[index].type << " head: " << PQ.head << " tail: " << PQ.tail << " occupancy: " << PQ.occupancy;
    cout << " event: " << PQ.entry[index].event_cycle << " current: " << current_core_cycle[PQ.entry[index].cpu] << endl; });

    if (packet->address == 0)
        assert(0);

    PQ.TO_CACHE++;
    PQ.ACCESS++;

    return -1;
}

void CACHE::return_data(PACKET *packet)
{
    // check MSHR information
    int mshr_index = check_mshr(packet);

    // sanity check
    if (mshr_index == -1) {
        cerr << "[" << NAME << "_MSHR] " << __func__ << " instr_id: " << packet->instr_id << " cannot find a matching entry!";
        cerr << " full_addr: " << hex << packet->full_addr;
        cerr << " address: " << packet->address << dec;
        cerr << " event: " << packet->event_cycle << " current: " << current_core_cycle[packet->cpu] << endl;
        assert(0);
    }

    // MSHR holds the most updated information about this request
    // no need to do memcpy
    MSHR.num_returned++;
    MSHR.entry[mshr_index].returned = COMPLETED;
    MSHR.entry[mshr_index].data = packet->data;
    memcpy( MSHR.entry[mshr_index].program_data, packet->program_data, CACHE_LINE_BYTES);
    MSHR.entry[mshr_index].latency = (current_core_cycle[packet->cpu] - MSHR.entry[mshr_index].event_cycle);
    if(MSHR.read_occupancy != 0)
        MSHR.entry[mshr_index].effective_latency += (uint64_t) ((double)(current_core_cycle[packet->cpu] - MSHR.entry[mshr_index].last_update_cycle)/(double)(MSHR.read_occupancy));
    else
        MSHR.entry[mshr_index].effective_latency += (current_core_cycle[packet->cpu] - MSHR.entry[mshr_index].last_update_cycle);


    // ADD LATENCY
    if (MSHR.entry[mshr_index].event_cycle < current_core_cycle[packet->cpu])
        MSHR.entry[mshr_index].event_cycle = current_core_cycle[packet->cpu] + LATENCY;
    else
        MSHR.entry[mshr_index].event_cycle += LATENCY;

    update_fill_cycle();

    DP (if (warmup_complete[packet->cpu]) {
    cout << "[" << NAME << "_MSHR] " <<  __func__ << " instr_id: " << MSHR.entry[mshr_index].instr_id;
    cout << " address: " << hex << MSHR.entry[mshr_index].address << " full_addr: " << MSHR.entry[mshr_index].full_addr;
    cout << " data: " << MSHR.entry[mshr_index].data << dec << " num_returned: " << MSHR.num_returned;
    cout << " index: " << mshr_index << " occupancy: " << MSHR.occupancy;
    cout << " event: " << MSHR.entry[mshr_index].event_cycle << " current: " << current_core_cycle[packet->cpu] << " next: " << MSHR.next_fill_cycle << endl; });
}

void CACHE::update_fill_cycle()
{
    // update next_fill_cycle
    uint64_t min_cycle = UINT64_MAX;
    uint32_t min_index = MSHR.SIZE;
    for (uint32_t i=0; i<MSHR.SIZE; i++) {
        if ((MSHR.entry[i].returned == COMPLETED) && (MSHR.entry[i].event_cycle < min_cycle)) {
            min_cycle = MSHR.entry[i].event_cycle;
            min_index = i;
        }

        DP (if (warmup_complete[MSHR.entry[i].cpu]) {
        cout << "[" << NAME << "_MSHR] " <<  __func__ << " checking instr_id: " << MSHR.entry[i].instr_id;
        cout << " address: " << hex << MSHR.entry[i].address << " full_addr: " << MSHR.entry[i].full_addr;
        cout << " data: " << MSHR.entry[i].data << dec << " returned: " << +MSHR.entry[i].returned << " fill_level: " << MSHR.entry[i].fill_level;
        cout << " index: " << i << " occupancy: " << MSHR.occupancy;
        cout << " event: " << MSHR.entry[i].event_cycle << " current: " << current_core_cycle[MSHR.entry[i].cpu] << " next: " << MSHR.next_fill_cycle << endl; });
    }
    
    MSHR.next_fill_cycle = min_cycle;
    MSHR.next_fill_index = min_index;
    if (min_index < MSHR.SIZE) {

        DP (if (warmup_complete[MSHR.entry[min_index].cpu]) {
        cout << "[" << NAME << "_MSHR] " <<  __func__ << " instr_id: " << MSHR.entry[min_index].instr_id;
        cout << " address: " << hex << MSHR.entry[min_index].address << " full_addr: " << MSHR.entry[min_index].full_addr;
        cout << " data: " << MSHR.entry[min_index].data << dec << " num_returned: " << MSHR.num_returned;
        cout << " event: " << MSHR.entry[min_index].event_cycle << " current: " << current_core_cycle[MSHR.entry[min_index].cpu] << " next: " << MSHR.next_fill_cycle << endl; });
    }
}

int CACHE::check_mshr(PACKET *packet)
{
    // search mshr
    for (uint32_t index=0; index<MSHR_SIZE; index++) {
        if (MSHR.entry[index].address == packet->address) {
            
            DP ( if (warmup_complete[packet->cpu]) {
            cout << "[" << NAME << "_MSHR] " << __func__ << " same entry instr_id: " << packet->instr_id << " prior_id: " << MSHR.entry[index].instr_id;
            cout << " address: " << hex << packet->address;
            cout << " full_addr: " << packet->full_addr << dec << endl; });

            return index;
        }
    }

    DP ( if (warmup_complete[packet->cpu]) {
    cout << "[" << NAME << "_MSHR] " << __func__ << " new address: " << hex << packet->address;
    cout << " full_addr: " << packet->full_addr << dec << endl; });

    DP ( if (warmup_complete[packet->cpu] && (MSHR.occupancy == MSHR_SIZE)) { 
    cout << "[" << NAME << "_MSHR] " << __func__ << " mshr is full";
    cout << " instr_id: " << packet->instr_id << " mshr occupancy: " << MSHR.occupancy;
    cout << " address: " << hex << packet->address;
    cout << " full_addr: " << packet->full_addr << dec;
    cout << " cycle: " << current_core_cycle[packet->cpu] << endl; });

    return -1;
}

void CACHE::add_mshr(PACKET *packet)
{
    uint32_t index = 0;
    bool is_demand = (packet->type == LOAD);

    // search mshr
    for (index=0; index<MSHR_SIZE; index++) {
        if (MSHR.entry[index].address == 0) {
            
            MSHR.entry[index] = *packet;
            MSHR.entry[index].returned = INFLIGHT;
            MSHR.entry[index].effective_latency = 0;
            MSHR.entry[index].last_update_cycle = current_core_cycle[packet->cpu];
            MSHR.occupancy++;
            if(is_demand)
                 MSHR.read_occupancy++;   

            DP ( if (warmup_complete[packet->cpu]) {
            cout << "[" << NAME << "_MSHR] " << __func__ << " instr_id: " << packet->instr_id;
            cout << " address: " << hex << packet->address << " full_addr: " << packet->full_addr << dec;
            cout << " index: " << index << " occupancy: " << MSHR.occupancy << endl; });

            break;
        }
    }

    if((!is_demand) || (MSHR.read_occupancy <= 1))
        return;

    for (uint32_t i=0; i<MSHR_SIZE; i++) {
        if (MSHR.entry[i].address != 0) {
            MSHR.entry[i].effective_latency += (uint64_t) ((double)(current_core_cycle[MSHR.entry[i].cpu] - MSHR.entry[i].last_update_cycle)/(double)(MSHR.read_occupancy - 1));
            MSHR.entry[i].last_update_cycle = current_core_cycle[MSHR.entry[i].cpu];
        }
    }

}

uint32_t CACHE::get_occupancy(uint8_t queue_type, uint64_t address)
{
    if (queue_type == 0)
        return MSHR.occupancy;
    else if (queue_type == 1)
        return RQ.occupancy;
    else if (queue_type == 2)
        return WQ.occupancy;
    else if (queue_type == 3)
        return PQ.occupancy;

    return 0;
}

uint32_t CACHE::get_size(uint8_t queue_type, uint64_t address)
{
    if (queue_type == 0)
        return MSHR.SIZE;
    else if (queue_type == 1)
        return RQ.SIZE;
    else if (queue_type == 2)
        return WQ.SIZE;
    else if (queue_type == 3)
        return PQ.SIZE;

    return 0;
}

void CACHE::increment_WQ_FULL(uint64_t address)
{
    WQ.FULL++;
}
