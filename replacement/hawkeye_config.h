#ifndef HAWKEYECONFIG_H
#define HAWKEYECONFIG_H

#include "optgen_simple.h"

#define MAX_SHCT 31
#define SHCT_SIZE_BITS 14
//#define SHCT_SIZE_BITS 11
#define SHCT_SIZE (1<<SHCT_SIZE_BITS)

#include "hawkeye_predictor.h"
#include "hawkeyegen_rrpv.h"


struct HawkeyeConfig
{
    uint32_t config[NUM_CPUS]; //Consider 3 choices for each CPU: NODP, Middle, ALLDP
    OPTgen perset_optgen[LLC_SETS]; // per-set occupancy vectors; we only use 64 of these
    HAWKEYE_PC_PREDICTOR* demand_predictor;  //Predictor
    HAWKEYE_PC_PREDICTOR* prefetch_predictor;  //Predictor
    HAWKEYEgen perset_hawkeyegen[LLC_SETS]; // per-set occupancy vectors; we only use 64 of these

    void init(int, int, int, int);
    void train(uint32_t set, uint32_t type, uint64_t curr_quanta, int last_quanta, bool last_prefetched, uint64_t last_pc, uint32_t cpu);
    //void simulate_hawkeye_gen(uint64_t paddr, uint32_t set, uint32_t type, uint64_t curr_quanta, int last_quanta, uint64_t pc, bool curr_prediction);
    void simulate_hawkeyegen_rrpv(uint32_t index, uint64_t paddr, uint32_t set, uint32_t type, uint64_t pc, bool curr_prediction, bool hit, uint32_t epoch);
    void simulate_hawkeyegen_rrpv_wb(uint32_t index, uint64_t paddr, uint32_t set, uint32_t epoch);
    uint32_t hawkeyegen_rrpv_find_index(uint32_t cpu, uint64_t paddr, uint32_t set, uint32_t type, METADATA*& detrain_info, bool& hit, uint32_t& feedback_epoch);
    void hawkeye_detrain(bool, uint64_t);
    bool predict(uint32_t set, uint32_t type, uint64_t ip);
    bool dp_policy(uint32_t cpu, uint32_t rd);
    double get_hit_rate(uint32_t cpu);
    double get_bw_usage(double);
    double get_improvement(double* baseline_hit_rate, double baseline_bw_usage, double dram_latency, double available_bw);
    void reset_stats(); 
    void new_epoch(uint32_t epoch_num);

    vector<vector<double> > hits;
    vector<vector<double> > accesses;
    vector<vector<double> > traffic;
    vector<vector<double> > feedback; //feedback is received when line hits or gets evicted

    void print();
};

void HawkeyeConfig::init(int core0_dp, int core1_dp, int core2_dp, int core3_dp)
{
    for (int i=0; i<LLC_SETS; i++) {
        perset_optgen[i].init(LLC_WAYS-2);
        perset_hawkeyegen[i].init(LLC_WAYS);
    }

    demand_predictor = new HAWKEYE_PC_PREDICTOR();
    prefetch_predictor = new HAWKEYE_PC_PREDICTOR();

    config[0] = core0_dp;
    if(NUM_CPUS >= 2)
        config[1] = core1_dp;
    if(NUM_CPUS >= 3)
        config[2] = core2_dp;
    if(NUM_CPUS >= 4)
        config[3] = core3_dp;
}

void HawkeyeConfig::train(uint32_t set, uint32_t type, uint64_t curr_quanta, int last_quanta, bool last_prefetched, uint64_t last_pc, uint32_t cpu)
{
    // This line has been used before. Since the right end of a usage interval is always 
    //a demand, ignore prefetches
    if((last_quanta >= 0) && (type != PREFETCH))
    {
        if( perset_optgen[set].should_cache(curr_quanta, last_quanta))
        {
            if(last_prefetched)
                prefetch_predictor->increment(last_pc);
            else 
                demand_predictor->increment(last_pc);
        }
        else
        {
            if(last_prefetched) 
                prefetch_predictor->decrement(last_pc);
            else 
                demand_predictor->decrement(last_pc);
        }
        perset_optgen[set].add_access(curr_quanta);
    }
    // This is the first time we are seeing this line (could be demand or prefetch)
    else if(last_quanta < 0)
    {
        if(type == PREFETCH)
            perset_optgen[set].add_prefetch(curr_quanta);
        else
            perset_optgen[set].add_access(curr_quanta);
    }
    else //This line is a prefetch
    {
        if(last_prefetched) // P-P
        {
            if (curr_quanta - last_quanta < 5*NUM_CPUS) 
                if(perset_optgen[set].should_cache(curr_quanta, last_quanta, true))
                    prefetch_predictor->increment(last_pc);
        }
        else // D-P
        {
            if(dp_policy(cpu, (curr_quanta-last_quanta)))
                if(perset_optgen[set].should_cache(curr_quanta, last_quanta, true))
                    demand_predictor->increment(last_pc);
        }

        perset_optgen[set].add_prefetch(curr_quanta);
    }
}

