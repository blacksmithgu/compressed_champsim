#ifndef HAWKEYEGEN_H
#define HAWKEYEGEN_H

using namespace std;

#include <iostream>

#include <math.h>
#include <set>
#include <vector>
#include <deque>
#include "lrugen.h"

struct HAWKEYEgen
{
    LRUgen cache_friendly_lrugen;
    uint64_t last_miss_quanta;
    uint64_t CACHE_SIZE;
    uint64_t access;
    uint64_t hit;

    void init(uint64_t size)
    {
        cache_friendly_lrugen.init(size);
        CACHE_SIZE = size;
        access = 0;
        hit = 0;
        last_miss_quanta = 0;
    }

    void add_access(bool cache_friendly, uint64_t curr_quanta, uint64_t last_quanta)
    {
        access++;
        if(!cache_friendly) 
        {
            last_miss_quanta = curr_quanta;
            return;
        }

        cache_friendly_lrugen.remove_access(last_quanta);
        cache_friendly_lrugen.add_access(curr_quanta);    
    }

    void add_access(bool cache_friendly, uint64_t curr_quanta)
    {
        access++;
        last_miss_quanta = curr_quanta;

        if(!cache_friendly)
            return;

        cache_friendly_lrugen.add_access(curr_quanta);    
    }

    bool should_cache(bool was_cache_friendly, uint64_t curr_quanta, uint64_t last_quanta)
    {
        if(!was_cache_friendly)
        {
            if(last_quanta == last_miss_quanta) // We have only seen hits since, and this is non-bypassing cache
            {
                hit++;
                return true;
            }

            return false; 
        }

        bool is_cache = cache_friendly_lrugen.should_cache(curr_quanta, last_quanta);
        if(is_cache)
            hit++;
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
