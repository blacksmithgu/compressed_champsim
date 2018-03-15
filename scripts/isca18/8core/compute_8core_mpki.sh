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

for i in `seq 1 50`;
do
    baseline_file="$baseline/mix""$i.txt"
    dut_file="$dut/mix""$i.txt"
    mpki=`perl get_mpki.pl $dut_file`
    baseline_mpki=`perl get_mpki.pl $baseline_file`
#    mpki=`perl get_opt_mpki.pl $baseline_file $dut_file`
    echo "$i, $mpki"

    average=`perl arithmean.pl $mpki $average 50`
    #improvement=`echo "100*((1 - $mpki/$baseline_mpki))" | bc -l`
    #echo "$i, $improvement"
    #average=`perl arithmean.pl $improvement $average 47`
done

echo "Average: $average"
