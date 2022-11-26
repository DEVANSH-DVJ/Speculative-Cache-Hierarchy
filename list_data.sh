#!/bin/bash

cd traces && find . -type f -path '*\.xz' | sort >../all_traces.txt && cd ..

SRC='results_1M'
find $SRC -type f -path '*\.txt' | sort >"$SRC"_list.txt
