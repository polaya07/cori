#!/bin/bash


# Links
# http://www.fftw.org/
# http://www.fftw.org/fftw3_doc/Installation-on-Unix.html

export CRAYPE_LINK_TYPE=dynamic

# Install directory
FFTW_VER=3.3.6-pl2
FFTW_ROOT_SINGLE=$HOME/sw/fftw-single/fftw-$FFTW_VER
FFTW_ROOT_DOUBLE=$HOME/sw/fftw-double/fftw-$FFTW_VER

# Download directory
download_dir=$HOME/Downloads/fftw
#########################################################

# Project directory
project_dir=$download_dir/fftw-$FFTW_VER
project_dir_single=${project_dir}-single
project_dir_double=${project_dir}-double

# Project repository
zipped_file=fftw-$FFTW_VER.tar.gz
repo_url=ftp://ftp.fftw.org/pub/fftw/$zipped_file


# Create directory for download and project src
mkdir -p $project_dir_single $project_dir_double


# Download project or unpack if necessary
if [ -z "$(ls -A $project_dir_single)" ] || [ -z "$(ls -A $project_dir_double)" ]; then
    if [ ! -f $download_dir/$zipped_file ]; then
        wget $repo_url -P $download_dir
        test $? -eq 0 || exit
    fi
    tar -xzf $download_dir/$zipped_file -C $project_dir_single --strip-components=1
    tar -xzf $download_dir/$zipped_file -C $project_dir_double --strip-components=1
    test $? -eq 0 || exit
fi


# Set common flags for building project
#fftw_flags="--enable-static=yes --enable-shared=yes --with-gnu-ld --enable-silent-rules --with-pic --enable-openmp --enable-threads --enable-sse2 --enable-avx --enable-avx2 --enable-avx-128-fma --enable-mpi"
fftw_flags="--enable-static=yes --enable-shared=yes --with-gnu-ld --enable-silent-rules --with-pic --enable-openmp --enable-threads --enable-sse2 --enable-avx --enable-avx2 --enable-avx512"


# Single FFTW

# Move to project directory
cd $project_dir_single

# Configure and build project
./configure --prefix=$FFTW_ROOT_SINGLE --enable-float $fftw_flags "CC=cc"
test $? -eq 0 || exit
make -j 4
test $? -eq 0 || exit

# Install project
mkdir -p $FFTW_ROOT_SINGLE
make install
test $? -eq 0 || exit


# Double FFTW

# Move to project directory
cd $project_dir_double

# Configure and build project
./configure --prefix=$FFTW_ROOT_DOUBLE $fftw_flags "CC=cc"
test $? -eq 0 || exit
make -j 4
test $? -eq 0 || exit

# Install project
mkdir -p $FFTW_ROOT_DOUBLE
make install
test $? -eq 0 || exit
