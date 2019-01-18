#!/bin/bash -l
#SBATCH -q regular
#SBATCH -N 1
#SBATCH -n 64
#SBATCH -C haswell
#SBATCH -t 00:30:00
#SBATCH -J power_measure

#knl SBATCH -C knl,quad,cache 



srun -n 1 --cpu-freq=1900000 ./powercap_basic
srun -n 1 --cpu-freq=2500000 ./powercap_basic
srun -n 1 --cpu-freq=3500000 ./powercap_basic


































