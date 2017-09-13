#!/bin/bash
if [ $# -lt 3 ] 
then
    echo "Usage : ./compute_4core_opt_miss_reduction.sh <baseline> <dut> <sc_baseline>"
    exit
fi

baseline=$1
echo $baseline
dut=$2
echo $dut
sc_baseline=$3
echo $sc_baseline

average=0.0
count=`ls -lh $baseline/*.txt | wc -l`
echo $count

dir=$(dirname "$0")
for i in `seq 1 70`;
do
    baseline_file="$baseline/mix""$i.txt"
    dut_file="$dut/mix""$i.txt"
    
    #echo "$baseline_file $dut_file"

    trace0=`sed -n ''$i'p' /u/akanksha/ChampSim_public/ChampSim_public/sim_list/4core_workloads.txt | awk '{print $1}'`
    trace1=`sed -n ''$i'p' /u/akanksha/ChampSim_public/ChampSim_public/sim_list/4core_workloads.txt | awk '{print $2}'`
    trace2=`sed -n ''$i'p' /u/akanksha/ChampSim_public/ChampSim_public/sim_list/4core_workloads.txt | awk '{print $3}'`
    trace3=`sed -n ''$i'p' /u/akanksha/ChampSim_public/ChampSim_public/sim_list/4core_workloads.txt | awk '{print $4}'`

    sc_file0="$sc_baseline/$trace0"".txt"
    sc_file1="$sc_baseline/$trace1"".txt"
    sc_file2="$sc_baseline/$trace2"".txt"
    sc_file3="$sc_baseline/$trace3"".txt"

#    echo "$sc_file0 $sc_file1 $sc_file2 $sc_file3"

    core0_dut_hit_rate=`perl ${dir}/get_percore_opt_hit_rate.pl $dut_file 0`
    core1_dut_hit_rate=`perl ${dir}/get_percore_opt_hit_rate.pl $dut_file 1`
    core2_dut_hit_rate=`perl ${dir}/get_percore_opt_hit_rate.pl $dut_file 2`
    core3_dut_hit_rate=`perl ${dir}/get_percore_opt_hit_rate.pl $dut_file 3`

    core0_baseline_hit_rate=`perl ${dir}/get_percore_hit_rate.pl $baseline_file 0`
    core1_baseline_hit_rate=`perl ${dir}/get_percore_hit_rate.pl $baseline_file 1`
    core2_baseline_hit_rate=`perl ${dir}/get_percore_hit_rate.pl $baseline_file 2`
    core3_baseline_hit_rate=`perl ${dir}/get_percore_hit_rate.pl $baseline_file 3`

    core0_sc_hit_rate=`perl ${dir}/get_hit_rate.pl $sc_file0`
    core1_sc_hit_rate=`perl ${dir}/get_hit_rate.pl $sc_file1`
    core2_sc_hit_rate=`perl ${dir}/get_hit_rate.pl $sc_file2`
    core3_sc_hit_rate=`perl ${dir}/get_hit_rate.pl $sc_file3`

    #echo "$core0_sc_hit_rate $core1_sc_hit_rate $core2_sc_hit_rate $core3_sc_hit_rate"

   # echo "$core0_baseline_hit_rate $core0_sc_hit_rate" 
    weighted_dut=`echo "($core0_dut_hit_rate/$core0_sc_hit_rate) + ($core1_dut_hit_rate/$core1_sc_hit_rate) + ($core2_dut_hit_rate/$core2_sc_hit_rate) + ($core3_dut_hit_rate/$core3_sc_hit_rate)" | bc -l`
    weighted_baseline=`echo "($core0_baseline_hit_rate/$core0_sc_hit_rate) + ($core1_baseline_hit_rate/$core1_sc_hit_rate) + ($core2_baseline_hit_rate/$core2_sc_hit_rate) + ($core3_baseline_hit_rate/$core3_sc_hit_rate)" | bc -l`

    weighted_hit_improvement=`echo "100*(($weighted_dut/$weighted_baseline)-1)" | bc -l`
    #echo "$i, $weighted_dut, $weighted_baseline, $weighted_hit_improvement"
    echo "$i, $weighted_hit_improvement"
    average=`perl ${dir}/arithmean.pl $weighted_hit_improvement $average $count`
done

echo "Average: $average"