void HawkeyeConfig::hawkeye_detrain(bool prefetched, uint64_t pc)
{
    //cout << hex << info.pc << dec << endl;
    if(prefetched)
        prefetch_predictor->decrement(pc);
    else
        demand_predictor->decrement(pc);
}

//TODO
uint32_t HawkeyeConfig::hawkeyegen_rrpv_find_index(uint32_t cpu, uint64_t paddr, uint32_t set, uint32_t type, METADATA*& detrain_info, bool& hit, uint32_t &feedback_epoch)
{
    int index = perset_hawkeyegen[set].check_hit(paddr, type, feedback_epoch);

    hit = (index >= 0);

    if(!hit)
        index = perset_hawkeyegen[set].get_victim(detrain_info, feedback_epoch);

    if((type == LOAD) || (type == RFO))
    {
        accesses[cpu]++;
        if(hit)
            hits[cpu]++;
        else
            traffic[cpu]++;
    }   
    else if(type == PREFETCH)
    {
        if(!hit)
            traffic[cpu]++;
    }

    return ((uint32_t)index);
}

void HawkeyeConfig::simulate_hawkeyegen_rrpv(uint32_t index, uint64_t paddr, uint32_t set, uint32_t type, uint64_t pc, bool curr_prediction, bool hit, uint32_t curr_epoch)
{
    assert(index < (int)LLC_WAYS);
    perset_hawkeyegen[set].update(index, paddr, curr_prediction, (type == PREFETCH), pc, hit, curr_epoch);
}

void HawkeyeConfig::simulate_hawkeyegen_rrpv_wb(uint32_t index, uint64_t paddr, uint32_t set, uint32_t curr_epoch)
{
    assert(index < (int)LLC_WAYS);
    perset_hawkeyegen[set].update_wb(index, paddr, curr_epoch);
}


/*
void HawkeyeConfig::simulate_hawkeye_gen(uint64_t paddr, uint32_t set, uint32_t type, uint64_t curr_quanta, int last_quanta, uint64_t pc, bool curr_prediction)
{
    if(last_quanta >= 0)
    {
        assert(last_prediction_map.find(paddr) != last_prediction_map.end());
        bool last_prediction = last_prediction_map[paddr];
        bool hit = perset_hawkeyegen[set].should_cache(last_prediction, curr_quanta, last_quanta, (type == PREFETCH));
        if(last_prediction && !hit) // We are going to evict a line
        {
            METADATA* info = perset_hawkeyegen[set].get_evict_metadata();
        //    cout<<  dec << set << " " << last_quanta << " " << curr_quanta << endl;
            assert(info);
            hawkeye_detrain(*info);
        }

        if(last_prediction)
            perset_hawkeyegen[set].add_access(curr_prediction, curr_quanta, last_quanta, (type == PREFETCH));
        else
            perset_hawkeyegen[set].add_access(curr_prediction, curr_quanta, (type == PREFETCH));
    }
    else 
    {
        METADATA* info = perset_hawkeyegen[set].get_evict_metadata();
        if(info)
            hawkeye_detrain(*info);

        perset_hawkeyegen[set].add_access(curr_prediction, curr_quanta, (type == PREFETCH));
    }

    METADATA* info = new METADATA((type == PREFETCH), pc); 
    perset_hawkeyegen[set].update_metadata(curr_prediction, curr_quanta, info);
    last_prediction_map[paddr] = curr_prediction;
}
*/

bool HawkeyeConfig::predict(uint32_t set, uint32_t type, uint64_t ip)
{
    // Get Hawkeye's prediction for this line
    bool new_prediction = demand_predictor->get_prediction (ip);
    if (type == PREFETCH)
        new_prediction = prefetch_predictor->get_prediction (ip);


    return new_prediction;
}

double HawkeyeConfig::get_hit_rate(uint32_t cpu)
{
    cout << "             " << hits[cpu] << " " << accesses[cpu] << endl;
    return ((double)hits[cpu]/(double)accesses[cpu]);
}

double HawkeyeConfig::get_bw_usage(double available_bw)
{
    double traffic_total = 0.0;
    for(unsigned int i=0; i<NUM_CPUS; i++)
        traffic_total += traffic[i];

    //double scaled_traffic = traffic_total*(LLC_SET)/256; // We sample 256 sets
    double scaled_traffic = traffic_total;

    return (scaled_traffic/available_bw);
}

