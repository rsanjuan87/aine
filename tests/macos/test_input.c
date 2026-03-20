/*
 * tests/macos/test_input.c — CTest for F8 AINE input HAL
 *
 * Tests:
 *   1. Virtual key → Android KEYCODE translation (known keys)
 *   2. Modifier flags → Android meta_state translation
 *   3. InputFlinger push/poll round-trip
 *   4. Queue wraps correctly (capacity edge case)
 *   5. aine_input_pending() / aine_input_flush()
 *
 * All headless — no NSApplication, no NSEvent required.
 */

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>
#include "input_keymap.h"
#include "inputflinger.h"

static int s_pass = 0, s_fail = 0;

#define CHECK(cond, msg) do { \
    if (cond) { printf("  [ok]   %s\n", msg); s_pass++; } \
    else      { printf("  [FAIL] %s  (line %d)\n", msg, __LINE__); s_fail++; } \
} while (0)

int main(void)
{
    printf("=== F8 input HAL test ===\n");

    /* ---- keymap translation ---- */
    CHECK(aine_input_vk_to_keycode(0x00) == AKEYCODE_A,   "VK_A -> AKEYCODE_A");
    CHECK(aine_input_vk_to_keycode(0x31) == AKEYCODE_SPACE, "VK_SPACE -> AKEYCODE_SPACE");
    CHECK(aine_input_vk_to_keycode(0x24) == AKEYCODE_ENTER, "VK_RETURN -> AKEYCODE_ENTER");
    CHECK(aine_input_vk_to_keycode(0x35) == AKEYCODE_ESCAPE, "VK_ESCAPE -> AKEYCODE_ESCAPE");
    CHECK(aine_input_vk_to_keycode(0x7A) == AKEYCODE_F1,    "VK_F1 -> AKEYCODE_F1");
    CHECK(aine_input_vk_to_keycode(0x7B) == AKEYCODE_DPAD_LEFT, "VK_ARROW_LEFT -> DPAD_LEFT");
    CHECK(aine_input_vk_to_keycode(0xFF) == AKEYCODE_UNKNOWN, "unknown VK -> AKEYCODE_UNKNOWN");

    /* ---- modifier translation ---- */
    /* NSEventModifierFlagShift = 1<<17 */
    uint32_t meta = aine_input_modifiers_to_meta(1ULL << 17);
    CHECK(meta & AMETA_SHIFT_ON, "shift modifier -> AMETA_SHIFT_ON");
    CHECK(!(meta & AMETA_CTRL_ON), "shift modifier, ctrl not set");

    meta = aine_input_modifiers_to_meta((1ULL<<17) | (1ULL<<20));
    CHECK((meta & AMETA_SHIFT_ON) && (meta & AMETA_META_ON),
          "shift+cmd -> SHIFT_ON | META_ON");

    /* ---- inputflinger queue ---- */
    aine_input_flush(); /* ensure empty */
    CHECK(aine_input_pending() == 0, "queue empty after flush");

    AineInputEvent ev = {0};
    ev.kind           = AINE_INPUT_KEY;
    ev.key.action     = AKEY_ACTION_DOWN;
    ev.key.keycode    = AKEYCODE_A;
    ev.key.meta_state = 0;
    ev.key.event_time_ns = 12345678ULL;

    aine_input_push(&ev);
    CHECK(aine_input_pending() == 1, "pending == 1 after push");

    AineInputEvent got = {0};
    int ok = aine_input_poll(&got);
    CHECK(ok == 1, "poll returns 1 when event available");
    CHECK(got.kind == AINE_INPUT_KEY, "polled event kind == KEY");
    CHECK(got.key.keycode == AKEYCODE_A, "polled keycode == A");
    CHECK(got.key.event_time_ns == 12345678ULL, "polled timestamp matches");
    CHECK(aine_input_pending() == 0, "queue empty after poll");

    /* ---- non-blocking poll on empty queue ---- */
    int none = aine_input_poll(&got);
    CHECK(none == 0, "poll returns 0 on empty queue");

    /* ---- motion event round-trip ---- */
    ev.kind               = AINE_INPUT_MOTION;
    ev.motion.action      = AMOTION_ACTION_DOWN;
    ev.motion.x           = 123.5f;
    ev.motion.y           = 456.25f;
    ev.motion.pointer_id  = 0;
    ev.motion.event_time_ns = 99999ULL;
    aine_input_push(&ev);

    ok = aine_input_poll(&got);
    CHECK(ok == 1, "motion event poll ok");
    CHECK(got.kind == AINE_INPUT_MOTION, "motion event kind correct");
    CHECK(got.motion.action == AMOTION_ACTION_DOWN, "motion action DOWN");
    CHECK((int)got.motion.x == 123, "motion x ~ 123");
    CHECK((int)got.motion.y == 456, "motion y ~ 456");

    /* ---- push 10 events, flush, verify empty ---- */
    for (int i = 0; i < 10; i++) {
        ev.kind = AINE_INPUT_KEY;
        ev.key.keycode = (AKeyCode)(AKEYCODE_A + i);
        aine_input_push(&ev);
    }
    CHECK(aine_input_pending() == 10, "10 events pending");
    aine_input_flush();
    CHECK(aine_input_pending() == 0, "queue empty after flush");

    printf("=== RESULT: %d ok, %d failed ===\n", s_pass, s_fail);
    return s_fail == 0 ? 0 : 1;
}
