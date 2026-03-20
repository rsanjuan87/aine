/*
 * aine-hals/input/input_keymap.c — macOS virtual key → Android KEYCODE table
 *
 * macOS key codes are Carbon virtual key codes (kVK_* in Carbon/Events.h).
 * Pure C — no AppKit/CoreGraphics needed.
 */

#include "input_keymap.h"
#include <stddef.h>

/* macOS Carbon virtual key codes (kVK_*) */
#define VK_A              0x00
#define VK_S              0x01
#define VK_D              0x02
#define VK_F              0x03
#define VK_H              0x04
#define VK_G              0x05
#define VK_Z              0x06
#define VK_X              0x07
#define VK_C              0x08
#define VK_V              0x09
#define VK_B              0x0B
#define VK_Q              0x0C
#define VK_W              0x0D
#define VK_E              0x0E
#define VK_R              0x0F
#define VK_Y              0x10
#define VK_T              0x11
#define VK_1              0x12
#define VK_2              0x13
#define VK_3              0x14
#define VK_4              0x15
#define VK_6              0x16
#define VK_5              0x17
#define VK_EQUAL          0x18
#define VK_9              0x19
#define VK_7              0x1A
#define VK_MINUS          0x1B
#define VK_8              0x1C
#define VK_0              0x1D
#define VK_RIGHT_BRACKET  0x1E
#define VK_O              0x1F
#define VK_U              0x20
#define VK_LEFT_BRACKET   0x21
#define VK_I              0x22
#define VK_P              0x23
#define VK_RETURN         0x24
#define VK_L              0x25
#define VK_J              0x26
#define VK_APOSTROPHE     0x27
#define VK_K              0x28
#define VK_SEMICOLON      0x29
#define VK_BACKSLASH      0x2A
#define VK_COMMA          0x2B
#define VK_SLASH          0x2C
#define VK_N              0x2D
#define VK_M              0x2E
#define VK_PERIOD         0x2F
#define VK_TAB            0x30
#define VK_SPACE          0x31
#define VK_GRAVE          0x32
#define VK_DELETE         0x33  /* Backspace */
#define VK_ESCAPE         0x35
#define VK_COMMAND        0x37
#define VK_SHIFT          0x38
#define VK_CAPS_LOCK      0x39
#define VK_OPTION         0x3A
#define VK_CONTROL        0x3B
#define VK_SHIFT_R        0x3C
#define VK_OPTION_R       0x3D
#define VK_CONTROL_R      0x3E
#define VK_COMMAND_R      0x36
#define VK_F1             0x7A
#define VK_F2             0x78
#define VK_F3             0x63
#define VK_F4             0x76
#define VK_F5             0x60
#define VK_F6             0x61
#define VK_F7             0x62
#define VK_F8             0x64
#define VK_F9             0x65
#define VK_F10            0x6D
#define VK_F11            0x67
#define VK_F12            0x6F
#define VK_HOME           0x73
#define VK_PAGE_UP        0x74
#define VK_FORWARD_DEL    0x75
#define VK_END            0x77
#define VK_PAGE_DOWN      0x79
#define VK_ARROW_LEFT     0x7B
#define VK_ARROW_RIGHT    0x7C
#define VK_ARROW_DOWN     0x7D
#define VK_ARROW_UP       0x7E

/* Mapping entry */
#define MAP(vk, ak) { (vk), (ak) }
#define MAP_END     { 0xFF, 0 }

