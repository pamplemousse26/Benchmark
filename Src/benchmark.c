#include "compat.h"
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include <malloc.h>

#ifdef _WIN32
#include <BaseTsd.h>
typedef SSIZE_T ssize_t;
#else
#include <fcntl.h>
#include <unistd.h>
#endif

#include "getopt.h"
#include "timer.h"
#include "bfs/bfs.h"
#include "verify/verify.h"
#include "xalloc.h"
#include "options.h"
#include "splittable_mrg.h"
#include "edge_struct.h"
#include "generator/make_graph.h"

static int64_t nvtx_scale;

static int64_t bfs_root[NBFS_max];

static double generation_time;
static double construction_time;
static double bfs_time[NBFS_max];
static int64_t bfs_nedge[NBFS_max];

static packed_edge * __restrict  IJ;
static int64_t nedge;

static void run_bfs (void);
static void output_results (const int64_t, int64_t, int64_t, const double, const double, const double, const double, const double, const double, const int, const double*, const int64_t*);

int main (int argc, char **argv)
{
    int64_t desired_nedge;
    if (sizeof (int64_t) < 8)
    {
        fprintf (stderr, "No 64-bit support.\n");
        return EXIT_FAILURE;
    }

    if (argc > 1)
        get_options (argc, argv);

    nvtx_scale = ((int64_t)1)<<SCALE;

    init_random();

    desired_nedge = nvtx_scale * edgefactor;
    assert (desired_nedge >= nvtx_scale);
    assert (desired_nedge >= edgefactor);

    if (!dumpname)
    {
        if (VERBOSE) fprintf (stderr, "Generating edge list...");
            TIME(generation_time, make_graph ((int)SCALE, desired_nedge, userseed, userseed, &nedge, (packed_edge**)(&IJ)));
        if (VERBOSE) fprintf (stderr, " done.\n");
    } else {
        #ifdef _WIN32
        FILE* fd;
        #else
        int fd;
        #endif
        ssize_t sz;

        #ifdef _WIN32
        if (fopen_s(&fd,dumpname, "r") == 0)
        #else
        if ((fd = open(dumpname, O_RDONLY)) < 0)
        #endif
        {
            perror ("Cannot open input graph file");
            return EXIT_FAILURE;
        }
        sz = nedge * sizeof (*IJ);
        #ifdef _WIN32
        if (sz != fread (IJ,1,sz,fd))
        #else
        if (sz != read (fd, IJ, sz))
        #endif
        {
            perror ("Error reading input graph file");
            return EXIT_FAILURE;
        }
        #ifdef _WIN32
        fclose(fd);
        #else
        close(fd);
        #endif
    }

    run_bfs ();

    xfree_large (IJ);

    output_results (SCALE, nvtx_scale, edgefactor, A, B, C, D,generation_time, construction_time, NBFS, bfs_time, bfs_nedge);

    return EXIT_SUCCESS;
}

