// Miscellaneous utilities
#pragma once

#include <errno.h>
#include <fcntl.h>
#include <sched.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include <stdexcept>

#ifdef USE_UFS
#include "fsapi.h"
#endif

void die(const char *errstr, ...)
    __attribute__((noreturn, __format__(__printf__, 1, 2)));
void edie(const char *errstr, ...)
    __attribute__((noreturn, __format__(__printf__, 1, 2)));
uint64_t now_usec(void);
int setaffinity(int c);

static void __attribute__((noreturn)) vdie(const char *errstr, va_list ap) {
  vfprintf(stderr, errstr, ap);
  fprintf(stderr, "\n");
  exit(1);
}

void __attribute__((noreturn)) die(const char *errstr, ...) {
  va_list ap;

  va_start(ap, errstr);
  vdie(errstr, ap);
}

void edie(const char *errstr, ...) {
  va_list ap;

  va_start(ap, errstr);
  vfprintf(stderr, errstr, ap);
  va_end(ap);
  fprintf(stderr, ": %s\n", strerror(errno));
  exit(1);
}

// It is supposed to make things handy, but somehow scalefs's code doesn't use
// it...
uint64_t now_usec(void) {
  struct timeval tv;
  if (gettimeofday(&tv, NULL) < 0) edie("gettimeofday");
  return ((uint64_t)tv.tv_sec) * 1000000ull + ((uint64_t)tv.tv_usec);
}

int setaffinity(int c) {
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  CPU_SET(c, &cpuset);
  if (sched_setaffinity(0, sizeof(cpuset), &cpuset) < 0)
    edie("setaffinity, sched_setaffinity failed");
  return 0;
}

#define NTEST 100000 /* No. of tests of gettimeofday() */

// Technically, this function is a little ill-shaped as the for-loop could be
// optimized out and return a zero... but even without optimization, this
// function will probably still return zero (<1 us)...
// Since scalefs has this piece of code for their benchmarks, we just leave it
// here... It does't help, but it doesn't hurt either (hopefully).
unsigned long get_timer_overhead() {
  uint64_t before, after, dummy;
  unsigned long timer_overhead;
  before = now_usec();
  for (int i = 0; i < NTEST; i++) dummy = now_usec();
  after = now_usec();
  return (after - before) / NTEST;
}

// uFS wrapper code below
#ifdef USE_UFS
#define Stat(pathname, statbuf) fs_stat(pathname, statbuf)
#define Fstat(fd, statbuf) fs_fstat(fd, statbuf)
// open is a little special as it may only take two args
int Open(const char *pathname, int flags, mode_t mode = 0) {
  return fs_open(pathname, flags, mode);
}
#define Close(fd) fs_close(fd)
#define Unlink(pathname) fs_unlink(pathname)
#define Mkdir(pathname, mode) fs_mkdir(pathname, mode)
#define Rmdir(pathname) fs_rmdir(pathname)
#define Fsync(fd) fs_fsync(fd)
#define Sync() fs_syncall()
#define Lseek(fd, offset, whence) fs_lseek(fd, offset, whence)
#define Read(fd, buf, count) fs_allocated_read(fd, buf, count)
#define Pread(fd, buf, count, offset) fs_allocated_pread(fd, buf, count, offset)
#define Write(fd, buf, count) fs_allocated_write(fd, buf, count)
#define Pwrite(fd, buf, count, offset) \
  fs_allocated_pwrite(fd, buf, count, offset)
#define Malloc(size) fs_malloc(size)
#define Free(ptr) fs_free(ptr)
#define MAX_SHM_KEYS 20
#define SHM_KEY_BASE 20190301
// @return aid
int initFs(char *keys_str, int &num_worker) {
  int aid = -1;
  if (!keys_str) {
    fprintf(stderr, "Shared memory keys not provided!\n");
    exit(1);
  }
  key_t keys[MAX_SHM_KEYS] = {
      0};  // 20 should be large enough (max number of workers)
  int len = 0;
  char *token = strtok(keys_str, ",");
  while (token) {
    if (len >= MAX_SHM_KEYS) {
      fprintf(stderr, "Too much keys!\n");
      exit(1);
    }
    keys[len] = atoi(token);
    if (!keys[len]) {
      fprintf(stderr, "Invalid key: %s\n", token);
      exit(1);
    }
    if (len == 0) {
      aid = keys[len];
    }
    keys[len] += SHM_KEY_BASE;
    ++len;
    token = strtok(NULL, ",");
  }
  num_worker = len;
  if (fs_init_multi(len, keys) < 0) {
    fprintf(stderr, "fs_init() error\n");
    exit(1);
  }
  return aid;
}
void exitFs() {
  if (fs_exit() < 0) fprintf(stderr, "fs_exit() error\n");
}

#else  // use POSIX apis
#define Stat(pathname, statbuf) stat(pathname, statbuf)
#define Fstat(fd, statbuf) fstat(fd, statbuf)
// open is a little special as it may only take two args
int Open(const char *pathname, int flags, mode_t mode) {
  return open(pathname, flags, mode);
}
int Open(const char *pathname, int flags) { return open(pathname, flags); }
#define Close(fd) close(fd)
#define Unlink(pathname) unlink(pathname)
#define Mkdir(pathname, mode) mkdir(pathname, mode)
#define Rmdir(pathname) rmdir(pathname)
#define Fsync(fd) fsync(fd)
#define Sync() sync()
#define Lseek(fd, offset, whence) lseek(fd, offset, whence)
#define Read(fd, buf, count) read(fd, buf, count)
#define Pread(fd, buf, count, offset) pread(fd, buf, count, offset)
#define Write(fd, buf, count) write(fd, buf, count)
#define Pwrite(fd, buf, count, offset) pwrite(fd, buf, count, offset)
#define Malloc(size) malloc(size)
#define Free(ptr) free(ptr)
void initFs(const char *keys_str) {}
void exitFs() {}
#endif  // USE_UFS