static const struct { unsigned char vk; AKeyCode ak; } k_keymap[] = {
    MAP(VK_A, AKEYCODE_A), MAP(VK_B, AKEYCODE_B), MAP(VK_C, AKEYCODE_C),
    MAP(VK_D, AKEYCODE_D), MAP(VK_E, AKEYCODE_E), MAP(VK_F, AKEYCODE_F),
    MAP(VK_G, AKEYCODE_G), MAP(VK_H, AKEYCODE_H), MAP(VK_I, AKEYCODE_I),
    MAP(VK_J, AKEYCODE_J), MAP(VK_K, AKEYCODE_K), MAP(VK_L, AKEYCODE_L),
    MAP(VK_M, AKEYCODE_M), MAP(VK_N, AKEYCODE_N), MAP(VK_O, AKEYCODE_O),
    MAP(VK_P, AKEYCODE_P), MAP(VK_Q, AKEYCODE_Q), MAP(VK_R, AKEYCODE_R),
    MAP(VK_S, AKEYCODE_S), MAP(VK_T, AKEYCODE_T), MAP(VK_U, AKEYCODE_U),
    MAP(VK_V, AKEYCODE_V), MAP(VK_W, AKEYCODE_W), MAP(VK_X, AKEYCODE_X),
    MAP(VK_Y, AKEYCODE_Y), MAP(VK_Z, AKEYCODE_Z),
    MAP(VK_0, AKEYCODE_0), MAP(VK_1, AKEYCODE_1), MAP(VK_2, AKEYCODE_2),
    MAP(VK_3, AKEYCODE_3), MAP(VK_4, AKEYCODE_4), MAP(VK_5, AKEYCODE_5),
    MAP(VK_6, AKEYCODE_6), MAP(VK_7, AKEYCODE_7), MAP(VK_8, AKEYCODE_8),
    MAP(VK_9, AKEYCODE_9),
    MAP(VK_RETURN,       AKEYCODE_ENTER),
    MAP(VK_SPACE,        AKEYCODE_SPACE),
    MAP(VK_TAB,          AKEYCODE_TAB),
    MAP(VK_DELETE,       AKEYCODE_DEL),
    MAP(VK_FORWARD_DEL,  AKEYCODE_FORWARD_DEL),
    MAP(VK_ESCAPE,       AKEYCODE_ESCAPE),
    MAP(VK_COMMA,        AKEYCODE_COMMA),
    MAP(VK_PERIOD,       AKEYCODE_PERIOD),
    MAP(VK_SLASH,        AKEYCODE_SLASH),
    MAP(VK_SEMICOLON,    AKEYCODE_SEMICOLON),
    MAP(VK_APOSTROPHE,   AKEYCODE_APOSTROPHE),
    MAP(VK_LEFT_BRACKET, AKEYCODE_LEFT_BRACKET),
    MAP(VK_RIGHT_BRACKET,AKEYCODE_RIGHT_BRACKET),
    MAP(VK_BACKSLASH,    AKEYCODE_BACKSLASH),
    MAP(VK_MINUS,        AKEYCODE_MINUS),
    MAP(VK_EQUAL,        AKEYCODE_EQUALS),
    MAP(VK_GRAVE,        AKEYCODE_GRAVE),
    MAP(VK_SHIFT,        AKEYCODE_SHIFT_LEFT),
    MAP(VK_SHIFT_R,      AKEYCODE_SHIFT_RIGHT),
    MAP(VK_CONTROL,      AKEYCODE_CTRL_LEFT),
    MAP(VK_CONTROL_R,    AKEYCODE_CTRL_RIGHT),
    MAP(VK_OPTION,       AKEYCODE_ALT_LEFT),
    MAP(VK_OPTION_R,     AKEYCODE_ALT_RIGHT),
    MAP(VK_COMMAND,      AKEYCODE_META_LEFT),
    MAP(VK_COMMAND_R,    AKEYCODE_META_RIGHT),
    MAP(VK_F1,  AKEYCODE_F1),  MAP(VK_F2,  AKEYCODE_F2),
    MAP(VK_F3,  AKEYCODE_F3),  MAP(VK_F4,  AKEYCODE_F4),
    MAP(VK_F5,  AKEYCODE_F5),  MAP(VK_F6,  AKEYCODE_F6),
    MAP(VK_F7,  AKEYCODE_F7),  MAP(VK_F8,  AKEYCODE_F8),
    MAP(VK_F9,  AKEYCODE_F9),  MAP(VK_F10, AKEYCODE_F10),
    MAP(VK_F11, AKEYCODE_F11), MAP(VK_F12, AKEYCODE_F12),
    MAP(VK_ARROW_UP,    AKEYCODE_DPAD_UP),
    MAP(VK_ARROW_DOWN,  AKEYCODE_DPAD_DOWN),
    MAP(VK_ARROW_LEFT,  AKEYCODE_DPAD_LEFT),
    MAP(VK_ARROW_RIGHT, AKEYCODE_DPAD_RIGHT),
    MAP(VK_PAGE_UP,     AKEYCODE_PAGE_UP),
    MAP(VK_PAGE_DOWN,   AKEYCODE_PAGE_DOWN),
    MAP(VK_HOME,        AKEYCODE_MOVE_HOME),
    MAP(VK_END,         AKEYCODE_MOVE_END),
    MAP_END
};

AKeyCode aine_input_vk_to_keycode(unsigned char vk)
{
    for (int i = 0; k_keymap[i].vk != 0xFF; i++) {
        if (k_keymap[i].vk == vk) return k_keymap[i].ak;
    }
    return AKEYCODE_UNKNOWN;
}

/* Translate macOS NSEventModifierFlags bits → Android meta_state bitmask */
uint32_t aine_input_modifiers_to_meta(uint64_t ns_modifier_flags)
{
    uint32_t meta = 0;
    /* NSEventModifierFlagShift = 1<<17, NSEventModifierFlagControl = 1<<18 */
    /* NSEventModifierFlagOption = 1<<19, NSEventModifierFlagCommand = 1<<20 */
    if (ns_modifier_flags & (1ULL<<17)) meta |= AMETA_SHIFT_ON;
    if (ns_modifier_flags & (1ULL<<18)) meta |= AMETA_CTRL_ON;
    if (ns_modifier_flags & (1ULL<<19)) meta |= AMETA_ALT_ON;
    if (ns_modifier_flags & (1ULL<<20)) meta |= AMETA_META_ON;
    return meta;
}
