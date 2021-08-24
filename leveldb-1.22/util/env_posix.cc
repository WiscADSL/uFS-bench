// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#include <dirent.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include <atomic>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <queue>
#include <set>
#include <string>
#include <thread>
#include <type_traits>
#include <unordered_map>
#include <utility>

#include "leveldb/env.h"
#include "leveldb/slice.h"
#include "leveldb/status.h"
#include "port/port.h"
#include "port/thread_annotations.h"
#include "util/env_posix_test_helper.h"
#include "util/posix_logger.h"

int g_appid = 0;

inline uint64_t get_current_tid() {
  pid_t curPid = syscall(__NR_gettid);
  return curPid;
}

#ifdef JL_LIBCFS

extern thread_local int threadFsTid;
int g_num_workers = 0;

#define FS_SHM_KEY_BASE 20190301
#define FS_MAX_NUM_WORKER 20

static const char* FSP_ENV_VAR_KEY_LIST = "FSP_KEY_LISTS";
static const char* g_fsp_num_workers_str = nullptr;
static key_t g_shm_keys[FS_MAX_NUM_WORKER];

static void clean_exit() { exit(fs_exit()); };

static void init_shm_keys(char* keys) {
  char* token = strtok(keys, ",");
  g_appid = atoi(token);
  key_t cur_key;
  int num_workers = 0;
  while (token != NULL) {
    cur_key = atoi(token);
    fprintf(stdout, "cur_key:%d\n", cur_key);
    g_shm_keys[num_workers++] = (FS_SHM_KEY_BASE) + cur_key;
    token = strtok(NULL, ",");
  }
  g_num_workers = num_workers;
}

static void init_fsp_access() {
  assert(g_fsp_num_workers_str == nullptr);
  g_fsp_num_workers_str = getenv(FSP_ENV_VAR_KEY_LIST);
  if (g_fsp_num_workers_str == nullptr) {
    fprintf(stderr, "ERROR env_key_str not set. set to 1 by default\n");
    g_fsp_num_workers_str = "1";
  }
  char* workers_str = strdup(g_fsp_num_workers_str);
  init_shm_keys(workers_str);
  int rt = fs_init_multi(g_num_workers, g_shm_keys);
  if (rt < 0) {
    fprintf(stderr, "cannot connect to FSP\n");
    exit(1);
  }
  free(workers_str);
  fs_init_thread_local_mem();
  if (g_appid == 1) {
    fs_start_dump_load_stats();
  }
  // int target = g_appid - 1;
  // fs_admin_thread_reassign(0, target % g_num_workers, FS_REASSIGN_FUTURE);
  fprintf(stderr, "init_fsp_access() DONE\n");
}

#endif

static int kSaveFd = -1;
const static char kSaveFname[] = "pread_result.txt";
const static char kWriteResultFname[] = "write_result.txt";

static void dump_write_result(const char* prdata, const char* fname, int fd,
                              size_t cnt, ssize_t ret_cnt) {
  if (kSaveFd < 0) {
    kSaveFd = open(kWriteResultFname, O_CREAT | O_RDWR, 0755);
    if (kSaveFd < 0) {
      fprintf(stderr, "cannot open save file\n");
    }
  }
  char head_str[100] = "";
  std::string s1(fname);
  std::string s2("000044.log");
  // std::string s2("MANIFEST-000002");
  if (s2.empty() || s1.find(s2) != std::string::npos) {
    sprintf(head_str, "write fname:%s fd:%d cnt:%lu ret:%ld\n", fname, fd, cnt,
            ret_cnt);
    write(kSaveFd, head_str, strlen(head_str));
    if (ret_cnt > 0) {
      write(kSaveFd, prdata, ret_cnt);
      write(kSaveFd, "\n", 1);
    }
    fsync(kSaveFd);
  }
}

static void dump_pread_result(char* prdata, const char* fname, int fd,
                              uint64_t off, size_t cnt, ssize_t ret_cnt) {
  if (kSaveFd < 0) {
    kSaveFd = open(kSaveFname, O_CREAT | O_RDWR, 0755);
    if (kSaveFd < 0) {
      fprintf(stderr, "cannot open save file\n");
    }
  }
  char head_str[100] = "";
  sprintf(head_str, "pread fname:%s fd:%d off:%lu cnt:%lu ret:%ld\n", fname, fd,
          off, cnt, ret_cnt);
  write(kSaveFd, head_str, strlen(head_str));
  if (ret_cnt > 0) {
    write(kSaveFd, prdata, ret_cnt);
    write(kSaveFd, "\n", 1);
  }
  fsync(kSaveFd);
}

namespace leveldb {

namespace {

// Set by EnvPosixTestHelper::SetReadOnlyMMapLimit() and MaxOpenFiles().
int g_open_read_only_file_limit = -1;

#ifdef JL_LIBCFS
// We do not support mmap() now.
constexpr const int kDefaultMmapLimit = 0;
#else
// Up to 1000 mmap regions for 64-bit binaries; none for 32-bit.
// constexpr const int kDefaultMmapLimit = (sizeof(void*) >= 8) ? 1000 : 0;
constexpr const int kDefaultMmapLimit = 0;
#endif

// Can be set using EnvPosixTestHelper::SetReadOnlyMMapLimit.
int g_mmap_limit = kDefaultMmapLimit;

constexpr const size_t kWritableFileBufferSize = 65536;

Status PosixError(const std::string& context, int error_number) {
  if (error_number == ENOENT) {
    return Status::NotFound(context, std::strerror(error_number));
  } else {
    return Status::IOError(context, std::strerror(error_number));
  }
}

// Helper class to limit resource usage to avoid exhaustion.
// Currently used to limit read-only file descriptors and mmap file usage
// so that we do not run out of file descriptors or virtual memory, or run into
// kernel performance problems for very large databases.
class Limiter {
 public:
  // Limit maximum number of resources to |max_acquires|.
  Limiter(int max_acquires) : acquires_allowed_(max_acquires) {}

  Limiter(const Limiter&) = delete;
  Limiter operator=(const Limiter&) = delete;

  // If another resource is available, acquire it and return true.
  // Else return false.
  bool Acquire() {
    int old_acquires_allowed =
        acquires_allowed_.fetch_sub(1, std::memory_order_relaxed);

    if (old_acquires_allowed > 0) return true;

    acquires_allowed_.fetch_add(1, std::memory_order_relaxed);
    return false;
  }

  // Release a resource acquired by a previous call to Acquire() that returned
  // true.
  void Release() { acquires_allowed_.fetch_add(1, std::memory_order_relaxed); }

 private:
  // The number of available resources.
  //
  // This is a counter and is not tied to the invariants of any other class, so
  // it can be operated on safely using std::memory_order_relaxed.
  std::atomic<int> acquires_allowed_;
};

// Implements sequential read access in a file using read().
//
// Instances of this class are thread-friendly but not thread-safe, as required
// by the SequentialFile API.
class PosixSequentialFile final : public SequentialFile {
 public:
  PosixSequentialFile(std::string filename, int fd)
      : fd_(fd), filename_(filename) {}
  ~PosixSequentialFile() override { close(fd_); }

