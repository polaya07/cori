#!/bin/bash -l
#SBATCH -q regular
#SBATCH -N 1
#SBATCH -n 64
#SBATCH -C haswell
#SBATCH -t 00:30:00
#SBATCH -J power_measure
#SBATCH --perf=likwid

#knl SBATCH -C knl,quad,cache 

module load papi 

./powercap_limit


































