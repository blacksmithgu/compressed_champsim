#ifndef HAWKEYEGEN_H
#define HAWKEYEGEN_H

using namespace std;

#include <iostream>

#include <math.h>
#include <set>
#include <vector>

struct METADATA
{
    uint64_t pc;
    bool prefetched;

    /*METADATA(bool _prefetched, uint64_t _pc)
    {
        pc = _pc;
        prefetched = _prefetched;
    }*/
};


struct HAWKEYEgen
{
    vector<uint64_t> tag;
    vector<uint32_t> rrpv;  
    vector<int> epoch;
    vector<METADATA> metadata;  
    uint64_t evict_addr;
    
    uint64_t CACHE_SIZE;
    uint64_t access;
    uint64_t hit;
    uint64_t prefetch_access;
    uint64_t redundant_prefetch;
    uint64_t maxRRPV;

    void init(uint64_t size)
    {
        rrpv.resize(size, 7);
        tag.resize(size, 0);
        epoch.resize(size, -1);
        metadata.resize(size);
        evict_addr = 0;

        CACHE_SIZE = size;
        access = 0;
        hit = 0;
        prefetch_access = 0;
        redundant_prefetch = 0;
        maxRRPV = 7;

        assert(tag.size() == 16);
        assert(rrpv.size() == 16);

        for (uint32_t j=0; j<size; j++) {
            rrpv[j] = maxRRPV;
            metadata[j].pc = 0;
            metadata[j].prefetched = false;
        }

    }

    void update(uint32_t index, uint64_t paddr, bool cache_friendly, bool prefetch, uint64_t pc, bool hit, uint32_t _epoch)
    {
        assert(index >= 0);
        assert(index < CACHE_SIZE);

        if(prefetch)
            prefetch_access++;
        else
            access++;

        tag[index] = paddr;
        epoch[index] = _epoch;

        if(!cache_friendly)
            rrpv[index] =7;
        else
        {
            rrpv[index] = 0;
            if(!hit)
            {
                bool saturated = false;
                for(uint32_t i=0; i<CACHE_SIZE; i++)
                    if (rrpv[i] == maxRRPV-1)
                        saturated = true;

                //Age all the cache-friendly  lines
                for(uint32_t i=0; i<CACHE_SIZE; i++)
                {
                    if (!saturated && rrpv[i] < (maxRRPV-1))
                        rrpv[i]++;
                }
            }
            rrpv[index] = 0;
        }

        metadata[index].pc = pc;
        if(prefetch)
        {
            if(!hit)
                metadata[index].prefetched = true;
        }
        else
            metadata[index].prefetched = false;
    }



    void update_wb(uint32_t index, uint64_t paddr, uint32_t _epoch)
    {
        assert(index >= 0);
        assert(index < CACHE_SIZE);

        tag[index] = paddr;
        epoch[index] = _epoch;

        metadata[index].prefetched = false;
    }

    uint32_t get_victim(METADATA*& info, int& feedback_epoch)
    {
        bool cache_averse_found = false;
        int evict_index = -1;
        for (uint32_t i=0; i<CACHE_SIZE; i++)
        {
            if (rrpv[i] == maxRRPV)
            {
                evict_index = i;
                cache_averse_found = true;
                break;
            }
        }

        if(!cache_averse_found)
        {
            uint32_t max_rrip = 0;
            for (uint32_t i=0; i<CACHE_SIZE; i++)
            {
                if (rrpv[i] >= max_rrip)
                {
                    max_rrip = rrpv[i];
                    evict_index = i;
                }
            }
            assert(evict_index != -1);
            info = &(metadata[evict_index]);
        }

        assert(evict_index >= 0);
        assert(evict_index < (int)CACHE_SIZE);
        feedback_epoch = epoch[evict_index];

        return evict_index;
    }

    int check_hit(uint64_t paddr, uint32_t type, int& feedback_epoch)
    {
        for(uint32_t i=0; i<tag.size(); i++)
        {
            if(paddr == tag[i])
            {
                feedback_epoch = epoch[i];
                if(type == PREFETCH)
                    redundant_prefetch++;
                else if(type != WRITEBACK)
                    hit++;

                return i; 
            }
        }

        return -1;
    }

    void print()
    {   
//        cache_friendly_lrugen.print();
    }
};

#endif
