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

#traffic_increase_average=0
count=`ls -lh $baseline/*.txt | wc -l`
echo $count

for i in `seq 1 100`;
do
    baseline_file="$baseline/mix""$i.txt"
    dut_file="$dut/mix""$i.txt"

    output=`perl opt_vs_sneakyopt.pl $baseline_file $dut_file`
    echo "$i, $output"
#    traffic_increase_average=`perl arithmean.pl $traffic_increase $traffic_increase_average $count`
done

#echo "Average: $traffic_increase_average"
