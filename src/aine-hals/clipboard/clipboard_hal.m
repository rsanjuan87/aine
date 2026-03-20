/*
 * src/aine-hals/clipboard/clipboard_hal.m — AINE Clipboard HAL
 *
 * NSPasteboard ↔ android.content.ClipboardManager bridge.
 * Thread-safe: NSPasteboard API is safe to call from any thread.
 */

#import <AppKit/NSPasteboard.h>
#import <Foundation/Foundation.h>
#include "clipboard_hal.h"
#include <string.h>

/* ── set text ─────────────────────────────────────────────────────── */
int aine_clip_set_text(const char *text)
{
    if (!text) return -1;
    NSPasteboard *pb = [NSPasteboard generalPasteboard];
    [pb clearContents];
    NSString *str = [NSString stringWithUTF8String:text];
    if (!str) return -1;
    BOOL ok = [pb setString:str forType:NSPasteboardTypeString];
    return ok ? 0 : -1;
}

/* ── get text ─────────────────────────────────────────────────────── */
int aine_clip_get_text(char *buf, size_t buf_size)
{
    if (!buf || buf_size == 0) return -1;
    NSPasteboard *pb = [NSPasteboard generalPasteboard];
    NSString *str = [pb stringForType:NSPasteboardTypeString];
    if (!str) { buf[0] = '\0'; return 0; }
    const char *utf8 = str.UTF8String;
    if (!utf8) { buf[0] = '\0'; return 0; }
    size_t len = strlen(utf8);
    size_t copy = (len < buf_size - 1) ? len : (buf_size - 1);
    memcpy(buf, utf8, copy);
    buf[copy] = '\0';
    return (int)copy;
}

/* ── has text ─────────────────────────────────────────────────────── */
int aine_clip_has_text(void)
{
    NSPasteboard *pb = [NSPasteboard generalPasteboard];
    NSArray<NSString *> *types = [pb types];
    for (NSString *t in types) {
        if ([t isEqualToString:NSPasteboardTypeString]) return 1;
    }
    return 0;
}

/* ── clear ────────────────────────────────────────────────────────── */
void aine_clip_clear(void)
{
    [[NSPasteboard generalPasteboard] clearContents];
}
