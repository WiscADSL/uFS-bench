import subprocess
import time
import sys
import os

# REQUIRE:
# - current working directory is filebench/
# - environment variables "CFS_ROOT_DIR", "MKFS_SPDK_BIN", "CFS_MAIN_BIN_NAME"
CFS_ROOT_DIR = os.environ['CFS_ROOT_DIR']
MKFS_SPDK_BIN = os.environ['MKFS_SPDK_BIN']
CFS_MAIN_BIN_NAME = os.environ['CFS_MAIN_BIN_NAME']

assert (len(sys.argv) == 3)
output_dir = sys.argv[1]
readonly = int(sys.argv[2]) == 1

# set up LD_LIBRARY_PATH
if "LD_LIBRARY_PATH" in os.environ:
    ld_lib_path = [os.environ["LD_LIBRARY_PATH"]]
else:
    ld_lib_path = []
ld_lib_path.append(f"{CFS_ROOT_DIR}/cfs/lib/tbb/build/tbb_build_release")
ld_lib_path.append(f"{CFS_ROOT_DIR}/cfs/build")
os.environ["LD_LIBRARY_PATH"] = ":".join(ld_lib_path)


def mkfs():
    # clear the data...
    subprocess.run([MKFS_SPDK_BIN, "mkfs"])
    time.sleep(1)


ready_fname = "/tmp/cfs_ready"
exit_fname = "/tmp/cfs_exit"
os.environ["READY_FILE_NAME"] = ready_fname


# Use same amount of workers and apps
def start_fsp(num_worker_app_pair, fsp_out):
    fsp_command = [CFS_MAIN_BIN_NAME]
    fsp_command.append(str(num_worker_app_pair))
    fsp_command.append(str(num_worker_app_pair + 1))
    offset_string = ""
    for j in range(0, num_worker_app_pair):
        offset_string += str(20 * j + 1)
        if j != num_worker_app_pair - 1:
            offset_string += ","
    fsp_command.append(offset_string)
    fsp_command.append(exit_fname)
    fsp_command.append("/tmp/spdk.conf")
    offset_string = ""
    for j in range(0, num_worker_app_pair):
        offset_string += str(j + 1)
        if j != num_worker_app_pair - 1:
            offset_string += ","
    fsp_command.append(offset_string)
    fsp_command.append("/tmp/fsp.conf")
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
    with open(exit_fname, 'w+') as f:
        f.write('Apparate')
    fs_proc.wait()
    # this kill should return non-zero for not finding fsMain...
    if subprocess.run(["pkill", "fsMain"]).returncode == 0:
        print("WARN: Detect fsMain after shutdown; kill...")


def start_filebench(num_worker_app_pair, cache_ratio, filebench_out):
    filebench_command = [
        "env", f"NUM_WORKER={num_worker_app_pair}", f"CACHE_HIT={cache_ratio}",
        "./filebench", "-f", f"workloads/webserver{num_worker_app_pair}-ro.f"
        if readonly else f"workloads/webserver{num_worker_app_pair}.f"
    ]
    print(filebench_command)
    return subprocess.run(filebench_command, stdout=filebench_out)


if not os.path.isdir(output_dir):
    os.makedirs(output_dir)

for cache_ratio in [0, 50, 75, 100]:
    # Use same amount of workers and apps
    for num_worker_app_pair in range(1, 11):
        mkfs()

        fsp_out = open(
            f"{output_dir}/num-app-{num_worker_app_pair}_ufs-cache-hit-{cache_ratio}.out",
            "w")
        filebench_out = open(
            f"{output_dir}/num-app-{num_worker_app_pair}_ufs-cache-hit-{cache_ratio}_filebench.out",
            "w")

        fs_proc = start_fsp(num_worker_app_pair, fsp_out)
        p = start_filebench(num_worker_app_pair, cache_ratio, filebench_out)

        shutdown_fsp(fs_proc)
        fsp_out.close()
        filebench_out.close()

        if p.returncode != 0:
            print("Filebench return non-zero exit code!")
            exit(1)
