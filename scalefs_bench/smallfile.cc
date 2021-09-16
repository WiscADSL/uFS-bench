#include "benchutils/coordinator.h"
#include "libutil.h"
// default configuration described in scalefs paper
#define NUMFILES 10000
#define NUMDIRS 100
#define FILESIZE 1024
#define COORD_SHM_FNAME "/coordinator"

enum SyncOption {
  SYNC_NONE,
  SYNC_UNLINK,
  SYNC_CREATE_UNLINK,
  FSYNC_CREATE,
  SYNC_OPTIONS_RANGE
};

const char *sync_options[] = {"SYNC_NONE", "SYNC_UNLINK", "SYNC_CREATE_UNLINK",
                              "FSYNC_CREATE"};
int sync_when = SYNC_UNLINK;  // default
unsigned long num_files = NUMFILES;
unsigned long num_dirs = NUMDIRS;
unsigned long nfiles_per_dir;
unsigned long timer_overhead = 0;
int per_cpu_dirs = 0;
int is_prep;  // prep or bench
int pin_core = -1;
char *topdir = NULL;
char *buf = NULL;
int total_cores = -1;
char *shm_keys_str = NULL;

void run_benchmark(int cpu);
void create_dirs(const char *topdir, unsigned long num_dirs);
uint64_t create_files(const char *topdir, char *buf, int cpu);
uint64_t read_files(const char *topdir, char *buf, int cpu);
uint64_t unlink_files(const char *topdir, char *buf, int cpu);
uint64_t sync_files(void);

void print_usage_and_exit() {
  fprintf(
      stderr,
      "Usage: smallfile {--prep|--bench} [CONFIG] <working directory>\n"
      "  [CONFIG]:\n"
      "    [-p [total_cores]]: creates files under per-cpu sub-directories;\n"
      "        if with `--prep', must specify how many cores will be used\n"
      "    [-d num_dirs]: number of directories to spread file creation\n"
      "    [-f num_files]: number of files to create in total;\n"
      "        only valid with `--bench'\n"
      "    [-c core]: core id to pin; only valid with `--bench'\n"
      "    [-y sync_when]: when to syn/Fsync; only valid with `--bench'\n"
      "        `sync_when' can be:\n"
      "          %d: no sync\n"
      "          %d: sync after unlink\n"
      "          %d: sync after create and unlink\n"
      "          %d: Fsync during create\n"
      "    [-k keys]: shared memory keys; only valid with `--bench' and uFS "
      "APIs\n\n",
      SYNC_NONE, SYNC_UNLINK, SYNC_CREATE_UNLINK, FSYNC_CREATE);
  exit(1);
}

void parse_arg(int argc, char **argv) {
  char ch;
  extern char *optarg;
  extern int optind;

  if (argc < 3) print_usage_and_exit();
  if (strcmp(argv[1], "--prep") == 0) {
    is_prep = 1;
  } else if (strcmp(argv[1], "--bench") == 0) {
    is_prep = 0;
  } else {
    fprintf(stderr, "Must specify `--prep' or `--bench'\n");
    print_usage_and_exit();
  }

  optind = 2;
  while ((ch = getopt(argc, argv, "p::d:f:c:y:k:")) != -1) {
    switch (ch) {
      case 'p':
        per_cpu_dirs = 1;
        if (optarg) total_cores = atoi(optarg);
        break;
      case 'd':
        num_dirs = atoi(optarg);
        break;
      case 'f':
        num_files = atoi(optarg);
        break;
      case 'c':
        pin_core = atoi(optarg);
        break;
      case 'y':  // When to call Sync()
        sync_when = atoi(optarg);
        break;
      case 'k':
        shm_keys_str = optarg;
        break;
      default:
        print_usage_and_exit();
    }
  }
  topdir = argv[optind];
  if (!topdir) print_usage_and_exit();

  if (is_prep) {
    if (per_cpu_dirs && total_cores < 0) {
      fprintf(stderr, "Must specify total number of cores by `-p'\n");
      print_usage_and_exit();
    }
    printf("Preparing LFS-Smallfile test on %s\n", topdir);
    printf("Directories are: %s\n", per_cpu_dirs ? "per-cpu" : "shared");
    printf("Number of dirs = %ld\n", num_dirs);
    fflush(stdout);
  } else {
    if (sync_when >= SYNC_OPTIONS_RANGE) {
      fprintf(stderr, "Invalid `sync_when': %d\n", sync_when);
      print_usage_and_exit();
    }
    if (pin_core < 0) {
      fprintf(stderr, "core id not specified!\n");
      print_usage_and_exit();
    }
    nfiles_per_dir = num_files / num_dirs;
    timer_overhead = get_timer_overhead();
    printf("Running LFS-Smallfile test on %s, on Core %d\n", topdir, pin_core);
    printf("Directories are: %s\n", per_cpu_dirs ? "per-cpu" : "shared");
    printf("Number of dirs = %ld\n", num_dirs);
    printf("Number of files = %ld\n", num_files);
    printf("File Size = %d bytes\n", FILESIZE);
    printf("Number of files per dir = %ld\n", nfiles_per_dir);
    printf("Sync/Fsync option: %s\n", sync_options[sync_when]);
    printf("Timer overhead = %ld us\n", timer_overhead);
    fflush(stdout);
  }
}

