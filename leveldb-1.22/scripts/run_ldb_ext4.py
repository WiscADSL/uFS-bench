import subprocess
import time
import sys
import os
import argparse
import logging
import shutil

# ASSUMPTION: current working directory is leveldb-1.22/

# REQUIRE:
# - current working directory is leveldb-1.22/
# - environment variables "SSD_NAME", "CFS_ROOT_DIR" and "KFS_MOUNT_PATH"
#   e.g. SSD_NAME=nvme0n1p4, KFS_MOUNT_PATH=/ssd-data
# - SSD must be mounted (this script will not mount, but will check)
DEV_NAME = "/dev/" + os.environ["SSD_NAME"]
CFS_ROOT_DIR = os.environ['CFS_ROOT_DIR']
# must be /ssd-data because it is hardcoded in filebench .f file
KFS_MOUNT_PATH = os.environ["KFS_MOUNT_PATH"]

# This is hard-coded in the do_work app...
LDB_DOWORK_APP_WRITE_DST_PATH = '/ssd-data/0'

workload_trace_map = {}
for name in ["a", "b", "c", "d", "e", "f"]:
    workload_trace_map[f"ycsb-{name}"] = f"traces/ycsb-{name}-10m-10m.txt"
workload_trace_map["fillseq"] = "traces/load-10m-16.txt"
workload_trace_map["fillrand"] = "traces/load-10m-16-random.txt"


# Since the commandline arguments for this script is too complicated, we use
# argparser...
def parse_cmd_args():
    parser = argparse.ArgumentParser(
        description='Run LevelDB on ext4 filesystem.')
    parser.add_argument('--workload', required=True, help='workload to run')
    parser.add_argument('--output-dir',
                        required=True,
                        help='directory to save the log')
    parser.add_argument(
        '--num-app-only',
        default=None,
        help=
        'only run the case with given number of LevelDB instances (useful for fill)'
    )
    parser.add_argument(
        '--reuse-data-dir',
        default=None,
        help=
        'If specified, reuse data in this directory as LevelDB start image (copy to this directory if workload is fillseq or fillrand; copy from if YCSB workload)'
    )
    return (parser.parse_args())


loglevel = logging.INFO
args = parse_cmd_args()

workload = args.workload
assert workload in workload_trace_map
trace = workload_trace_map[workload]
is_load = 'load' in trace
output_dir = args.output_dir
num_app_only = args.num_app_only
reuse_data_dir = args.reuse_data_dir

# check mount
is_mounted = False
with open('/proc/mounts', "r") as f:
    for line in f:
        curr_dev_name = line.strip().split()[0]
        if curr_dev_name == DEV_NAME:
            is_mounted = True
if not is_mounted:
    print("ext4 must be mounted and finish all lazy operations "
          "before running this script!")
    exit(1)


def start_leveldb(trace_path, num_app, num_worker, log_dir):
    ldb_load_command = [
        "python3", "scripts/run_ldb_common.py",
        str(num_app),
        str(num_worker), trace_path, log_dir
    ]
    return subprocess.run(ldb_load_command)


num_app_to_run = list(range(1, 11)) if num_app_only is None \
    else [int(num_app_only)]

for num_app in num_app_to_run:
    # must read copy from reuse directory
    if reuse_data_dir is not None and not is_load:
        # first remove existing data
        for appid in range(1, num_app + 1):
            ret = subprocess.call(["rm", "-rf", f"/ssd-data/0/{appid}"])
            print(f"cleanup: /ssd-data/0/{appid}")
            assert ret == 0
        # then copy
        for appid in range(1, num_app + 1):
            ret = subprocess.call([
                "cp", "-rf", f"{reuse_data_dir}/{appid}",
                f"/ssd-data/0/{appid}"
            ])
            print(f"copy: {reuse_data_dir}/{appid} -> /ssd-data/0/{appid}")
            assert ret == 0

    ldb_output_dir = f"{output_dir}/{workload}_num-app-{num_app}_leveldb"
    os.mkdir(ldb_output_dir)

    p = start_leveldb(trace, num_app, num_app, ldb_output_dir)
    if p.returncode != 0:
        print("LevelDB return non-zero exit code!")
        exit(1)

    if reuse_data_dir is not None and is_load:
        for appid in range(1, num_app + 1):
            ret = subprocess.call(["rm", "-rf", f"{reuse_data_dir}/{appid}"])
            print(f"cleanup: {reuse_data_dir}/{appid}")
            assert ret == 0
        # then copy
        for appid in range(1, num_app + 1):
            ret = subprocess.call([
                "cp", "-rf", f"/ssd-data/0/{appid}",
                f"{reuse_data_dir}/{appid}"
            ])
            print(f"copy: /ssd-data/0/{appid} -> {reuse_data_dir}/{appid}")
            assert ret == 0
