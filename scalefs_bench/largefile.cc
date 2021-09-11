#include "benchutils/coordinator.h"
#include "libutil.h"
// default configuration described in scalefs paper
#define FILESIZE (100 << 20)
#define IOSIZE 4096
#define COORD_SHM_FNAME "/coordinator"

unsigned long timer_overhead = 0;
int per_cpu_dirs = 0;
int is_prep;      // prep or bench
int is_overwrite; // create or overwrite
int pin_core = -1;
char *topdir = NULL;
char *buf = NULL;
unsigned long file_size = FILESIZE;
int total_cores = -1;

void run_createfile_benchmark(int cpu);
void run_overwritefile_benchmark(int cpu);
void create_dirs(const char *topdir);
uint64_t create_largefile(const char *topdir, char *buf, int cpu);
uint64_t overwrite_largefile(const char *topdir, char *buf, int cpu);

void print_usage_and_exit() {
  fprintf(
      stderr,
      "Usage: largefile {--prep|--bench {create|overwrite}} [CONFIG] <working "
      "directory>\n"
      "  [CONFIG]:\n"
      "    [-p [total_cores]]: creates files under per-cpu sub-directories;\n"
      "        if with `--prep', must specify how many cores will be used\n"
      "    [-c core]: core id to pin; only valid with `--bench'\n"
      "    [-z size]: file size to create in MB (default 100); only valid with `--bench'\n\n");
  exit(1);
}

void parse_arg(int argc, char **argv) {
  char ch;
  extern char *optarg;
  extern int optind;

  if (argc < 3)
    print_usage_and_exit();
  if (strcmp(argv[1], "--prep") == 0) {
    is_prep = 1;
    optind = 2;
  } else if (strcmp(argv[1], "--bench") == 0) {
    is_prep = 0;
    if (strcmp(argv[2], "create") == 0) {
      is_overwrite = 0;
    } else if (strcmp(argv[2], "overwrite") == 0) {
      is_overwrite = 1;
    } else {
      fprintf(stderr,
              "`--bench' must specify the workload type (create/overwrite)!\n");
      print_usage_and_exit();
    }
    optind = 3;
  } else {
    fprintf(stderr, "Must specify `--prep' or `--bench'!\n");
    print_usage_and_exit();
  }

  while ((ch = getopt(argc, argv, "p::c:z:")) != -1) {
    switch (ch) {
    case 'p':
      per_cpu_dirs = 1;
      if (optarg)
        total_cores = atoi(optarg);
      break;
    case 'c':
      pin_core = atoi(optarg);
      break;
    case 'z':
      file_size = ((unsigned long)atoi(optarg)) << 20;
      break;
    default:
      print_usage_and_exit();
    }
  }
  topdir = argv[optind];
  if (!topdir)
    print_usage_and_exit();

  if (is_prep) {
    if (total_cores < 0) {
      fprintf(stderr, "Must specify total number of cores by `-p'\n");
      print_usage_and_exit();
    }
    printf("Preparing LFS-Largefile test on %s\n", topdir);
    printf("Directories are: %s\n", per_cpu_dirs ? "per-cpu" : "shared");
    fflush(stdout);
  } else {
    if (pin_core < 0) {
      fprintf(stderr, "core id not specified!\n");
      print_usage_and_exit();
    }
    timer_overhead = get_timer_overhead();
    printf("Running LFS-Largefile (%s) test on %s, on Core %d\n",
           is_overwrite ? "overwrite" : "create", topdir, pin_core);
    printf("Directories are: %s\n", per_cpu_dirs ? "per-cpu" : "shared");
    printf("File Size = %ld bytes\n", file_size);
    printf("Timer overhead = %ld us\n", timer_overhead);
    fflush(stdout);
  }
}

int main(int argc, char **argv) {
  parse_arg(argc, argv);

  initFs();

  if (is_prep) {
    create_dirs(topdir);
    Sync();
    goto done;
  }

  buf = (char *)Malloc(IOSIZE);
  if (!buf) {
    fprintf(stderr, "ERROR: Failed to allocate buffer");
    exit(1);
  }
  for (int i = 0; i < IOSIZE; i++)
		buf[i] = 'b';

  if (!is_overwrite)
    run_createfile_benchmark(pin_core);
  else
    run_overwritefile_benchmark(pin_core);
  
  Free(buf);

done:
  exitFs();
  return 0;
}

