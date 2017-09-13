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

tpa_average=0
count=`ls -lh $baseline/*.txt | wc -l`
echo $count

for i in `seq 1 70`;
do
    dut_file="$dut/mix""$i.txt"

    tpa=`perl tpa.pl $dut_file`
    echo "$i, $tpa"
    tpa_average=`perl arithmean.pl $tpa $tpa_average $count`
done

echo "Average: $tpa_average"
