#!/usr/bin/env bash
# Builds 'common' compressed and uncompressed policies.
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
CHAMPSIM=${SCRIPT_DIR}/..

# Compressed: clru, crrip, chawkeye (YACCgen), camp
${CHAMPSIM}/build_champsim.sh --no-superblock --policy clru --name clru-nosb
${CHAMPSIM}/build_champsim.sh --no-superblock --policy crrip --name crrip-nosb
${CHAMPSIM}/build_champsim.sh --no-superblock --policy chawkeye --name chawkeye-nosb
${CHAMPSIM}/build_champsim.sh --no-superblock --policy camp --name camp --name camp-nosb
