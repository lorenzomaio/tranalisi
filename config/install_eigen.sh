#/bin/bash -e

wget https://gitlab.com/libeigen/eigen/-/archive/3.3.7/eigen-3.3.7.tar.gz -O -|tar xzvf -
cd eigen*
mkdir build
cd build
cmake .. -DCMAKE_INSTALL_PREFIX:PATH=$HOME
make
make install
cd ../..
rm -fr eigen*

