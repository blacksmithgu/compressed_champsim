#!/bin/bash
if [ $# -lt 1 ] 
then
    echo "Usage : ./compile_csv.sh <dut>"
    exit
fi

TRACE_DIR=/scratch/cluster/akanksha/CRCRealTraces

dut=$1

dir=$(dirname "$0")
echo "Benchmark, Misses, CPI"
for f in ${TRACE_DIR}/*.gz
do
    benchmark=$(basename "$f")
    benchmark="${benchmark%.*}"
    benchmark="${benchmark%.*}"
    dut_file="$dut/$benchmark"".txt"

    misses=`perl ${dir}/get_misses.pl $dut_file`
    cpi=`perl ${dir}/get_cpi.pl $dut_file`
    echo "$benchmark, $misses, $cpi"
done
