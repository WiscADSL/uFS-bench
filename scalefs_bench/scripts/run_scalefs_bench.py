import os
import sys
import subprocess
import time
import logging

COORD_PATH = f"{os.environ['CFS_ROOT_DIR']}/cfs_bench/build/bins/cfs_bench_coordinator"
MKFS_SPDK_BIN = os.environ['MKFS_SPDK_BIN']
CFS_MAIN_BIN_NAME = os.environ['CFS_MAIN_BIN_NAME']

# 1-based index
coord_pin_core = 21
num_cpu = 10


def print_usage_and_exit(argv0):
    print(
        f"Usage: {argv0} {{smallfile|largefile}} {{ufs|ext4}} <log_dir> [--no-cleanup]"
    )
    exit(1)


if len(sys.argv) != 4 and len(sys.argv) != 5:
    print_usage_and_exit(sys.argv[0])

bench = sys.argv[1]
fs_type = sys.argv[2]
log_dir = sys.argv[3]
no_cleanup = False
if len(sys.argv) == 5:
    if sys.argv[4] == "--no-cleanup":
        no_cleanup = True
    else:
        print(f"Unknown argument: {sys.argv[4]}")
        print_usage_and_exit()

if bench == "smallfile":
    is_small = True
elif bench == "largefile":
    is_small = False
else:
    print_usage_and_exit(sys.argv[0])

if fs_type == "ufs":
    is_ufs = True
    exper_dir = "/"
elif fs_type == "ext4":
    is_ufs = False
    exper_dir = "/ssd-data/scalefs_bench"
    os.makedirs(exper_dir, exist_ok=True)
else:
    print_usage_and_exit(sys.argv[0])

ready_fname = "/tmp/cfs_ready"
exit_fname = "/tmp/cfs_exit"
os.environ["READY_FILE_NAME"] = ready_fname


def cleanup():
    subprocess.call(["killall", "largefile"])
    subprocess.call(["killall", "smallfile"])
    subprocess.call(["killall", "cfs_bench_coordinator"])
    subprocess.call(["killall", "fsMain"])
    if is_ufs:
        ret = subprocess.call([MKFS_SPDK_BIN, "mkfs"])
        assert ret == 0
    else:
        subprocess.call(f"rm -rf {exper_dir}/*", shell=True)
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
    subprocess.call(["fstrim", "-a", "-v"])
    time.sleep(1)


def prep_config():
    with open("/tmp/fsp.conf", "w") as f:
        f.write(
            'splitPolicyNum = "5";\nserverCorePolicyNo = "5";\ndirtyFlushRatio = "0.9";\nraNumBlock = "16";\n'
        )
    with open("/tmp/spdk.conf", "w") as f:
        f.write('dev_name = "spdkSSD";\ncore_mask = "0x2";\nshm_id = "9";\n')


# Use same number of workers as apps
def start_fsp(num_app, fsp_out):
    fsp_command = [CFS_MAIN_BIN_NAME]
    fsp_command.append(str(num_app))
    fsp_command.append(str(num_app))
    offset_string = ",".join(str(10 * j + 1) for j in range(num_app))
    fsp_command.append(offset_string)
    fsp_command.append(exit_fname)
    fsp_command.append("/tmp/spdk.conf")
    # pin workers to 11, 12, 13, etc. (1-based index)
    offset_string = ",".join(str(num_cpu + j + 1) for j in range(num_app))
    logging.info(
        f"Pin uFS Server workers to cores: {offset_string} (1-based index)")
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
    # Create a exit file to signal server
    with open(exit_fname, 'w+') as f:
        f.write('Apparate')  # This is just for fun :)
    fs_proc.wait()
    # this kill should return non-zero for not finding fsMain...
    if subprocess.call(["pkill", "fsMain"]) == 0:
        print("WARN: Detect fsMain after shutdown; kill...")


def start_coord(num_app):
    logging.info(f"Start coordinator on core {coord_pin_core} (1-based index)")
    return subprocess.Popen(
        [COORD_PATH, "-n",
         str(num_app), "-c",
         str(coord_pin_core)])


def run_smallfile(nc):
    curr_log_dir = f"{log_dir}/smallfile_ncpu-{nc}"
    os.mkdir(curr_log_dir)
    if is_ufs:
        with open(f"{curr_log_dir}/fsp.log", "w") as f:
            fs_proc = start_fsp(nc + 1, f)
    ret = subprocess.call(["build/smallfile", "--prep", exper_dir])
    assert ret == 0

    curr_log_subdir = f"{curr_log_dir}/create"
    coord_proc = start_coord(nc)
    bench_list = []
    for c in range(nc):
        with open(f"{curr_log_subdir}/core-{c}.log", "w") as f:
            cmd = [
                "build/smallfile", "--bench", "-y", "3", "-c",
                str(c), "-k", ",".join(str(i * 10 + c + 1) for i in range(nc)),
                exper_dir
            ]
            logging.info(f"Start app: {cmd}")
            p = subprocess.Popen(cmd, stdout=f)
        bench_list.append(p)
        logging.info(f"Pin smallfile on core {c} (0-based index)")
    ret = coord_proc.wait()
    assert ret == 0
    for b in bench_list:
        ret = b.wait()
        assert ret == 0
    if is_ufs:
        shutdown_fsp(fs_proc)


def run_largefile(nc):
    curr_log_dir = f"{log_dir}/largefile_ncpu-{nc}"
    os.mkdir(curr_log_dir)
    if is_ufs:
        with open(f"{curr_log_dir}/fsp.log", "w") as f:
            fs_proc = start_fsp(2 * nc + 1, f)
    ret = subprocess.call(["build/largefile", "--prep", exper_dir])
    assert ret == 0

    for workload in ["create", "overwrite"]:
        curr_log_subdir = f"{curr_log_dir}/{workload}"
        coord_proc = start_coord(nc)
        bench_list = []
        for c in range(nc):
            with open(f"{curr_log_subdir}/core-{c}.log", "w") as f:
                cmd = [
                    "build/largefile", "--bench", workload, "-c",
                    str(c), "-k",
                    ",".join(str(i * 10 + c + 1) for i in range(nc)), exper_dir
                ]
                logging.info(f"Start app: {cmd}")
                p = subprocess.Popen(cmd, stdout=f)
            bench_list.append(p)
            logging.info(
                f"Pin largefile-{workload} on core {c} (0-based index)")
        ret = coord_proc.wait()
        assert ret == 0
        for b in bench_list:
            ret = b.wait()
            assert ret == 0
    if is_ufs:
        shutdown_fsp(fs_proc)


logging.basicConfig(level=logging.INFO)
if is_ufs:
    prep_config()
for nc in range(1, num_cpu + 1):
    cleanup()
    if is_small:
        run_smallfile(nc)
    else:
        run_largefile(nc, "create")
        run_largefile(nc, "overwrite")
if not no_cleanup:
    cleanup()