  Status Read(size_t n, Slice* result, char* scratch) override {
    Status status;
    while (true) {
#ifdef JL_LIBCFS
      ::size_t read_size = fs_allocated_read(fd_, scratch, n);
      // fprintf(stdout, "fs_allocated_read fd:%d n:%ld size:%ld\n", fd_, n,
      // read_size);
#else
      ::ssize_t read_size = ::read(fd_, scratch, n);
#endif
      if (read_size < 0) {  // Read error.
        if (errno == EINTR) {
          continue;  // Retry
        }
        fprintf(stderr, "env_posix.cc: Read() error\n");
        status = PosixError(filename_, errno);
        break;
      }
      *result = Slice(scratch, read_size);
      break;
    }
    return status;
  }

  Status Skip(uint64_t n) override {
    fprintf(stdout, "lseek is not supported yet\n");
    exit(-1);
    if (::lseek(fd_, n, SEEK_CUR) == static_cast<off_t>(-1)) {
      return PosixError(filename_, errno);
    }
    return Status::OK();
  }

 private:
  const int fd_;
  const std::string filename_;
};

// Implements random read access in a file using pread().
//
// Instances of this class are thread-safe, as required by the RandomAccessFile
// API. Instances are immutable and Read() only calls thread-safe library
// functions.
class PosixRandomAccessFile final : public RandomAccessFile {
 public:
  // The new instance takes ownership of |fd|. |fd_limiter| must outlive this
  // instance, and will be used to determine if .
  PosixRandomAccessFile(std::string filename, int fd, Limiter* fd_limiter)
      : has_permanent_fd_(fd_limiter->Acquire()),
        fd_(has_permanent_fd_ ? fd : -1),
        fd_limiter_(fd_limiter),
        filename_(std::move(filename)) {
    if (!has_permanent_fd_) {
      assert(fd_ == -1);
#ifdef JL_LIBCFS
      fs_close(fd);  // The file will be opened on every read.
#else
      ::close(fd);  // The file will be opened on every read.
#endif
    }
  }

  ~PosixRandomAccessFile() override {
    if (has_permanent_fd_) {
      assert(fd_ != -1);
#ifdef JL_LIBCFS
      fs_close(fd_);
#else
      ::close(fd_);
#endif
      fd_limiter_->Release();
    }
  }

  Status Read(uint64_t offset, size_t n, Slice* result,
              char* scratch) const override {
    int fd = fd_;
    if (!has_permanent_fd_) {
#ifdef JL_LIBCFS
      fd = fs_open2(filename_.c_str(), O_RDONLY);
#else
      fd = ::open(filename_.c_str(), O_RDONLY);
#endif
      if (fd < 0) {
        return PosixError(filename_, errno);
      }
    }

    assert(fd != -1);

    Status status;
#ifdef JL_LIBCFS
    // ssize_t read_size = fs_pread(fd, scratch, n, static_cast<off_t>(offset));
    // fprintf(stderr, "alloc_read ptr%p\n", scratch);
    if (scratch == nullptr) {
      throw std::runtime_error("");
    }
#ifdef JL_LIBCFS_CPC
    ssize_t read_size =
        fs_cpc_pread(fd, scratch, n, static_cast<off_t>(offset));
#else
    ssize_t read_size =
        fs_allocated_pread(fd, scratch, n, static_cast<off_t>(offset));
#endif  // JL_LIBCFS_CPC
    // dump_pread_result(scratch, filename_.c_str(), fd, offset, n, read_size);
    // fprintf(stdout, "fs_pread(fd:%d n=%ld offset:%lu) ret:%ld\n", fd, n,
    // offset, read_size);
#else
    ssize_t read_size = ::pread(fd, scratch, n, static_cast<off_t>(offset));
    // dump_pread_result(scratch, filename_.c_str(),fd, offset, n, read_size);
#endif
    *result = Slice(scratch, (read_size < 0) ? 0 : read_size);
    if (read_size < 0) {
      fprintf(stderr,
              "env_posix.cc: pread() error fname:%s offset:%lu n:%ld "
              "read_size:%ld\n",
              filename_.c_str(), offset, n, read_size);
      // An error: return a non-ok status.
      throw std::runtime_error("pread error");
      status = PosixError(filename_, errno);
    }
    if (!has_permanent_fd_) {
      // Close the temporary file descriptor opened earlier.
      assert(fd != fd_);
#ifdef JL_LIBCFS
      fs_close(fd);
#else
      ::close(fd);
#endif
    }
    if (!status.ok()) {
      fprintf(stderr, "env_posix.cc: Read() return error\n");
    }
    return status;
  }

 private:
  const bool has_permanent_fd_;  // If false, the file is opened on every read.
  const int fd_;                 // -1 if has_permanent_fd_ is false.
  Limiter* const fd_limiter_;
  const std::string filename_;
};

// Implements random read access in a file using mmap().
//
// Instances of this class are thread-safe, as required by the RandomAccessFile
// API. Instances are immutable and Read() only calls thread-safe library
// functions.
class PosixMmapReadableFile final : public RandomAccessFile {
 public:
  // mmap_base[0, length-1] points to the memory-mapped contents of the file. It
  // must be the result of a successful call to mmap(). This instances takes
  // over the ownership of the region.
  //
  // |mmap_limiter| must outlive this instance. The caller must have already
  // aquired the right to use one mmap region, which will be released when this
  // instance is destroyed.
  PosixMmapReadableFile(std::string filename, char* mmap_base, size_t length,
                        Limiter* mmap_limiter)
      : mmap_base_(mmap_base),
        length_(length),
        mmap_limiter_(mmap_limiter),
        filename_(std::move(filename)) {}

  ~PosixMmapReadableFile() override {
    ::munmap(static_cast<void*>(mmap_base_), length_);
    mmap_limiter_->Release();
  }

  Status Read(uint64_t offset, size_t n, Slice* result,
              char* scratch) const override {
    if (offset + n > length_) {
      *result = Slice();
      return PosixError(filename_, EINVAL);
    }

    *result = Slice(mmap_base_ + offset, n);
    return Status::OK();
  }

 private:
  char* const mmap_base_;
  const size_t length_;
  Limiter* const mmap_limiter_;
  const std::string filename_;
};

class FSPRandomAccessFile final : public RandomAccessFile {
 public:
  // The new instance takes ownership of |fd|. |fd_limiter| must outlive this
  // instance, and will be used to determine if .
  FSPRandomAccessFile(std::string filename, int fd, Limiter* fd_limiter)
      : has_permanent_fd_(fd_limiter->Acquire()),
        fd_(has_permanent_fd_ ? fd : -1),
        fd_limiter_(fd_limiter),
        filename_(std::move(filename)) {
    if (!has_permanent_fd_) {
      assert(fd_ == -1);
#ifdef JL_LIBCFS
      fs_close(fd);  // The file will be opened on every read.
#else
      ::close(fd);  // The file will be opened on every read.
#endif
    }
  }

