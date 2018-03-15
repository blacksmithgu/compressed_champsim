#include "on_chip_info.h"
#include <iostream>

static inline unsigned int CRC_FloorLog2(unsigned int n)
{
    unsigned int p = 0;

    assert (n != 0);

    if (n & 0xffff0000) { p += 16; n >>= 16; }
    if (n & 0x0000ff00) { p +=  8; n >>=  8; }
    if (n & 0x000000f0) { p +=  4; n >>=  4; }
    if (n & 0x0000000c) { p +=  2; n >>=  2; }
    if (n & 0x00000002) { p +=  1; }

    return p;
}

OnChipInfo::OnChipInfo() 
{
    sp_amc_evictions = 0;
    ps_amc_evictions = 0;
    curr_timestamp = 0;
    num_sets = AMC_SIZE/AMC_WAYS;
    unsigned int indexShift = CRC_FloorLog2( num_sets );    
    indexMask  = (1 << indexShift) - 1;
    reset();
}

void OnChipInfo::reset()
{
    off_chip_mapping.reset();
    ps_amc.resize(num_sets);
    sp_amc.resize(num_sets);
    for (unsigned int i=0; i<num_sets; i++)
    {
        ps_amc[i].clear();
        sp_amc[i].clear();
    }
}

bool OnChipInfo::get_structural_address(uint64_t phy_addr, unsigned int& str_addr)
{
    curr_timestamp++;
    //cerr<<"here "<<phy_addr<<"\n";	
    unsigned int setId = (phy_addr >> 6) & indexMask;
//    std::cout << std::hex << phy_addr << std::dec << " " << setId << std::endl;
    std::map<uint64_t, OnChip_PS_Entry*>& ps_map = ps_amc[setId];
    std::map<uint64_t, OnChip_PS_Entry*>::iterator ps_iter = ps_map.find(phy_addr);
    if(ps_iter == ps_map.end()) {
	#ifdef DEBUG	
	  (*outf)<<"In get_structural address of phy_addr "<<phy_addr<<", str addr not found\n";
	#endif  
        #ifndef TLB_SYNC
            assert(0);
        if(off_chip_mapping.get_structural_address(phy_addr, str_addr))
        {
            update(phy_addr, str_addr);
            return true;
        }
        #endif
	return false;
    }
    else {
	if(ps_iter->second->valid) {
	//if(ps_iter->second->valid) {
		str_addr = ps_iter->second->str_addr;
                ps_iter->second->last_access = curr_timestamp;
	        #ifdef DEBUG    
          		(*outf)<<"In get_structural address of phy_addr "<<phy_addr<<", str addr is "<<str_addr<<"\n";
        	#endif
		return true;
	}
	else {
		#ifdef DEBUG    
          		(*outf)<<"In get_structural address of phy_addr "<<phy_addr<<", str addr not valid\n";
	        #endif
		return false;
	}
    }			
}

bool OnChipInfo::get_physical_address(uint64_t& phy_addr, unsigned int str_addr)
{
    curr_timestamp++;

    unsigned int setId = str_addr & indexMask;
    std::map<unsigned int, OnChip_SP_Entry*>& sp_map = sp_amc[setId];

    std::map<unsigned int, OnChip_SP_Entry*>::iterator sp_iter = sp_map.find(str_addr);
    if(sp_iter == sp_map.end()) {
        #ifdef DEBUG    
          (*outf)<<"In get_physical_address of str_addr "<<str_addr<<", phy addr not found\n";
        #endif
        #ifndef TLB_SYNC
            assert(0);
        if(off_chip_mapping.get_physical_address(phy_addr, str_addr))
        {
            update(phy_addr, str_addr);
            return true;
        }
        #endif

	return false;
    }
    else {
	if(sp_iter->second->valid) {
		phy_addr = sp_iter->second->phy_addr;
                sp_iter->second->last_access = curr_timestamp;
	        #ifdef DEBUG    
        	  (*outf)<<"In get_physical_address of str_addr "<<str_addr<<", phy addr is "<<phy_addr<<"\n";
        	#endif
		return true;
	}
	else {
	        #ifdef DEBUG    
        	  std::cout <<"In get_physical_address of str_addr "<<str_addr<<", phy addr not valid\n";
	        #endif

		return false;
	}
    }
}

