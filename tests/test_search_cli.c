/* =============================================================================
 * test_search_cli.c — minimal CLI smoke for pdf_view_search()
 *
 *   usage: test_search_cli <pdf> <needle> [start_page] [max_hits]
 *
 *   Exit codes:
 *     0 — at least one hit found
 *     1 — zero hits (still "ran" successfully — useful for negative tests)
 *     2 — open / API error
 *
 *   Prints one line per hit: page <N>  bbox=<x>,<y>,<w>,<h>
 * =========================================================================== */

#include "pdf_view.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

int main(int argc, char **argv)
{
    if (argc < 3) {
        fprintf(stderr,
                "usage: %s <pdf> <needle> [start_page=0] [max_hits=200]\n",
                argv[0]);
        return 2;
    }
    const char *path   = argv[1];
    const char *needle = argv[2];
    int start = (argc > 3) ? atoi(argv[3]) : 0;
    int cap   = (argc > 4) ? atoi(argv[4]) : 200;

    pdf_view_sys_init(0);
    pdf_view_t *v = pdf_view_open(path);
    if (!v) {
        fprintf(stderr, "open failed: %s\n", pdf_view_last_error());
        pdf_view_sys_deinit();
        return 2;
    }
    int total = pdf_view_page_count(v);
    fprintf(stderr, "[search-cli] pages=%d needle=\"%s\" start=%d cap=%d\n",
            total, needle, start, cap);

    pdf_search_result_t *res =
        pdf_view_search(v, needle, start, /*wrap*/ true, cap);
    if (!res) {
        fprintf(stderr, "search failed: %s\n", pdf_view_last_error());
        pdf_view_close(v);
        pdf_view_sys_deinit();
        return 2;
    }

    printf("HITS=%d truncated=%d\n", res->count, res->truncated);
    for (int i = 0; i < res->count; i++) {
        pdf_search_hit_t *h = &res->hits[i];
        printf("  [%d] page=%d bbox=%.2f,%.2f,%.2f,%.2f\n",
               i, h->page, h->x, h->y, h->w, h->h);
    }
    int rc = (res->count > 0) ? 0 : 1;
    pdf_search_result_free(res);
    pdf_view_close(v);
    pdf_view_sys_deinit();
    return rc;
}
