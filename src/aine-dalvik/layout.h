/* aine-dalvik/layout.h — Generic Android XML layout inflater + renderer.
 *
 * AINE reads a companion "aine-res.txt" file (placed alongside the DEX) that
 * maps integer resource IDs to layout XML paths and string values.  When an
 * app calls setContentView(int), AINE inflates the layout and builds an
 * AineViewNode tree.  The tree is laid out and rendered to the canvas at each
 * frame (instead of dispatching onDraw to a custom View — system Views like
 * TextView and Button render themselves via AINE's generic view renderer).
 *
 * Supported view types: LinearLayout, TextView, Button, MaterialButton.
 * Supported layout attributes: orientation, layout_weight, match_parent,
 * wrap_content, padding, margin.
 *
 * This is generic Android API translation — nothing specific to any app.
 */
#pragma once
#include <stdint.h>

/* ── View types ─────────────────────────────────────────────────────────── */
typedef enum {
    AINE_VIEW_GENERIC   = 0,
    AINE_VIEW_LINEAR_H  = 1,   /* LinearLayout orientation=horizontal */
    AINE_VIEW_LINEAR_V  = 2,   /* LinearLayout orientation=vertical   */
    AINE_VIEW_TEXTVIEW  = 3,
    AINE_VIEW_BUTTON    = 4,
} AineViewType;

/* ── View node ──────────────────────────────────────────────────────────── */
#define AINE_VIEW_MAX_TEXT     512
#define AINE_VIEW_MAX_CHILDREN 64

typedef struct AineViewNode {
    int            res_id;     /* R.id.xxx  (0 = no id) */
    AineViewType   vtype;

    /* Text */
    char           text[AINE_VIEW_MAX_TEXT];
    uint32_t       text_color; /* 0xAARRGGBB */
    float          text_size;  /* logical pixels  */
    int            gravity_end;/* 1 = right-align text */

    /* Background */
    uint32_t       bg_color;
    int            has_bg;

    /* Layout params */
    float          layout_weight;
    int            layout_w;   /* -1=match_parent, -2=wrap_content, >0=dp */
    int            layout_h;
    float          padding_l, padding_t, padding_r, padding_b;
    float          margin;     /* uniform margin (buttons) */

    /* Computed geometry (filled by aine_layout_measure) */
    float          x, y, w, h;

    /* Children */
    struct AineViewNode *children[AINE_VIEW_MAX_CHILDREN];
    int                  n_children;
} AineViewNode;

/* ── Resource map ───────────────────────────────────────────────────────── */
#define AINE_RES_MAX_IDS     256
#define AINE_RES_MAX_STRINGS 256

typedef struct {
    uint32_t res_id;
    char     name[64];
} AineResIdEntry;

typedef struct {
    char          layout_xml[256];  /* local path to XML file */
    uint32_t      layout_res_id;    /* e.g. 0x7f040000 */

    AineResIdEntry ids[AINE_RES_MAX_IDS];
    int            n_ids;

    char           str_names [AINE_RES_MAX_STRINGS][64];
    char           str_values[AINE_RES_MAX_STRINGS][256];
    int            n_strings;
} AineResMap;

/* ── API ────────────────────────────────────────────────────────────────── */

/* Load aine-res.txt; base_dir is the directory containing the DEX file.
 * Returns NULL if file not found (graceful degradation). */
AineResMap   *aine_res_load(const char *base_dir);
void          aine_res_free(AineResMap *map);

/* Look up a string resource by name ("button_clear" → "AC"). */
const char   *aine_res_string(AineResMap *map, const char *name);

/* Look up the integer ID for a named view ("textDisplay" → 0x7f030014). */
uint32_t      aine_res_id_by_name(AineResMap *map, const char *name);

/* Inflate the layout associated with res_id.
 * Returns NULL if resource map has no matching layout. */
AineViewNode *aine_layout_inflate(AineResMap *map, uint32_t res_id);

/* Compute positions for all nodes given bounding rect (w × h starting at 0,0). */
void          aine_layout_measure(AineViewNode *node,
                                  float x, float y, float w, float h);

/* Draw the entire view tree to the canvas (canvas must be initialised). */
void          aine_layout_draw(AineViewNode *root);

/* Find a node by resource ID (depth-first). */
AineViewNode *aine_layout_find_by_id(AineViewNode *root, int res_id);

/* Hit-test: find the innermost node (w/ res_id) that contains (x,y). */
AineViewNode *aine_layout_hit_test(AineViewNode *root, float x, float y);

/* Update the text shown by a view node (thread-safe wrapper). */
void          aine_layout_set_text(AineViewNode *node, const char *text);

void          aine_layout_free(AineViewNode *root);
