#!/bin/bash

function build_runMake() {
	if [ -f  "$(command -v nproc)" ]; then
		threads=$(nproc --all)
		echo "Building multithreaded ($threads)..."
		make --jobs=$threads
	else
		echo "Building singlethreaded..."
		make
	fi
}

if [ -n "$1" ]; then

	if [ "$1" == "install" ]; then
	
		echo "Installing IMGC..."
			
		mkdir -p build
		cd build
		cmake -Wno-dev -DCMAKE_BUILD_TYPE=Release ../
		build_runMake
		if [ $(uname) == "Darwin" ] || [ "$2" == "nosudo" ]; then
			echo "Installing IMGC as user..."
      		make install
    	else
			echo "Installing IMGC as super-user..."
			sudo make install
    	fi
	
	elif [ "$1" == "dbginstall" ]; then
	
		echo "Installing IMGC [DEBUG]..."
			
		mkdir -p build
		cd build
		cmake -Wno-dev -DCMAKE_BUILD_TYPE=Debug ../
		build_runMake
		if [ $(uname) == "Darwin" ] || [ "$2" == "nosudo" ]; then
			echo "Installing IMGC as user..."
      		make install
    	else
			echo "Installing IMGC as super-user..."
			sudo make install
    	fi


	elif [ "$1" == "delbuild" ]; then
	
		echo "Deleting build-folder..."
		rm -r build

	else

		echo "Unknown argument \"$1\"!"
		echo "Available arguments: "
		echo "	install [nosudo] 	- Installs Libraries and Include files"
		echo "	dbginstall [nosudo] - Installs Libraries and Include files in debug-mode"
		echo "	delbuild 	- Deletes all buld files (build/)"
		echo "No arguments will just run a normal build."

	fi

else

	mkdir -p build
	cd build
	cmake -Wno-dev ../
	build_runMake

fi