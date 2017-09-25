#!/bin/bash
if [ $# -lt 1 ] 
then
    echo "Usage : ./run_all_sims.sh <binary> <output>"
    exit
fi

binary=$1
echo $binary
output=$2
echo $output

TRACE_DIR=/scratch/cluster/akanksha/CRCRealTraces

while read line; do
    benchmark=$line
    trace_file=$benchmark
#    echo $f $benchmark 

    output_dir="/scratch/cluster/akanksha/CRCRealOutput/1core/obol/$output/"
    decision_dir="/scratch/cluster/akanksha/CRCRealOutput/1core/obol/opt_decisions/"
    if [ ! -e "$output_dir" ] ; then
        mkdir $output_dir
        mkdir "$output_dir/scripts"
    fi
    condor_dir="$output_dir"
    script_name="$benchmark"

    #cd $output_dir
    #command="/u/akanksha/ChampsimGitHub/ChampSim/scripts/run_champsim.sh $binary 0 1000 $trace_file $output_dir low_bandwidth"
    command="(/u/akanksha/ChampsimGitHub/ChampSim/bin/$binary -warmup_instructions 0 -simulation_instructions 250000000 -output_decision $decision_dir/$trace_file.decision -hide_heartbeat -traces $TRACE_DIR/$trace_file.trace.gz) &> $output_dir/$trace_file.txt"

    /u/akanksha/cache_study/condor_shell --silent --log --condor_dir="$condor_dir" --condor_suffix="$benchmark" --output_dir="$output_dir/scripts" --simulate --script_name="$script_name" --cmdline="$command"

        #Submit the condor file
     /lusr/opt/condor/bin/condor_submit $output_dir/scripts/$script_name.condor

done < /u/akanksha/ChampsimGitHub/ChampSim/sim_list/crc2_list.txt