  ~FSPRandomAccessFile() override {
    if (has_permanent_fd_) {
      assert(fd_ != -1);
#ifdef JL_LIBCFS
      fs_close_ldb(fd_);
#else
      ::close(fd_);
#endif
      fd_limiter_->Release();
    }
  }

  Status Read(uint64_t offset, size_t n, Slice* result,
              char* scratch) const override {
    int fd = fd_;
    if (!has_permanent_fd_) {
#ifdef JL_LIBCFS
      fd = fs_open2(filename_.c_str(), O_RDONLY);
#else
      fd = ::open(filename_.c_str(), O_RDONLY);
#endif
      if (fd < 0) {
        return PosixError(filename_, errno);
      }
    }

    assert(fd != -1);

    Status status;
#ifdef JL_LIBCFS
    // ssize_t read_size = fs_pread(fd, scratch, n, static_cast<off_t>(offset));
    // fprintf(stderr, "alloc_read ptr%p\n", scratch);
    if (scratch == nullptr) {
      throw std::runtime_error("");
    }

    ssize_t read_size =
        fs_allocated_pread_ldb(fd, scratch, n, static_cast<off_t>(offset));

    // dump_pread_result(scratch, filename_.c_str(), fd, offset, n, read_size);
    // fprintf(stdout, "fs_pread(fd:%d n=%ld offset:%lu) ret:%ld\n", fd, n,
    // offset, read_size);
#else
    ssize_t read_size = ::pread(fd, scratch, n, static_cast<off_t>(offset));
    // dump_pread_result(scratch, filename_.c_str(),fd, offset, n, read_size);
#endif
    *result = Slice(scratch, (read_size < 0) ? 0 : read_size);
    if (read_size < 0) {
      fprintf(stderr,
              "env_posix.cc: pread() error fname:%s offset:%lu n:%ld "
              "read_size:%ld\n",
              filename_.c_str(), offset, n, read_size);
      // An error: return a non-ok status.
      throw std::runtime_error("pread error");
      status = PosixError(filename_, errno);
    }
    if (!has_permanent_fd_) {
      // Close the temporary file descriptor opened earlier.
      assert(fd != fd_);
#ifdef JL_LIBCFS
      fs_close(fd);
#else
      ::close(fd);
#endif
    }
    if (!status.ok()) {
      fprintf(stderr, "env_posix.cc: Read() return error\n");
    }
    return status;
  }

 private:
  const bool has_permanent_fd_;  // If false, the file is opened on every read.
  const int fd_;                 // -1 if has_permanent_fd_ is false.
  Limiter* const fd_limiter_;
  const std::string filename_;
};


class PosixWritableFile final : public WritableFile {
 public:
  PosixWritableFile(std::string filename, int fd)
      : fd_(fd),
        is_manifest_(IsManifest(filename)),
        filename_(std::move(filename)),
        dirname_(Dirname(filename_)) {
    tid_ = get_current_tid();
#ifdef JL_LIBCFS
    // buf_ = static_cast<char*>(fs_malloc(kWritableFileBufferSize));
    // fprintf(stderr, "buf_:%p filename_:%s\n", buf_, filename_.c_str());
#else
    // buf_ = static_cast<char*>(malloc(kWritableFileBufferSize));
#endif
  }

  ~PosixWritableFile() override {
    if (fd_ >= 0) {
      // Ignoring any potential errors
      Close();
    }
    // uint64_t curTid = get_current_tid();
    // if (curTid != tid_) {
    //  fprintf(stderr, "start_tid:%lu end_tid:%lu\n", tid_, curTid);
    //}
    auto tid = std::this_thread::get_id();
    auto it = buf_map_.find(tid);
    assert(it != buf_map_.end());
#ifdef JL_LIBCFS
    fs_free((void*)(it->second));
#else
    free((void*)(it->second));
#endif
  }

  Status Append(const Slice& data) override {
    size_t write_size = data.size();
    const char* write_data = data.data();

    char* cur_buf = GetCurThreadBuf();

    // Fit as much as possible into buffer.
    size_t copy_size =
        std::min(write_size, kWritableFileBufferSize - pos_map_[cur_buf]);
    assert(cur_buf != nullptr);
    std::memcpy(cur_buf + pos_map_[cur_buf], write_data, copy_size);
    write_data += copy_size;
    write_size -= copy_size;
    // pos_ += copy_size;
    pos_map_[cur_buf] += copy_size;
    if (write_size == 0) {
      return Status::OK();
    }

    // Can't fit in buffer, so need to do at least one write.
    Status status = FlushBuffer();
    if (!status.ok()) {
      return status;
    }

    // Small writes go to buffer, large writes are written directly.
    if (write_size < kWritableFileBufferSize) {
      // std::memcpy(buf_, write_data, write_size);
      std::memcpy(cur_buf, write_data, write_size);
      // pos_ = write_size;
      pos_map_[cur_buf] = write_size;
      return Status::OK();
    }
    // return WriteUnbuffered(write_data, write_size);
#ifdef JL_LIBCFS
    fprintf(stderr, "WARN: Call WriteUnbuffered with extra copy");
    char* write_data_buf = (char*)fs_malloc(write_size);
    Status s = WriteUnbuffered(write_data_buf, write_size);
    fs_free((void*)write_data_buf);
    throw std::runtime_error("writeUnbuffered with extra copy");
    return s;
#else
    return WriteUnbuffered(write_data, write_size);
#endif
  }

  Status Close() override {
    Status status = FlushBuffer();
#ifdef JL_LIBCFS
    const int close_result = fs_close(fd_);
#else
    const int close_result = ::close(fd_);
#endif
    if (close_result < 0 && status.ok()) {
      status = PosixError(filename_, errno);
    }
    fd_ = -1;
    return status;
  }

  Status Flush() override { return FlushBuffer(); }

  Status Sync() override {
    // Ensure new files referred to by the manifest are in the filesystem.
    //
    // This needs to happen before the manifest file is flushed to disk, to
    // avoid crashing in a state where the manifest refers to files that are not
    // yet on disk.
    Status status = SyncDirIfManifest();
    if (!status.ok()) fprintf(stderr, "Sync()-1 s:?%d\n", status.ok());
    if (!status.ok()) {
      return status;
    }

    status = FlushBuffer();
    if (!status.ok()) fprintf(stderr, "Sync()-2 s:?%d\n", status.ok());
    if (!status.ok()) {
      return status;
    }
    return SyncFd(fd_, filename_);
  }

