#!/bin/bash
if [ $# -lt 1 ] 
then
    echo "Usage : ./compile_csv.sh <dut>"
    exit
fi

TRACE_DIR=/scratch/cluster/akanksha/CRCRealTraces

dut=$1

dir=$(dirname "$0")
echo "Benchmark, Misses, Cost, Cycles, IPC"
for f in ${TRACE_DIR}/*.gz
do
    benchmark=$(basename "$f")
    benchmark="${benchmark%.*}"
    benchmark="${benchmark%.*}"
    dut_file="$dut/$benchmark"".txt"

    misses=`perl ${dir}/get_misses.pl $dut_file`
    cpi=`perl ${dir}/get_cpi.pl $dut_file`
    ipc=`perl ${dir}/get_ipc.pl $dut_file`
    cost=`perl ${dir}/get_cost.pl $dut_file`
    echo "$benchmark, $misses, $cost, $cpi, $ipc"
done
