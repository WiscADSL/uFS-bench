import os
import sys
import subprocess
import time

COORD_PATH = f"{os.environ['CFS_ROOT_DIR']}/cfs_bench/build/bins/cfs_bench_coordinator"
MKFS_SPDK_BIN = os.environ['MKFS_SPDK_BIN']
CFS_MAIN_BIN_NAME = os.environ['CFS_MAIN_BIN_NAME']

# 1-based index
coord_pin_core = 21

def print_usage_and_exit(argv0):
    print(f"Usage: {argv0} {{smallfile|largefile}} {{ufs|ext4}} <log_dir>")
    exit(1)


if len(sys.argv) != 4:
    print_usage_and_exit(sys.argv[0])

bench = sys.argv[1]
fs_type = sys.argv[2]
log_dir = sys.argv[3]
num_cpu = 10

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
    if is_ufs:
        ret = subprocess.run([MKFS_SPDK_BIN, "mkfs"])
        assert ret == 0
    else:
        ret = subprocess.run(f"rm -rf {exper_dir}/*", shell=True)
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
        f.write('splitPolicyNum = "5";\nserverCorePolicyNo = "5";\ndirtyFlushRatio = "0.9";\nraNumBlock = "16";\n')
    with open("/tmp/spdk.conf", "w") as f:
        f.write('dev_name = "spdkSSD";\ncore_mask = "0x2";\nshm_id = "9";\n')

# Use same number of workers as apps
def start_fsp(num_app, fsp_out):
    fsp_command = [CFS_MAIN_BIN_NAME]
    fsp_command.append(str(num_app))
    fsp_command.append(str(num_app))
    offset_string = ",".join(str(10 * j + 1) for j in range(0, num_app))
    fsp_command.append(offset_string)
    fsp_command.append(exit_fname)
    fsp_command.append("/tmp/spdk.conf")
    offset_string = ",".join(str(j + 1) for j in range(0, num_app))
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
    if subprocess.run(["pkill", "fsMain"]).returncode == 0:
        print("WARN: Detect fsMain after shutdown; kill...")

def start_coord(num_app):
    return subprocess.Popen([COORD_PATH, "-n", str(num_app), "-c", str(coord_pin_core)])

def run_smallfile(nc):
    curr_log_dir = f"{log_dir}/small-create_ncpu-{nc}"
    os.mkdir(curr_log_dir)
    if is_ufs:
        # prep fsp.conf
        with open(f"{curr_log_dir}/fsp.log", "w") as f:
            fs_proc = start_fsp(nc, "/tmp/fsp.conf", f)
    p = subprocess.run(["build/smallfile", "--prep", exper_dir])
    assert p.returncode == 0

    coord_proc = start_coord(nc)
    bench_list = []
    for c in range(nc):
        with open(f"{curr_log_dir}/core-{c}.log", "w") as f:
            bench_list.append(
                subprocess.Popen(["build/smallfile", "--bench", "-y", "3", "-c", str(c), exper_dir], stdout=f)
            )
    ret = coord_proc.wait()
    assert ret == 0
    for b in bench_list:
        ret = coord_proc.wait()
        assert ret == 0
    if is_ufs:
        shutdown_fsp(fs_proc)

def run_largefile(nc, workload):
    assert workload == "create" or workload == "overwrite"
    curr_log_dir = f"{log_dir}/large-{workload}_ncpu-{nc}"
    os.mkdir(curr_log_dir)
    if is_ufs:
        # prep fsp.conf
        with open(f"{curr_log_dir}/fsp.log", "w") as f:
            fs_proc = start_fsp(nc, "/tmp/fsp.conf", f)
    p = subprocess.run(["build/largefile", "--prep", exper_dir])
    assert p.returncode == 0

    coord_proc = start_coord(nc)
    bench_list = []
    for c in range(nc):
        with open(f"{curr_log_dir}/core-{c}.log", "w") as f:
            bench_list.append(
                subprocess.Popen(["build/largefile", "--bench", workload, "-c", str(c), exper_dir], stdout=f)
            )
    ret = coord_proc.wait()
    assert ret == 0
    for b in bench_list:
        ret = coord_proc.wait()
        assert ret == 0
    if is_ufs:
        shutdown_fsp(fs_proc)


if is_ufs:
    prep_config()
for nc in range(1, num_cpu + 1):
    cleanup()
    if is_small:
        run_smallfile(nc)
    else:
        run_largefile(nc, "create")
        run_largefile(nc, "overwrite")
cleanup()
