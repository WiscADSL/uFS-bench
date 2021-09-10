cmake_minimum_required(VERSION 3.11)
project(cfs_bench)

set(CMAKE_CXX_STANDARD 14)

set(default_build_type Release)

option(USE_UFS "Build with uFS APIs" ON)

set(SMALLFILE_BIN_NAME "smallfile")
set(LARGEFILE_BIN_NAME "largefile")

add_executable(${SMALLFILE_BIN_NAME} smallfile.cc libutil.h)
add_executable(${LARGEFILE_BIN_NAME} largefile.cc libutil.h)

if (USE_UFS)
  set(CFS_DIR "$ENV{CFS_ROOT_DIR}/cfs")
  set(CFS_BENCH_DIR "$ENV{CFS_ROOT_DIR}/cfs_bench")
  include_directories("${CFS_DIR}/include")
  include_directories("${CFS_BENCH_DIR}/include")
  target_link_libraries(${SMALLFILE_BIN_NAME} PUBLIC ${CFS_DIR}/build/libcfs.so)
  target_link_libraries(${SMALLFILE_BIN_NAME} PUBLIC ${CFS_DIR}/lib/tbb/build/tbb_build_release/libtbb.so.2)
  target_link_libraries(${SMALLFILE_BIN_NAME} PUBLIC ${CFS_DIR}/lib/tbb/build/tbb_build_release/libtbbmalloc.so.2)
  target_link_libraries(${LARGEFILE_BIN_NAME} PUBLIC ${CFS_DIR}/build/libcfs.so)
  target_link_libraries(${LARGEFILE_BIN_NAME} PUBLIC ${CFS_DIR}/lib/tbb/build/tbb_build_release/libtbb.so.2)
  target_link_libraries(${LARGEFILE_BIN_NAME} PUBLIC ${CFS_DIR}/lib/tbb/build/tbb_build_release/libtbbmalloc.so.2)
endif(USE_UFS)
