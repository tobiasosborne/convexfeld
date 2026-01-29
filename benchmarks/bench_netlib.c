/**
 * @file bench_netlib.c
 * @brief Netlib LP benchmark runner
 *
 * Runs the Netlib LP test suite and compares against reference solutions.
 * Reference values from Gurobi 10 with 1e-8 tolerance.
 */

#define _POSIX_C_SOURCE 199309L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <dirent.h>
#include "convexfeld/convexfeld.h"
#include "convexfeld/cxf_mps.h"

#define MAX_PROBLEMS 150
#define MAX_NAME_LEN 64
#define REL_TOL 1e-4  /* 0.01% relative tolerance */
#define ABS_TOL 1e-6  /* Absolute tolerance for near-zero objectives */

typedef struct {
    char name[MAX_NAME_LEN];
    double ref_obj;
    int has_ref;
} Problem;

static Problem g_problems[MAX_PROBLEMS];
static int g_num_problems = 0;

static int load_reference_solutions(const char *csv_path) {
    FILE *f = fopen(csv_path, "r");
    if (!f) {
        fprintf(stderr, "Cannot open reference file: %s\n", csv_path);
        return -1;
    }

    char line[256];
    /* Skip header */
    if (!fgets(line, sizeof(line), f)) {
        fclose(f);
        return -1;
    }

    while (fgets(line, sizeof(line), f) && g_num_problems < MAX_PROBLEMS) {
        char name[MAX_NAME_LEN];
        char status[32];
        double solve_time, obj;

        if (sscanf(line, "%63[^,],%31[^,],%lf,%lf", name, status, &solve_time, &obj) == 4) {
            if (strcmp(status, "OPTIMAL") == 0) {
                snprintf(g_problems[g_num_problems].name, MAX_NAME_LEN, "%s", name);
                g_problems[g_num_problems].ref_obj = obj;
                g_problems[g_num_problems].has_ref = 1;
                g_num_problems++;
            }
        }
    }

    fclose(f);
    return g_num_problems;
}

static const Problem *find_reference(const char *name) {
    for (int i = 0; i < g_num_problems; i++) {
        if (strcmp(g_problems[i].name, name) == 0) {
            return &g_problems[i];
        }
    }
    return NULL;
}

static int check_objective(double computed, double reference) {
    double abs_err = fabs(computed - reference);
    double rel_err = fabs(reference) > ABS_TOL ? abs_err / fabs(reference) : abs_err;
    return rel_err < REL_TOL || abs_err < ABS_TOL;
}

