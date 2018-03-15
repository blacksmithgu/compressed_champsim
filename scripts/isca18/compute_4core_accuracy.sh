#!/bin/bash
if [ $# -lt 2 ] 
then
    echo "Usage : ./run_all_sims.sh <baseline> <dut>"
    exit
fi

baseline=$1
echo $baseline
dut=$2
echo $dut
average=0.0

#for i in `seq 1 60`;
while read line; do
    benchmark=$line
    baseline_file="$baseline/$benchmark"".txt"
    dut_file="$dut/$benchmark"".txt"
    #baseline_file="$baseline/mix""$i.txt"
    #dut_file="$dut/mix""$i.txt"
    mpki=`perl get_accuracy.pl $dut_file`
    echo "$benchmark, $mpki"
    average=`perl arithmean.pl $mpki $average 100`
#done
done < /u/akanksha/MyChampSim/ChampSim/sim_list/crc2_list.txt

echo "Average: $average"
