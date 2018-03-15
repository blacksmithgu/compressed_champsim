//Hawkeye Cache Replacement Tool v2.0
//UT AUSTIN RESEARCH LICENSE (SOURCE CODE)
//The University of Texas at Austin has developed certain software and documentation that it desires to
//make available without charge to anyone for academic, research, experimental or personal use.
//This license is designed to guarantee freedom to use the software for these purposes. If you wish to
//distribute or make other use of the software, you may purchase a license to do so from the University of
//Texas.
///////////////////////////////////////////////
//                                            //
//     Hawkeye [Jain and Lin, ISCA' 16]       //
//     Akanksha Jain, akanksha@cs.utexas.edu  //
//                                            //
///////////////////////////////////////////////


#ifndef OPTGEN_H
#define OPTGEN_H

using namespace std;

#include <iostream>
#include "cache.h"
#include <math.h>
#include <set>
#include <vector>

struct ADDR_INFO
{
    uint64_t addr;
    uint32_t last_quanta;
    uint64_t PC; 
    bool prefetched;
    uint32_t lru;
    bool last_prediction;
    bool is_high_cost_predicted; // for Obol
    bool detrained;

    void init(unsigned int curr_quanta, bool is_next_high_cost = false)
    {
        last_quanta = 0;
        PC = 0;
        prefetched = false;
        lru = 0;
        is_high_cost_predicted = is_next_high_cost;
        detrained = false;
    }

    void update(unsigned int curr_quanta, uint64_t _pc, bool prediction, bool is_next_high_cost = false)
    {
        last_quanta = curr_quanta;
        PC = _pc;
        last_prediction = prediction;
        is_high_cost_predicted = is_next_high_cost;
        detrained = false;
    }

    void mark_prefetch()
    {
        prefetched = true;
    }
};

struct OPTgen
{
    vector<unsigned int> liveness_history;
    vector<unsigned int> liveness_history_stable;

    uint64_t num_cache;
    uint64_t per_core_num_cache[NUM_CPUS];
    uint64_t num_dont_cache;
    uint64_t access;
    uint64_t per_core_access[NUM_CPUS];
    uint64_t prefetch;
    uint64_t prefetch_cachehit;

    uint64_t CACHE_SIZE;

    void init(uint64_t size)
    {
        num_cache = 0;
        num_dont_cache = 0;
        access = 0;
        for(unsigned int i=0; i<NUM_CPUS; i++) {
            per_core_access[i] = 0;
            per_core_num_cache[i] = 0;
        }
        CACHE_SIZE = size;
        prefetch = 0;
        prefetch_cachehit = 0;
        liveness_history.assign(OPTGEN_VECTOR_SIZE, 0);
        liveness_history_stable.assign(OPTGEN_VECTOR_SIZE, 0);
    }

    void add_access(uint64_t curr_quanta, unsigned int core_id = 0)
    {
        access++;
        per_core_access[core_id]++;
        // TODO: initialize liveness with 0 or 1?
        liveness_history[curr_quanta] = 0;
        liveness_history_stable[curr_quanta] = 0;
    }

    void add_prefetch(uint64_t curr_quanta)
    {
        prefetch++;
        liveness_history[curr_quanta] = 0;
        liveness_history_stable[curr_quanta] = 0;
    }

    bool should_cache(uint64_t curr_quanta, uint64_t last_quanta, bool prefetch=false, unsigned int core_id = 0)
    {
        bool is_cache = true;

        unsigned int i = last_quanta;
        while (i != curr_quanta)
        {
            if(liveness_history[i] >= CACHE_SIZE)
            {
                is_cache = false;
                break;
            }

            i = (i+1) % liveness_history.size();
        }

        //if ((is_cache) && (last_quanta != curr_quanta))
        if ((is_cache))
        {
            i = last_quanta;
            while (i != curr_quanta)
            {
                liveness_history[i]++;
                liveness_history_stable[i]++;
                i = (i+1) % liveness_history.size();
            }
            assert(i == curr_quanta);
        }

        if(!prefetch)
        {
            if (is_cache) {
                num_cache++;    
                per_core_num_cache[core_id]++;
            }
            else num_dont_cache++;
        }
        else
        {
            if(is_cache)
                prefetch_cachehit++;
        }

        return is_cache;    
    }

    bool should_cache_probe(uint64_t curr_quanta, uint64_t last_quanta)
    {
        bool is_cache = true;

        unsigned int i = last_quanta;
        while (i != curr_quanta)
        {
            if(liveness_history[i] >= CACHE_SIZE)
            {
                is_cache = false;
                break;
            }

            i = (i+1) % liveness_history.size();
        }

        return is_cache;    
    }

    bool should_cache_tentative(uint64_t curr_quanta, uint64_t last_quanta)
    {
        bool is_cache = true;

        unsigned int i = last_quanta;
        while (i != curr_quanta)
        {
            if(liveness_history[i] >= CACHE_SIZE)
            {
                is_cache = false;
                break;
            }

            i = (i+1) % liveness_history.size();
        }

        //if ((is_cache) && (last_quanta != curr_quanta))
        if ((is_cache))
        {
            i = last_quanta;
            while (i != curr_quanta)
            {
                liveness_history[i]++;
                i = (i+1) % liveness_history.size();
            }
            assert(i == curr_quanta);
        }

        return is_cache;    
    }

    void revert_to_checkpoint()
    {
        assert(liveness_history.size() == liveness_history_stable.size());
        for(unsigned int i=0; i < liveness_history.size(); i++)
            liveness_history[i] = liveness_history_stable[i];
    }

    uint64_t get_num_opt_accesses()
    {
        //assert((num_cache+num_dont_cache) == access);
        return access;
    }

    uint64_t get_num_opt_hits()
    {
        return num_cache;

        uint64_t num_opt_misses = access - num_cache;
        return num_opt_misses;
    }

    double get_num_opt_hits(unsigned int core_id)
    {
        return per_core_num_cache[core_id];
    }

    uint64_t get_traffic()
    {
        return (prefetch - prefetch_cachehit + access - num_cache);
    }

    void print()
    {
        cout << "Liveness Info " << liveness_history.size() << endl;        
        for(unsigned int i=0 ; i < liveness_history.size(); i++)
            cout << liveness_history[i] << ", ";

        cout << endl << endl;

        cout << "Liveness Stable Info " << liveness_history_stable.size() << endl;        
        for(unsigned int i=0 ; i < liveness_history_stable.size(); i++)
            cout << liveness_history_stable[i] << ", ";

        cout << endl << endl;
    }
};

#endif
