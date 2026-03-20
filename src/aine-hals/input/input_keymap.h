/* input_keymap.h — macOS virtual key → Android KEYCODE translation */
#ifndef AINE_INPUT_KEYMAP_H
#define AINE_INPUT_KEYMAP_H

#include "input_event.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Translate Carbon virtual key code to Android AKEYCODE */
AKeyCode aine_input_vk_to_keycode(unsigned char vk);

/* Translate NSEventModifierFlags (uint64_t) → Android AMETA_* bitmask */
uint32_t aine_input_modifiers_to_meta(uint64_t ns_modifier_flags);

#ifdef __cplusplus
}
#endif

#endif /* AINE_INPUT_KEYMAP_H */
