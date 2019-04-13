#!/usr/bin/env bash
# Builds 'common' compressed and uncompressed policies.
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
CHAMPSIM=${SCRIPT_DIR}/..

# Compressed: clru, crrip, chawkeye (YACCgen), camp
${CHAMPSIM}/build_champsim.sh --superblock no --policy clru --name clru-nosb
${CHAMPSIM}/build_champsim.sh --superblock no --policy crrip --name crrip-nosb
${CHAMPSIM}/build_champsim.sh --superblock no --policy chawkeye --name chawkeye-nosb
${CHAMPSIM}/build_champsim.sh --superblock no --policy camp --name camp --name camp-nosb
