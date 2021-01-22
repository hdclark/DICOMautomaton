#!/bin/bash

read -p "Running this script will replace all files in DICOMautomaton/sycl/data are you sure you want to continue? ([Y/y] or ^C)" -n 1 -r
echo
if [[ $REPLY =~ ^[Yy]$ ]]
then
    cd /DICOMautomaton/sycl/
    bash ./compile_and_install.sh 
    cd data/
    rm -r *
    gen_test_inputs
    run_model -a aif.txt -v vif.txt -c c.txt
fi

