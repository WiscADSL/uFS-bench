libdir = @libdir@/filebench

SUBDIRS = workloads cvars

# we depend on this lex & yacc generated file
BUILT_SOURCES = parser_gram.h

bin_PROGRAMS = filebench
filebench_SOURCES = eventgen.c fb_avl.c fb_localfs.c \
		    fb_random.c fileset.c flowop.c flowop_library.c \
		    gamma_dist.c ipc.c misc.c multi_client_sync.c \
		    parser_gram.y parser_lex.l procflow.c stats.c \
		    threadflow.c utils.c vars.c ioprio.c \
		    eventgen.h  fb_random.h  fileset.h  fsplug.h \
		    ipc.h   multi_client_sync.h  parsertypes.h  stats.h \
		    utils.h config.h fb_avl.h filebench.h flowop.h gamma_dist.h \
		    misc.h procflow.h threadflow.h vars.h ioprio.h flag.h \
		    fbtime.c fbtime.h \
		    fb_cvar.c fb_cvar.h aslr.c aslr.h \
		    cvars/mtwist/mtwist.c cvars/mtwist/mtwist.h

EXTRA_DIST = LICENSE

# CFS_ROOT_DIR is an environment variable

ACLOCAL_AMFLAGS = -I m4
AM_YFLAGS = -d
# TBB usually have its build results in a directory with a name like 
# `linux_intel64_gcc_cc10_libc2.32_kernel5.8.0_release/`, which may vary in
# different systems/machines. For simplicity, our build script will symbolically
# link this directory to `tbb_build_release`
AM_LDFLAGS = -L${CFS_ROOT_DIR}/cfs/build -L${CFS_ROOT_DIR}/cfs/lib/tbb/build/tbb_build_release/
if GCC_USED
AM_CFLAGS = -Wall -Wno-unknown-pragmas -I${CFS_ROOT_DIR}/cfs/include -I${CFS_ROOT_DIR}/cfs/lib/tbb/include/
endif
# if running uFS, set the environment variable LIB_CFS to '-lcfs'; otherwise, set to empty
filebench_LDADD = ${LIB_CFS} -ltbb -ltbbmalloc

# if running uFS, set the environment variable CFS_MACRO to '-DCFS'; otherwise, set to empty
# if running varmail, set the environment variable CFS_MACRO to '-DVAR'; otherwise, set to empty
DEFS = -D_REENTRANT -DYYDEBUG -DYY_NO_INPUT -DFBLIBDIR=\"$(libdir)\" -D_LARGEFILE64_SOURCE -D_GNU_SOURCE ${CFS_MACRO} ${VAR_MACRO}