void run_bfs (void)
{
    int * __restrict  has_adj;
    int m, err;
    int64_t k, t;

    if (VERBOSE) fprintf (stderr, "Creating graph...");
        TIME(construction_time, err = create_graph_from_edgelist (IJ, nedge));
    if (VERBOSE) fprintf (stderr, "done.\n");
    if (err)
    {
        fprintf (stderr, "Failure creating graph.\n");
        exit (EXIT_FAILURE);
    }

    if (!rootname)
    {
        has_adj = xmalloc_large (nvtx_scale * sizeof (*has_adj));
        OMP("omp parallel")
        {
            OMP("omp for")
            for (k = 0; k < nvtx_scale; ++k)
                has_adj[k] = 0;
            OMP("omp for")
            for (k = 0; k < nedge; ++k)
            {
                const int64_t i = get_v0_from_edge(&IJ[k]);
                const int64_t j = get_v1_from_edge(&IJ[k]);
                if (i != j)
                    has_adj[i] = has_adj[j] = 1;
            }
        }

        m = 0;
        t = 0;
        while (m < NBFS && t < nvtx_scale)
        {
            double R = mrg_get_double_orig (prng_state);
            if (!has_adj[t] || (nvtx_scale - t)*R > NBFS - m) ++t;
            else bfs_root[m++] = t++;
        }
        if (t >= nvtx_scale && m < NBFS)
        {
            if (m > 0)
            {
                fprintf (stderr, "Cannot find %d sample roots of non-self degree > 0, using %d.\n", NBFS, m);
                NBFS = m;
            } else {
                fprintf (stderr, "Cannot find any sample roots of non-self degree > 0.\n");
                exit (EXIT_FAILURE);
            }
        }

        xfree_large (has_adj);
    }
    else
    {
        #ifdef _WIN32
        FILE* fd;
        #else
        int fd;
        #endif
        ssize_t sz;
        #ifdef _WIN32
        if (fopen_s(&fd,rootname, "r") == 0)
        #else
        if ((fd = open (rootname, O_RDONLY)) < 0)
        #endif
        {
            perror ("Cannot open input BFS root file");
            exit (EXIT_FAILURE);
        }
        sz = NBFS * sizeof (*bfs_root);
        #ifdef _WIN32
        if (sz != fread (bfs_root,1,sz,fd))
        #else
        if (sz != read (fd, bfs_root, sz))
        #endif
        {
            perror ("Error reading input BFS root file");
            exit (EXIT_FAILURE);
        }
        #ifdef _WIN32
        fclose (fd);
        #else
        close (fd);
        #endif
    }

    for (m = 0; m < NBFS; ++m)
    {
        int64_t *bfs_tree, max_bfsvtx;

        bfs_tree = xmalloc_large (nvtx_scale * sizeof (*bfs_tree));
        assert (bfs_root[m] < nvtx_scale);

        if (VERBOSE) fprintf (stderr, "Running bfs %d...", m);
        TIME(bfs_time[m], err = make_bfs_tree (bfs_tree, &max_bfsvtx, bfs_root[m]));
        if (VERBOSE) fprintf (stderr, "done\n");

        if (err)
        {
            perror ("make_bfs_tree failed");
            abort ();
        }

        if (VERBOSE) fprintf (stderr, "Verifying bfs %d...", m);
        bfs_nedge[m] = verify_bfs_tree (bfs_tree, max_bfsvtx, bfs_root[m], IJ, nedge);
        if (VERBOSE) fprintf (stderr, "done\n");
        if (bfs_nedge[m] < 0)
        {
            fprintf (stderr, "bfs %d from %" PRId64 " failed verification (%" PRId64 ")\n", m, bfs_root[m], bfs_nedge[m]);
            abort ();
        }

        xfree_large (bfs_tree);
    }

    destroy_graph ();
}

static int dcmp (const void *a, const void *b)
{
    const double da = *(const double*)a;
    const double db = *(const double*)b;
    if (da > db) return 1;
    if (db > da) return -1;
    if (da == db) return 0;
    fprintf (stderr, "No NaNs permitted in output.\n");
    abort ();
    return 0;
}

