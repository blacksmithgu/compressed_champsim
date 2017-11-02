#!/bin/bash
if [ $# -lt 2 ] 
then
    echo "Usage : ./compute_cost_reductions.sh <baseline> <dut>"
    exit
fi

baseline=$1
echo $baseline
dut=$2
echo $dut

cost_reduction_average=0
count=`ls -lh $baseline/*.txt | wc -l`
echo $count

dir=$(dirname "$0")
for f in /scratch/cluster/akanksha/CRCRealTraces/*.gz
do
    benchmark=$(basename "$f")
    benchmark="${benchmark%.*}"
    benchmark="${benchmark%.*}"
    baseline_file="$baseline/$benchmark"".txt"
    dut_file="$dut/$benchmark"".txt"

    #cost_reduction=`perl hits.pl $name $config $policy`
    cost_reduction=`perl ${dir}/cost-reduction.pl $baseline_file $dut_file`
    echo "$benchmark, $cost_reduction"
    cost_reduction_average=`perl ${dir}/arithmean.pl $cost_reduction $cost_reduction_average $count`
done

echo "Average: $cost_reduction_average"
