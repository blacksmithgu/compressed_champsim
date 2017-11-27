#ifndef HAWKEYEGEN_H
#define HAWKEYEGEN_H

using namespace std;

#include <iostream>

#include <math.h>
#include <set>
#include <vector>
#include <deque>
#include "lrugen.h"

struct METADATA
{
    uint64_t pc;
    bool prefetched;

    METADATA(bool _prefetched, uint64_t _pc)
    {
        pc = _pc;
        prefetched = _prefetched;
    }
};


struct HAWKEYEgen
{
    LRUgen cache_friendly_lrugen;
    vector<METADATA*> meta_data;
    uint64_t last_miss_quanta;
    uint64_t CACHE_SIZE;
    uint64_t access;
    uint64_t hit;
    uint64_t prefetch_access;
    uint64_t redundant_prefetch;

    void init(uint64_t size)
    {
        cache_friendly_lrugen.init(size);
        meta_data.clear();
        CACHE_SIZE = size;
        access = 0;
        hit = 0;
        prefetch_access = 0;
        redundant_prefetch = 0;
        last_miss_quanta = 0;
    }

    void add_access(bool cache_friendly, uint64_t curr_quanta, uint64_t last_quanta, bool prefetch)
    {
        if(prefetch)
            prefetch_access++;
        else
            access++;

        if(!cache_friendly) 
        {
            last_miss_quanta = curr_quanta;
            return;
        }

        cache_friendly_lrugen.remove_access(last_quanta);
        cache_friendly_lrugen.add_access(curr_quanta);    
    }

    void add_access(bool cache_friendly, uint64_t curr_quanta, bool prefetch)
    {
        if(prefetch)
            prefetch_access++;
        else
            access++;

        last_miss_quanta = curr_quanta;

        if(!cache_friendly)
            return;

        cache_friendly_lrugen.add_access(curr_quanta);    
    }

    void update_metadata(bool cache_friendly, uint64_t curr_quanta, METADATA* info)
    {
        if(!cache_friendly) 
            return;

        if(curr_quanta > meta_data.size())
            meta_data.resize(curr_quanta);
        assert(curr_quanta == meta_data.size());
       
        meta_data.push_back(info);
    }

    METADATA* get_evict_metadata()
    {
        int evict_pointer = cache_friendly_lrugen.get_evict_pointer();
        //cout << dec << "EP: " << evict_pointer << " " << cache_friendly_lrugen.cache_occupancy << endl;
        METADATA* info = NULL;
        if(evict_pointer != -1)
            info = meta_data[evict_pointer];
        return info;
    } 

    bool should_cache(bool was_cache_friendly, uint64_t curr_quanta, uint64_t last_quanta, bool prefetch)
    {
        if(!was_cache_friendly)
        {
            if(last_quanta == last_miss_quanta) // We have only seen hits since, and this is non-bypassing cache
            {
                if(prefetch)
                    redundant_prefetch++;
                else
                    hit++;
                return true;
            }

            return false; 
        }

        bool is_cache = cache_friendly_lrugen.should_cache(curr_quanta, last_quanta);
        if(is_cache)
        {
            if(prefetch)
                redundant_prefetch++;
            else
                hit++;
        }
        else
            last_miss_quanta = curr_quanta;

        return is_cache;    
    }

    void print()
    {   
        cache_friendly_lrugen.print();
    }
};

#endif
