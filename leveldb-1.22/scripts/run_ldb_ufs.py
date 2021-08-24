import subprocess
import time
import sys
import os

# REQUIRE:
# - current working directory is leveldb-1.22/
# - environment variables "CFS_ROOT_DIR", "MKFS_SPDK_BIN", "CFS_MAIN_BIN_NAME"
# - spdk has already taken over SSD
CFS_ROOT_DIR = os.environ['CFS_ROOT_DIR']
MKFS_SPDK_BIN = os.environ['MKFS_SPDK_BIN']
CFS_MAIN_BIN_NAME = os.environ['CFS_MAIN_BIN_NAME']

assert (len(sys.argv) == 3)
workload = sys.argv[1]
output_dir = sys.argv[2]

workload_trace_map = {}
for name in ["a", "b", "c", "d", "e", "f"]:
    workload_trace_map[f"ycsb-{name}"] = f"traces/ycsb-{name}-10m-10m.txt"
workload_trace_map["fillseq"] = "traces/load-10m-16.txt"
workload_trace_map["fillrand"] = "traces/load-10m-16-random.txt"

assert workload in workload_trace_map
trace = workload_trace_map[workload]


def mkfs():
    # clear the data...
    subprocess.run([MKFS_SPDK_BIN, "mkfs"])
    time.sleep(1)


def checkpoint_journal():
    ckpt_bin_name = "{}/cfs/build/test/fsproc/fsProcOfflineCheckpointer".format(
        CFS_ROOT_DIR)
    p = subprocess.run(ckpt_bin_name, shell=True)
    if p.returncode != 0:
        raise RuntimeError('Cannot checkpoint the journal.')


ready_fname = "/tmp/cfs_ready"
exit_fname = "/tmp/cfs_exit"
os.environ["READY_FILE_NAME"] = ready_fname


# Use same number of workers as apps
def start_fsp(num_app, ufs_config_path, fsp_out):
    fsp_command = [CFS_MAIN_BIN_NAME]
    fsp_command.append(str(num_app))
    fsp_command.append(str(num_app))
    offset_string = ""
    for j in range(0, num_app):
        offset_string += str(10 * j + 1)
        if j != num_app - 1:
            offset_string += ","
    fsp_command.append(offset_string)
    fsp_command.append(exit_fname)
    fsp_command.append("/tmp/spdk.conf")
    offset_string = ""
    for j in range(0, num_app):
        offset_string += str(j + 1)
        if j != num_app - 1:
            offset_string += ","
    fsp_command.append(offset_string)
    fsp_command.append(ufs_config_path)
    print(fsp_command)

    os.system(f"rm -rf {ready_fname}")
    os.system(f"rm -rf {exit_fname}")

    fs_proc = subprocess.Popen(fsp_command, stdout=fsp_out)
    while not os.path.exists(ready_fname):
        if fs_proc.poll() is not None:
            raise RuntimeError("uFS Server exits unexpectedly")
        time.sleep(0.1)
    return fs_proc


def shutdown_fsp(fs_proc):
    # Create a exit file to signal server
    with open(exit_fname, 'w+') as f:
        f.write('Apparate')  # This is just for fun :)
    fs_proc.wait()
    # this kill should return non-zero for not finding fsMain...
    if subprocess.run(["pkill", "fsMain"]).returncode == 0:
        print("WARN: Detect fsMain after shutdown; kill...")


def load_data(num_app, num_worker, log_dir):
    ldb_load_command = "python3 scripts/run_ldb_common.py {} {} traces/load-10m-16.txt".format(
        num_app, num_app)
    ldb_load_command = [
        "python3", "scripts/run_ldb_common.py",
        str(num_app),
        str(num_worker), "traces/load-10m-16.txt", log_dir
    ]
    return subprocess.run(ldb_load_command)


def start_leveldb(trace_path, num_app, num_worker, log_dir):
    ldb_load_command = [
        "python3", "scripts/run_ldb_common.py",
        str(num_app),
        str(num_worker), trace_path, log_dir
    ]
    return subprocess.run(ldb_load_command)


for num_app in range(1, 11):
    # Do mkfs
    mkfs()

    # Load data
    with open(f"{output_dir}/fill_num-app-{num_app}_ufs.out", "w") as fsp_out:
        fs_proc = start_fsp(num_app, "ufs_cfgs/ufs-fill.conf", fsp_out)

    ldb_output_dir = f"{output_dir}/fill_num-app-{num_app}_leveldb"
    os.mkdir(ldb_output_dir)

    # use same number of workers as apps
    p = start_leveldb(workload_trace_map[f"fillseq"], num_app, num_app,
                      ldb_output_dir)
    shutdown_fsp(fs_proc)
    if p.returncode != 0:
        print("LevelDB return non-zero exit code!")
        exit(1)

    # Checkpoint the journal
    checkpoint_journal()

    # Run trace
    with open(f"{output_dir}/{workload}_num-app-{num_app}_ufs.out", "w") as fsp_out:
        fs_proc = start_fsp(num_app, f"ufs_cfgs/ufs-{workload}.conf", fsp_out)

    ldb_output_dir = f"{output_dir}/{workload}_num-app-{num_app}_leveldb"
    os.mkdir(ldb_output_dir)

    # use same number of workers as apps
    p = start_leveldb(trace, num_app, num_app, ldb_output_dir)
    shutdown_fsp(fs_proc)
    if p.returncode != 0:
        print("LevelDB return non-zero exit code!")
        exit(1)
