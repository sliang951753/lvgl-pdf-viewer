/* =============================================================================
 * test_pageapi_cli.c — headless smoke for non-search pdf_view APIs
 *
 *   usage: test_pageapi_cli <pdf>
 *
 *   Exercises (without any display server):
 *     - pdf_view_sys_init / pdf_view_open / pdf_view_close
 *     - pdf_view_page_count returns > 0
 *     - pdf_view_page_size returns plausible positive points for every page
 *     - pdf_view_page_size on out-of-range index returns false
 *     - pdf_view_render_page on page 0 returns a non-NULL descriptor with
 *       header.w > 0 and header.h > 0
 *     - pdf_view_prefetch + pdf_view_cache_clear do not crash
 *
 *   Exit codes: 0 = all pass, 1 = at least one failure, 2 = open / API error.
 * =========================================================================== */

#include "pdf_view.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

static int g_pass = 0;
static int g_fail = 0;

#define CHECK(cond, label) do {                          \
    if (cond) { printf("  PASS  %s\n", label); g_pass++; } \
    else      { printf("  FAIL  %s\n", label); g_fail++; } \
} while (0)

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "usage: %s <pdf>\n", argv[0]);
        return 2;
    }
    const char *path = argv[1];

    pdf_view_sys_init(0);
    pdf_view_t *v = pdf_view_open(path);
    if (!v) {
        fprintf(stderr, "open failed: %s\n", pdf_view_last_error());
        pdf_view_sys_deinit();
        return 2;
    }

    int n = pdf_view_page_count(v);
    CHECK(n > 0, "page_count > 0");

    /* page_size on every page: positive, finite */
    bool all_sizes_ok = true;
    for (int i = 0; i < n; i++) {
        float w = 0, h = 0;
        bool ok = pdf_view_page_size(v, i, &w, &h);
        if (!ok || w <= 0.0f || h <= 0.0f) {
            fprintf(stderr, "page %d size bad: ok=%d w=%.2f h=%.2f\n",
                    i, ok, w, h);
            all_sizes_ok = false;
            break;
        }
    }
    CHECK(all_sizes_ok, "page_size positive for every page");

    /* page_size on out-of-range index */
    float w = 7, h = 7;
    bool bad = pdf_view_page_size(v, n + 100, &w, &h);
    CHECK(!bad, "page_size rejects out-of-range index");

    bool bad2 = pdf_view_page_size(v, -1, &w, &h);
    CHECK(!bad2, "page_size rejects negative index");

    /* render page 0 */
    const lv_image_dsc_t *dsc =
        pdf_view_render_page(v, 0, 800, 600, 1.0f);
    bool render_ok = dsc != NULL && dsc->header.w > 0 && dsc->header.h > 0;
    CHECK(render_ok, "render_page(0) returns dsc with w>0 h>0");

    /* prefetch + cache_clear should never crash */
    pdf_view_prefetch(v, 0, 800, 600, 1.0f);
    pdf_view_cache_clear(v);
    CHECK(true, "prefetch + cache_clear no-crash");

    /* re-render after cache_clear */
    const lv_image_dsc_t *dsc2 =
        pdf_view_render_page(v, 0, 800, 600, 1.0f);
    CHECK(dsc2 != NULL && dsc2->header.w > 0,
          "render_page(0) after cache_clear");

    pdf_view_close(v);
    pdf_view_sys_deinit();

    printf("\n  TOTAL  pass=%d fail=%d\n", g_pass, g_fail);
    return (g_fail == 0) ? 0 : 1;
}