 private:
  char* GetCurThreadBuf() {
    auto cur_id = std::this_thread::get_id();
    auto it = buf_map_.find(cur_id);
    if (it == buf_map_.end()) {
      char* cur_buf = nullptr;
#ifdef JL_LIBCFS
      cur_buf = static_cast<char*>(fs_malloc(kWritableFileBufferSize));
#else
      cur_buf = static_cast<char*>(malloc(kWritableFileBufferSize));
#endif
      buf_map_.emplace(cur_id, cur_buf);
      pos_map_.emplace(cur_buf, 0);
      return cur_buf;
    } else {
      return it->second;
    }
  }
  Status FlushBuffer() {
    char* cur_buf = GetCurThreadBuf();
    Status status = WriteUnbuffered(GetCurThreadBuf(), pos_map_[cur_buf]);
    pos_map_[cur_buf] = 0;
    // pos_ = 0;
    return status;
  }

  Status WriteUnbuffered(const char* data, size_t size) {
    while (size > 0) {
#ifdef JL_LIBCFS
      // ssize_t write_result = fs_write(fd_, data, size);
      char* data_ptr = const_cast<char*>(data);
      ssize_t write_result = fs_allocated_write(fd_, data_ptr, size);
      // if (write_result != size) {
      //   fprintf(stderr, ">>>>>>>ERROR write size not match\n");
      // }
      // if (size > 0) {
      //   dump_write_result(data, filename_.c_str(), 0, size, write_result);
      // }
#else
      ssize_t write_result = ::write(fd_, data, size);
      // if (write_result != size) {
      //   fprintf(stderr, ">>>>>>>ERROR write size not match\n");
      // }
      // if (size > 0) {
      //   dump_write_result(data, filename_.c_str(), 0, size, write_result);
      // }
#endif
      if (write_result < 0) {
        if (errno == EINTR) {
          continue;  // Retry
        }
        fprintf(stderr, "env_posix.cc:write error filename_%s return:%ld\n",
                filename_.c_str(), write_result);
        return PosixError(filename_, errno);
      }
      data += write_result;
      size -= write_result;
    }
    return Status::OK();
  }

  Status SyncDirIfManifest() {
    Status status;
    if (!is_manifest_) {
      return status;
    }

#ifdef JL_LIBCFS
    int fd = fs_open2(dirname_.c_str(), O_RDONLY);
#else
    int fd = ::open(dirname_.c_str(), O_RDONLY);
#endif
    if (fd < 0) {
      status = PosixError(dirname_, errno);
    } else {
      status = SyncFd(fd, dirname_);
#ifdef JL_LIBCFS
      fs_close(fd);
#else
      ::close(fd);
#endif
    }
    if (!status.ok()) {
      fprintf(stderr, "env_posix.cc:SyncDirIfManifest error\n");
    }
    return status;
  }

  // Ensures that all the caches associated with the given file descriptor's
  // data are flushed all the way to durable media, and can withstand power
  // failures.
  //
  // The path argument is only used to populate the description string in the
  // returned Status if an error occurs.
  static Status SyncFd(int fd, const std::string& fd_path) {
#if HAVE_FULLFSYNC
    // On macOS and iOS, fsync() doesn't guarantee durability past power
    // failures. fcntl(F_FULLFSYNC) is required for that purpose. Some
    // filesystems don't support fcntl(F_FULLFSYNC), and require a fallback to
    // fsync().
    if (::fcntl(fd, F_FULLFSYNC) == 0) {
      return Status::OK();
    }
#endif  // HAVE_FULLFSYNC

#if HAVE_FDATASYNC
#ifdef JL_LIBCFS
    bool sync_success = fs_fdatasync(fd) == 0;
#else
    bool sync_success = ::fdatasync(fd) == 0;
#endif  // JL_LIBCFS
#else
#ifdef JL_LIBCFS
    bool sync_success = fs_fsync(fd) == 0;
#else
    bool sync_success = ::fsync(fd) == 0;
#endif  // JL_LIBCFS
#endif  // HAVE_FDATASYNC

    if (sync_success) {
      return Status::OK();
    } else {
      fprintf(stderr, "env_posix.cc:SyncFd fsync() error path:%s\n",
              fd_path.c_str());
    }
    return PosixError(fd_path, errno);
  }

  // Returns the directory name in a path pointing to a file.
  //
  // Returns "." if the path does not contain any directory separator.
  static std::string Dirname(const std::string& filename) {
    std::string::size_type separator_pos = filename.rfind('/');
    if (separator_pos == std::string::npos) {
      return std::string(".");
    }
    // The filename component should not contain a path separator. If it does,
    // the splitting was done incorrectly.
    assert(filename.find('/', separator_pos + 1) == std::string::npos);

    return filename.substr(0, separator_pos);
  }

  // Extracts the file name from a path pointing to a file.
  //
  // The returned Slice points to |filename|'s data buffer, so it is only valid
  // while |filename| is alive and unchanged.
  static Slice Basename(const std::string& filename) {
    std::string::size_type separator_pos = filename.rfind('/');
    if (separator_pos == std::string::npos) {
      return Slice(filename);
    }
    // The filename component should not contain a path separator. If it does,
    // the splitting was done incorrectly.
    assert(filename.find('/', separator_pos + 1) == std::string::npos);

    return Slice(filename.data() + separator_pos + 1,
                 filename.length() - separator_pos - 1);
  }

  // True if the given file is a manifest file.
  static bool IsManifest(const std::string& filename) {
    return Basename(filename).starts_with("MANIFEST");
  }

  // buf_[0, pos_ - 1] contains data to be written to fd_.
  // char buf_[kWritableFileBufferSize];
  // char* buf_{nullptr};
  std::unordered_map<std::thread::id, char*> buf_map_;
  std::unordered_map<char*, size_t> pos_map_;
  // size_t pos_;
  int fd_;
  uint64_t tid_;

  const bool is_manifest_;  // True if the file's name starts with MANIFEST.
  const std::string filename_;
  const std::string dirname_;  // The directory of filename_.
};

class FSPWritableFile final : public WritableFile {
 public:
  FSPWritableFile(std::string filename, int fd)
      : fd_(fd),
        is_manifest_(IsManifest(filename)),
        filename_(std::move(filename)),
        dirname_(Dirname(filename_)) {
    tid_ = get_current_tid();
#ifdef JL_LIBCFS
    // buf_ = static_cast<char*>(fs_malloc(kWritableFileBufferSize));
    // fprintf(stderr, "buf_:%p filename_:%s\n", buf_, filename_.c_str());
#else
    // buf_ = static_cast<char*>(malloc(kWritableFileBufferSize));
#endif
  }

  ~FSPWritableFile() override {
    if (fd_ >= 0) {
      // Ignoring any potential errors
      Close();
    }
    // uint64_t curTid = get_current_tid();
    // if (curTid != tid_) {
    //  fprintf(stderr, "start_tid:%lu end_tid:%lu\n", tid_, curTid);
    //}
    auto tid = std::this_thread::get_id();
    auto it = buf_map_.find(tid);
    assert(it != buf_map_.end());
#ifdef JL_LIBCFS
    fs_free((void*)(it->second));
#else
    free((void*)(it->second));
#endif
  }

