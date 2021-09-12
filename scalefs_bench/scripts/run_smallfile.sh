#!/bin/bash

set -e
set -u

exper_dir=$1
log_dir=$2
NCPU=10

coord_path=${CFS_ROOT_DIR}/cfs_bench/build/bins/cfs_bench_coordinator

# usage: run ncpu
run() {
	killall largefile
	killall smallfile
	build/smallfile --prep "$exper_dir"
	mkdir "$log_dir/create_ncpu-$1"
	"$coord_path" -n "$1" -c $(($1 + 1)) & # start coordinator
	for ((c = 0; c < $1; c++)); do
		build/smallfile --bench -y 3 -c "$c" "$exper_dir" >"$log_dir/small-create_ncpu-$1/core-$c.log" &
	done
	wait # wait for all jobs above
}

for ((nc = 1; nc <= "$NCPU"; nc++)); do
	run $nc
done
