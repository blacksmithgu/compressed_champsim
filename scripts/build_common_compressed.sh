#!/usr/bin/env bash
# Builds 'common' compressed and uncompressed policies.
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"

# Uncompressed: Hawkeye, LRU
${SCRIPT_DIR}/../build_champsim.sh --uncompressed --policy hawkeye_final --name hawkeye
${SCRIPT_DIR}/../build_champsim.sh --uncompressed --policy lru --name lru

# Compressed: clru, chawkeye (OPTgen), chawkeye (YACCgen)
${SCRIPT_DIR}/../build_champsim.sh --policy clru --name clru
${SCRIPT_DIR}/../build_champsim.sh --policy chawkeye --name chawkeye_optgen -- -DCACHEGEN="OPTgen\<1024\>"
${SCRIPT_DIR}/../build_champsim.sh --policy chawkeye --name chawkeye_yaccgen -- -DCACHEGEN="YACCgen\<1024\>"
