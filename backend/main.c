/*
 * main.c — ValveWise CLI entry point
 * Teammate B owns this file.
 *
 * Usage:
 *   valvewise --input <file.inp> [--output <file.json>] [--budget N]
 *             [--fail pipe1,pipe2] [--stuck valve1] [--mode full|rank|reach]
 *             [--seed N] [--verbose]
 */

/* Pull in strdup on POSIX; on Windows _strdup is used below */
#if !defined(_WIN32)
#  define _POSIX_C_SOURCE 200809L
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Portable strdup shim: use _strdup on Windows (MinGW/MSVC), strdup elsewhere */
#ifdef _WIN32
#  define vw_strdup(s) _strdup(s)
#else
#  define vw_strdup(s) strdup(s)
#endif

#include "graph.h"
#include "hashmap.h"
#include "inp_parser.h"
#include "isolation.h"
#include "optimizer.h"
#include "json_output.h"

/* ── CLI defaults ─────────────────────────────────────────────────── */
#define DEFAULT_OUTPUT  "output.json"
#define DEFAULT_BUDGET  3
#define DEFAULT_SEED    42
#define DEFAULT_MODE    "full"

/* ── Helpers ──────────────────────────────────────────────────────── */

static void print_usage(const char *prog) {
    fprintf(stderr,
        "Usage: %s --input <file.inp> [options]\n"
        "Options:\n"
        "  --output <file>       JSON output path (default: output.json)\n"
        "  --budget N            Valve placement budget (default: 3)\n"
        "  --fail id1,id2,...    Comma-separated pipe IDs to burst\n"
        "  --stuck id1,id2,...   Comma-separated valve IDs to mark stuck-open\n"
        "  --mode full|rank|reach  Analysis mode (default: full)\n"
        "  --seed N              RNG seed for random placement (default: 42)\n"
        "  --verbose             Print progress to stderr\n",
        prog);
}

/** Split a comma-separated string of IDs into an array. Caller frees. */
static char **split_ids(const char *csv, int *out_count) {
    *out_count = 0;
    if (!csv || csv[0] == '\0') return NULL;

    char *copy = vw_strdup(csv);
    if (!copy) return NULL;

    int cap = 16, cnt = 0;
    char **arr = malloc((size_t)cap * sizeof(char *));
    if (!arr) { free(copy); return NULL; }

    char *tok = strtok(copy, ",");
    while (tok) {
        if (cnt >= cap) {
            cap *= 2;
            char **tmp = realloc(arr, (size_t)cap * sizeof(char *));
            if (!tmp) break;
            arr = tmp;
        }
        arr[cnt++] = vw_strdup(tok);
        tok = strtok(NULL, ",");
    }
    free(copy);
    *out_count = cnt;
    return arr;
}

static void free_ids(char **ids, int count) {
    if (!ids) return;
    for (int i = 0; i < count; i++) free(ids[i]);
    free(ids);
}

/* ── main ─────────────────────────────────────────────────────────── */

