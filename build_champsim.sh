#!/bin/sh

OPTIONS=$(getopt -o b:p:r:p:c:n: --long branch:,l1prefetcher:,l2prefetcher:,policy:,cores:,name: -- "$@")

if [ $? != 0 ]; then echo "Failed to parse options..." >& 2; exit 1; fi

eval set -- "$OPTIONS"

# GETOPTs for obtaining build configuration; defaults are set here and overwritten via command line arguments.
BRANCH=bimodal       # branch/*.bpred
L1D_PREFETCHER=no    # prefetcher/*.l1d_pref
L2C_PREFETCHER=no    # prefetcher/*.l2c_pref
LLC_REPLACEMENT=lru  # replacement/*.llc_repl
NUM_CORE=1
COMPILE_OPTIONS=
BINARY_NAME=

while true; do
    case "$1" in
        --branch) BRANCH=$2; shift 2;;
        --l1prefetcher) L1D_PREFETCHER=$2; shift 2;;
        --l2prefetcher) L2C_PREFETCHER=$2; shift 2;;
        --policy) LLC_REPLACEMENT=$2; shift 2;;
        --cores) NUM_CORE=$2; shift 2;;
        --name) BINARY_NAME=$2; shift 2;;
        --) shift; break;;
        *) break;;
    esac
done

COMPILE_OPTIONS=$@

if [ -z "${BINARY_NAME}" ]; then
    BINARY_NAME="${BRANCH}-${L1D_PREFETCHER}-${L2C_PREFETCHER}-${LLC_REPLACEMENT}-${NUM_CORE}core"
fi

############## Some useful macros ###############
BOLD=$(tput bold)
NORMAL=$(tput sgr0)

embed_newline()
{
   local p="$1"
   shift
   for i in "$@"
   do
      p="$p\n$i"         # Append
   done
   echo -e "$p"          # Use -e
}
#################################################

# Sanity check
if [ ! -f ./branch/${BRANCH}.bpred ] || [ ! -f ./prefetcher/${L1D_PREFETCHER}.l1d_pref ] || [ ! -f ./prefetcher/${L2C_PREFETCHER}.l2c_pref ] || [ ! -f ./replacement/${LLC_REPLACEMENT}.llc_repl ]; then
	echo "${BOLD}Possible Branch Predictor: ${NORMAL}"
	LIST=$(ls branch/*.bpred | cut -d '/' -f2 | cut -d '.' -f1)
	p=$( embed_newline $LIST )
	echo "$p"

	echo "${BOLD}Possible L1D Prefetcher: ${NORMAL}"
	LIST=$(ls prefetcher/*.l1d_pref | cut -d '/' -f2 | cut -d '.' -f1)
	p=$( embed_newline $LIST )
	echo "$p"

	echo
	echo "${BOLD}Possible L2C Prefetcher: ${NORMAL}"
	LIST=$(ls prefetcher/*.l2c_pref | cut -d '/' -f2 | cut -d '.' -f1)
	p=$( embed_newline $LIST )
	echo "$p"

	echo
	echo "${BOLD}Possible LLC Replacement: ${NORMAL}"
	LIST=$(ls replacement/*.llc_repl | cut -d '/' -f2 | cut -d '.' -f1)
	p=$( embed_newline $LIST )
	echo "$p"
	exit
fi

# Check for multi-core
if [ "$NUM_CORE" != "1" ]; then
    echo "${BOLD}Building multi-core Champsim (with ${NUM_CORE} cores)...${NORMAL}"
    COMPILE_OPTIONS="${COMPILE_OPTIONS} -DNUM_CPUS=${NUM_CORE} -DDRAM_CHANNELS=2 -DDRAM_CHANNELS_LOG2=1"
else
    echo "${BOLD}Building single-core ChampSim...${NORMAL}"
fi
echo
echo "Command Line Arguments: ${COMPILE_OPTIONS}"

# Change prefetchers and replacement policy
# This is terrible, why do this?
cp branch/${BRANCH}.bpred branch/branch_predictor.cc
cp prefetcher/${L1D_PREFETCHER}.l1d_pref prefetcher/l1d_prefetcher.cc
cp prefetcher/${L2C_PREFETCHER}.l2c_pref prefetcher/l2c_prefetcher.cc
cp replacement/${LLC_REPLACEMENT}.llc_repl replacement/llc_replacement.cc

# Build
mkdir -p bin
rm -f bin/champsim
make clean
make ExternalCFlags="${COMPILE_OPTIONS}"

# Sanity check
echo ""
if [ ! -f bin/champsim ]; then
    echo "${BOLD}ChampSim build FAILED!${NORMAL}"
    echo ""
    exit
fi

echo "${BOLD}ChampSim is successfully built"
echo "Branch Predictor: ${BRANCH}"
echo "L1D Prefetcher: ${L1D_PREFETCHER}"
echo "L2C Prefetcher: ${L2C_PREFETCHER}"
echo "LLC Replacement: ${LLC_REPLACEMENT}"
echo "Cores: ${NUM_CORE}"
echo "Binary: bin/${BINARY_NAME}${NORMAL}"
echo ""
mv bin/champsim bin/${BINARY_NAME}

cp branch/bimodal.bpred branch/branch_predictor.cc
cp prefetcher/no.l1d_pref prefetcher/l1d_prefetcher.cc
cp prefetcher/no.l2c_pref prefetcher/l2c_prefetcher.cc
cp replacement/lru.llc_repl replacement/llc_replacement.cc
