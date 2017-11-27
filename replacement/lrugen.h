#ifndef LRUGEN_H
#define LRUGEN_H

using namespace std;

#include <iostream>

#include <math.h>
#include <set>
#include <vector>
#include <deque>

struct LRUADDR_INFO
{
    uint64_t last_prefetch_quanta;
    uint64_t last_quanta;
    uint64_t last_demand_quanta;
    uint64_t PC; 
    uint64_t prefetchPC; 
    bool prefetched;
    bool prediction;

    void init(uint64_t curr_quanta)
    {
        last_prefetch_quanta = 0;
        last_quanta = 0;
        last_demand_quanta = 0;
        PC = 0;
        prefetchPC = 0;
        prefetched = false;
    }

    void update(uint64_t curr_quanta, uint64_t _pc, bool _prediction, bool update_demand=true)
    {
        last_quanta = curr_quanta;
        last_prefetch_quanta = 0;
        PC = _pc;
        prefetched = false;
        prediction = _prediction;
        if(update_demand)
            last_demand_quanta = curr_quanta;
    }

    void update_prefetch(uint64_t curr_quanta, uint64_t _pc, bool _prediction)
    {
        last_prefetch_quanta = curr_quanta;
        prefetchPC = _pc;
        prefetched = true;
        prediction = _prediction;
    }
};

struct LRUgen
{
    vector<unsigned int> access_history;

    uint64_t cache_occupancy;
    uint64_t evict_pointer;
    uint64_t num_cache;
    uint64_t num_dont_cache;
    uint64_t access;
    uint64_t prefetch;
    uint64_t redundant_prefetch;

    uint64_t CACHE_SIZE;

    void init(uint64_t size)
    {
        num_cache = 0;
        num_dont_cache = 0;
        access = 0;
        prefetch = 0;
        redundant_prefetch = 0;
        CACHE_SIZE = size;
        access_history.resize(1, 0); // Start index from 1
        evict_pointer = 0;
        cache_occupancy = 0;
    }

    void add_prefetch(uint64_t curr_quanta, uint64_t last_quanta, uint64_t degree)
    {
        assert(last_quanta < access_history.size());
        assert(access_history[last_quanta] > 0);
        //access_history[last_quanta]--;
        access_history[last_quanta] = 0;

        add_prefetch(curr_quanta, degree);    
    }
    
    void remove_access(uint64_t last_quanta)
    {
        assert(cache_occupancy > 0);
        cache_occupancy--;
        assert(last_quanta < access_history.size());
        assert(access_history[last_quanta] > 0);
        access_history[last_quanta]--;
    }

    void add_access(uint64_t curr_quanta, uint64_t last_quanta)
    {
        remove_access(last_quanta);

        add_access(curr_quanta);    
    }

    void add_access(uint64_t curr_quanta)
    {
        access++;
        if(curr_quanta > access_history.size())
            access_history.resize(curr_quanta, 0);
        assert(curr_quanta == access_history.size());
        access_history.push_back(1);
        cache_occupancy++;
        update_evict_pointer();
    }

    void add_prefetch(uint64_t curr_quanta, uint64_t degree)
    {
        //prefetch++;
        prefetch += degree;
        if(curr_quanta > access_history.size())
            access_history.resize(curr_quanta, 0);
        assert(curr_quanta == access_history.size());
        //access_history.push_back(1);
        access_history.push_back(degree);
        cache_occupancy += degree;
        update_evict_pointer();
    }

    void update_evict_pointer()
    {
        if(cache_occupancy < CACHE_SIZE)
            return;
        //assert(access_history[evict_pointer] == 1);
        for(uint64_t i=evict_pointer; i<access_history.size(); i++)
        {
            if(access_history[i] == 1)
            {
                evict_pointer = i;
                break;
            }
        }
    }

    int get_evict_pointer()
    {
        if(cache_occupancy < CACHE_SIZE)
            return -1;
        return evict_pointer;
    }

    uint64_t get_last_cache_resident_index()
    {   
        if(access_history.size() == 0)
            return 0;

        uint64_t last_cache_resident_index = 0;
        uint64_t num_outstanding = 0;
        for(int i=access_history.size()-1; i>=0; i--)
        {
            num_outstanding += access_history[i];
            if(num_outstanding == CACHE_SIZE)
            {
                last_cache_resident_index = i;
                break;
            }
        }

        return last_cache_resident_index;
    }

    bool should_cache(uint64_t curr_quanta, uint64_t last_quanta)
    {
        bool is_cache = true;
        uint64_t num_outstanding = 0;

        if(curr_quanta > access_history.size())
            access_history.resize(curr_quanta, 0);
        assert(curr_quanta == access_history.size());
        for(uint64_t i=last_quanta; i<curr_quanta; i++)
            num_outstanding += access_history[i];

        if(num_outstanding <= CACHE_SIZE)
        {
            is_cache = true;    
            //num_cache++;
            num_cache += access_history[last_quanta];
        }
        else
        {   
            is_cache = false;
            num_dont_cache++;
        }

        return is_cache;    
    }

    bool should_cache_prefetch(uint64_t curr_quanta, uint64_t last_quanta, uint64_t degree)
    {
        bool is_cache = true;
        uint64_t num_outstanding = 0;

        for(uint64_t i=last_quanta; i<curr_quanta; i++)
            num_outstanding += access_history[i];

        if(num_outstanding <= CACHE_SIZE)
        {
            is_cache = true;    
            //redundant_prefetch++;
            redundant_prefetch += degree;
        }
        else
        {   
            is_cache = false;
        }

        return is_cache;    
    }

    uint64_t get_reuse_distance(uint64_t curr_quanta, uint64_t last_quanta)
    {
        if(curr_quanta > access_history.size())
            access_history.resize(curr_quanta, 0);
        assert(access_history.size() >= curr_quanta);
        uint64_t num_outstanding = 0;

        for(uint64_t i=last_quanta; i<curr_quanta; i++)
            num_outstanding += access_history[i];

        //assert(num_outstanding <= CACHE_SIZE);
        return num_outstanding;
    }

    uint64_t get_num_lru_accesses()
    {
        return access;
    }

    uint64_t get_num_lru_hits()
    {
        return num_cache;
    }

    void print()
    {
        for(unsigned int i=0 ; i<access_history.size(); i++)
            cout << access_history[i] << " ";
        
        cout << endl << endl;
    }
};

#endif
