#!/bin/bash

# Sorry about the hardcoded paths, I'll fix them...
export LD_LIBRARY_PATH="/u/msbrenan/research/cbc/build/lib:${LD_LIBRARY_PATH}"
export PYTHONPATH=/u/msbrenan/.local/lib/python3.5/site-packages/ 
/u/msbrenan/research/compressed_champ_sim/scripts/interval2ilp.py $@
