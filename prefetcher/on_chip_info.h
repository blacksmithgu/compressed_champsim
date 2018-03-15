#ifndef __ONCHIP_H__
#define __ONCHIP_H__

#include <map>
#include <vector>
#include <assert.h>
#include "off_chip_info.h"
#include "cache.h"

using namespace std;

#define AMC_WAYS 16*16
//TODO: Set AMC_SIZE equal to the number of TLB entries * number of cache lines per page
#define AMC_SIZE STLB_SET*STLB_WAY*PAGE_SIZE/(BLOCK_SIZE)
#define STREAM_MAX_LENGTH 1024
#define STREAM_MAX_LENGTH_BITS 10

//When TLB_SYNC is defined, the data will be maintained in an on-chip cache of size AMC_SIZE
//TODO: undef TLB SYNC to try out an idealized version
#define TLB_SYNC

class OnChip_PS_Entry 
{
  public:
    unsigned int str_addr;
    bool valid;
    unsigned int confidence;
    bool tlb_resident;
    uint64_t last_access;

    OnChip_PS_Entry() {
	reset();
    }

    void reset(){
        valid = false;
        str_addr = 0;
        confidence = 0;
        tlb_resident = true;
        last_access = 0;
    }
    void set(unsigned int addr){
        //if(!cached)
        //    return;

        reset();
        str_addr = addr;
        valid = true;
        confidence = 3;
    }
    void increase_confidence(){
        assert(valid);
        confidence = (confidence == 3) ? confidence : (confidence+1);
    }
    bool lower_confidence(){
        assert(valid);
        confidence = (confidence == 0) ? confidence : (confidence-1);
        return confidence;
    }
    void mark_tlb_resident()
    {
       tlb_resident = true;
    }
    void mark_tlb_evicted()
    {
       tlb_resident = false;
    }

};

class OnChip_SP_Entry 
{
  public:
    uint64_t phy_addr;
    bool valid;
    bool tlb_resident;
    uint64_t last_access;

    void reset(){
        valid = false;
        phy_addr = 0;
        tlb_resident = true;
        last_access = 0;
    }

    void set(uint64_t addr){
    //    if(!cached)
    //        return;
        phy_addr = addr;
        valid = true;
    }
    void mark_tlb_resident()
    {
       tlb_resident = true;
    }
    void mark_tlb_evicted()
    {
       tlb_resident = false;
    }
};

class OnChipInfo
{
    uint64_t curr_timestamp;
    unsigned int num_sets;
    unsigned int indexMask;

   public:
    OffChipInfo off_chip_mapping;
    std::vector < std::map<uint64_t,OnChip_PS_Entry*> > ps_amc;
    std::vector < std::map<unsigned int,OnChip_SP_Entry*> > sp_amc;

    OnChipInfo();

    void reset();
    bool get_structural_address(uint64_t phy_addr, unsigned int& str_addr);
    bool get_physical_address(uint64_t& phy_addr, unsigned int str_addr);
    void update(uint64_t phy_addr, unsigned int str_addr);
    void invalidate(uint64_t phy_addr, unsigned int str_addr);
    void increase_confidence(uint64_t phy_addr);
    bool lower_confidence(uint64_t phy_addr);
    bool exists_off_chip(uint64_t);
    void print();
    void mark_tlb_resident(uint64_t addr);
    void mark_not_tlb_resident(uint64_t addr);

    void evict_ps_amc(unsigned int);
    void evict_sp_amc(unsigned int );
    unsigned int ps_amc_evictions;
    unsigned int sp_amc_evictions;
};


#endif
