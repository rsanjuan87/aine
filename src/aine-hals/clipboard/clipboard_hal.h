// aine-hals/clipboard/clipboard_hal.h — AINE Clipboard HAL
// Maps android.content.ClipboardManager ↔ NSPasteboard (macOS ARM64).
#pragma once
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Set plain-text content to clipboard.  text must be NUL-terminated. */
int aine_clip_set_text(const char *text);

/* Get plain-text content from clipboard.
 * Writes at most buf_size-1 bytes into buf and NUL-terminates.
 * Returns number of bytes written (excluding NUL), or -1 on error. */
int aine_clip_get_text(char *buf, size_t buf_size);

/* Returns 1 if clipboard has text content, 0 otherwise. */
int aine_clip_has_text(void);

/* Clear clipboard. */
void aine_clip_clear(void);

#ifdef __cplusplus
}
#endif
