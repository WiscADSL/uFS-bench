#!/bin/bash

set -e
set -u

exper_dir=$1
NCPU=$2
log_dir=$3

coord_path=${CFS_ROOT_DIR}/cfs_bench/build/bins/cfs_bench_coordinator

# usage: run ncpu
run() {
	killall largefile
	killall smallfile
	build/largefile --prep "$exper_dir"
	mkdir "$log_dir/create_ncpu-$1"
	"$coord_path" -n "$1" -c $(($1 + 1)) & # start coordinator
	for ((c = 0; c < $1; c++)); do
		build/largefile --bench create -c "$c" "$exper_dir" >"$log_dir/large-create_ncpu-$1/core-$c.log" &
	done
	wait # wait for all jobs above
	killall largefile
	"$coord_path" -n "$1" -c $(($1 + 1)) &
	mkdir "$log_dir/overwrite_ncpu-$1"
	for ((c = 0; c < $1; c++)); do
		build/largefile --bench overwrite -c "$c" "$exper_dir" >"$log_dir/large-overwrite_ncpu-$1/core-$c.log" &
	done
	wait
}

for ((nc = 1; nc <= "$NCPU"; nc++)); do
	run $nc
done
