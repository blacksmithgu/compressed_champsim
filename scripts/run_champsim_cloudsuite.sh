TRACE_DIR=/scratch/cluster/akanksha/CRCRealTraces
binary=${1}
n_warm=${2}
n_sim=${3}
trace=${4}
outputdir=${5}
option=${6}

echo "(/u/akanksha/MyChampSim/ChampSim/bin/${binary} -warmup_instructions ${n_warm}000000 -simulation_instructions ${n_sim}000000 -${option} -traces ${TRACE_DIR}/${trace}.trace.gz) &> ${outputdir}/${trace}.txt"
(/u/akanksha/MyChampSim/ChampSim/bin/${binary} -warmup_instructions ${n_warm}000000 -simulation_instructions ${n_sim}000000 -${option} -hide_heartbeat -traces ${TRACE_DIR}/${trace}.trace.gz) &> ${outputdir}/${trace}.txt

