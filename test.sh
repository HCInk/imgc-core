#!/bin/bash

if [ "$1" == "no-container" ]; then

	mkdir -p test-run
	cd test-run
	wget --no-clobber https://hcink.org/x/ci-test-sets/imgc-test-set-20200619.zip
	unzip -o imgc-test-set-20200619.zip
	rm -rf test_files results
	mkdir -p test_files/one test_files/two test_files/three test_files/four test_files/five test_files/six
	../build/imgc-tests
	if [ "$2" == "generate" ]; then
    	mkdir ../results
		node ../assets/test.js -a generate -f ../results/test-solutions.json -r test_files
	else
		node ../assets/test.js -f ../assets/test-solutions.json -r test_files
	fi

else

	docker build --build-arg build_hash="$(date)" -t hcink/imgc-tests:latest -f .ci/Dockerfiles/Local .
#	rm -rf .ci/docker-wd
	if [ "$1" == "generate" ]; then
		echo "Generating solutions..."
		docker run -it hcink/imgc-tests:latest sh -c './test.sh no-container generate > /dev/null 2>&1 && cat results/test-solutions.json' > assets/test-solutions.json
		echo "Generation of solutions finished."
	else
		echo "Testing..."
		docker run -it hcink/imgc-tests:latest sh -c './test.sh no-container > /dev/null 2>&1 && cat test-run/results/result.xml' > test-result.xml
		echo "Testing finished."
	fi

fi
