#!/bin/sh
mkdir -p .ci/setup
cd .ci/setup

if [ ! -d "torasu-cpp" ] ; then
	git clone "https://gitlab.com/HCInk/torasu/torasu-cpp/" "torasu-cpp"
fi

cd torasu-cpp
./setup.sh
./build.sh install

cd ..

cd ../../


git submodule update --init