  Status Append(const Slice& data) override {
    size_t write_size = data.size();
    const char* write_data = data.data();

    char* cur_buf = GetCurThreadBuf();

    // Fit as much as possible into buffer.
    size_t copy_size =
        std::min(write_size, kWritableFileBufferSize - pos_map_[cur_buf]);
    assert(cur_buf != nullptr);
    std::memcpy(cur_buf + pos_map_[cur_buf], write_data, copy_size);
    write_data += copy_size;
    write_size -= copy_size;
    // pos_ += copy_size;
    pos_map_[cur_buf] += copy_size;
    if (write_size == 0) {
      return Status::OK();
    }

    // Can't fit in buffer, so need to do at least one write.
    Status status = FlushBuffer();
    if (!status.ok()) {
      return status;
    }

    // Small writes go to buffer, large writes are written directly.
    if (write_size < kWritableFileBufferSize) {
      // std::memcpy(buf_, write_data, write_size);
      std::memcpy(cur_buf, write_data, write_size);
      // pos_ = write_size;
      pos_map_[cur_buf] = write_size;
      return Status::OK();
    }
    // return WriteUnbuffered(write_data, write_size);
#ifdef JL_LIBCFS
    fprintf(stderr, "WARN: Call WriteUnbuffered with extra copy");
    char* write_data_buf = (char*)fs_malloc(write_size);
    Status s = WriteUnbuffered(write_data_buf, write_size);
    fs_free((void*)write_data_buf);
    throw std::runtime_error("writeUnbuffered with extra copy");
    return s;
#else
    return WriteUnbuffered(write_data, write_size);
#endif
  }

  Status Close() override {
    Status status = FlushBuffer();
#ifdef JL_LIBCFS
    const int close_result = fs_close_ldb(fd_);
#else
    const int close_result = ::close(fd_);
#endif
    if (close_result < 0 && status.ok()) {
      status = PosixError(filename_, errno);
    }
    fd_ = -1;
    return status;
  }

  Status Flush() override { return FlushBuffer(); }

  Status Sync() override {
    // Ensure new files referred to by the manifest are in the filesystem.
    //
    // This needs to happen before the manifest file is flushed to disk, to
    // avoid crashing in a state where the manifest refers to files that are not
    // yet on disk.
    Status status = SyncDirIfManifest();
    if (!status.ok()) fprintf(stderr, "Sync()-1 s:?%d\n", status.ok());
    if (!status.ok()) {
      return status;
    }

    status = FlushBuffer();
    if (!status.ok()) fprintf(stderr, "Sync()-2 s:?%d\n", status.ok());
    if (!status.ok()) {
      return status;
    }
    return SyncFd(fd_, filename_);
  }

 protected:
  char* GetCurThreadBuf() {
    auto cur_id = std::this_thread::get_id();
    auto it = buf_map_.find(cur_id);
    if (it == buf_map_.end()) {
      char* cur_buf = nullptr;
#ifdef JL_LIBCFS
      cur_buf = static_cast<char*>(fs_malloc(kWritableFileBufferSize));
#else
      cur_buf = static_cast<char*>(malloc(kWritableFileBufferSize));
#endif
      buf_map_.emplace(cur_id, cur_buf);
      pos_map_.emplace(cur_buf, 0);
      return cur_buf;
    } else {
      return it->second;
    }
  }
  Status FlushBuffer() {
    char* cur_buf = GetCurThreadBuf();
    Status status = WriteUnbuffered(GetCurThreadBuf(), pos_map_[cur_buf]);
    pos_map_[cur_buf] = 0;
    // pos_ = 0;
    return status;
  }

  Status WriteUnbuffered(const char* data, size_t size) {
    while (size > 0) {
#ifdef JL_LIBCFS
      // ssize_t write_result = fs_write(fd_, data, size);
      char* data_ptr = const_cast<char*>(data);
      ssize_t write_result = fs_allocated_write_ldb(fd_, data_ptr, size);
      // if (write_result != size) {
      //   fprintf(stderr, ">>>>>>>ERROR write size not match\n");
      // }
      // if (size > 0) {
      //   dump_write_result(data, filename_.c_str(), 0, size, write_result);
      // }
#else
      ssize_t write_result = ::write(fd_, data, size);
      // if (write_result != size) {
      //   fprintf(stderr, ">>>>>>>ERROR write size not match\n");
      // }
      // if (size > 0) {
      //   dump_write_result(data, filename_.c_str(), 0, size, write_result);
      // }
#endif
      if (write_result < 0) {
        if (errno == EINTR) {
          continue;  // Retry
        }
        fprintf(stderr, "env_posix.cc:write error filename_%s return:%ld\n",
                filename_.c_str(), write_result);
        return PosixError(filename_, errno);
      }
      data += write_result;
      size -= write_result;
    }
    return Status::OK();
  }

  Status SyncDirIfManifest() {
    Status status;
    if (!is_manifest_) {
      return status;
    }

#ifdef JL_LIBCFS
    int fd = fs_open2(dirname_.c_str(), O_RDONLY);
#else
    int fd = ::open(dirname_.c_str(), O_RDONLY);
#endif
    if (fd < 0) {
      status = PosixError(dirname_, errno);
    } else {
      status = SyncFd(fd, dirname_);
#ifdef JL_LIBCFS
      fs_close(fd);
#else
      ::close(fd);
#endif
    }
    if (!status.ok()) {
      fprintf(stderr, "env_posix.cc:SyncDirIfManifest error\n");
    }
    return status;
  }

  // Ensures that all the caches associated with the given file descriptor's
  // data are flushed all the way to durable media, and can withstand power
  // failures.
  //
  // The path argument is only used to populate the description string in the
  // returned Status if an error occurs.
  static Status SyncFd(int fd, const std::string& fd_path) {
#if HAVE_FULLFSYNC
    // On macOS and iOS, fsync() doesn't guarantee durability past power
    // failures. fcntl(F_FULLFSYNC) is required for that purpose. Some
    // filesystems don't support fcntl(F_FULLFSYNC), and require a fallback to
    // fsync().
    if (::fcntl(fd, F_FULLFSYNC) == 0) {
      return Status::OK();
    }
#endif  // HAVE_FULLFSYNC

#if HAVE_FDATASYNC
#ifdef JL_LIBCFS
    bool sync_success = fs_wsync(fd) == 0;
#else
    bool sync_success = ::fdatasync(fd) == 0;
#endif  // JL_LIBCFS
#else
#ifdef JL_LIBCFS
    bool sync_success = fs_wsync(fd) == 0;
#else
    bool sync_success = ::fsync(fd) == 0;
#endif  // JL_LIBCFS
#endif  // HAVE_FDATASYNC

    if (sync_success) {
      return Status::OK();
    } else {
      fprintf(stderr, "env_posix.cc:SyncFd fsync() error path:%s\n",
              fd_path.c_str());
    }
    return PosixError(fd_path, errno);
  }

