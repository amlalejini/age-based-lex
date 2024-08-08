#!/usr/bin/env bash

# This script submits jobs in sets

SET=0 # which subdir or set are we submitting

current_date=$(date +"%Y-%m-%d") # for now, make sure dates line up

REPO_DIR=/mnt/home/suzuekar/MABE2/runs/GPTP2024
CONFIG_DIR=${REPO_DIR}
EXEC_DIR=${REPO_DIR}

SCRATCH_DIR=/mnt/scratch/suzuekar/GPTP2024
DATA_DIR=${SCRATCH_DIR}/${current_date}
JOB_DIR=${DATA_DIR}/jobs

# Make all generated submission scripts executable, sbatch
for script in ${JOB_DIR}/set-$SET/*.sh; do
    chmod +x "$script"
    sbatch "$script"
done