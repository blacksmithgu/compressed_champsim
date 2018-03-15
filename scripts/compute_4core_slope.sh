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

for i in `seq 1 100`;
do
    baseline_file="$baseline/mix""$i.txt"
    dut_file="$dut/mix""$i.txt"
    slope_core0=`perl get_percore_slope.pl $baseline_file $dut_file 0`
    slope_core1=`perl get_percore_slope.pl $baseline_file $dut_file 1`
    slope_core2=`perl get_percore_slope.pl $baseline_file $dut_file 2`
    slope_core3=`perl get_percore_slope.pl $baseline_file $dut_file 3`
    echo "$i, $slope_core0, $slope_core1, $slope_core2, $slope_core3"
done
