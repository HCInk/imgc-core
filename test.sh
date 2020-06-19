#!/bin/bash

if [ "$1" == "no-container" ]; then

	mkdir -p test-run
	cd test-run
	wget --no-clobber https://hcink.org/x/ci-test-sets/imgc-test-set-20200619.zip
	unzip -o imgc-test-set-20200619.zip
	rm -rf test_files results
	mkdir -p test_files/one test_files/two test_files/three test_files/four test_files/five
	../build/imgc-tests
	if [ "$2" == "generate" ]; then
		node ../assets/test.js -a generate -f ../assets/out.json -r test_files
	else
		node ../assets/test.js -f ../assets/out.json -r test_files
	fi

else

	docker build --build-arg build_hash="$(date)" -t hcink/imgc-dev -f .ci/Dockerfiles/Build .
#	rm -rf .ci/docker-wd
	mkdir -p .ci/docker-wd/build
	if [ "$1" == "generate" ]; then
		docker run -v $PWD:/app -v $PWD/.ci/docker-wd/build:/app/build -u $(id -u):$(id -g) -it hcink/imgc-dev bash -c './build.sh && ./test.sh no-container generate'
	else 
		docker run -v $PWD:/app -v $PWD/.ci/docker-wd/build:/app/build -u $(id -u):$(id -g) -it hcink/imgc-dev bash -c './build.sh && ./test.sh no-container'
	fi

fi