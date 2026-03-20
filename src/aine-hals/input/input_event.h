/* aine-hals/input/input_event.h — Android input event types for AINE
 *
 * Mirrors android/input.h subset needed for NSEvent translation.
 */
#ifndef AINE_INPUT_EVENT_H
#define AINE_INPUT_EVENT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* -----------------------------------------------------------------------
 * Android KeyEvent action + keycodes (subset of android/keycodes.h)
 * ---------------------------------------------------------------------- */
typedef enum {
    AKEY_ACTION_DOWN  = 0,
    AKEY_ACTION_UP    = 1,
    AKEY_ACTION_MULTIPLE = 2,
} AKeyAction;

/* Common Android KEYCODE values (matching android/keycodes.h) */
typedef enum {
    AKEYCODE_UNKNOWN        = 0,
    AKEYCODE_HOME           = 3,
    AKEYCODE_BACK           = 4,
    AKEYCODE_DPAD_UP        = 19,
    AKEYCODE_DPAD_DOWN      = 20,
    AKEYCODE_DPAD_LEFT      = 21,
    AKEYCODE_DPAD_RIGHT     = 22,
    AKEYCODE_DPAD_CENTER    = 23,
    AKEYCODE_0              = 7,
    AKEYCODE_1              = 8,
    AKEYCODE_2              = 9,
    AKEYCODE_3              = 10,
    AKEYCODE_4              = 11,
    AKEYCODE_5              = 12,
    AKEYCODE_6              = 13,
    AKEYCODE_7              = 14,
    AKEYCODE_8              = 15,
    AKEYCODE_9              = 16,
    AKEYCODE_A              = 29,
    AKEYCODE_B              = 30,
    AKEYCODE_C              = 31,
    AKEYCODE_D              = 32,
    AKEYCODE_E              = 33,
    AKEYCODE_F              = 34,
    AKEYCODE_G              = 35,
    AKEYCODE_H              = 36,
    AKEYCODE_I              = 37,
    AKEYCODE_J              = 38,
    AKEYCODE_K              = 39,
    AKEYCODE_L              = 40,
    AKEYCODE_M              = 41,
    AKEYCODE_N              = 42,
    AKEYCODE_O              = 43,
    AKEYCODE_P              = 44,
    AKEYCODE_Q              = 45,
    AKEYCODE_R              = 46,
    AKEYCODE_S              = 47,
    AKEYCODE_T              = 48,
    AKEYCODE_U              = 49,
    AKEYCODE_V              = 50,
    AKEYCODE_W              = 51,
    AKEYCODE_X              = 52,
    AKEYCODE_Y              = 53,
    AKEYCODE_Z              = 54,
    AKEYCODE_COMMA          = 55,
    AKEYCODE_PERIOD         = 56,
    AKEYCODE_SPACE          = 62,
    AKEYCODE_ENTER          = 66,
    AKEYCODE_DEL            = 67,   /* Backspace */
    AKEYCODE_ESCAPE         = 111,
    AKEYCODE_CTRL_LEFT      = 113,
    AKEYCODE_CTRL_RIGHT     = 114,
    AKEYCODE_SHIFT_LEFT     = 59,
    AKEYCODE_SHIFT_RIGHT    = 60,
    AKEYCODE_ALT_LEFT       = 57,   /* Option */
    AKEYCODE_ALT_RIGHT      = 58,   /* Option-R */
    AKEYCODE_META_LEFT      = 117,  /* Cmd */
    AKEYCODE_META_RIGHT     = 118,  /* Cmd-R */
    AKEYCODE_TAB            = 61,
    AKEYCODE_MINUS          = 69,
    AKEYCODE_EQUALS         = 70,
    AKEYCODE_LEFT_BRACKET   = 71,
    AKEYCODE_RIGHT_BRACKET  = 72,
    AKEYCODE_BACKSLASH      = 73,
    AKEYCODE_SEMICOLON      = 74,
    AKEYCODE_APOSTROPHE     = 75,
    AKEYCODE_SLASH          = 76,
    AKEYCODE_AT             = 77,
    AKEYCODE_GRAVE          = 68,
    AKEYCODE_F1             = 131,
    AKEYCODE_F2             = 132,
    AKEYCODE_F3             = 133,
    AKEYCODE_F4             = 134,
    AKEYCODE_F5             = 135,
    AKEYCODE_F6             = 136,
    AKEYCODE_F7             = 137,
    AKEYCODE_F8             = 138,
    AKEYCODE_F9             = 139,
    AKEYCODE_F10            = 140,
    AKEYCODE_F11            = 141,
    AKEYCODE_F12            = 142,
    AKEYCODE_PAGE_UP        = 92,
    AKEYCODE_PAGE_DOWN      = 93,
    AKEYCODE_MOVE_HOME      = 122,
    AKEYCODE_MOVE_END       = 123,
    AKEYCODE_INSERT         = 124,
    AKEYCODE_FORWARD_DEL    = 112,
} AKeyCode;

/* Android meta key flags */
#define AMETA_SHIFT_ON       0x01
#define AMETA_ALT_ON         0x02
#define AMETA_CTRL_ON        0x1000
#define AMETA_META_ON        0x10000

/* -----------------------------------------------------------------------
 * Android MotionEvent action codes
 * ---------------------------------------------------------------------- */
typedef enum {
    AMOTION_ACTION_DOWN          = 0,
    AMOTION_ACTION_UP            = 1,
    AMOTION_ACTION_MOVE          = 2,
    AMOTION_ACTION_CANCEL        = 3,
    AMOTION_ACTION_POINTER_DOWN  = 5,
    AMOTION_ACTION_POINTER_UP    = 6,
    AMOTION_ACTION_HOVER_MOVE    = 7,
    AMOTION_ACTION_SCROLL        = 8,
} AMotionAction;

/* -----------------------------------------------------------------------
 * AINE input event structures
 * ---------------------------------------------------------------------- */
typedef struct {
    AKeyAction action;
    AKeyCode   keycode;
    uint32_t   meta_state;   /* AMETA_* bitmask */
    uint64_t   event_time_ns;
} AineKeyEvent;

typedef struct {
    AMotionAction action;
    float         x, y;          /* window-space pixels */
    float         raw_x, raw_y;  /* screen-space pixels */
    float         scroll_x;      /* for AMOTION_ACTION_SCROLL */
    float         scroll_y;
    uint32_t      pointer_id;
    uint64_t      event_time_ns;
} AineMotionEvent;

typedef enum {
    AINE_INPUT_KEY,
    AINE_INPUT_MOTION,
} AineInputEventKind;

typedef struct {
    AineInputEventKind kind;
    union {
        AineKeyEvent    key;
        AineMotionEvent motion;
    };
} AineInputEvent;

#ifdef __cplusplus
}
#endif

#endif /* AINE_INPUT_EVENT_H */
