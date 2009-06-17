#ifndef __DARSHAN_LOG_FORMAT_H
#define __DARSHAN_LOG_FORMAT_H

#include <unistd.h>
#include <sys/types.h>
#include <stdint.h>
#include "darshan-config.h"

/* update this on file format changes */
#define CP_VERSION "1.21"

/* size (in bytes) of job record */
#define CP_JOB_RECORD_SIZE 1024

/* max length of exe string within job record (not counting '\0') */
#define CP_EXE_LEN (CP_JOB_RECORD_SIZE - sizeof(struct darshan_job) - 1)

/* size (in bytes) of each file record */
#define CP_FILE_RECORD_SIZE (sizeof(struct darshan_file))

/* max length of name suffix string within file record (not counting '\0') */
#define CP_NAME_SUFFIX_LEN 11

/* per file statistics */
enum darshan_indices
{
    CP_INDEP_OPENS = 0,          /* count of MPI independent opens */
    CP_COLL_OPENS,               /* count of MPI collective opens */
    CP_INDEP_READS,              /* count of independent MPI reads */
    CP_INDEP_WRITES,             /* count of independent MPI writes */
    CP_COLL_READS,               /* count of collective MPI reads */
    CP_COLL_WRITES,              /* count of collective MPI writes */
    CP_SPLIT_READS,              /* count of split collective MPI reads */
    CP_SPLIT_WRITES,             /* count of split collective MPI writes */
    CP_NB_READS,                 /* count of nonblocking MPI reads */
    CP_NB_WRITES,                /* count of nonblocking MPI writes */
    CP_SYNCS,
    CP_POSIX_READS,              /* count of posix reads */
    CP_POSIX_WRITES,             /* count of posix writes */
    CP_POSIX_OPENS,              /* count of posix opens */
    CP_POSIX_SEEKS,              /* count of posix seeks */
    CP_POSIX_STATS,              /* count of posix stat/lstat/fstats */
    CP_POSIX_MMAPS,              /* count of posix mmaps */
    CP_POSIX_FREADS,
    CP_POSIX_FWRITES,
    CP_POSIX_FOPENS,
    CP_POSIX_FSEEKS,
    CP_POSIX_FSYNCS,
    CP_POSIX_FDSYNCS,
    /* type categories */
    CP_COMBINER_NAMED,           /* count of each MPI datatype category */
    CP_COMBINER_DUP,
    CP_COMBINER_CONTIGUOUS,
    CP_COMBINER_VECTOR,
    CP_COMBINER_HVECTOR_INTEGER,
    CP_COMBINER_HVECTOR,
    CP_COMBINER_INDEXED,
    CP_COMBINER_HINDEXED_INTEGER,
    CP_COMBINER_HINDEXED,
    CP_COMBINER_INDEXED_BLOCK,
    CP_COMBINER_STRUCT_INTEGER,
    CP_COMBINER_STRUCT,
    CP_COMBINER_SUBARRAY,
    CP_COMBINER_DARRAY,
    CP_COMBINER_F90_REAL,
    CP_COMBINER_F90_COMPLEX,
    CP_COMBINER_F90_INTEGER,
    CP_COMBINER_RESIZED,
    CP_HINTS,                     /* count of MPI hints used */
    CP_VIEWS,                     /* count of MPI set view calls */
    CP_MODE,                      /* mode of file */
    CP_BYTES_READ,                /* total bytes read */
    CP_BYTES_WRITTEN,             /* total bytes written */
    CP_MAX_BYTE_READ,             /* highest offset byte read */
    CP_MAX_BYTE_WRITTEN,          /* highest offset byte written */
    CP_CONSEC_READS,              /* count of consecutive reads */
    CP_CONSEC_WRITES,             /* count of consecutive writes */
    CP_SEQ_READS,                 /* count of sequential reads */
    CP_SEQ_WRITES,                /* count of sequential writes */
    CP_RW_SWITCHES,               /* number of times switched between read and write */
    CP_MEM_NOT_ALIGNED,           /* count of accesses not mem aligned */
    CP_MEM_ALIGNMENT,             /* mem alignment in bytes */
    CP_FILE_NOT_ALIGNED,          /* count of accesses not file aligned */
    CP_FILE_ALIGNMENT,            /* file alignment in bytes */
    /* buckets */
    CP_SIZE_READ_0_100,           /* count of posix read size ranges */
    CP_SIZE_READ_100_1K,
    CP_SIZE_READ_1K_10K,
    CP_SIZE_READ_10K_100K,
    CP_SIZE_READ_100K_1M,
    CP_SIZE_READ_1M_4M,
    CP_SIZE_READ_4M_10M,
    CP_SIZE_READ_10M_100M,
    CP_SIZE_READ_100M_1G,
    CP_SIZE_READ_1G_PLUS,
    /* buckets */
    CP_SIZE_WRITE_0_100,          /* count of posix write size ranges */
    CP_SIZE_WRITE_100_1K,
    CP_SIZE_WRITE_1K_10K,
    CP_SIZE_WRITE_10K_100K,
    CP_SIZE_WRITE_100K_1M,
    CP_SIZE_WRITE_1M_4M,
    CP_SIZE_WRITE_4M_10M,
    CP_SIZE_WRITE_10M_100M,
    CP_SIZE_WRITE_100M_1G,
    CP_SIZE_WRITE_1G_PLUS,
    /* buckets */
    CP_SIZE_READ_AGG_0_100,       /* count of MPI read size ranges */
    CP_SIZE_READ_AGG_100_1K,
    CP_SIZE_READ_AGG_1K_10K,
    CP_SIZE_READ_AGG_10K_100K,
    CP_SIZE_READ_AGG_100K_1M,
    CP_SIZE_READ_AGG_1M_4M,
    CP_SIZE_READ_AGG_4M_10M,
    CP_SIZE_READ_AGG_10M_100M,
    CP_SIZE_READ_AGG_100M_1G,
    CP_SIZE_READ_AGG_1G_PLUS,
    /* buckets */
    CP_SIZE_WRITE_AGG_0_100,      /* count of MPI write size ranges */
    CP_SIZE_WRITE_AGG_100_1K,
    CP_SIZE_WRITE_AGG_1K_10K,
    CP_SIZE_WRITE_AGG_10K_100K,
    CP_SIZE_WRITE_AGG_100K_1M,
    CP_SIZE_WRITE_AGG_1M_4M,
    CP_SIZE_WRITE_AGG_4M_10M,
    CP_SIZE_WRITE_AGG_10M_100M,
    CP_SIZE_WRITE_AGG_100M_1G,
    CP_SIZE_WRITE_AGG_1G_PLUS,
    /* buckets */
    CP_EXTENT_READ_0_100,          /* count of MPI read extent ranges */
    CP_EXTENT_READ_100_1K,
    CP_EXTENT_READ_1K_10K,
    CP_EXTENT_READ_10K_100K,
    CP_EXTENT_READ_100K_1M,
    CP_EXTENT_READ_1M_4M,
    CP_EXTENT_READ_4M_10M,
    CP_EXTENT_READ_10M_100M,
    CP_EXTENT_READ_100M_1G,
    CP_EXTENT_READ_1G_PLUS,
    /* buckets */
    CP_EXTENT_WRITE_0_100,         /* count of MPI write extent ranges */
    CP_EXTENT_WRITE_100_1K,
    CP_EXTENT_WRITE_1K_10K,
    CP_EXTENT_WRITE_10K_100K,
    CP_EXTENT_WRITE_100K_1M,
    CP_EXTENT_WRITE_1M_4M,
    CP_EXTENT_WRITE_4M_10M,
    CP_EXTENT_WRITE_10M_100M,
    CP_EXTENT_WRITE_100M_1G,
    CP_EXTENT_WRITE_1G_PLUS,
    /* counters */
    CP_STRIDE1_STRIDE,             /* the four most frequently appearing strides */
    CP_STRIDE2_STRIDE,
    CP_STRIDE3_STRIDE,
    CP_STRIDE4_STRIDE,
    CP_STRIDE1_COUNT,              /* count of each of the most frequent strides */
    CP_STRIDE2_COUNT,
    CP_STRIDE3_COUNT,
    CP_STRIDE4_COUNT,
    CP_ACCESS1_ACCESS,             /* the four most frequently appearing access sizes */
    CP_ACCESS2_ACCESS,
    CP_ACCESS3_ACCESS,
    CP_ACCESS4_ACCESS,
    CP_ACCESS1_COUNT,              /* count of each of the most frequent access sizes */
    CP_ACCESS2_COUNT,
    CP_ACCESS3_COUNT,
    CP_ACCESS4_COUNT,

