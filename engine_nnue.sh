#!/bin/bash
export EVAL_MODE=nnue
export NNUE_MODEL=test_model.bin
exec ./engine "$@"
