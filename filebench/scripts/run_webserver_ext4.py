import subprocess
import time
import sys
import os

# REQUIRE:
# - current working directory is filebench/
# - environment variables "SSD_NAME", "CFS_ROOT_DIR" and "KFS_MOUNT_PATH"
#   e.g. SSD_NAME=nvme0n1p4, KFS_MOUNT_PATH=/ssd-data
DEV_NAME = "/dev/" + os.environ["SSD_NAME"]
CFS_ROOT_DIR = os.environ['CFS_ROOT_DIR']
# must be /ssd-data because it is hardcoded in filebench .f file
KFS_MOUNT_PATH = os.environ["KFS_MOUNT_PATH"]

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


def cleanup():
    ret = subprocess.call(f"rm -rf {KFS_MOUNT_PATH}/*", shell=True)
    assert ret == 0
    ret = subprocess.call(["sync"])
    assert ret == 0
    with open("/proc/sys/vm/drop_caches", "w") as f:
        f.write("1\n")
        f.flush()
    with open("/proc/sys/vm/drop_caches", "w") as f:
        f.write("2\n")
        f.flush()
    with open("/proc/sys/vm/drop_caches", "w") as f:
        f.write("3\n")
        f.flush()
    # this call is considered to be "best-effect"
    # we don't really care too much if it fails...
    subprocess.call(["fstrim", "-a", "-v"])


if not os.path.isdir(output_dir):
    os.makedirs(output_dir)

for num_app in range(1, 11):
    cleanup()

    filebench_command = [
        "env", f"NUM_WORKER={num_app}", "./filebench", "-f",
        f"workloads/webserver{num_app}-ro.f"
        if readonly else f"workloads/webserver{num_app}.f"
    ]
    filebench_out = open(f"{output_dir}/num-app-{num_app}_ext4_filebench.out",
                         "w")

    print(filebench_command)
    p = subprocess.run(filebench_command, stdout=filebench_out)
    filebench_out.close()

    if p.returncode != 0:
        print("Filebench return non-zero exit code!")
        exit(1)

cleanup()
