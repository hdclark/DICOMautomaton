#!/bin/bash

function find_strings {
    sed -i -e "s!^#define TRIE_NUM .*\$!#define TRIE_NUM $1!" Findtries.cc
    ./compile.sh
    ./findtries input/hashtable_exhaustive_dicom_tags output/$1
}


for i in {1..20} ; do
    echo "Finding all character strings of length $i"
    find_strings $i
done


if [ -a output/garbage ] ; then
    rm output/garbage 
fi

if [ -a output/garbage2 ] ; then
    rm output/garbage2
fi

cat output/* > output/garbage
cat output/garbage | sort > output/garbage2

if [ -a output/garbage ] ; then
    rm output/garbage
fi



exit


