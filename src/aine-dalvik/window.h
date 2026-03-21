/* aine-dalvik/window.h — NSApplication bootstrap for --window mode.
 *
 * Wraps the Activity lifecycle inside a native NSWindow + NS run-loop pump.
 * Works headlessly too: if no display / Metal device is available, the
 * Activity still runs and the window is silently skipped.
 */
#pragma once

typedef struct AineInterp AineInterp; /* forward-declare; avoid pulling interp.h here */

#ifdef __cplusplus
extern "C" {
#endif

/*
 * aine_window_run — run an Activity inside (optionally) a native NSWindow.
 *
 *  1. Initialises NSApplication (best-effort; graceful on headless).
 *  2. Creates an NSWindow + CAMetalLayer backing (skipped if no Metal device).
 *  3. Dispatches interp_run_main() to a background thread.
 *  4. Pumps the main-thread NSRunLoop in 16 ms slices until the interpreter
 *     finishes (Activity lifecycle + any postDelayed handlers).
 *  5. Closes the window and returns the interpreter exit code.
 *
 * Must be called from the main thread.
 */
int aine_window_run(AineInterp *interp, const char *class_descriptor);

#ifdef __cplusplus
}
#endif