double HawkeyeConfig::get_improvement(double* baseline_hit_rate, double baseline_bw_usage, double dram_latency, double available_bw)
{
    double improvement = 0.0;

    double traffic_increase = 100*(get_bw_usage(available_bw) - baseline_bw_usage);
    double predicted_dram_latency = dram_latency*(1 + ((traffic_increase*abs(traffic_increase))/100));
    cout << "   Latency: " << dram_latency << " " << predicted_dram_latency << endl;
    cout << "   BW: " << baseline_bw_usage << " " << get_bw_usage(available_bw) << endl;
   
    if(dram_latency == 0)
    {
        dram_latency = 200;
        predicted_dram_latency = 200;
    } 
    //assert(traffic_increase <= 1);
    for(unsigned int i=0; i<NUM_CPUS; i++)
    {
        double dut_hitrate = get_hit_rate(i);
        cout << "      Hitrate: " << baseline_hit_rate[i] << " " << dut_hitrate << endl;
        assert(dut_hitrate <= 1);

        double percore_improvement = (LLC_LATENCY*baseline_hit_rate[i] + (dram_latency+LLC_LATENCY)*(1-baseline_hit_rate[i]))/(LLC_LATENCY*dut_hitrate + (LLC_LATENCY + predicted_dram_latency)*(1-dut_hitrate));
        
        improvement += percore_improvement;
        cout << "   Improvement: " << percore_improvement << endl;
    }

    return improvement;
}

void HawkeyeConfig::reset_stats()
{
    for (int i=0; i<NUM_CPUS; i++) {
        hits[i] = 0;
        accesses[i] = 0;
        traffic[i] = 0;
    }
}

void HawkeyeConfig::new_epoch(uint32_t epoch_num)
{
    hits.resize(epoch_num+1);
    accesses.resize(epoch_num+1);
    traffic.resize(epoch_num+1);
    feedback.resize(epoch_num+1);

    hits[epoch_num].resize(NUM_CPUS,0);
    accesses[epoch_num].resize(NUM_CPUS,0);
    feedback[epoch_num].resize(NUM_CPUS,0);
    traffic[epoch_num].resize(NUM_CPUS, 0);


}

void HawkeyeConfig::print()
{
    unsigned int hits = 0;
    unsigned int accesses = 0;
    unsigned int traffic = 0;
    unsigned int hawkeyegen_hits = 0;
    unsigned int hawkeyegen_rp = 0;
    unsigned int hawkeyegen_accesses = 0;
    unsigned int hawkeyegen_prefetches = 0;

    for(unsigned int i=0; i<LLC_SETS; i++)
    {
        accesses += perset_optgen[i].access;
        hits += perset_optgen[i].get_num_opt_hits();
        traffic += perset_optgen[i].get_traffic();

        hawkeyegen_hits += perset_hawkeyegen[i].hit;
        hawkeyegen_rp += perset_hawkeyegen[i].redundant_prefetch;
        hawkeyegen_accesses += perset_hawkeyegen[i].access;
        hawkeyegen_prefetches += perset_hawkeyegen[i].prefetch_access;
    }

    std::cout << "OPTgen accesses: " << accesses << std::endl;
    std::cout << "OPTgen hits: " << hits << std::endl;
    std::cout << "OPTgen hit rate: " << 100*(double)hits/(double)accesses << std::endl;
    std::cout << "Traffic: " << traffic << " " << 100*(double)traffic/(double)accesses << std::endl;

    cout << endl << endl;
    cout << "Hawkeyegen stats: " << hawkeyegen_hits << " " << hawkeyegen_accesses << " " << 100*(double)hawkeyegen_hits/(double)hawkeyegen_accesses << endl;
    cout << "Hawkeyegen prefetches: " << hawkeyegen_rp << " " << hawkeyegen_prefetches << " " << 100*(double)hawkeyegen_rp/(double)hawkeyegen_prefetches << endl;
    cout << "Hawkeye Traffic: " << (hawkeyegen_accesses - hawkeyegen_hits + hawkeyegen_prefetches - hawkeyegen_rp) << endl;

    cout << endl << endl;

}

bool HawkeyeConfig::dp_policy(uint32_t cpu, uint32_t rd)
{
    if(config[cpu] == 0) //NoDP
        return false;
    else if(config[cpu] == 1) //Middle
    {
        if (rd < 5*NUM_CPUS) 
            return true;
        return false;
    }
    else if(config[cpu] == 2)
        return true; //AllDP

    assert(0);
}

#endif
