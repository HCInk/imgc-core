#!/bin/sh
mkdir -p .ci/setup
cd .ci/setup

if [ ! -d "torasu-cpp" ] ; then
	git clone "https://gitlab.com/HCInk/torasu/torasu-cpp/" "torasu-cpp"
	cd torasu-cpp
else
	cd torasu-cpp
	git pull
fi

./setup.sh
./build.sh install nosudo

cd ..

cd ../../

git submodule update --init