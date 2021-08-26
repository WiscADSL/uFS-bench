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

assert (len(sys.argv) == 2)
output_dir = sys.argv[1]

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


def start_fsp(num_app, num_worker, fsp_out):
    fsp_command = [CFS_MAIN_BIN_NAME]
    fsp_command.append(str(num_worker))
    fsp_command.append(str(num_app + 1))
    offset_string = ""
    for j in range(0, num_worker):
        offset_string += str(20 * j + 1)
        if j != num_worker - 1:
            offset_string += ","
    fsp_command.append(offset_string)
    fsp_command.append(exit_fname)
    fsp_command.append("/tmp/spdk.conf")
    offset_string = ""
    for j in range(0, num_worker):
        offset_string += str(j + 1)
        if j != num_worker - 1:
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

fsp_shutdown_timeout = 15
def shutdown_fsp(fs_proc):
    with open(exit_fname, 'w+') as f:
        f.write('Apparate')
    try:
        fs_proc.wait(fsp_shutdown_timeout)
        # if there is a large amount of data to flush before exits, it could
        # take a while before gracefully exit, but we don't really care...
        # to speed up the experiments, we just kill it...
    except subprocess.TimeoutExpired:
        print(f"WARN: uFS server doesn't exit after {fsp_shutdown_timeout} seconds; will enforce shutdown")
        subprocess.run(["pkill", "fsMain"])


def start_filebench(num_app, num_worker, filebench_out):
    filebench_command = [
        "env", f"NUM_WORKER={num_worker}", f"NUM_APP={num_app}", "./filebench",
        "-f", f"workloads/varmail{num_app}.f"
    ]
    print(filebench_command)
    return subprocess.run(filebench_command, stdout=filebench_out)


if not os.path.isdir(output_dir):
    os.makedirs(output_dir)

for num_worker in range(1, 11):
    for num_app in range(1, 11):
        mkfs()

        fsp_out = open(f"{output_dir}/num-app-{num_app}_ufs-{num_worker}.out",
                       "w")
        filebench_out = open(
            f"{output_dir}/num-app-{num_app}_ufs-{num_worker}_filebench.out",
            "w")

        fs_proc = start_fsp(num_app, num_worker, fsp_out)
        p = start_filebench(num_app, num_worker, filebench_out)

        shutdown_fsp(fs_proc)
        fsp_out.close()
        filebench_out.close()

        if p.returncode != 0:
            print("Filebench return non-zero exit code!")
            exit(1)
