#!/bin/bash
if [ $# -lt 2 ] 
then
    echo "Usage : ./compute_miss_reductions.sh <baseline> <dut>"
    exit
fi

baseline=$1
echo $baseline
dut=$2
echo $dut

miss_reduction_average=0
#count=`ls -lh $baseline/*.txt | wc -l`

count=`cat /u/akanksha/MyChampSim/ChampSim/sim_list/crc2_list.txt | wc -l`
echo $count

dir=$(dirname "$0")
#for f in /scratch/cluster/akanksha/CRCRealTraces/*.gz
#do
while read line; do
    benchmark=$line
    #benchmark=$(basename "$f")
    #benchmark="${benchmark%.*}"
    #benchmark="${benchmark%.*}"
    baseline_file="$baseline/$benchmark"".txt"
    dut_file="$dut/$benchmark"".txt"

    #miss_reduction=`perl hits.pl $name $config $policy`
    miss_reduction=`perl ${dir}/miss-reduction.pl $baseline_file $dut_file`
    echo "$benchmark, $miss_reduction"
    miss_reduction_average=`perl ${dir}/arithmean.pl $miss_reduction $miss_reduction_average $count`
#done
done < /u/akanksha/MyChampSim/ChampSim/sim_list/crc2_list.txt

echo "Average: $miss_reduction_average"
