#!/bin/bash

BOOST_VER=1.65.1
FFTW_VER=3.3.6.3
CMAKE_VER=3.11.4

module swap PrgEnv-intel PrgEnv-gnu
module swap craype-haswell craype-mic-knl 
module load cray-fftw/$FFTW_VER
module load boost/$BOOST_VER
module load cmake/$CMAKE_VER

# Set tools and library paths
project_name=gearshifft
project_dirs=$HOME/projects
project_topdir=$project_dirs/$project_name
project_releasedir=$project_topdir/release
project_installdir=${project_topdir}-knl
project_repo=https://github.com/mpicbg-scicomp/gearshifft.git

# Create directory for projects if does not exists
if [ ! -d $project_dirs ]; then
    mkdir -p $project_dirs
fi

# Download project repo if does not exists or its directory is empty
if [ ! -d $project_topdir ] || [ $(ls $project_topdir | wc -l) -eq 0 ]; then
    git clone $project_repo $project_topdir
fi

# Create release directory if does not exists, else
# remove its contents
if [ ! -d $project_releasedir ]; then
    mkdir $project_releasedir
else
    rm -rf $project_releasedir/*
fi

cd $project_releasedir

BOOST_ROOT=/usr/common/software/boost/$BOOST_VER/gnu/haswell
FFTW_ROOT=/opt/cray/pe/fftw/$FFTW_VER/haswell
export MKLROOT=/opt/intel/compilers_and_libraries/linux/mkl
export CMAKE_PREFIX_PATH=$BOOST_ROOT:$FFTW_ROOT:$CMAKE_PREFIX_PATH
export CRAYPE_LINK_TYPE=dynamic


# Build project
# No wrappers
#CC=cc CXX=CC FC=ftn cmake -DCMAKE_CXX_FLAGS="-O3 -ffast-math -std=c++14 -fopenmp" -DCMAKE_EXE_LINKER_FLAGS="-lpthread" -DCMAKE_INSTALL_PREFIX=$project_installdir -DGEARSHIFFT_INSTALL_CONFIG_PATH=$project_installdir/configs ..
# GNU wrappers
#CC=cc CXX=CC FC=ftn cmake -DFFTWWrappers_GNU_LIBRARIES=$HOME/mkl_fftw_wrappers -DFFTWWrappers_MKL_LIBRARIES=/opt/intel/lib/intel64/libiomp5.a -DCMAKE_CXX_FLAGS="-O3 -ffast-math -std=c++14 -fopenmp" -DCMAKE_EXE_LINKER_FLAGS="-lpthread" -DCMAKE_INSTALL_PREFIX=$project_installdir -DGEARSHIFFT_INSTALL_CONFIG_PATH=$project_installdir/configs ..
# Intel wrappers
CC=cc CXX=CC FC=ftn cmake -DFFTWWrappers_INTEL_LIBRARIES=$HOME/mkl_fftw_wrappers -DFFTWWrappers_MKL_LIBRARIES=/opt/intel/lib/intel64/libiomp5.a -DCMAKE_CXX_FLAGS="-O3 -ffast-math -std=c++14 -fopenmp -march=knl" -DCMAKE_EXE_LINKER_FLAGS="-lpthread" -DCMAKE_INSTALL_PREFIX=$project_installdir -DGEARSHIFFT_INSTALL_CONFIG_PATH=$project_installdir/configs ..
test $? -eq 0 || exit

# Compile project
make
test $? -eq 0 || exit

echo "DONE BUILDING $project_name!"

# Install project
make install

echo "DONE INSTALLING $project_name!"
