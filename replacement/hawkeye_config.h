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
    uint32_t hawkeyegen_rrpv_find_index(uint32_t cpu, uint64_t paddr, uint32_t set, uint32_t type, METADATA*& detrain_info, bool& hit);
    void hawkeye_detrain(bool, uint64_t);
    bool predict(uint32_t set, uint32_t type, uint64_t ip);
    bool dp_policy(uint32_t cpu, uint32_t rd);
    double get_hit_rate(uint32_t cpu);
    double get_bw_usage(double);
    double get_feedback_proportion();
    double get_improvement(double* baseline_hit_rate, double baseline_bw_usage, double dram_latency, double available_bw);
    void new_epoch(uint32_t epoch_num);

    vector<vector<double> > hits;
    vector<vector<double> > accesses;
    vector<vector<double> > prefetches;
    vector<vector<double> > traffic;
    vector<vector<double> > feedback_hits; //feedback is received when line hits or gets evicted
    vector<vector<double> > feedback_evicts; //feedback is received when line hits or gets evicted

    vector<vector<uint64_t> > dd_intervals;
    vector<vector<uint64_t> > dd_intervals_miss;
    vector<vector<uint64_t> > pd_intervals;
    vector<vector<uint64_t> > dp_intervals;
    vector<vector<uint64_t> > dp_intervals_cached;
    vector<vector<uint64_t> > pp_intervals;
    vector<vector<uint64_t> > pp_intervals_cached;
    
    uint64_t dd_intervals_count[NUM_CPUS], pd_intervals_count[NUM_CPUS], pp_intervals_count[NUM_CPUS], dp_intervals_count[NUM_CPUS], dd_intervals_miss_count[NUM_CPUS];
    uint64_t dp_intervals_cached_count[NUM_CPUS], pp_intervals_cached_count[NUM_CPUS];

    uint64_t dyn_threshold[NUM_CPUS];

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

    dd_intervals.resize(NUM_CPUS);
    dd_intervals_miss.resize(NUM_CPUS);
    dp_intervals.resize(NUM_CPUS);
    dp_intervals_cached.resize(NUM_CPUS);
    pd_intervals.resize(NUM_CPUS);
    pp_intervals.resize(NUM_CPUS);
    pp_intervals_cached.resize(NUM_CPUS);

    for(unsigned int i=0; i<NUM_CPUS; i++)
    {
        dp_intervals[i].clear();
        dp_intervals_cached[i].clear();
        pp_intervals_cached[i].clear();
        dd_intervals[i].clear();
        dd_intervals_miss[i].clear();
        pd_intervals[i].clear();
        pp_intervals[i].clear();

        dp_intervals_count[i] = 0;
        dd_intervals_count[i] = 0;
        dd_intervals_miss_count[i] = 0;
        pd_intervals_count[i] = 0;
        pp_intervals_count[i] = 0;
        dp_intervals_cached_count[i] = 0;
        pp_intervals_cached_count[i] = 0;
    
        dyn_threshold[i] = 5*NUM_CPUS;
    }

}

