#!/bin/bash

read -p "Running this script will replace all files in DICOMautomaton/sycl/data are you sure you want to continue? ([Y/y] or ^C)" -n 1 -r
echo
if [[ $REPLY =~ ^[Yy]$ ]]
then
    cd /DICOMautomaton/sycl/
    #bash ./compile_and_install.sh 
    cd data/input/
    #rm -r *
    #gen_test_inputs

    text1=$(cat inputkParams.txt)
    IFS=' '
    read -a inputkParams <<< "$text1"

    cd ../output/

    if (( $# == 0 )); then
        echo "Running model with smooth generated data..."
        run_model -a ../input/aif.txt -v ../input/vif.txt -c ../input/c.txt
    else
        while getopts "nf" OPTION
        do
            case $OPTION in
                n)
                    echo "Running model with noisy generated data..."
                    run_model -a ../input/aif_noise.txt -v ../input/vif_noise.txt -c ../input/c_noise.txt
                    ;;
                f)
                    echo "Running model with low-pass filtered data..."
                    run_model -a ../input/aif_noise.txt -v ../input/vif_noise.txt -c ../input/sanitized_c.txt
                    ;;
                *)
                    echo "Flag not recognized. Please use -n to run model with noise."
                    exit
                    ;;
        esac
    done
    fi

    text2=$(cat kParams.txt)
    IFS=' '
    read -a outputkParams <<< "$text2"

    echo " * * * Results * * * "
    echo "input k1a:${inputkParams[0]} input k1v:${inputkParams[1]} input k2:${inputkParams[2]}"
    #echo "input k1a proportion:${inputkParams[0] / (inputkParams[0] + inputkParams[1]} \n"
    #echo "input k1v proportion:${inputkParams[1] / (inputkParams[0] + inputkParams[1]} \n"
    echo "output k1a:${outputkParams[0]} output k1v:${outputkParams[1]} output k2:${outputkParams[2]}"
    #echo "output k1a proportion:${outputkParams[0] / (outputkParams[0] + outputkParams[1]} \n"
    #echo "output k1v proportion:${outputkParams[1] / (outputkParams[0] + outputkParams[1]} \n"
fi