static double get_time_sec(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

typedef struct {
    int passed;
    int failed;
    int errors;
    int skipped;
} Stats;

static void run_benchmark(const char *mps_path, const char *name, Stats *stats) {
    const Problem *ref = find_reference(name);
    if (!ref) {
        printf("  %-20s SKIP (no reference)\n", name);
        stats->skipped++;
        return;
    }

    CxfEnv *env = NULL;
    CxfModel *model = NULL;
    int rc;

    rc = cxf_loadenv(&env, NULL);
    if (rc != CXF_OK) {
        printf("  %-20s ERROR (loadenv: %d)\n", name, rc);
        stats->errors++;
        return;
    }

    rc = cxf_newmodel(env, &model, name, 0, NULL, NULL, NULL, NULL, NULL);
    if (rc != CXF_OK) {
        printf("  %-20s ERROR (newmodel: %d)\n", name, rc);
        cxf_freeenv(env);
        stats->errors++;
        return;
    }

    rc = cxf_readmps(model, mps_path);
    if (rc != CXF_OK) {
        printf("  %-20s ERROR (readmps: %d)\n", name, rc);
        cxf_freemodel(model);
        cxf_freeenv(env);
        stats->errors++;
        return;
    }

    double t0 = get_time_sec();
    rc = cxf_optimize(model);
    double elapsed = get_time_sec() - t0;

    if (model->status == CXF_OPTIMAL) {
        int ok = check_objective(model->obj_val, ref->ref_obj);
        if (ok) {
            printf("  %-20s PASS  obj=%.6e (ref=%.6e) [%.3fs]\n",
                   name, model->obj_val, ref->ref_obj, elapsed);
            stats->passed++;
        } else {
            double rel_err = fabs(model->obj_val - ref->ref_obj) /
                             (fabs(ref->ref_obj) > ABS_TOL ? fabs(ref->ref_obj) : 1.0);
            printf("  %-20s FAIL  obj=%.6e (ref=%.6e, err=%.2e) [%.3fs]\n",
                   name, model->obj_val, ref->ref_obj, rel_err, elapsed);
            stats->failed++;
        }
    } else {
        const char *status_str;
        switch (model->status) {
            case CXF_INFEASIBLE: status_str = "INFEASIBLE"; break;
            case CXF_UNBOUNDED: status_str = "UNBOUNDED"; break;
            case CXF_INF_OR_UNBD: status_str = "INF_OR_UNBD"; break;
            case CXF_ITERATION_LIMIT: status_str = "ITER_LIMIT"; break;
            case CXF_TIME_LIMIT: status_str = "TIME_LIMIT"; break;
            case CXF_NUMERIC: status_str = "NUMERIC"; break;
            default: status_str = "UNKNOWN"; break;
        }
        printf("  %-20s FAIL  status=%s (expected OPTIMAL) [%.3fs]\n",
               name, status_str, elapsed);
        stats->failed++;
    }

    cxf_freemodel(model);
    cxf_freeenv(env);
}

int main(int argc, char **argv) {
    const char *mps_dir = "benchmarks/netlib/feasible";
    const char *csv_path = "benchmarks/netlib/feasible_gurobi_1e-8.csv";
    const char *filter = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--dir") == 0 && i + 1 < argc) {
            mps_dir = argv[++i];
        } else if (strcmp(argv[i], "--csv") == 0 && i + 1 < argc) {
            csv_path = argv[++i];
        } else if (strcmp(argv[i], "--filter") == 0 && i + 1 < argc) {
            filter = argv[++i];
        } else if (strcmp(argv[i], "--help") == 0) {
            printf("Usage: %s [--dir DIR] [--csv CSV] [--filter NAME]\n", argv[0]);
            printf("  --dir DIR     Directory with .mps files (default: %s)\n", mps_dir);
            printf("  --csv CSV     Reference solutions CSV (default: %s)\n", csv_path);
            printf("  --filter NAME Only run benchmarks containing NAME\n");
            return 0;
        }
    }

    printf("ConvexFeld Netlib Benchmark Suite\n");
    printf("==================================\n");

    int n = load_reference_solutions(csv_path);
    if (n <= 0) {
        fprintf(stderr, "Failed to load reference solutions\n");
        return 1;
    }
    printf("Loaded %d reference solutions\n\n", n);

    DIR *dir = opendir(mps_dir);
    if (!dir) {
        fprintf(stderr, "Cannot open directory: %s\n", mps_dir);
        return 1;
    }

    Stats stats = {0, 0, 0, 0};
    struct dirent *entry;
    int count = 0;

    while ((entry = readdir(dir)) != NULL) {
        const char *name = entry->d_name;
        size_t len = strlen(name);

        if (len < 5 || strcmp(name + len - 4, ".mps") != 0) continue;

        /* Extract problem name (without .mps) */
        char prob_name[MAX_NAME_LEN];
        size_t name_len = len - 4 < MAX_NAME_LEN - 1 ? len - 4 : MAX_NAME_LEN - 1;
        memcpy(prob_name, name, name_len);
        prob_name[name_len] = '\0';

        if (filter && strstr(prob_name, filter) == NULL) continue;

        char path[512];
        snprintf(path, sizeof(path), "%s/%s", mps_dir, name);

        run_benchmark(path, prob_name, &stats);
        count++;
    }

    closedir(dir);

    printf("\n==================================\n");
    printf("Results: %d passed, %d failed, %d errors, %d skipped\n",
           stats.passed, stats.failed, stats.errors, stats.skipped);
    printf("Total: %d benchmarks\n", count);

    return (stats.failed + stats.errors) > 0 ? 1 : 0;
}