void OnChipInfo::evict_ps_amc(unsigned int setId)
{
    uint64_t min_timestamp = (uint64_t)(-1);
    uint64_t min_addr = 0;
    uint64_t min_ntr_timestamp = (uint64_t)(-1);
    uint64_t min_ntr_addr = 0;
    //std::cout << "Evict PC AMC: " << ps_map.size() << std::endl;

    std::map<uint64_t, OnChip_PS_Entry*>& ps_map = ps_amc[setId];

    for(std::map<uint64_t, OnChip_PS_Entry*>::iterator ps_iter = ps_map.begin(); ps_iter != ps_map.end(); ps_iter++)
    {
        if (!(ps_iter->second->tlb_resident))
        {
            if (ps_iter->second->last_access < min_ntr_timestamp)
            {
                min_ntr_timestamp = ps_iter->second->last_access;
                min_ntr_addr = ps_iter->first;
            }

        //    delete ps_iter->second;
         //   ps_map.erase(ps_iter);
          //  return;
        }
        if (ps_iter->second->last_access < min_timestamp)
        {
            min_timestamp = ps_iter->second->last_access;
            min_addr = ps_iter->first;
        }
    }

    uint64_t evict_addr = min_addr;
    if(min_ntr_addr != 0)
        evict_addr = min_ntr_addr;

    //Evict LRU line
    std::map<uint64_t, OnChip_PS_Entry*>::iterator ps_iter = ps_map.find(evict_addr);
    //std::cout << "LRU: " << std::hex << evict_addr << std::dec << std::endl;
    assert(ps_iter != ps_map.end());
    //cout << "Update off chip: " << hex << evict_addr << " " << ps_iter->second->str_addr << dec << endl;

    off_chip_mapping.update_physical(evict_addr, ps_iter->second->str_addr);
    delete ps_iter->second;
    ps_map.erase(ps_iter);

    ps_amc_evictions++;
}

void OnChipInfo::evict_sp_amc(unsigned int setId)
{
    uint64_t min_timestamp = (uint64_t)(-1);
    unsigned int min_addr = 0;
    uint64_t min_ntr_timestamp = (uint64_t)(-1);
    unsigned int min_ntr_addr = 0;
    //std::cout << "Evict SP AMC: " << sp_map.size() << std::endl;
    std::map<unsigned int, OnChip_SP_Entry*>& sp_map = sp_amc[setId];

    for(std::map<unsigned int, OnChip_SP_Entry*>::iterator sp_iter = sp_map.begin(); sp_iter != sp_map.end(); sp_iter++)
    {
        if (!(sp_iter->second->tlb_resident))
        {
            if (sp_iter->second->last_access < min_ntr_timestamp)
            {
                min_ntr_timestamp = sp_iter->second->last_access;
                min_ntr_addr = sp_iter->first;
            }
            //       std::cout << "Not TLB resident: " << std::hex << sp_iter->first << std::dec << std::endl;
            //    delete sp_iter->second;
            //   sp_map.erase(sp_iter);
            //  return;
        }
        if (sp_iter->second->last_access < min_timestamp)
        {
            min_timestamp = sp_iter->second->last_access;
            min_addr = sp_iter->first;
        }
    }

    unsigned int evict_addr = min_addr;
    if(min_ntr_addr != 0)
        evict_addr = min_ntr_addr;

    //Evict LRU line
    //std::cout << "LRU: " << std::hex << min_addr << std::dec << std::endl;
    std::map<unsigned int, OnChip_SP_Entry*>::iterator sp_iter = sp_map.find(evict_addr);
    assert(sp_iter != sp_map.end());
    off_chip_mapping.update_structural(sp_iter->second->phy_addr, evict_addr);
    delete sp_iter->second;
    sp_map.erase(sp_iter);
    
    sp_amc_evictions++;
}

void OnChipInfo::update(uint64_t phy_addr, unsigned int str_addr)
{
    #ifdef DEBUG    
         (*outf)<<"In off_chip_info update, phy_addr is "<<phy_addr<<", str_addr is "<<str_addr<<"\n";
    #endif
	
    unsigned int ps_setId = (phy_addr >> 6) & indexMask;
    std::map<uint64_t, OnChip_PS_Entry*>& ps_map = ps_amc[ps_setId];

    unsigned int sp_setId = str_addr & indexMask;
    std::map<unsigned int, OnChip_SP_Entry*>& sp_map = sp_amc[sp_setId];

    while (ps_map.size() > AMC_WAYS)
        evict_ps_amc(ps_setId);

    while (sp_map.size() > AMC_WAYS)
        evict_sp_amc(sp_setId);

    //PS Map Update
    std::map<uint64_t, OnChip_PS_Entry*>::iterator ps_iter = ps_map.find(phy_addr);
    if(ps_iter == ps_map.end()) {
	OnChip_PS_Entry* ps_entry = new OnChip_PS_Entry();
	ps_map[phy_addr] = ps_entry;
	ps_map[phy_addr]->set(str_addr);
        ps_map[phy_addr]->last_access = curr_timestamp;
    }
    else {
    	ps_iter->second->set(str_addr);
        ps_iter->second->last_access = curr_timestamp;
    }	

    //SP Map Update
    std::map<unsigned int, OnChip_SP_Entry*>::iterator sp_iter = sp_map.find(str_addr);
    if(sp_iter == sp_map.end()) {
	OnChip_SP_Entry* sp_entry = new OnChip_SP_Entry();
	sp_map[str_addr] = sp_entry;
	sp_map[str_addr]->set(phy_addr);
	sp_map[str_addr]->last_access = curr_timestamp;
    }
    else {
	sp_iter->second->set(phy_addr);
        sp_iter->second->last_access = curr_timestamp;
    }
    //std::cout << ps_map.size() << " " << sp_map.size() << std::endl;	
}

