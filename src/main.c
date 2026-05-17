/* =============================================================================
 * main.c  –  Application entry point (cross-platform)
 *
 * Windows:  WinMain (required by LVGL Win32 driver)
 * Linux:    main()
 *
 * Usage:
 *   lvgl_pdf_viewer <path_to_pdf> [width] [height]
 *
 *   Defaults: 800x480 if no size given (or if size < minimum)
 * =========================================================================== */

#include "lvgl.h"
#include "platform/platform.h"
#include "pdf/pdf_view.h"
#include "ui/ui_main.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DEFAULT_W 800
#define DEFAULT_H 480
#define PDF_MEM_STORE_MB 256

/* -------------------------------------------------------------------------- */
/* Argument parsing helper                                                     */
/* -------------------------------------------------------------------------- */

typedef struct {
    const char *pdf_path;
    int width;
    int height;
} AppArgs;

static void parse_args(int argc, char **argv, AppArgs *args)
{
    args->pdf_path = NULL;
    args->width    = DEFAULT_W;
    args->height   = DEFAULT_H;

    /* Collect non-option positional args */
    int pos = 0;
    for (int i = 1; i < argc; i++) {
        if (pos == 0) { args->pdf_path = argv[i]; pos++; }
        else if (pos == 1) { args->width  = atoi(argv[i]); pos++; }
        else if (pos == 2) { args->height = atoi(argv[i]); pos++; }
    }

    if (args->width  < 320) args->width  = DEFAULT_W;
    if (args->height < 240) args->height = DEFAULT_H;
}

/* -------------------------------------------------------------------------- */
/* Common startup / main loop                                                  */
/* -------------------------------------------------------------------------- */

static int run(int argc, char **argv)
{
    AppArgs args;
    parse_args(argc, argv, &args);

    if (args.pdf_path == NULL) {
        fprintf(stderr, "Usage: lvgl_pdf_viewer <path.pdf> [width] [height]\n");
        return 1;
    }

    printf("[main] PDF: %s  Display: %dx%d\n",
           args.pdf_path, args.width, args.height);

    /* 1. LVGL init */
    lv_init();

    /* 2. Platform (window + input) */
    platform_init(args.width, args.height);

    /* 3. MuPDF */
    pdf_view_sys_init(PDF_MEM_STORE_MB);

    pdf_view_t *view = pdf_view_open(args.pdf_path);
    if (view == NULL) {
        fprintf(stderr, "[main] Cannot open PDF: %s\n", pdf_view_last_error());
        pdf_view_sys_deinit();
        return 1;
    }

    /* 4. UI */
    ui_main_create(view, args.width, args.height);

    /* 5. Main loop */
    while (!platform_should_quit()) {
        platform_loop_once();
    }

    /* 6. Cleanup */
    pdf_view_close(view);
    pdf_view_sys_deinit();
    lv_deinit();

    return 0;
}

/* -------------------------------------------------------------------------- */
/* Platform entry points                                                       */
/* -------------------------------------------------------------------------- */

#ifdef _WIN32

#include <windows.h>

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev,
                   LPSTR lpCmdLine, int nCmdShow)
{
    (void)hInst; (void)hPrev; (void)nCmdShow;

    /* Convert LPSTR command line to argc/argv */
    int    argc = 0;
    LPWSTR *wargv = CommandLineToArgvW(GetCommandLineW(), &argc);
    char  **argv = calloc(argc + 1, sizeof(char *));
    for (int i = 0; i < argc; i++) {
        int len = WideCharToMultiByte(CP_UTF8, 0, wargv[i], -1,
                                      NULL, 0, NULL, NULL);
        argv[i] = malloc(len);
        WideCharToMultiByte(CP_UTF8, 0, wargv[i], -1,
                            argv[i], len, NULL, NULL);
    }
    LocalFree(wargv);

    int ret = run(argc, argv);

    for (int i = 0; i < argc; i++) free(argv[i]);
    free(argv);
    return ret;
}

#else  /* Linux / T507 */

int main(int argc, char **argv)
{
    return run(argc, argv);
}

#endif
