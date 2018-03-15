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

for i in `seq 1 80`;
do
    baseline_file="$baseline/mix""$i.txt"
    dut_file="$dut/mix""$i.txt"
    mpki=`perl get_tpki.pl $dut_file`
    #mpki=`perl get_opt_tpki.pl $baseline_file $dut_file`
    echo "$i, $mpki"
    average=`perl arithmean.pl $mpki $average 80`
done

echo "Average: $average"
