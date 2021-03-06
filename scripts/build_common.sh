#!/usr/bin/env bash
# Builds 'common' compressed and uncompressed policies.
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
CHAMPSIM=${SCRIPT_DIR}/..

# Uncompressed: Hawkeye, LRU, RRIP
${CHAMPSIM}/build_champsim.sh --uncompressed --policy hawkeye_final --name hawkeye
${CHAMPSIM}/build_champsim.sh --uncompressed --policy lru --name lru
${CHAMPSIM}/build_champsim.sh --uncompressed --policy drrip --name drrip

# Compressed: clru, crrip, chawkeye (YACCgen), camp
${CHAMPSIM}/build_champsim.sh --policy clru --name clru
${CHAMPSIM}/build_champsim.sh --policy crrip --name crrip
${CHAMPSIM}/build_champsim.sh --policy chawkeye --name chawkeye
${CHAMPSIM}/build_champsim.sh --no-superblock --policy chawkeye --name chawkeye-nosb -- -DREDUCER=reducer_sum
${CHAMPSIM}/build_champsim.sh --no-superblock --policy camp --name camp