  // Returns the directory name in a path pointing to a file.
  //
  // Returns "." if the path does not contain any directory separator.
  static std::string Dirname(const std::string& filename) {
    std::string::size_type separator_pos = filename.rfind('/');
    if (separator_pos == std::string::npos) {
      return std::string(".");
    }
    // The filename component should not contain a path separator. If it does,
    // the splitting was done incorrectly.
    assert(filename.find('/', separator_pos + 1) == std::string::npos);

    return filename.substr(0, separator_pos);
  }

  // Extracts the file name from a path pointing to a file.
  //
  // The returned Slice points to |filename|'s data buffer, so it is only valid
  // while |filename| is alive and unchanged.
  static Slice Basename(const std::string& filename) {
    std::string::size_type separator_pos = filename.rfind('/');
    if (separator_pos == std::string::npos) {
      return Slice(filename);
    }
    // The filename component should not contain a path separator. If it does,
    // the splitting was done incorrectly.
    assert(filename.find('/', separator_pos + 1) == std::string::npos);

    return Slice(filename.data() + separator_pos + 1,
                 filename.length() - separator_pos - 1);
  }

  // True if the given file is a manifest file.
  static bool IsManifest(const std::string& filename) {
    return Basename(filename).starts_with("MANIFEST");
  }

  // buf_[0, pos_ - 1] contains data to be written to fd_.
  // char buf_[kWritableFileBufferSize];
  // char* buf_{nullptr};
  std::unordered_map<std::thread::id, char*> buf_map_;
  std::unordered_map<char*, size_t> pos_map_;
  // size_t pos_;
  int fd_;
  uint64_t tid_;

  const bool is_manifest_;  // True if the file's name starts with MANIFEST.
  const std::string filename_;
  const std::string dirname_;  // The directory of filename_.

};

int LockOrUnlock(int fd, bool lock) {
  errno = 0;
  struct ::flock file_lock_info;
  std::memset(&file_lock_info, 0, sizeof(file_lock_info));
  file_lock_info.l_type = (lock ? F_WRLCK : F_UNLCK);
  file_lock_info.l_whence = SEEK_SET;
  file_lock_info.l_start = 0;
  file_lock_info.l_len = 0;  // Lock/unlock entire file.
#ifdef JL_LIBCFS
  return 0;
#endif
  return ::fcntl(fd, F_SETLK, &file_lock_info);
}

// Instances are thread-safe because they are immutable.
class PosixFileLock : public FileLock {
 public:
  PosixFileLock(int fd, std::string filename)
      : fd_(fd), filename_(std::move(filename)) {}

  int fd() const { return fd_; }
  const std::string& filename() const { return filename_; }

 private:
  const int fd_;
  const std::string filename_;
};

// Tracks the files locked by PosixEnv::LockFile().
//
// We maintain a separate set instead of relying on fcntrl(F_SETLK) because
// fcntl(F_SETLK) does not provide any protection against multiple uses from the
// same process.
//
// Instances are thread-safe because all member data is guarded by a mutex.
class PosixLockTable {
 public:
  bool Insert(const std::string& fname) LOCKS_EXCLUDED(mu_) {
    mu_.Lock();
    bool succeeded = locked_files_.insert(fname).second;
    mu_.Unlock();
    return succeeded;
  }
  void Remove(const std::string& fname) LOCKS_EXCLUDED(mu_) {
    mu_.Lock();
    locked_files_.erase(fname);
    mu_.Unlock();
  }

 private:
  port::Mutex mu_;
  std::set<std::string> locked_files_ GUARDED_BY(mu_);
};

class PosixEnv : public Env {
 public:
  std::unordered_map<std::thread::id, char*> wait_for_gc_bufs;
  PosixEnv();
  ~PosixEnv() override {
    static char msg[] = "PosixEnv singleton destroyed. Unsupported behavior!\n";
    std::fwrite(msg, 1, sizeof(msg), stderr);
    std::abort();
  }

  Status NewSequentialFile(const std::string& filename,
                           SequentialFile** result) override {
#ifdef JL_LIBCFS
    int fd = fs_open2(filename.c_str(), O_RDONLY);
#else
    int fd = ::open(filename.c_str(), O_RDONLY);
#endif
    if (fd < 0) {
      *result = nullptr;
      return PosixError(filename, errno);
    }

    *result = new PosixSequentialFile(filename, fd);
    return Status::OK();
  }

  Status NewRandomAccessFile(const std::string& filename,
                             RandomAccessFile** result) override {
    *result = nullptr;
#ifdef JL_LIBCFS
    int fd = fs_open2(filename.c_str(), O_RDONLY);
#else
    int fd = ::open(filename.c_str(), O_RDONLY);
#endif
    if (fd < 0) {
      return PosixError(filename, errno);
    }

    if (!mmap_limiter_.Acquire()) {
      *result = new PosixRandomAccessFile(filename, fd, &fd_limiter_);
      return Status::OK();
    }

    uint64_t file_size;
    Status status = GetFileSize(filename, &file_size);
    if (status.ok()) {
      void* mmap_base =
          ::mmap(/*addr=*/nullptr, file_size, PROT_READ, MAP_SHARED, fd, 0);
      if (mmap_base != MAP_FAILED) {
        *result = new PosixMmapReadableFile(filename,
                                            reinterpret_cast<char*>(mmap_base),
                                            file_size, &mmap_limiter_);
      } else {
        status = PosixError(filename, errno);
      }
    }
#ifdef JL_LIBCFS
    fs_close(fd);
#else
    ::close(fd);
#endif
    if (!status.ok()) {
      mmap_limiter_.Release();
    }
    return status;
  }

  Status NewFSPRandomAccessFile(const std::string& filename,
                             RandomAccessFile** result) override {
    *result = nullptr;
#ifdef JL_LIBCFS
    int fd = fs_open_ldb2(filename.c_str(), O_RDONLY);
#else
    int fd = ::open(filename.c_str(), O_RDONLY);
#endif
    if (fd < 0) {
      return PosixError(filename, errno);
    }

    if (!mmap_limiter_.Acquire()) {
      *result = new FSPRandomAccessFile(filename, fd, &fd_limiter_);
      return Status::OK();
    }

    throw std::runtime_error("mmap not supported");
  }

  Status NewWritableFile(const std::string& filename,
                         WritableFile** result) override {
#ifdef JL_LIBCFS
    int fd = fs_open(filename.c_str(), O_TRUNC | O_WRONLY | O_CREAT, 0644);
#else
    int fd = ::open(filename.c_str(), O_TRUNC | O_WRONLY | O_CREAT, 0644);
#endif
    if (fd < 0) {
      *result = nullptr;
      return PosixError(filename, errno);
    }

    *result = new PosixWritableFile(filename, fd);
    return Status::OK();
  }

  Status NewFSPWritableFile(const std::string& filename,
                         WritableFile** result) override {
#ifdef JL_LIBCFS
    int fd = fs_open_ldb(filename.c_str(), O_TRUNC | O_WRONLY | O_CREAT, 0644);
#else
    int fd = ::open(filename.c_str(), O_TRUNC | O_WRONLY | O_CREAT, 0644);
#endif
    if (fd < 0) {
      *result = nullptr;
      return PosixError(filename, errno);
    }

    *result = new FSPWritableFile(filename, fd);
    return Status::OK();
  }

