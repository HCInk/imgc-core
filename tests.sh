#!/bin/sh

mkdir test-run
cd test-run
rm -r *.mp4 *.zip results
wget https://hcink.org/x/ci-test-sets/imgc-test-set-20200619.zip
unzip imgc-test-set-20200619.zip
mkdir -p test_files/one test_files/two test_files/three test_files/four test_files/five
../build/imgc-tests
node ../assets/test.js -f ../assets/out.json -r test_files
