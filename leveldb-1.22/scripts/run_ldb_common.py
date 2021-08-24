# This script launch multiple LevelDB instances
# This is designed to be low-level script so it only accpets trace path instead
# of workload name
# Only exit when all instances exit
import sys
import subprocess

assert (len(sys.argv) == 5)
num_app = int(sys.argv[1])
num_worker = int(sys.argv[2])
trace = sys.argv[3]
output_dir = sys.argv[4]

ENV1 = "FSP_KEY_LISTS="
workers = [0, 10, 20, 30, 40, 50, 60, 70, 80, 90]
is_load = "load" in trace

# Here is the protocol:
# - the benchmarking data is always in `/ssd-data/0` (hardcoded in `do_work`)
# - sometimes to save time used in data prep, we may backup leveldb images into
#   `/ssd-data/1`

# clean up
if is_load:  # if load, cleanup existing data
    for appid in range(1, num_app + 1):
        ret = subprocess.call(["rm", "-rf", f"/ssd-data/0/{appid}"])
        assert ret == 0
        # we use "-f" flag, so if not exist, it won't complain
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

ldb_proc_list = []

for appid in range(1, num_app + 1):
    command = ["env"]
    env_string = ""
    for i in range(num_worker):
        env_string += str(workers[i] + appid)
        if i != num_worker - 1:
            env_string += ","
    command.append(ENV1 + env_string)
    command.append("build/do_work")
    command.append("-d")
    command.append(str(appid))
    command.append("-n")
    a = 0
    if is_load:
        if "random" in trace:
            a = 2000000
        else:
            a = 10000000
    else:
        a = 100000
    command.append(str(a))
    command.append("-v")
    command.append("80")
    command.append("-wef" if is_load else "-ef")
    command.append(trace)
    print(command)

    with open(f"{output_dir}/leveldb-{appid}.out", "w") as ldb_out:
        ldb_proc = subprocess.Popen(command, stdout=ldb_out)
        ldb_proc_list.append(ldb_proc)

script_exit_code = 0

for appid_0base, ldb_proc in enumerate(ldb_proc_list):
    ret = ldb_proc.wait()
    if ret != 0:
        print(f"LevelDB-{appid_0base+1} exits unexpectedly with exit code {ret}")
        script_exit_code = 1

if script_exit_code == 0:
    print('run_ldb_common done!')
else:
    print('run_ldb_common fails!')
sys.exit(script_exit_code)
