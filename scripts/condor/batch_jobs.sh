#!/bin/sh

BUILD_DIR=/u/roshan/CacheReplacement/ChampSim/bin
LOG_DIR=/u/roshan/CacheReplacement/ChampSim/logs
TRACE_DIR=/scratch/cluster/akanksha/CRCRealTraces
EXEC_NAME=bimodal-no-no-obol_readaccess_decisions-1core
JOB_ID="`date +%Y_%m_%d_%H_%M_%S`";
mkdir -p ${LOG_DIR}/temp
mkdir -p ${LOG_DIR}/${EXEC_NAME}/${JOB_ID}

CUR_DIR=$(dirname "$0")
for f in ${TRACE_DIR}/*.trace.gz; do
  filename=`basename "${f}"`
  BENCHMARK="${filename%.*.*}"
  cp ${CUR_DIR}/template_job job
  sed -i 's%BUILD_DIR%'"${BUILD_DIR}"'%g' job
  sed -i 's%LOG_DIR%'"${LOG_DIR}"'%g' job
  sed -i 's%TRACE_DIR%'"${TRACE_DIR}"'%g' job
  sed -i 's/EXEC_NAME/'"${EXEC_NAME}"'/g' job
  sed -i 's/JOB_ID/'"${JOB_ID}"'/g' job
  sed -i 's/BENCHMARK/'"${BENCHMARK}"'/g' job
  condor_submit job
  rm job
done