    CP_NUM_INDICES,
};

/* floating point statistics */
enum f_darshan_indices
{
    CP_F_OPEN_TIMESTAMP = 0,    /* timestamp of first open */
    CP_F_READ_START_TIMESTAMP,  /* timestamp of first read */
    CP_F_WRITE_START_TIMESTAMP, /* timestamp of first write */
    CP_F_CLOSE_TIMESTAMP,       /* timestamp of last close */
    CP_F_READ_END_TIMESTAMP,    /* timestamp of last read */
    CP_F_WRITE_END_TIMESTAMP,   /* timestamp of last write */
    CP_F_POSIX_READ_TIME,       /* cumulative posix read time */
    CP_F_POSIX_WRITE_TIME,      /* cumulative posix write time */
    CP_F_POSIX_META_TIME,       /* cumulative posix meta time */
    CP_F_MPI_META_TIME,         /* cumulative mpi-io meta time */
    CP_F_MPI_READ_TIME,         /* cumulative mpi-io read time */
    CP_F_MPI_WRITE_TIME,        /* cumulative mpi-io write time */
    CP_F_NUM_INDICES,
};

/* statistics for any kind of file */
struct darshan_file
{
    uint64_t hash;
    int rank;
    int64_t counters[CP_NUM_INDICES];
    double fcounters[CP_F_NUM_INDICES];
    char name_suffix[CP_NAME_SUFFIX_LEN+1];
};

/* statistics for the job as a whole */
struct darshan_job
{
    char version_string[10];
    uid_t uid;
    long start_time;
    long end_time;
    int nprocs;
};

/* convenience macros for printing out counters */
#define CP_PRINT_HEADER() printf("#<rank>\t<file>\t<counter>\t<value>\t<name suffix>\n")

#if SIZEOF_LONG_INT == 4
#  define llu(x) (x)
#  define lld(x) (x)
#  define SCANF_lld "%lld"
#elif SIZEOF_LONG_INT == 8
#  define llu(x) (unsigned long long)(x)
#  define lld(x) (long long)(x)
#  define SCANF_lld "%ld"
#else
#  error Unexpected sizeof(long int)
#endif

/* for integers */
#define CP_PRINT(__job, __file, __counter) do {\
        printf("%d\t%llu\t%s\t%lld\t...%s\n", \
            (__file)->rank, llu((__file)->hash), #__counter, \
            lld((__file)->counters[__counter]), (__file)->name_suffix); \
} while(0)

/* for double floats */
#define CP_F_PRINT(__job, __file, __counter) do {\
        printf("%d\t%llu\t%s\t%f\t...%s\n", \
            (__file)->rank, llu((__file)->hash), #__counter, \
            (__file)->fcounters[__counter], (__file)->name_suffix); \
} while(0)

#endif /* __DARSHAN_LOG_FORMAT_H */