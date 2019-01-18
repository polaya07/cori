#!/bin/bash -l
#SBATCH -q regular
#SBATCH -N 1
#SBATCH -n 64
#SBATCH -C haswell
#SBATCH -t 00:20:00
#SBATCH -J gearshifft
#knl SBATCH -C knl,quad,cache 

#Modules' versions
BOOST_VER=1.65.1
FFTW_VER=3.3.6.3

#Load specific modules
module swap PrgEnv-intel PrgEnv-gnu
module load cray-fftw/$FFTW_VER
module load boost/$BOOST_VER

# -N: nodos -t tiempomax -C filter de procesador. -J name job -q categoria para darle prioridad al trabajo.

CURDIR=$HOME/projects/gearshifft

#Data size from extensions
FEXSMALL=$CURDIR/share/gearshifft/extents_small.conf
FEXLARGE=$CURDIR/share/gearshifft/extents_1d_fftw_large.conf
FEXCOMPLETE=$CURDIR/share/gearshifft/extents_1d_fftw_copy.conf
FEXPRUEBA=$CURDIR/share/gearshifft/extents_1d_fftw.conf
FEXTENTS1D=$CURDIR/share/gearshifft/extents_1d_publication.conf
FEXTENTS1DFFTW=$CURDIR/share/gearshifft/extents_1d_fftw.conf  # excluded a few very big ones
FEXTENTS2D=$CURDIR/share/gearshifft/extents_2d_publication.conf
FEXTENTS3D=$CURDIR/share/gearshifft/extents_3d_publication.conf
FEXTENTS=$CURDIR/share/gearshifft/extents_all_publication.conf

#Run with specific FFT
today=$(date +%Y_%m_%d-%H:%M:%S) 

#gnu-nw
#time $HOME/projects/gearshifft-gnu-nw/bin/gearshifft_fftw -f $FEXTENTS1DFFTW -v -o "$HOME/projects/results/$today-gnu_nw-1d.csv" --rigor estimate

#time $HOME/projects/gearshifft-gnu-nw/bin/gearshifft_fftw -f $FEXLARGE -v -o "$HOME/projects/results/$today-gnu_nw-1d.csv" --rigor estimate

#gnu-wc

$HOME/projects/gearshifft-gnu-wc/bin/gearshifft_fftwwrappers -f $FEXSMALL -v -o "$HOME/projects/results/$today-gnu_wc-papi.csv" --rigor estimate

#time $HOME/projects/gearshifft-gnu-wc/bin/gearshifft_fftwwrappers -f $FEXPRUEBA -v -o "$HOME/projects/results/$today-gnu_wc-1d.csv" -r */float/*/Inplace_Real --rigor measure


