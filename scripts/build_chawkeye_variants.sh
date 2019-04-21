#!/usr/bin/env bash
# Builds 'common' compressed and uncompressed policies.
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
CHAMPSIM=${SCRIPT_DIR}/..

# Default configurations.
${CHAMPSIM}/build_champsim.sh --policy chawkeye --name chawkeye-default
${CHAMPSIM}/build_champsim.sh --no-superblock --policy chawkeye --name chawkeye-nosb

# Loop through other configurations; sleeps are for wierd make issues.
for predictor in "PCPredictor" "PCAndCompressionPredictor" "SizePredictor"; do
    for scorer in "rrpv" "mve"; do
        for reducer in "average" "sum"; do
            # Build with and without superblocks.
            ${CHAMPSIM}/build_champsim.sh --policy chawkeye --name chawkeye-$predictor-$scorer-$reducer \
                -- -DPREDICTOR=$predictor -DSCORE_FUNC=score_$scorer -DREDUCER_FUNC=reducer_$reducer
            sleep 1

            ${CHAMPSIM}/build_champsim.sh --policy chawkeye --name chawkeye-nosb-$predictor-$scorer-$reducer \
                --no-superblock -- -DPREDICTOR=$predictor -DSCORE_FUNC=score_$scorer -DREDUCER_FUNC=reducer_$reducer
            sleep 1
        done
    done
done
