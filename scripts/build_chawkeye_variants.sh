#!/usr/bin/env bash
# Builds 'common' compressed and uncompressed policies.
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
CHAMPSIM=${SCRIPT_DIR}/..

# Default configuration.
${CHAMPSIM}/build_champsim.sh --policy chawkeye --name chawkeye-default

# Loop through other configurations
for predictor in "PCPredictor" "PCAndCompressionPredictor" "SizePredictor"; do
    for rrpv_max in "15" "63"; do
        for scorer in "rrpv" "mve"; do
            for reducer in "average" "sum"; do
                ${CHAMPSIM}/build_champsim.sh --policy chawkeye --name chawkeye-$predictor-$rrpv_max-$scorer-$reducer \
                    -- -DPREDICTOR=$predictor -DRRPV_MAX_VALUE=$rrpv_max -DSCORE_FUNC=score_$scorer \
                    -DREDUCER_FUNC=reducer_$reducer
            done
        done
    done
done