int main(int argc, char **argv) {
  parse_arg(argc, argv);

#ifdef USE_UFS
  printf("INFO: using uFS APIs\n");
#else
  printf("INFO: using POSIX APIs\n");
#endif

  int num_workers = 0;
  int aid = initFs(shm_keys_str, num_workers);

  buf = (char *)Malloc(FILESIZE);
  if (is_prep) {
    /* Create the directories for the files to be spread amongst */
    create_dirs(topdir, num_dirs);
    Sync();
    goto done;
  }

  if (!buf) {
    fprintf(stderr, "ERROR: Failed to allocate buffer");
    exit(1);
  }

  if ((aid - 1) % num_workers != 0) {
    int target = (aid - 1) % num_workers;
    fprintf(stderr, "fileset reassigned to worker:%d num_workers:%d str:%s\n",
            target, num_workers, shm_keys_str);
    fs_admin_thread_reassign(0, target, FS_REASSIGN_FUTURE);
  }
  for (int i = 0; i < FILESIZE; i++) {
    buf[i] = 'a';
  }
  run_benchmark(pin_core);

done:
  fprintf(stderr, "done: aid: %d\n", aid);
  Free(buf);
  exitFs();
  return 0;
}

void run_benchmark(int cpu) {
  unsigned long sync1_usec = 0;
  unsigned long sync2_usec = 0;
  unsigned long create_usec = 0;
  unsigned long read_usec = 0;
  unsigned long unlink_usec = 0;
  float sec, tp;
  float total_sec = 0;

  if (setaffinity(cpu) < 0) {
    printf("setaffinity failed for cpu %d\n", cpu);
    return;
  }

  CoordinatorClient cc(COORD_SHM_FNAME, 0);
  cc.notify_server_that_client_is_ready();
  cc.wait_till_server_says_start();

  create_usec = create_files(topdir, buf, cpu);

  if (sync_when == SYNC_CREATE_UNLINK) sync1_usec = sync_files();

  read_usec = read_files(topdir, buf, cpu);
  unlink_usec = unlink_files(topdir, buf, cpu);

  if (sync_when == SYNC_UNLINK || sync_when == SYNC_CREATE_UNLINK)
    sync2_usec = sync_files();

  cc.notify_server_that_client_stopped();

  // print results
  printf("Test            Time(sec)       Files/sec\n");
  printf("----            ---------       ---------\n");

  sec = ((float)create_usec) / 1000000.0;
  tp = ((float)num_files) / sec;
  total_sec += sec;
  printf("create_files    %7.3f\t\t%7.3f\n", sec, tp);

  if (sync1_usec) {
    sec = ((float)sync1_usec) / 1000000.0;
    tp = ((float)num_files) / sec;
    total_sec += sec;
    printf("sync_files_1  %7.3f\t\t%7.3f\n", sec, tp);
  }

  sec = ((float)read_usec) / 1000000.0;
  tp = ((float)num_files) / sec;
  total_sec += sec;
  printf("read_files      %7.3f\t\t%7.3f\n", sec, tp);

  sec = ((float)unlink_usec) / 1000000.0;
  tp = ((float)num_files) / sec;
  total_sec += sec;
  printf("unlink_files    %7.3f\t\t%7.3f\n", sec, tp);

  if (sync2_usec) {
    sec = ((float)sync2_usec) / 1000000.0;
    tp = ((float)num_files) / sec;
    total_sec += sec;
    printf("sync_files_2  %7.3f\t\t%7.3f\n", sec, tp);
  }

  tp = ((float)num_files) / total_sec;
  printf("core %d total   %7.3f\t\t%7.3f\n", cpu, total_sec, tp);
  fflush(stdout);
}

