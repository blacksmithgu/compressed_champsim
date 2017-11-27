#ifndef OPTGEN_H
#define OPTGEN_H

using namespace std;

#include <iostream>

#include <math.h>
#include <set>
#include <vector>
#include <deque>

struct ADDR_INFO
{
    uint64_t last_quanta;
    uint64_t PC; 
    bool prefetched;

    void init(uint64_t curr_quanta)
    {
        last_quanta = 0;
        PC = 0;
        prefetched = false;
    }

    void update(uint64_t curr_quanta, uint64_t _pc)
    {
        last_quanta = curr_quanta;
        PC = _pc;
    }

    void mark_prefetch()
    {
        prefetched = true;
    }
};

struct MRUgen
{
    vector<unsigned int> liveness_history;

    uint64_t num_cache;
    uint64_t num_dont_cache;
    uint64_t access;

    uint64_t CACHE_SIZE;

    void init(uint64_t size)
    {
        num_cache = 0;
        num_dont_cache = 0;
        access = 0;
        CACHE_SIZE = size;
        liveness_history.resize(1, 0); // Start index from 1
    }

    void add_access(uint64_t curr_quanta, bool hit)
    {
        access++;
        assert(curr_quanta == liveness_history.size());
        assert(liveness_history.back() <= CACHE_SIZE);
        liveness_history.push_back(liveness_history.back());

        // We still have space in the cache so let's go ahead and give this line space for future
        if(!hit && liveness_history[curr_quanta] < CACHE_SIZE) 
            liveness_history[curr_quanta]++;
        else if(!hit && (liveness_history[curr_quanta] == CACHE_SIZE))
        {
            // Since the cache is full, the previous line will be booted out because we are not bypassing
            assert(liveness_history[curr_quanta-1] == CACHE_SIZE);
            liveness_history[curr_quanta-1]++;  // Make sure this line is a miss in the future
        }
    }

    bool should_cache(uint64_t curr_quanta, uint64_t last_quanta)
    {
        bool is_cache = true;

        if(liveness_history[last_quanta] <= CACHE_SIZE) // We already alloted cache capacity for this line!
        {
            is_cache = true;
            num_cache++;
        }
        else
        {
            is_cache = false;
            num_dont_cache++;
        }

        return is_cache;    
    }


    uint64_t get_num_mru_accesses()
    {
        return access;
    }

    uint64_t get_num_mru_hits()
    {
        return num_cache;
    }

    void print()
    {
        for(unsigned int i=0 ; i<liveness_history.size(); i++)
            cout << liveness_history[i] << " ";
        
        cout << endl << endl;
    }
};

#endif
