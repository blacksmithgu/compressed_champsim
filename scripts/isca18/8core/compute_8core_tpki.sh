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

for i in `seq 1 47`;
do
    baseline_file="$baseline/mix""$i.txt"
    dut_file="$dut/mix""$i.txt"
    tpki=`perl get_tpki.pl $dut_file`
    baseline_tpki=`perl get_tpki.pl $baseline_file`
    echo "$i, $tpki"
#    mpki=`perl get_opt_tpki.pl $baseline_file $dut_file`
#    improvement=`echo "100*((1 - $tpki/$baseline_tpki))" | bc -l`
#    echo "$i, $improvement"
#    average=`perl arithmean.pl $improvement $average 47`
    average=`perl arithmean.pl $tpki $average 50`
done

echo "Average: $average"
