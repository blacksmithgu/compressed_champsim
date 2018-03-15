#!/bin/bash
if [ $# -lt 1 ] 
then
    echo "Usage : ./run_all_sims.sh <dut>"
    exit
fi

dut=$1
echo $dut

for i in `seq 1 100`;
do
    dut_file="$dut/mix""$i.txt"

    ipc_average=0
    for k in `seq 0 3`;
    do
        ipc_core=`perl get_percore_ipc.pl $dut_file $k`
#        echo $ipc_core
        ipc_average=`perl arithmean.pl $ipc_core $ipc_average 4`
    done
    echo "$i, $ipc_average"
done