  Status NewAppendableFile(const std::string& filename,
                           WritableFile** result) override {
#ifdef JL_LIBCFS
    int fd = fs_open(filename.c_str(), O_APPEND | O_WRONLY | O_CREAT, 0644);
#else
    int fd = ::open(filename.c_str(), O_APPEND | O_WRONLY | O_CREAT, 0644);
#endif
    if (fd < 0) {
      *result = nullptr;
      return PosixError(filename, errno);
    }

    *result = new PosixWritableFile(filename, fd);
    return Status::OK();
  }

  Status NewFSPAppendableFile(const std::string& filename,
                           WritableFile** result) override {
#ifdef JL_LIBCFS
    int fd = fs_open_ldb(filename.c_str(), O_APPEND | O_WRONLY | O_CREAT, 0644);
#else
    int fd = ::open(filename.c_str(), O_APPEND | O_WRONLY | O_CREAT, 0644);
#endif
    if (fd < 0) {
      *result = nullptr;
      return PosixError(filename, errno);
    }

    *result = new FSPWritableFile(filename, fd);
    return Status::OK();
  }

  bool FileExists(const std::string& filename) override {
    struct stat statbuf;
#ifdef JL_LIBCFS
    return (fs_stat(filename.c_str(), &statbuf) == 0);
#else
    return ::access(filename.c_str(), F_OK) == 0;
#endif
  }