void create_dirs(const char *topdir, unsigned long num_dirs) {
  int ret, fd;
  unsigned long i, j;
  char dir[128], sub_dir[128];

  if (per_cpu_dirs) {
    for (j = 0; j < total_cores; j++) {
      snprintf(sub_dir, 128, "%s/cpu-%ld", topdir, j);
      if ((ret = Mkdir(sub_dir, 0777)) != 0)
        die("Mkdir %s failed %d\n", sub_dir, ret);

      fd = Open(sub_dir, O_RDONLY | O_DIRECTORY);
      if (fd >= 0) {
        if ((ret = Fsync(fd)) < 0) die("Fsync %s failed %d\n", sub_dir, ret);
        Close(fd);
      }

      fd = Open(topdir, O_RDONLY | O_DIRECTORY);
      if (fd >= 0) {
        if ((ret = Fsync(fd)) < 0) die("Fsync %s failed %d\n", topdir, ret);
        Close(fd);
      }

      for (i = 0; i < num_dirs; i++) {
        snprintf(dir, 128, "%s/cpu-%ld/dir-%ld", topdir, j, i);
        if ((ret = Mkdir(dir, 0777)) != 0)
          die("Mkdir %s failed %d\n", dir, ret);

        fd = Open(dir, O_RDONLY | O_DIRECTORY);
        if (fd >= 0) {
          if ((ret = Fsync(fd)) < 0) die("Fsync %s failed %d\n", dir, ret);
          Close(fd);
        }
      }

      fd = Open(sub_dir, O_RDONLY | O_DIRECTORY);
      if (fd >= 0) {
        if ((ret = Fsync(fd)) < 0) die("Fsync %s failed %d\n", sub_dir, ret);
        Close(fd);
      }
    }
  } else {
    for (i = 0; i < num_dirs; i++) {
      snprintf(dir, 128, "%s/dir-%ld", topdir, i);
      if ((ret = Mkdir(dir, 0777)) != 0) die("Mkdir %s failed %d\n", dir, ret);

      fd = Open(dir, O_RDONLY | O_DIRECTORY);
      if (fd >= 0) {
        if ((ret = Fsync(fd)) < 0) die("Fsync %s failed %d\n", dir, ret);
        Close(fd);
      }

      fd = Open(topdir, O_RDONLY | O_DIRECTORY);
      if (fd >= 0) {
        if ((ret = Fsync(fd)) < 0) die("Fsync %s failed %d\n", topdir, ret);
        Close(fd);
      }
    }
  }
}

uint64_t create_files(const char *topdir, char *buf, int cpu) {
  int fd;
  unsigned long i, j;
  ssize_t size;
  char filename[128];
  uint64_t before, after;

  before = now_usec();
  /* Create phase */
  for (i = 0, j = 0; i < num_files; i++) {
    if (per_cpu_dirs)
      snprintf(filename, 128, "%s/cpu-%d/dir-%ld/file-%d-%ld", topdir, cpu, j,
               cpu, i);
    else
      snprintf(filename, 128, "%s/dir-%ld/file-%d-%ld", topdir, j, cpu, i);

    fd = Open(filename, O_WRONLY | O_CREAT | O_EXCL,
              S_IWUSR | S_IRUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);

    if (fd == -1) die("Open %s failed\n", filename);

    size = Write(fd, buf, FILESIZE);
    if (size == -1) die("Write %s failed\n", filename);

    if (sync_when == FSYNC_CREATE && Fsync(fd) < 0)
      die("Fsync %s failed\n", filename);

    if (Close(fd) < 0) die("Close %s failed\n", filename);

    if ((i + 1) % nfiles_per_dir == 0) j++;
  }
  after = now_usec();
  return after - before - timer_overhead;
}

uint64_t read_files(const char *topdir, char *buf, int cpu) {
  int fd;
  unsigned long i, j;
  ssize_t size;
  char filename[128];
  uint64_t before, after;

  before = now_usec();
  /* Read phase */
  for (i = 0, j = 0; i < num_files; i++) {
    if (per_cpu_dirs)
      snprintf(filename, 128, "%s/cpu-%d/dir-%ld/file-%d-%ld", topdir, cpu, j,
               cpu, i);
    else
      snprintf(filename, 128, "%s/dir-%ld/file-%d-%ld", topdir, j, cpu, i);

    fd = Open(filename, O_RDONLY);
    if (fd == -1) die("Open %s failed\n", filename);

    size = Read(fd, buf, FILESIZE);
    if (size == -1) die("Read %s failed\n", filename);

    if (Close(fd) < 0) die("Close %s failed\n", filename);

    if ((i + 1) % nfiles_per_dir == 0) j++;
  }
  after = now_usec();
  return after - before - timer_overhead;
}

uint64_t unlink_files(const char *topdir, char *buf, int cpu) {
  int ret;
  unsigned long i, j;
  char filename[128];
  uint64_t before, after;

  before = now_usec();
  /* Unlink phase */
  for (i = 0, j = 0; i < num_files; i++) {
    if (per_cpu_dirs)
      snprintf(filename, 128, "%s/cpu-%d/dir-%ld/file-%d-%ld", topdir, cpu, j,
               cpu, i);
    else
      snprintf(filename, 128, "%s/dir-%ld/file-%d-%ld", topdir, j, cpu, i);

    ret = Unlink(filename);
    if (ret == -1) die("Unlink %s failed\n", filename);

    if ((i + 1) % nfiles_per_dir == 0) j++;
  }
  after = now_usec();
  return after - before - timer_overhead;
}

uint64_t sync_files(void) {
  uint64_t before, after;

  before = now_usec();
  /* Sync everything */
  Sync();
  after = now_usec();
  return after - before - timer_overhead;
}