void OnChipInfo::invalidate(uint64_t phy_addr, unsigned int str_addr)
{
    unsigned int ps_setId = (phy_addr >> 6) & indexMask;
    std::map<uint64_t, OnChip_PS_Entry*>& ps_map = ps_amc[ps_setId];

    unsigned int sp_setId = str_addr & indexMask;
    std::map<unsigned int, OnChip_SP_Entry*>& sp_map = sp_amc[sp_setId];
    #ifdef DEBUG    
         (*outf)<<"In off_chip_info invalidate, phy_addr is "<<phy_addr<<", str_addr is "<<str_addr<<"\n";
    #endif
    //PS Map Invalidate
    std::map<uint64_t, OnChip_PS_Entry*>::iterator ps_iter = ps_map.find(phy_addr);
    if(ps_iter != ps_map.end()) {
	ps_iter->second->reset();
	delete ps_iter->second;
	ps_map.erase(ps_iter);
    }
    else {
	//TODO TBD
    }

    //SP Map Invalidate
    std::map<unsigned int, OnChip_SP_Entry*>::iterator sp_iter = sp_map.find(str_addr);
    if(sp_iter != sp_map.end()) {
	sp_iter->second->reset();
	delete sp_iter->second;
	sp_map.erase(sp_iter);
    }
    else {
	//TODO TBD
    }
}

void OnChipInfo::increase_confidence(uint64_t phy_addr)
{
    unsigned int ps_setId = (phy_addr >> 6) & indexMask;
    std::map<uint64_t, OnChip_PS_Entry*>& ps_map = ps_amc[ps_setId];
    #ifdef DEBUG    
         (*outf)<<"In off_chip_info increase_confidence, phy_addr is "<<phy_addr<<"\n";
    #endif
    std::map<uint64_t, OnChip_PS_Entry*>::iterator ps_iter = ps_map.find(phy_addr);
    if(ps_iter != ps_map.end()) {
	ps_iter->second->increase_confidence();
    }
    else {
//	assert(0);
    }
}

bool OnChipInfo::lower_confidence(uint64_t phy_addr)
{
    bool ret = false;

    unsigned int ps_setId = (phy_addr >> 6) & indexMask;
    std::map<uint64_t, OnChip_PS_Entry*>& ps_map = ps_amc[ps_setId];
    #ifdef DEBUG    
         (*outf)<<"In off_chip_info lower_confidence, phy_addr is "<<phy_addr<<"\n";
    #endif
	
    std::map<uint64_t, OnChip_PS_Entry*>::iterator ps_iter = ps_map.find(phy_addr);
    if(ps_iter != ps_map.end()) {
	ret = ps_iter->second->lower_confidence();
    }
    else {
//	assert(0);
    }
    return ret;
}

void OnChipInfo::mark_not_tlb_resident(uint64_t phy_addr)
{
    unsigned int ps_setId = (phy_addr >> 6) & indexMask;
    std::map<uint64_t, OnChip_PS_Entry*>& ps_map = ps_amc[ps_setId];

    std::map<uint64_t, OnChip_PS_Entry*>::iterator ps_iter = ps_map.find(phy_addr);
    if(ps_iter != ps_map.end() )
    {
        unsigned int str_addr = ps_iter->second->str_addr;

        unsigned int sp_setId = str_addr & indexMask;
        std::map<unsigned int, OnChip_SP_Entry*>& sp_map = sp_amc[sp_setId];
        std::map<unsigned int, OnChip_SP_Entry*>::iterator sp_iter = sp_map.find(str_addr);
        //assert(sp_iter != sp_map.end());
        if(sp_iter != sp_map.end())
            sp_iter->second->mark_tlb_evicted();
        ps_iter->second->mark_tlb_evicted();
    }

}

void OnChipInfo::mark_tlb_resident(uint64_t phy_addr)
{
    unsigned int ps_setId = (phy_addr >> 6) & indexMask;
    std::map<uint64_t, OnChip_PS_Entry*>& ps_map = ps_amc[ps_setId];

    std::map<uint64_t, OnChip_PS_Entry*>::iterator ps_iter = ps_map.find(phy_addr);
    if(ps_iter != ps_map.end() )
    {
        unsigned int str_addr = ps_iter->second->str_addr;

        unsigned int sp_setId = str_addr & indexMask;
        std::map<unsigned int, OnChip_SP_Entry*>& sp_map = sp_amc[sp_setId];
        std::map<unsigned int, OnChip_SP_Entry*>::iterator sp_iter = sp_map.find(str_addr);
        //assert(sp_iter != sp_map.end());
        if(sp_iter != sp_map.end())
            sp_iter->second->mark_tlb_resident();
        ps_iter->second->mark_tlb_resident();
    }

}


void OnChipInfo::print()
{
/*    for(std::map<unsigned int, OnChip_SP_Entry*>::iterator it = sp_map.begin(); it != sp_map.end(); it++)
    {
	if(it->second->valid) 
            std::cout << std::hex << it->first << "  " << (it->second)->phy_addr << std::endl;
    }
*/
}