void HawkeyeConfig::train(uint32_t set, uint32_t type, uint64_t curr_quanta, int last_quanta, bool last_prefetched, uint64_t last_pc, uint32_t cpu)
{
    // This line has been used before. Since the right end of a usage interval is always 
    //a demand, ignore prefetches
    if((last_quanta >= 0) && (type != PREFETCH))
    {
        uint64_t interval_length = (curr_quanta - last_quanta);
        if(last_prefetched)
        {
            pd_intervals_count[cpu]++;
            if(pd_intervals[cpu].size() < (interval_length+1))    
                pd_intervals[cpu].resize((interval_length+1), 0);
            pd_intervals[cpu][interval_length]++;
        }
        else
        {
            dd_intervals_count[cpu]++;
            if(dd_intervals[cpu].size() < (interval_length+1))    
                dd_intervals[cpu].resize((interval_length+1), 0);
            dd_intervals[cpu][interval_length]++;
        }

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
            {
                dd_intervals_miss_count[cpu]++;

                if(dd_intervals_miss[cpu].size() < (interval_length+1))    
                    dd_intervals_miss[cpu].resize((interval_length+1), 0);
                dd_intervals_miss[cpu][interval_length]++;

                demand_predictor->decrement(last_pc);
            }
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
        uint64_t interval_length = (curr_quanta - last_quanta);
        if(last_prefetched) // P-P
        {
            pp_intervals_count[cpu]++;
            if(pp_intervals[cpu].size() < (interval_length+1))    
                pp_intervals[cpu].resize((interval_length+1), 0);
            pp_intervals[cpu][interval_length]++;

            if(perset_optgen[set].should_cache_probe(curr_quanta, last_quanta))
            {
                pp_intervals_cached_count[cpu]++;
                if(pp_intervals_cached[cpu].size() < (interval_length+1))    
                    pp_intervals_cached[cpu].resize((interval_length+1), 0);
                pp_intervals_cached[cpu][interval_length]++;
            }

            //if (curr_quanta - last_quanta < 5*NUM_CPUS) 
            if(dp_policy(cpu, (curr_quanta-last_quanta)))
                if(perset_optgen[set].should_cache(curr_quanta, last_quanta, true))
                    prefetch_predictor->increment(last_pc);
        }
        else // D-P
        {
            dp_intervals_count[cpu]++;
            if(dp_intervals[cpu].size() < (interval_length+1))    
                dp_intervals[cpu].resize((interval_length+1), 0);
            dp_intervals[cpu][interval_length]++;

            if(perset_optgen[set].should_cache_probe(curr_quanta, last_quanta))
            {
                dp_intervals_cached_count[cpu]++;
                if(dp_intervals_cached[cpu].size() < (interval_length+1))    
                    dp_intervals_cached[cpu].resize((interval_length+1), 0);
                dp_intervals_cached[cpu][interval_length]++;
            }

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

uint32_t HawkeyeConfig::hawkeyegen_rrpv_find_index(uint32_t cpu, uint64_t paddr, uint32_t set, uint32_t type, METADATA*& detrain_info, bool& hit)
{
    int feedback_epoch = -1;
    int index = perset_hawkeyegen[set].check_hit(paddr, type, feedback_epoch);

    hit = (index >= 0);

    if(!hit)
    {
        index = perset_hawkeyegen[set].get_victim(detrain_info, feedback_epoch);
        if(feedback_epoch >= 0)
        {
            assert(feedback_epoch < (int)feedback_evicts.size());
            feedback_evicts[feedback_epoch][cpu]++;
        }
    }
    else
    {
        if(feedback_epoch >= 0)
        {
            assert(feedback_epoch < (int)feedback_hits.size());
            feedback_hits[feedback_epoch][cpu]++;
        }
    }

    if((type == LOAD) || (type == RFO))
    {
        accesses.back()[cpu]++;
        if(hit)
            hits.back()[cpu]++;
        else
            traffic.back()[cpu]++;
    }   
    else if(type == PREFETCH)
    {
        prefetches.back()[cpu]++;
        if(!hit)
            traffic.back()[cpu]++;
    }
    else
        prefetches.back()[cpu]++;

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
    cout << "             " << hits.back()[cpu] << " " << accesses.back()[cpu] << endl;
    return ((double)hits.back()[cpu]/(double)accesses.back()[cpu]);
}

double HawkeyeConfig::get_bw_usage(double available_bw)
{
    double traffic_total = 0.0;
    for(unsigned int i=0; i<NUM_CPUS; i++)
        traffic_total += traffic.back()[i];

    //double scaled_traffic = traffic_total*(LLC_SET)/256; // We sample 256 sets
    double scaled_traffic = traffic_total;

    return (scaled_traffic/available_bw);
}

double HawkeyeConfig::get_feedback_proportion()
{
    double feedback_total = 0.0;
    double requests_total = 0.0;
    for(unsigned int i=0; i<NUM_CPUS; i++)
    {
        feedback_total += feedback_hits.back()[i] + feedback_evicts.back()[i];
        requests_total += accesses.back()[i] + prefetches.back()[i];
    }

    return (100*feedback_total/requests_total);
}

double HawkeyeConfig::get_improvement(double* baseline_hit_rate, double baseline_bw_usage, double dram_latency, double available_bw)
{
    double improvement = 0.0;

    double traffic_increase = 100*(get_bw_usage(available_bw) - baseline_bw_usage);
    double predicted_dram_latency = dram_latency*(1 + ((traffic_increase*abs(traffic_increase))/100));
    cout << "   Latency: " << dram_latency << " " << predicted_dram_latency << endl;
    cout << "   BW: " << baseline_bw_usage << " " << get_bw_usage(available_bw) << endl;
    cout << "   Feedback: " << get_feedback_proportion() << endl;
   
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

void HawkeyeConfig::new_epoch(uint32_t epoch_num)
{
    hits.resize(epoch_num+1);
    accesses.resize(epoch_num+1);
    prefetches.resize(epoch_num+1);
    traffic.resize(epoch_num+1);
    feedback_hits.resize(epoch_num+1);
    feedback_evicts.resize(epoch_num+1);

    hits[epoch_num].resize(NUM_CPUS,0);
    accesses[epoch_num].resize(NUM_CPUS,0);
    prefetches[epoch_num].resize(NUM_CPUS,0);
    feedback_hits[epoch_num].resize(NUM_CPUS,0);
    feedback_evicts[epoch_num].resize(NUM_CPUS,0);
    traffic[epoch_num].resize(NUM_CPUS, 0);

    vector<uint64_t> demand;
    vector<uint64_t> supply;
    vector<vector<uint64_t> > supply_percore;
    supply_percore.resize(NUM_CPUS);
    vector<uint64_t> area_saved;
    double total_area_saved = 0;
    double total_demand = 0;
    double total_supply = 0;
    double supply_length = 0;
    double supply_length_percore[NUM_CPUS] = {0};
    double demand_length = 0;
    double demand_length_percore[NUM_CPUS] = {0};
    double supply_count = 0;
    double demand_count = 0;
    double total_intervals = 0;

    cout << "Computing dynamic threshold " << endl;
    for(unsigned int k=0; k< NUM_CPUS; k++)
    {
        total_intervals += dd_intervals_count[k] + pd_intervals_count[k] + pp_intervals_count[k] + dp_intervals_count[k];
        supply_count += dp_intervals_cached_count[k] + pp_intervals_cached_count[k];
        demand_count += dd_intervals_miss_count[k];

        if(demand.size() < dd_intervals_miss[k].size())
            demand.resize(dd_intervals_miss[k].size(), 0);

        if(supply.size() < dp_intervals_cached[k].size())
            supply.resize(dp_intervals_cached[k].size(), 0);
        if(supply.size() < pp_intervals_cached[k].size())
            supply.resize(pp_intervals_cached[k].size(), 0);

        if(supply_percore[k].size() < dp_intervals_cached[k].size())
            supply_percore[k].resize(dp_intervals_cached[k].size(), 0);
        if(supply_percore[k].size() < pp_intervals_cached[k].size())
            supply_percore[k].resize(pp_intervals_cached[k].size(), 0);

        for(unsigned int i=0; i<dd_intervals_miss[k].size(); i++)
        {
            demand[i] += dd_intervals_miss[k][i]*i;
            total_demand += dd_intervals_miss[k][i]*i;
            demand_length_percore[k] += dd_intervals_miss[k][i]*i;
        }

        for(unsigned int i=0; i<dp_intervals_cached[k].size(); i++)
        {
            supply[i] += dp_intervals_cached[k][i]*i;
            total_supply += dp_intervals_cached[k][i]*i;
            supply_length_percore[k] += dp_intervals_cached[k][i]*i;
            supply_percore[k][i] += dp_intervals_cached[k][i]*i;
        }

        for(unsigned int i=0; i<pp_intervals_cached[k].size(); i++)
        {
            supply[i] += pp_intervals_cached[k][i]*i;
            total_supply += pp_intervals_cached[k][i]*i;
            supply_length_percore[k] += pp_intervals_cached[k][i]*i;
            supply_percore[k][i] += pp_intervals_cached[k][i]*i;
        }

        supply_length_percore[k] = supply_length_percore[k]/(double)(dp_intervals_cached_count[k] + pp_intervals_cached_count[k]);
        if((dp_intervals_cached_count[k] == 0) && (pp_intervals_cached_count[k] == 0))
            supply_length_percore[k] = 0;
        demand_length_percore[k] = demand_length_percore[k]/(double)dd_intervals_miss_count[k];
    }

    supply_length = total_supply/supply_count;
    demand_length = total_demand/demand_count;

    cout << "Supply length:  ";
    for(unsigned int k=0; k< NUM_CPUS; k++)
        cout << supply_length_percore[k] << ", ";
    cout << endl;

    cout << "Demand: ";
    for(unsigned int i=0; i<demand.size(); i++)
        cout << demand[i] << " ";
    cout << endl;

    cout << "Supply: ";
    for(unsigned int i=0; i<supply.size(); i++)
        cout << supply[i] << " ";
    cout << endl;

    uint64_t threshold = 5*NUM_CPUS;
    area_saved.resize(demand.size(), 0);
    cout << "Area saved: ";
    for(unsigned int i=0; i<area_saved.size(); i++)
    {
        if(i >= supply.size())
            break;

        if(demand[i] < supply[i])
            area_saved[i] = demand[i];
        else
            area_saved[i] = supply[i];

        cout << area_saved[i] << " ";
        total_area_saved += area_saved[i];
    }

    cout << endl;

    cout << "Total area saved: " << total_area_saved << endl;

    double sliding_total = 0;   
    for(unsigned int i=0; i<area_saved.size(); i++)
    {
        sliding_total += (double)area_saved[i];
        if(sliding_total >= (0.01*total_area_saved))
        {
            threshold = i;
            break;
        }
    }

    for(unsigned int k=0; k< NUM_CPUS; k++)
        dyn_threshold[k] = threshold;

    //cout << "Thresh: " << threshold << endl;
    double potential_hits =  ((supply_count*supply_length)/demand_length)/(double)total_intervals;
    cout << "Potential hits: " << potential_hits << endl;
    cout << "Demand/Supply: " << total_demand << " " << total_supply << endl;

    int leftover_demand = total_demand;

    if(leftover_demand < total_supply)
    {
        for(unsigned int k=0; k< NUM_CPUS; k++)
            if(supply.size() > threshold)
                dyn_threshold[k] = supply.size();
    }

    while((leftover_demand > 0) && (leftover_demand < total_supply))
    {
        uint64_t max_supply_length = 0;
        uint64_t max_supply_index = 0;
        for(unsigned int k=0; k< NUM_CPUS; k++)
        {
            if(supply_length_percore[k] > max_supply_length)
            {
                max_supply_length = supply_length_percore[k];
                max_supply_index = k;
            }
        }
        //assert(max_supply_length > 0);
        cout << "Max: " << max_supply_index << " " << max_supply_length << endl;

        double max_supply = (supply_length_percore[max_supply_index]*(dp_intervals_cached_count[max_supply_index]+pp_intervals_cached_count[max_supply_index]));
        if(max_supply > leftover_demand)
        {
            for(int i=supply_percore[max_supply_index].size()-1; i>=0; i--)
            {
                //cout  << i << " " << leftover_demand << endl;
                leftover_demand = leftover_demand - supply_percore[max_supply_index][i];
                if(leftover_demand <=0)
                {
                    dyn_threshold[max_supply_index] = (i>0) ? (i-1) : 0;
                    cout << "Revise Thresh: "  << max_supply_index << ": " << i << endl;
                    supply_length_percore[max_supply_index] = 0;

                    break;
                }
            }
            assert(leftover_demand <= 0);
            supply_length_percore[max_supply_index] = 0;
            cout << "Leftover: "<< leftover_demand << endl;
            break;
        }
        else 
        {
            dyn_threshold[max_supply_index] = 0;
            //dyn_threshold[max_supply_index] = threshold;
            leftover_demand = leftover_demand - max_supply;
            assert(leftover_demand > 0);
        }

        supply_length_percore[max_supply_index] = 0;
    }
    //else keep things as it is


    cout << "Final thresh: ";
    for(unsigned int k=0; k< NUM_CPUS; k++)
        cout << dyn_threshold[k] << ", ";
    cout << endl << endl;
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

    for(unsigned int k=0; k< NUM_CPUS; k++)
    {
        cout << "Core " << k << endl;
        uint64_t total_intervals = dd_intervals_count[k] + pd_intervals_count[k] + pp_intervals_count[k] + dp_intervals_count[k];

        double supply_length = 0;
        double demand_length = 0;
        double supply_count = dp_intervals_cached_count[k] + pp_intervals_cached_count[k];
        double demand_count = dd_intervals_miss_count[k];
        
        cout << "D-D Intervals " << 100*(double)dd_intervals_count[k]/(double)total_intervals << endl;
        //for(unsigned int i=0; i<dd_intervals[k].size(); i++)
        //    cout << dd_intervals[k][i] << " ";
        //cout << endl << endl;

        cout << "P-D Intervals " << 100*(double)pd_intervals_count[k]/(double)total_intervals << endl;
        //for(unsigned int i=0; i<pd_intervals[k].size(); i++)
        //    cout << pd_intervals[k][i] << " ";
        //cout << endl << endl;

        cout << "P-P Intervals " << 100*(double)pp_intervals_count[k]/(double)total_intervals << endl;
        //for(unsigned int i=0; i<pp_intervals[k].size(); i++)
        //    cout << pp_intervals[k][i] << " ";
        //cout << endl << endl;

        cout << "D-P Intervals " << 100*(double)dp_intervals_count[k]/(double)total_intervals << endl;
        //for(unsigned int i=0; i<dp_intervals[k].size(); i++)
        //{
         //   supply_length += i*dp_intervals[k][i];
         //   cout << dp_intervals[k][i] << " ";
        //}
        //supply_length = supply_length/(double)dp_intervals_count[k];
        //cout << endl << endl;

        uint64_t pp_supply = 0;
        uint64_t dp_supply = 0;

        cout << "D-D Miss Intervals " << 100*(double)dd_intervals_miss_count[k]/(double)total_intervals << endl;

        cout << "D-P Cached Intervals " << 100*(double)dp_intervals_cached_count[k]/(double)total_intervals << endl;
        for(unsigned int i=0; i<dp_intervals_cached[k].size(); i++)
        {
            dp_supply += dp_intervals_cached[k][i]*i;
        }
        cout << endl << endl;

        cout << "P-P Cached Intervals " << 100*(double)pp_intervals_cached_count[k]/(double)total_intervals << endl;
        for(unsigned int i=0; i<pp_intervals_cached[k].size(); i++)
        {
            pp_supply += pp_intervals_cached[k][i]*i;
        }


        cout << "DP-Supply " << dp_supply << endl;
        cout << "PP-Supply " << pp_supply << endl;
        cout << endl << endl;
        cout << endl << endl;
    }
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
    else if(config[cpu] == 3)
    {
        if (rd < dyn_threshold[cpu]) 
            return true;
        return false;
    }

    assert(0);
}

#endif