int main(int argc, char *argv[]) {
    if (argc < 2) { print_usage(argv[0]); return 1; }

    /* Parse CLI arguments */
    const char *input_file  = NULL;
    const char *output_file = DEFAULT_OUTPUT;
    const char *mode        = DEFAULT_MODE;
    const char *fail_csv    = NULL;
    const char *stuck_csv   = NULL;
    int         budget      = DEFAULT_BUDGET;
    unsigned    seed        = DEFAULT_SEED;
    bool        verbose     = false;

    for (int i = 1; i < argc; i++) {
        if      (strcmp(argv[i], "--input")   == 0 && i+1 < argc) input_file  = argv[++i];
        else if (strcmp(argv[i], "--output")  == 0 && i+1 < argc) output_file = argv[++i];
        else if (strcmp(argv[i], "--budget")  == 0 && i+1 < argc) budget      = atoi(argv[++i]);
        else if (strcmp(argv[i], "--fail")    == 0 && i+1 < argc) fail_csv    = argv[++i];
        else if (strcmp(argv[i], "--stuck")   == 0 && i+1 < argc) stuck_csv   = argv[++i];
        else if (strcmp(argv[i], "--mode")    == 0 && i+1 < argc) mode        = argv[++i];
        else if (strcmp(argv[i], "--seed")    == 0 && i+1 < argc) seed        = (unsigned)atoi(argv[++i]);
        else if (strcmp(argv[i], "--verbose") == 0)                verbose     = true;
        else if (strcmp(argv[i], "--help")    == 0) { print_usage(argv[0]); return 0; }
    }

    if (!input_file) {
        fprintf(stderr, "Error: --input is required.\n");
        print_usage(argv[0]);
        return 1;
    }

    /* ── 1. Build graph and parse .inp ──────────────────────────── */
    if (verbose) fprintf(stderr, "[1/6] Parsing %s ...\n", input_file);

    Graph        *g  = graph_create(32, 64);
    NetworkIndex *ni = network_index_create();
    if (!g || !ni) { fprintf(stderr, "Out of memory\n"); return 1; }

    if (parse_inp_file(input_file, g, ni) != 0) {
        fprintf(stderr, "Failed to parse '%s'\n", input_file);
        graph_free(g); network_index_free(ni);
        return 1;
    }
    if (verbose) graph_print_summary(g);

    /* ── 2. Apply --fail ───────────────────────────────────────── */
    int   fail_count = 0;
    char **fail_ids  = split_ids(fail_csv, &fail_count);
    int  *failed_link_indices = NULL;
    int   fli_count = 0;

    if (fail_count > 0) {
        failed_link_indices = malloc((size_t)fail_count * sizeof(int));
        if (verbose) fprintf(stderr, "[2/6] Simulating %d pipe failure(s)...\n", fail_count);
        for (int i = 0; i < fail_count; i++) {
            int idx = -1;
            if (!hashmap_get(ni->link_map, fail_ids[i], &idx)) {
                fprintf(stderr, "Warning: pipe '%s' not found, skipping.\n", fail_ids[i]);
                continue;
            }
            simulate_failure(g, idx);
            failed_link_indices[fli_count++] = idx;
        }
    }
    free_ids(fail_ids, fail_count);

    /* ── 3. Apply --stuck ──────────────────────────────────────── */
    int   stuck_count = 0;
    char **stuck_ids  = split_ids(stuck_csv, &stuck_count);

    if (stuck_count > 0) {
        if (verbose) fprintf(stderr, "[3/6] Marking %d stuck valve(s)...\n", stuck_count);
        for (int i = 0; i < stuck_count; i++) {
            int idx = -1;
            if (!hashmap_get(ni->link_map, stuck_ids[i], &idx)) {
                fprintf(stderr, "Warning: valve '%s' not found, skipping.\n", stuck_ids[i]);
                continue;
            }
            g->links[idx].status = STATUS_STUCK;
        }
    }
    free_ids(stuck_ids, stuck_count);

    /* ── 4. Structural analysis ────────────────────────────────── */
    if (verbose) fprintf(stderr, "[4/6] Running structural analysis (DFS/Tarjan)...\n");
    StructuralResult *sr = compute_structural_analysis(g);

    /* ── 5. Reachability ───────────────────────────────────────── */
    if (verbose) fprintf(stderr, "[5/6] Computing reachability (BFS)...\n");
    ReachabilityResult *reach = compute_reachability(g);

    /* ── 6. Isolation boundary ─────────────────────────────────── */
    IsolationResult *iso = NULL;
    if (fli_count > 0) {
        if (verbose) fprintf(stderr, "      Computing isolation boundary...\n");
        iso = compute_isolation(g, failed_link_indices, fli_count);
    }

    /* ── 7. Rank critical links ────────────────────────────────── */
    if (verbose) fprintf(stderr, "      Ranking critical links...\n");
    int ranked_count = 0;
    RankedLink *ranked = rank_links(g, sr, RANK_BY_DISRUPTION, &ranked_count);

    /* ── 8. Optimization (full mode only) ──────────────────────── */
    OptimizationReport *report = NULL;
    if (strcmp(mode, "full") == 0) {
        if (verbose) fprintf(stderr, "[6/6] Running optimization (budget=%d)...\n", budget);
        report = run_full_optimization(g, budget,
                                       failed_link_indices, fli_count,
                                       sr, seed);
    }

    /* ── 9. Serialize and write JSON ───────────────────────────── */
    if (verbose) fprintf(stderr, "      Writing %s ...\n", output_file);
    char *json = json_full_report(g, reach, iso, sr, ranked, ranked_count, report);
    if (json) {
        if (json_write_to_file(output_file, json) == 0) {
            if (verbose) fprintf(stderr, "Done. Results written to %s\n", output_file);
            else printf("%s\n", output_file);
        } else {
            fprintf(stderr, "Error: could not write to '%s'\n", output_file);
        }
        json_free(json);
    }

    /* ── 10. Print summary to stderr if verbose ─────────────────── */
    if (verbose && reach) {
        fprintf(stderr, "\n=== Summary ===\n");
        fprintf(stderr, "  Nodes: %d | Links: %d | Sources: %d\n",
                g->node_count, g->link_count, g->source_count);
        fprintf(stderr, "  Disconnected junctions: %d (%.2f L/s demand)\n",
                reach->disconnected_count, reach->disconnected_demand);
        if (iso) {
            fprintf(stderr, "  Valves to close: %d\n", iso->valve_count);
            fprintf(stderr, "  Extra disrupted by stuck valves: %d\n", iso->extra_disrupted_count);
            fprintf(stderr, "  Total disrupted demand after isolation: %.2f L/s\n",
                    iso->total_disrupted_demand);
        }
        if (sr) fprintf(stderr, "  Bridges: %d | Articulation points: %d\n",
                        sr->bridge_count, sr->ap_count);
        if (report && report->greedy)
            fprintf(stderr, "  Greedy optimization: %.1f%% improvement (%.3f ms)\n",
                    report->greedy->improvement_pct, report->greedy->runtime_ms);
    }

    /* ── Cleanup ─────────────────────────────────────────────────── */
    free(failed_link_indices);
    free(ranked);
    free_reachability(reach);
    free_isolation(iso);
    free_structural(sr);
    free_optimization_report(report);
    network_index_free(ni);
    graph_free(g);
    return 0;
}
