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

#for f in /scratch/cluster/akanksha/CRCRealTraces/*.gz
while read line; 
do
    benchmark=$line
    #benchmark=$(basename "$f")
    #benchmark="${benchmark%.*}"
    #benchmark="${benchmark%.*}"
    trace_file=$benchmark
#    echo $f $benchmark 

#    output_dir="/scratch/cluster/akanksha/CRCRealOutput/1core/L2-PAC/$output/"
    #output_dir="/scratch/cluster/akanksha/CRCRealOutput/1core/PAC/$output/"
    output_dir="/scratch/cluster/akanksha/CRCRealOutput/1core/ISB/$output/"
    #output_dir="/scratch/cluster/akanksha/CRCRealOutput/4coreCRC/SCBaseline/$output/"
#    output_dir="/scratch/cluster/akanksha/CRCRealOutput/1core/p-traces/$output/"
#    output_dir="/scratch/cluster/akanksha/CRCRealOutput/1core/traces/$output/"

    if [ ! -e "$output_dir" ] ; then
        mkdir $output_dir
        mkdir "$output_dir/scripts"
    fi
    condor_dir="$output_dir"
    script_name="$benchmark"

    #decision_file="/scratch/cluster/akanksha/CRCRealOutput/1core/traces/flexmin_ped5/$benchmark"."txt"

    #cd $output_dir
    command="/u/akanksha/MyChampSim/ChampSim/scripts/run_champsim.sh $binary 200 1000 $trace_file $output_dir low_bandwidth\ -output_decision\ $decision_file"
    #command="/u/akanksha/MyChampSim/ChampSim/scripts/run_champsim.sh $binary 200 1000 $trace_file $output_dir"

    /u/akanksha/cache_study/condor_shell --silent --log --condor_dir="$condor_dir" --condor_suffix="$benchmark" --output_dir="$output_dir/scripts" --simulate --script_name="$script_name" --cmdline="$command"

        #Submit the condor file
     /lusr/opt/condor/bin/condor_submit $output_dir/scripts/$script_name.condor

#done

done < /u/akanksha/MyChampSim/ChampSim/sim_list/crc2_list.txt