  Status GetChildren(const std::string& directory_path,
                     std::vector<std::string>* result) override {
    result->clear();
#ifdef JL_LIBCFS
    struct CFS_DIR* dir = fs_opendir(directory_path.c_str());
#else
    ::DIR* dir = ::opendir(directory_path.c_str());
#endif
    if (dir == nullptr) {
      return PosixError(directory_path, errno);
    }
    struct ::dirent* entry;
#ifdef JL_LIBCFS
    while ((entry = fs_readdir(dir)) != nullptr) {
      // fprintf(stdout, "entry->d_name:%s\n", entry->d_name);
#else
    while ((entry = ::readdir(dir)) != nullptr) {
#endif
      result->emplace_back(entry->d_name);
    }
#ifdef JL_LIBCFS
    fs_closedir(dir);
#else
    ::closedir(dir);
#endif
    return Status::OK();
  }

  Status DeleteFile(const std::string& filename) override {
#ifdef JL_LIBCFS
    if (fs_unlink(filename.c_str()) != 0) {
#else
    if (::unlink(filename.c_str()) != 0) {
#endif
      return PosixError(filename, errno);
    }
    return Status::OK();
  }

  Status CreateDir(const std::string& dirname) override {
#ifdef JL_LIBCFS
    if (fs_mkdir(dirname.c_str(), 0755) != 0) {
#else
    if (::mkdir(dirname.c_str(), 0755) != 0) {
#endif
      return PosixError(dirname, errno);
    }
    return Status::OK();
  }

  Status DeleteDir(const std::string& dirname) override {
#ifdef JL_LIBCFS
    if (fs_rmdir(dirname.c_str()) != 0) {
#else
    if (::rmdir(dirname.c_str()) != 0) {
#endif
      return PosixError(dirname, errno);
    }
    return Status::OK();
  }

  Status GetFileSize(const std::string& filename, uint64_t* size) override {
    struct ::stat file_stat;
#ifdef JL_LIBCFS
    if (fs_stat(filename.c_str(), &file_stat) != 0) {
#else
    if (::stat(filename.c_str(), &file_stat) != 0) {
#endif
      *size = 0;
      return PosixError(filename, errno);
    }
    *size = file_stat.st_size;
    return Status::OK();
  }

  Status RenameFile(const std::string& from, const std::string& to) override {
#ifdef JL_LIBCFS
    if (fs_rename(from.c_str(), to.c_str()) != 0) {
#else
    if (std::rename(from.c_str(), to.c_str()) != 0) {
#endif
      return PosixError(from, errno);
    }
    return Status::OK();
  }

  Status LockFile(const std::string& filename, FileLock** lock) override {
    *lock = nullptr;

#ifdef JL_LIBCFS
    int fd = fs_open(filename.c_str(), O_RDWR | O_CREAT, 0644);
#else
    int fd = ::open(filename.c_str(), O_RDWR | O_CREAT, 0644);
#endif
    if (fd < 0) {
      return PosixError(filename, errno);
    }

    if (!locks_.Insert(filename)) {
#ifdef JL_LIBCFS
      fs_close(fd);
#else
      ::close(fd);
#endif
      return Status::IOError("lock " + filename, "already held by process");
    }

    if (LockOrUnlock(fd, true) == -1) {
      int lock_errno = errno;
#ifdef JL_LIBCFS
      fs_close(fd);
#else
      ::close(fd);
#endif
      locks_.Remove(filename);
      return PosixError("lock " + filename, lock_errno);
    }

    *lock = new PosixFileLock(fd, filename);
    return Status::OK();
  }

  Status UnlockFile(FileLock* lock) override {
    PosixFileLock* posix_file_lock = static_cast<PosixFileLock*>(lock);
    if (LockOrUnlock(posix_file_lock->fd(), false) == -1) {
      return PosixError("unlock " + posix_file_lock->filename(), errno);
    }
    locks_.Remove(posix_file_lock->filename());
#ifdef JL_LIBCFS
    fs_close(posix_file_lock->fd());
#else
    ::close(posix_file_lock->fd());
#endif
    delete posix_file_lock;
    return Status::OK();
  }

  void Schedule(void (*background_work_function)(void* background_work_arg),
                void* background_work_arg) override;

  void StartThread(void (*thread_main)(void* thread_main_arg),
                   void* thread_main_arg) override;

  Status GetTestDirectory(std::string* result) override {
    const char* env = std::getenv("TEST_TMPDIR");
    if (env && env[0] != '\0') {
      *result = env;
    } else {
      char buf[100];
      std::snprintf(buf, sizeof(buf), "/tmp/leveldbtest-%d",
                    static_cast<int>(::geteuid()));
      *result = buf;
    }

    // The CreateDir status is ignored because the directory may already exist.
    CreateDir(*result);

    return Status::OK();
  }

  Status NewLogger(const std::string& filename, Logger** result) override {
    std::FILE* fp = std::fopen(filename.c_str(), "w");
    if (fp == nullptr) {
      *result = nullptr;
      return PosixError(filename, errno);
    } else {
      *result = new PosixLogger(fp);
      return Status::OK();
    }
  }

  uint64_t NowMicros() override {
    static constexpr uint64_t kUsecondsPerSecond = 1000000;
    struct ::timeval tv;
    ::gettimeofday(&tv, nullptr);
    return static_cast<uint64_t>(tv.tv_sec) * kUsecondsPerSecond + tv.tv_usec;
  }

  void SleepForMicroseconds(int micros) override { ::usleep(micros); }

 private:
  void BackgroundThreadMain();

  static void BackgroundThreadEntryPoint(PosixEnv* env) {
    env->BackgroundThreadMain();
  }

  // Stores the work item data in a Schedule() call.
  //
  // Instances are constructed on the thread calling Schedule() and used on the
  // background thread.
  //
  // This structure is thread-safe beacuse it is immutable.
  struct BackgroundWorkItem {
    explicit BackgroundWorkItem(void (*function)(void* arg), void* arg)
        : function(function), arg(arg) {}

    void (*const function)(void*);
    void* const arg;
  };

  port::Mutex background_work_mutex_;
  port::CondVar background_work_cv_ GUARDED_BY(background_work_mutex_);
  bool started_background_thread_ GUARDED_BY(background_work_mutex_);

  std::queue<BackgroundWorkItem> background_work_queue_
      GUARDED_BY(background_work_mutex_);

  PosixLockTable locks_;  // Thread-safe.
  Limiter mmap_limiter_;  // Thread-safe.
  Limiter fd_limiter_;    // Thread-safe.
};

// Return the maximum number of concurrent mmaps.
int MaxMmaps() { return g_mmap_limit; }

// Return the maximum number of read-only files to keep open.
int MaxOpenFiles() {
  if (g_open_read_only_file_limit >= 0) {
    return g_open_read_only_file_limit;
  }
  struct ::rlimit rlim;
  if (::getrlimit(RLIMIT_NOFILE, &rlim)) {
    // getrlimit failed, fallback to hard-coded default.
    g_open_read_only_file_limit = 50;
  } else if (rlim.rlim_cur == RLIM_INFINITY) {
    g_open_read_only_file_limit = std::numeric_limits<int>::max();
  } else {
    // Allow use of 20% of available file descriptors for read-only files.
    g_open_read_only_file_limit = rlim.rlim_cur / 5;
  }
  fprintf(stdout, "g_open_read_only_file_limit:%d\n",
          g_open_read_only_file_limit);
  return g_open_read_only_file_limit;
}

}  // namespace

PosixEnv::PosixEnv()
    : background_work_cv_(&background_work_mutex_),
      started_background_thread_(false),
      mmap_limiter_(MaxMmaps()),
      fd_limiter_(MaxOpenFiles()) {}

void PosixEnv::Schedule(
    void (*background_work_function)(void* background_work_arg),
    void* background_work_arg) {
  background_work_mutex_.Lock();

  // Start the background thread, if we haven't done so already.
  if (!started_background_thread_) {
    started_background_thread_ = true;
    std::thread background_thread(PosixEnv::BackgroundThreadEntryPoint, this);
    background_thread.detach();
  }

  // If the queue is empty, the background thread may be waiting for work.
  if (background_work_queue_.empty()) {
    background_work_cv_.Signal();
  }

  background_work_queue_.emplace(background_work_function, background_work_arg);
  background_work_mutex_.Unlock();
}

void PosixEnv::BackgroundThreadMain() {
#ifdef JL_LIBCFS
  fs_init_thread_local_mem();
  printf("bg thread local mem init success\n");
  if (g_num_workers > 1) {
    // fprintf(stderr, ">>>>>>> assign compaction to:%d\n", g_num_workers - 1);
    // fs_admin_thread_reassign(0, g_num_workers - 1, FS_REASSIGN_FUTURE);
  }
  fprintf(stderr, "BackgroundThreadMain threadFsTid:%d\n", threadFsTid);
  // int target = g_appid - 1;
  // fs_admin_thread_reassign(0, target % g_num_workers, FS_REASSIGN_FUTURE);
#endif
  pin_to_cpu_core(21 + g_appid * 2 - 1);
  while (true) {
    background_work_mutex_.Lock();

    // Wait until there is work to be done.
    while (background_work_queue_.empty()) {
      background_work_cv_.Wait();
    }

    assert(!background_work_queue_.empty());
    auto background_work_function = background_work_queue_.front().function;
    void* background_work_arg = background_work_queue_.front().arg;
    background_work_queue_.pop();

    background_work_mutex_.Unlock();
    background_work_function(background_work_arg);
  }
}

namespace {

// Wraps an Env instance whose destructor is never created.
//
// Intended usage:
//   using PlatformSingletonEnv = SingletonEnv<PlatformEnv>;
//   void ConfigurePosixEnv(int param) {
//     PlatformSingletonEnv::AssertEnvNotInitialized();
//     // set global configuration flags.
//   }
//   Env* Env::Default() {
//     static PlatformSingletonEnv default_env;
//     return default_env.env();
//   }
template <typename EnvType>
class SingletonEnv {
 public:
  SingletonEnv() {
#if !defined(NDEBUG)
    env_initialized_.store(true, std::memory_order::memory_order_relaxed);
#endif  // !defined(NDEBUG)
    static_assert(sizeof(env_storage_) >= sizeof(EnvType),
                  "env_storage_ will not fit the Env");
    static_assert(alignof(decltype(env_storage_)) >= alignof(EnvType),
                  "env_storage_ does not meet the Env's alignment needs");
#ifdef JL_LIBCFS
    init_fsp_access();
#endif
    g_appid = atoi(strtok(getenv("FSP_KEY_LISTS"), ","));
    pin_to_cpu_core(21 + g_appid * 2 - 2);
    new (&env_storage_) EnvType();
  }
  ~SingletonEnv() = default;

  SingletonEnv(const SingletonEnv&) = delete;
  SingletonEnv& operator=(const SingletonEnv&) = delete;

  Env* env() { return reinterpret_cast<Env*>(&env_storage_); }

  static void AssertEnvNotInitialized() {
#if !defined(NDEBUG)
    assert(!env_initialized_.load(std::memory_order::memory_order_relaxed));
#endif  // !defined(NDEBUG)
  }

 private:
  typename std::aligned_storage<sizeof(EnvType), alignof(EnvType)>::type
      env_storage_;
#if !defined(NDEBUG)
  static std::atomic<bool> env_initialized_;
#endif  // !defined(NDEBUG)
};

#if !defined(NDEBUG)
template <typename EnvType>
std::atomic<bool> SingletonEnv<EnvType>::env_initialized_;
#endif  // !defined(NDEBUG)

using PosixDefaultEnv = SingletonEnv<PosixEnv>;

}  // namespace

void PosixEnv::StartThread(void (*thread_main)(void* thread_main_arg),
                           void* thread_main_arg) {
  std::thread new_thread(thread_main, thread_main_arg);
  new_thread.detach();
}

void EnvPosixTestHelper::SetReadOnlyFDLimit(int limit) {
  PosixDefaultEnv::AssertEnvNotInitialized();
  g_open_read_only_file_limit = limit;
}

void EnvPosixTestHelper::SetReadOnlyMMapLimit(int limit) {
  PosixDefaultEnv::AssertEnvNotInitialized();
  g_mmap_limit = limit;
}

Env* Env::Default() {
  static PosixDefaultEnv env_container;
  return env_container.env();
}

}  // namespace leveldb