void statistics (double *out, double *data, int64_t n)
{
    long double s, mean;
    double t;
    int k;

    qsort (data, n, sizeof (*data), dcmp);
    out[0] = data[0];
    t = (n+1) / 4.0;
    k = (int) t;
    if (t == k)
        out[1] = data[k];
    else
        out[1] = 3*(data[k]/4.0) + data[k+1]/4.0;
    t = (n+1) / 2.0;
    k = (int) t;
    if (t == k)
        out[2] = data[k];
    else
        out[2] = data[k]/2.0 + data[k+1]/2.0;
    t = 3*((n+1) / 4.0);
    k = (int) t;
    if (t == k)
        out[3] = data[k];
    else
        out[3] = data[k]/4.0 + 3*(data[k+1]/4.0);
    out[4] = data[n-1];

    s = data[n-1];
    for (k = (int)(n-1); k > 0; --k)
        s += data[k-1];
    mean = s/n;
    out[5] = mean;
    s = data[n-1] - mean;
    s *= s;
    for(k = (int)(n-1); k > 0; --k)
    {
        long double tmp = data[k-1] - mean;
        s += tmp * tmp;
    }
    out[6] = sqrt (s/(n-1));

    s = (data[0]? 1.0L/data[0] : 0);
    for (k = 1; k < n; ++k)
        s += (data[k]? 1.0L/data[k] : 0);
    out[7] = n/s;
    mean = s/n;

    s = (data[0]? 1.0L/data[0] : 0) - mean;
    s *= s;
    for (k = 1; k < n; ++k)
    {
        long double tmp = (data[k]? 1.0L/data[k] : 0) - mean;
        s += tmp * tmp;
    }
    s = (sqrt (s)/(n-1)) * out[7] * out[7];
    out[8] = s;
}

#define NSTAT 9
#define PRINT_STATS(lbl, israte)					\
do {									\
    printf ("min_%s: %20.17e\n", lbl, stats[0]);			\
    printf ("firstquartile_%s: %20.17e\n", lbl, stats[1]);		\
    printf ("median_%s: %20.17e\n", lbl, stats[2]);			\
    printf ("thirdquartile_%s: %20.17e\n", lbl, stats[3]);		\
    printf ("max_%s: %20.17e\n", lbl, stats[4]);			\
    if (!israte) {							\
        printf ("mean_%s: %20.17e\n", lbl, stats[5]);			\
        printf ("stddev_%s: %20.17e\n", lbl, stats[6]);			\
    } else {								\
        printf ("harmonic_mean_%s: %20.17e\n", lbl, stats[7]);		\
        printf ("harmonic_stddev_%s: %20.17e\n", lbl, stats[8]);	\
    }									\
} while (0)

void output_results (const int64_t SCALE, int64_t nvtx_scale, int64_t edgefactor, const double A, const double B, const double C, const double D, const double generation_time, const double construction_time, const int NBFS, const double *bfs_time, const int64_t *bfs_nedge)
{
    int k;
    int64_t sz;
    double *tm;
    double *stats;

    tm = alloca (NBFS * sizeof (*tm));
    stats = alloca (NSTAT * sizeof (*stats));
    if (!tm || !stats)
    {
        perror ("Error allocating within final statistics calculation.");
        abort ();
    }

    sz = (int64_t)(1L << SCALE) * edgefactor * 2 * sizeof(int64_t);
    printf ("SCALE: %" PRId64 "\nsommet: %" PRId64 "\nedgefactor: %" PRId64 "\n" "terasize: %20.17e\n",SCALE, nvtx_scale, edgefactor, sz/1.0e12);
    printf ("A: %20.17e\nB: %20.17e\nC: %20.17e\nD: %20.17e\n", A, B, C, D);
    printf ("generation_time: %20.17e\n", generation_time);
    printf ("construction_time: %20.17e\n", construction_time);
    printf ("nbfs: %d\n", NBFS);

    printf ("\n=====TIME STATISTICS=====\n\n", NBFS);

    memcpy (tm, bfs_time, NBFS*sizeof(tm[0]));
    statistics (stats, tm, NBFS);
    PRINT_STATS("time", 0);

    printf ("\n=====EDGE STATISTICS=====\n\n", NBFS);

    for (k = 0; k < NBFS; ++k)
        tm[k] = (double)bfs_nedge[k];
    statistics (stats, tm, NBFS);
    PRINT_STATS("nedge", 0);

    printf ("\n=====TEPS STATISTICS=====\n\n", NBFS);

    for (k = 0; k < NBFS; ++k)
        tm[k] = bfs_nedge[k] / bfs_time[k];
    statistics (stats, tm, NBFS);
    PRINT_STATS("TEPS", 1);

    printf("\n");
}