#!/bin/bash

make clean

#Modules Versions
BOOST_VER=1.67.0
FFTW_VER=3.3.6.5
CMAKE_VER=3.11.4

#Load necessary modules
module swap PrgEnv-intel PrgEnv-gnu
module load cray-fftw/$FFTW_VER
module load boost/$BOOST_VER
module load cmake/$CMAKE_VER

# Set build option
#build_option=gnu-nw
build_option=gnu-wc
#build_option=gnu-wf
#build_option=intel-wc
#build_option=intel-wf
#build_option=intel-knl

# Set tools and library paths
project_name=gearshifft
project_dirs=$HOME/projects
project_topdir=$project_dirs/$project_name
project_releasedir=$project_topdir/release
project_installdir=${project_topdir}-${build_option}
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
#BOOST_ROOT=/usr/common/software/boost/$BOOST_VER/intel/mic-knl
#FFTW_ROOT=/opt/cray/pe/fftw/$FFTW_VER/mic_knl
#FFTW_ROOT=$HOME/sw/fftw-double/fftw-3.3.6-pl2
FFTW_ROOT=/opt/cray/pe/fftw/$FFTW_VER/haswell  #or intel / gnu

export MKLROOT=/opt/intel/compilers_and_libraries/linux/mkl
export CMAKE_PREFIX_PATH=$BOOST_ROOT:$FFTW_ROOT:$CMAKE_PREFIX_PATH
export CRAYPE_LINK_TYPE=dynamic


#DCMAKE_CXX_FLAGS:  is used to add flags for all C++ targets. That's handy to pass general arguments like warning levels or to selected required C++ standards. It has no effect on C or Fortran targets and the user might pass additional flags.
#DCMAKE_EXE_LINKER_FLAGS: Linker flags to be used to create executables.These flags will be used by the linker when creating an executable.
#DCMAKE_INSTALL_PREFIX: Install directory used by install.If "make install" is invoked or INSTALL is built, this directory is prepended onto all install directories. 
#DGEARSHIFFT_INSTALL_CONFIG_PATH: Install config files in that path

#BUILD WITH WRAPPER
#https://software.intel.com/en-us/mkl-developer-reference-c-building-your-own-fftw3-interface-wrapper-library
#Change the current directory to the wrapper directory
#Add the MKL library path

# Build project
case $build_option in
	gnu-nw) # No wrappers
		CC=cc CXX=CC FC=ftn cmake -DCMAKE_CXX_FLAGS="-O3 -ffast-math -std=c++14 -fopenmp" -DCMAKE_EXE_LINKER_FLAGS="-lpthread" -DCMAKE_INSTALL_PREFIX=$project_installdir -DGEARSHIFFT_INSTALL_CONFIG_PATH=$project_installdir/configs ..
		test $? -eq 0 || exit
		;;
	gnu-wc) # GNU wrappers, C version w3xc
		CC=cc CXX=CC FC=ftn cmake -DFFTWWrappers_GNU_LIBRARIES=$HOME/mkl_fftw_wrappers/libfftw3xc_gnu.a -DFFTWWrappers_MKL_LIBRARIES=/opt/intel/lib/intel64/libiomp5.a -DCMAKE_CXX_FLAGS="-O3 -ffast-math -std=c++14 -fopenmp" -DCMAKE_EXE_LINKER_FLAGS="-lpthread" -DCMAKE_INSTALL_PREFIX=$project_installdir -DGEARSHIFFT_INSTALL_CONFIG_PATH=$project_installdir/configs ..
		test $? -eq 0 || exit
		;;
	gnu-wf) # GNU wrappers, Fortran version w3xf
		CC=cc CXX=CC FC=ftn cmake -DFFTWWrappers_GNU_LIBRARIES=$HOME/mkl_fftw_wrappers/libfftw3xf_gnu.a -DFFTWWrappers_MKL_LIBRARIES=/opt/intel/lib/intel64/libiomp5.a -DCMAKE_CXX_FLAGS="-O3 -ffast-math -std=c++14 -fopenmp" -DCMAKE_EXE_LINKER_FLAGS="-lpthread" -DCMAKE_INSTALL_PREFIX=$project_installdir -DGEARSHIFFT_INSTALL_CONFIG_PATH=$project_installdir/configs ..
		test $? -eq 0 || exit
		;;
	intel-wc) # Intel wrappers, C version w3xc
		CC=cc CXX=CC FC=ftn cmake -DFFTWWrappers_INTEL_LIBRARIES=$HOME/mkl_fftw_wrappers/libfftw3xc_intel.a -DFFTWWrappers_MKL_LIBRARIES=/opt/intel/lib/intel64/libiomp5.a -DCMAKE_CXX_FLAGS="-O3 -ffast-math -std=c++14 -fopenmp" -DCMAKE_EXE_LINKER_FLAGS="-lpthread" -DCMAKE_INSTALL_PREFIX=$project_installdir -DGEARSHIFFT_INSTALL_CONFIG_PATH=$project_installdir/configs ..
		test $? -eq 0 || exit
		;;
	intel-wf) # Intel wrappers, Fortran version w3xf
		CC=cc CXX=CC FC=ftn cmake -DFFTWWrappers_INTEL_LIBRARIES=$HOME/mkl_fftw_wrappers/libfftw3xf_intel.a -DFFTWWrappers_MKL_LIBRARIES=/opt/intel/lib/intel64/libiomp5.a -DCMAKE_CXX_FLAGS="-O3 -ffast-math -std=c++14 -fopenmp" -DCMAKE_EXE_LINKER_FLAGS="-lpthread" -DCMAKE_INSTALL_PREFIX=$project_installdir -DGEARSHIFFT_INSTALL_CONFIG_PATH=$project_installdir/configs ..
		test $? -eq 0 || exit
		;;
	intel-knl) # KNL Intel wrappers, C version w3xc
		CC=cc CXX=CC FC=ftn cmake -DFFTWWrappers_INTEL_LIBRARIES=$HOME/mkl_fftw_wrappers/libfftw3xc_intel.a -DFFTWWrappers_MKL_LIBRARIES=/opt/intel/lib/intel64/libiomp5.a -DCMAKE_CXX_FLAGS="-O3 -ffast-math -std=c++14 -fopenmp -march=knl" -DCMAKE_EXE_LINKER_FLAGS="-lpthread " -DCMAKE_INSTALL_PREFIX=$project_installdir -DGEARSHIFFT_INSTALL_CONFIG_PATH=$project_installdir/configs ..
		test $? -eq 0 || exit
		;;
	*)
		echo "Error: invalid build option....exiting."
		exit
		;;
esac

# Compile project
make
test $? -eq 0 || exit

echo "DONE BUILDING $project_name!"

# Install project
make install

echo "DONE INSTALLING $project_name!"
