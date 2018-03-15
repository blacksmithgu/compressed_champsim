#!/bin/bash
if [ $# -lt 1 ] 
then
    echo "Usage : ./run_all_sims.sh <dut>"
    exit
fi

dut=$1
echo $dut

tpa_average=0

for i in `seq 1 100`;
do
    dut_file="$dut/mix""$i.txt"

#    tpa=`perl tpa.pl $dut_file`
    tpa=`perl get_used_dram_bw.pl $dut_file`
    echo "$i, $tpa"
    tpa_average=`perl arithmean.pl $tpa $tpa_average 100`
done

echo "Average: $tpa_average"
