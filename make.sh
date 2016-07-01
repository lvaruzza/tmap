if [ -d build ]; then
 rm -Rf build
fi

# For Mac use homebrew GCC 5 instead of LLVM
#export CC=gcc-5
#export CXX=g++-5

mkdir build
cd build
cmake ..
make