void run_createfile_benchmark(int cpu) {
  if (setaffinity(cpu) < 0)
    printf("setaffinity failed for cpu %d\n", cpu);
  CoordinatorClient cc(COORD_SHM_FNAME, 0);
  cc.notify_server_that_client_is_ready();
  cc.wait_till_server_says_start();
  uint64_t usec = create_largefile(topdir, buf, cpu);
  float sec = ((float) usec) / 1000000.0;
  float tp = ((float)(file_size >> 20)) / sec;
  printf("Test            Time(sec)       Files/sec\n");
  printf("----            ---------       ---------\n");
	printf("create_files\t%7.3f sec throughput : %7.3f MB/sec\n",
		sec, tp);

}

void run_overwritefile_benchmark(int cpu) {
  if (setaffinity(cpu) < 0)
    printf("setaffinity failed for cpu %d\n", cpu);
  CoordinatorClient cc(COORD_SHM_FNAME, 0);
  cc.notify_server_that_client_is_ready();
  cc.wait_till_server_says_start();
  int64_t usec = overwrite_largefile(topdir, buf, cpu);
  float sec = ((float) usec) / 1000000.0;
  float tp = ((float)(file_size >> 20)) / sec;
  printf("Test            Time(sec)       Files/sec\n");
  printf("----            ---------       ---------\n");
  printf("overwrite_files\t%7.3f sec throughput : %7.3f MB/sec\n",
		sec, tp);
}

void create_dirs(const char *topdir) {
  int ret, fd;
  char dir[128], sub_dir[128];

  if (per_cpu_dirs) {
    for (int i = 0; i < total_cores; i++) {
      snprintf(sub_dir, 128, "%s/cpu-%d", topdir, i);
      if ((ret = Mkdir(sub_dir, 0777)) != 0)
        die("Mkdir %s failed %d\n", sub_dir, ret);

      fd = Open(sub_dir, O_RDONLY | O_DIRECTORY);
      if (fd >= 0) {
        if ((ret = Fsync(fd)) < 0)
          die("Fsync %s failed %d\n", sub_dir, ret);
        Close(fd);
      }

      fd = Open(topdir, O_RDONLY | O_DIRECTORY);
      if (fd >= 0) {
        if ((ret = Fsync(fd)) < 0)
          die("Fsync %s failed %d\n", topdir, ret);
        Close(fd);
      }
    }
  } else {
    snprintf(dir, 128, "%s/dir", topdir);
    if ((ret = Mkdir(dir, 0777)) != 0)
      die("Mkdir %s failed %d\n", dir, ret);

    fd = Open(dir, O_RDONLY | O_DIRECTORY);
    if (fd >= 0) {
      if ((ret = Fsync(fd)) < 0)
        die("Fsync %s failed %d\n", dir, ret);
      Close(fd);
    }

    fd = Open(topdir, O_RDONLY | O_DIRECTORY);
    if (fd >= 0) {
      if ((ret = Fsync(fd)) < 0)
        die("Fsync %s failed %d\n", topdir, ret);
      Close(fd);
    }
  }
}

uint64_t create_largefile(const char *topdir, char *buf, int cpu) {
  char filename[128];
  uint64_t before, after;

  before = now_usec();
  /* Create and Fsync a large file. */
  if (per_cpu_dirs)
    snprintf(filename, 128, "%s/cpu-%d/largefile", topdir, cpu);
  else
    snprintf(filename, 128, "%s/dir/largefile-%d", topdir, cpu);

  int fd = Open(filename, O_WRONLY | O_CREAT | O_EXCL,
                S_IWUSR | S_IRUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
  if (fd < 0)
    die("Open");

  int count = file_size / IOSIZE;
  for (int i = 0; i < count; i++) {
    if (Write(fd, buf, IOSIZE) != IOSIZE)
      die("Write");
  }

  if (Fsync(fd) < 0)
    die("Fsync");

  Close(fd);
  after = now_usec();
  return after - before - timer_overhead;
}

uint64_t overwrite_largefile(const char *topdir, char *buf, int cpu) {
  char filename[128];
  uint64_t before, after;

  before = now_usec();
  /* Create and Fsync a large file. */
  if (per_cpu_dirs)
    snprintf(filename, 128, "%s/cpu-%d/largefile", topdir, cpu);
  else
    snprintf(filename, 128, "%s/dir/largefile-%d", topdir, cpu);

  int fd = Open(filename, O_WRONLY);
  if (fd < 0)
    die("Open");

  if (Lseek(fd, 0, SEEK_SET) < 0)
    die("Lseek");

  int count = file_size / IOSIZE;
  for (int i = 0; i < count; i++) {
    if (Write(fd, buf, IOSIZE) != IOSIZE)
      die("Write");
  }

  if (Fsync(fd) < 0)
    die("Fsync");

  Close(fd);
  after = now_usec();
  return after - before - timer_overhead;
}
