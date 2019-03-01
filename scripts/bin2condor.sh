#!/bin/bash
# Creates condor run files for all executables in a champ sim /bin folder.
# This is very hardcoded :)
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"

OPTIONS=$(getopt -o n:o:r:t:u: --long name:,output:,rundir:,traces:,user: -- "$@")

if [ $? != 0 ]; then echo "Failed to parse options..." >& 2; exit 1; fi
eval set -- "$OPTIONS"

RUN_NAME=
SCRIPT_OUTPUT=
RUNDIR=
USER=$(whoami)
TRACE_DIR=

while true; do
    case "$1" in
        --name|-n) RUN_NAME=$2; shift 2;;
        --output|-o) SCRIPT_OUTPUT=$2; shift 2;;
        --rundir|-r) RUNDIR=$2; shift 2;;
        --user|-u) USER=$2; shift 2;;
        --traces|-t) TRACE_DIR=$2; shift 2;;
        --) shift; break;;
        *) break;;
    esac
done

if [ -z "${RUN_NAME}" ]; then
    echo "Need a run name (--name)"
    exit 1
fi

if [ -z "${SCRIPT_OUTPUT}" ]; then
    echo "Need a directory to write the condor scripts to (--output)"
    exit 1
fi

if [ -z "${RUNDIR}" ]; then
    echo "Need a directory that ChampSim outputs will go into (--rundir)"
    exit 1
fi

if [ -z "${TRACE_DIR}" ]; then
    echo "Need a directory where the trace files are located (--traces)"
    exit 1
fi

# The assumed location of the bin/ folder the executables are in.
BIN=$(realpath ${SCRIPT_DIR}/../bin)

mkdir -p ${SCRIPT_OUTPUT}

# Generate a condor run script for each binary.
for EXE in $(ls ${BIN}); do
    ${SCRIPT_DIR}/gen_condor_compressed_champsim.sh --traces ${TRACE_DIR} --executable ${BIN}/${EXE} \
        --rundir ${RUNDIR}/${EXE} --user ${USER} > ${SCRIPT_OUTPUT}/${EXE}.condor
done
