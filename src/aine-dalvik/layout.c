/* aine-dalvik/layout.c — Generic Android XML layout inflater + LinearLayout renderer.
 *
 * AINE reads a companion "aine-res.txt" file placed alongside the DEX files.
 * When an app calls setContentView(int layoutResId), AINE inflates the layout,
 * builds an AineViewNode tree, measures it, and renders it on each frame
 * instead of dispatching onDraw to a custom View subclass.
 *
 * Supported view types  : LinearLayout, TextView, Button, MaterialButton.
 * Supported attributes  : orientation, layout_weight, match_parent,
 *                         wrap_content, padding, margin, textColor, textSize,
 *                         gravity, background, text (@string/ resolved),
 *                         id (@+id/ resolved via resource map).
 *
 * This is generic Android API translation — nothing specific to any single app.
 */

#include "layout.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#ifdef __APPLE__
#include "canvas.h"
#endif

/* ── dp-to-pixel scale factor ──────────────────────────────────────────────── *
 * 2.0 = 160 dpi logical → 320 dpi physical (typical macOS retina).            *
 * Adjust if a display-info API is available.                                   */
#define DP_SCALE 2.0f

/* ═══════════════════════════════════════════════════════════════════════════ *
 * 1.  Resource map                                                             *
 * ═══════════════════════════════════════════════════════════════════════════ */

AineResMap *aine_res_load(const char *base_dir)
{
    if (!base_dir) return NULL;

    char path[640];
    snprintf(path, sizeof(path), "%s/aine-res.txt", base_dir);

    FILE *f = fopen(path, "r");
    if (!f) {
        fprintf(stderr, "[layout] no resource file at %s\n", path);
        return NULL;
    }

    AineResMap *m = calloc(1, sizeof(AineResMap));
    if (!m) { fclose(f); return NULL; }

    char line[512];
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = 0;
        if (!line[0] || line[0] == '#') continue;

        /* layout:0xHEX=rel/path */
        if (strncmp(line, "layout:", 7) == 0) {
            char *eq = strchr(line + 7, '=');
            if (eq) {
                m->layout_res_id = (uint32_t)strtoul(line + 7, NULL, 16);
                snprintf(m->layout_xml, sizeof(m->layout_xml),
                         "%s/%s", base_dir, eq + 1);
            }
        }
        /* id:name=0xHEX */
        else if (strncmp(line, "id:", 3) == 0) {
            if (m->n_ids < AINE_RES_MAX_IDS) {
                char *eq = strchr(line + 3, '=');
                if (eq) {
                    int idx = m->n_ids++;
                    size_t len = (size_t)(eq - (line + 3));
                    if (len >= sizeof(m->ids[idx].name))
                        len = sizeof(m->ids[idx].name) - 1;
                    memcpy(m->ids[idx].name, line + 3, len);
                    m->ids[idx].name[len] = 0;
                    m->ids[idx].res_id = (uint32_t)strtoul(eq + 1, NULL, 16);
                }
            }
        }
        /* string:name=value */
        else if (strncmp(line, "string:", 7) == 0) {
            if (m->n_strings < AINE_RES_MAX_STRINGS) {
                char *eq = strchr(line + 7, '=');
                if (eq) {
                    int idx = m->n_strings++;
                    size_t len = (size_t)(eq - (line + 7));
                    if (len >= sizeof(m->str_names[idx]))
                        len = sizeof(m->str_names[idx]) - 1;
                    memcpy(m->str_names[idx], line + 7, len);
                    m->str_names[idx][len] = 0;
                    strncpy(m->str_values[idx], eq + 1,
                            sizeof(m->str_values[idx]) - 1);
                }
            }
        }
    }
    fclose(f);

    fprintf(stderr, "[layout] loaded res: layout_id=0x%x ids=%d strings=%d\n",
            m->layout_res_id, m->n_ids, m->n_strings);
    return m;
}

void aine_res_free(AineResMap *map) { free(map); }

const char *aine_res_string(AineResMap *map, const char *name)
{
    if (!map || !name) return NULL;
    for (int i = 0; i < map->n_strings; i++)
        if (strcmp(map->str_names[i], name) == 0)
            return map->str_values[i];
    return NULL;
}

uint32_t aine_res_id_by_name(AineResMap *map, const char *name)
{
    if (!map || !name) return 0;
    for (int i = 0; i < map->n_ids; i++)
        if (strcmp(map->ids[i].name, name) == 0)
            return map->ids[i].res_id;
    return 0;
}

/* ═══════════════════════════════════════════════════════════════════════════ *
 * 2.  Minimal XML tokenizer                                                   *
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef struct { const char *buf; int len; int pos; } XmlBuf;

static void xb_skip_ws(XmlBuf *xb)
{
    while (xb->pos < xb->len && isspace((unsigned char)xb->buf[xb->pos]))
        xb->pos++;
}

static void xb_skip_to_lt(XmlBuf *xb)
{
    while (xb->pos < xb->len && xb->buf[xb->pos] != '<') xb->pos++;
}

/* Read a "name" token (no whitespace / '=' / '>' / '/' / '<'). */
static int xb_read_name(XmlBuf *xb, char *buf, int sz)
{
    xb_skip_ws(xb);
    int start = xb->pos;
    while (xb->pos < xb->len) {
        char c = xb->buf[xb->pos];
        if (isspace((unsigned char)c) || c == '=' || c == '>' || c == '/' || c == '<')
            break;
        xb->pos++;
    }
    int len = xb->pos - start;
    if (len <= 0) { if (sz > 0) buf[0] = 0; return 0; }
    if (len >= sz) len = sz - 1;
    memcpy(buf, xb->buf + start, (size_t)len);
    buf[len] = 0;
    return len;
}

/* Read a quoted attribute value (single or double quotes, or unquoted). */
static int xb_read_value(XmlBuf *xb, char *buf, int sz)
{
    xb_skip_ws(xb);
    if (xb->pos >= xb->len) { if (sz > 0) buf[0] = 0; return 0; }
    char q = xb->buf[xb->pos];
    if (q != '"' && q != '\'')
        return xb_read_name(xb, buf, sz);   /* unquoted */
    xb->pos++;  /* skip opening quote */
    int start = xb->pos;
    while (xb->pos < xb->len && xb->buf[xb->pos] != q) xb->pos++;
    int len = xb->pos - start;
    if (xb->pos < xb->len) xb->pos++;  /* skip closing quote */
    if (len >= sz) len = sz - 1;
    memcpy(buf, xb->buf + start, (size_t)len);
    buf[len] = 0;
    return len;
}

/* ═══════════════════════════════════════════════════════════════════════════ *
 * 3.  Attribute value helpers                                                 *
 * ═══════════════════════════════════════════════════════════════════════════ */

/* Parse "16dp" / "24sp" / "match_parent" / "wrap_content" → float dp.
 * Returns -1.0 for match_parent, -2.0 for wrap_content. */
static float parse_dimen(const char *v)
{
    if (!v || !v[0]) return 0.0f;
    if (strcmp(v, "match_parent") == 0 || strcmp(v, "fill_parent") == 0) return -1.0f;
    if (strcmp(v, "wrap_content") == 0) return -2.0f;
    char *end;
    float val = strtof(v, &end);
    /* ignore trailing unit suffix (dp, sp, px, …) */
    return val;
}

/* Resolve @android:color/name or #RRGGBB / #AARRGGBB → 0xAARRGGBB. */
static uint32_t parse_color(const char *v)
{
    if (!v || !v[0]) return 0;
    if (strncmp(v, "@android:color/", 15) == 0) {
        const char *n = v + 15;
        if (strcmp(n, "black")                == 0) return 0xFF000000;
        if (strcmp(n, "white")                == 0) return 0xFFFFFFFF;
        if (strcmp(n, "darker_gray")          == 0 ||
            strcmp(n, "darkgray")             == 0 ||
            strcmp(n, "dark_gray")            == 0) return 0xFF444444;
        if (strcmp(n, "gray")                 == 0) return 0xFF888888;
        if (strcmp(n, "holo_blue_light")      == 0) return 0xFF33B5E5;
        if (strcmp(n, "holo_green_light")     == 0) return 0xFF99CC00;
        if (strcmp(n, "transparent")          == 0) return 0x00000000;
        if (strcmp(n, "background_dark")      == 0) return 0xFF1C1C1E;
        if (strcmp(n, "primary_text_dark")    == 0) return 0xFFFFFFFF;
        if (strcmp(n, "secondary_text_dark")  == 0) return 0xFF888888;
        return 0xFF888888; /* fallback: gray */
    }
    if (v[0] == '#') {
        uint32_t c = (uint32_t)strtoul(v + 1, NULL, 16);
        size_t l = strlen(v);
        if (l <= 7) c |= 0xFF000000; /* no alpha → opaque */
        return c;
    }
    /* ?attr or other reference — transparent */
    return 0x00000000;
}

static const char *resolve_text(const char *v, AineResMap *map)
{
    if (!v || !v[0]) return "";
    if (strncmp(v, "@string/", 8) == 0) {
        const char *r = map ? aine_res_string(map, v + 8) : NULL;
        return r ? r : "";
    }
    return v;
}

static uint32_t resolve_id(const char *v, AineResMap *map)
{
    if (!v || !v[0]) return 0;
    const char *name = NULL;
    if (strncmp(v, "@+id/", 5) == 0)      name = v + 5;
    else if (strncmp(v, "@id/", 4) == 0)  name = v + 4;
    else return 0;
    return map ? aine_res_id_by_name(map, name) : 0;
}

/* Strip namespace prefix "android:", "app:", "tools:", … */
static const char *strip_ns(const char *n)
{
    const char *c = strchr(n, ':');
    return c ? c + 1 : n;
}

/* ═══════════════════════════════════════════════════════════════════════════ *
 * 4.  XML element → AineViewNode                                              *
 * ═══════════════════════════════════════════════════════════════════════════ */

static AineViewType tag_to_vtype(const char *tag)
{
    /* Strip package/path prefix: "com.google.…MaterialButton" → "MaterialButton" */
    const char *t = strrchr(tag, '.');
    t = t ? t + 1 : tag;
    if (strcmp(t, "LinearLayout")        == 0 ||
        strcmp(t, "FrameLayout")         == 0 ||
        strcmp(t, "ScrollView")          == 0) return AINE_VIEW_LINEAR_V;
    if (strcmp(t, "RelativeLayout")      == 0 ||
        strcmp(t, "ConstraintLayout")    == 0) return AINE_VIEW_LINEAR_V;
    if (strcmp(t, "Button")              == 0 ||
        strcmp(t, "MaterialButton")      == 0 ||
        strcmp(t, "ImageButton")         == 0) return AINE_VIEW_BUTTON;
    if (strcmp(t, "TextView")            == 0 ||
        strcmp(t, "EditText")            == 0 ||
        strcmp(t, "CheckBox")            == 0 ||
        strcmp(t, "RadioButton")         == 0) return AINE_VIEW_TEXTVIEW;
    return AINE_VIEW_GENERIC;
}

static AineViewNode *node_new(void)
{
    AineViewNode *n = calloc(1, sizeof(AineViewNode));
    if (!n) return NULL;
    n->text_color    = 0xFFFFFFFF;
    n->text_size     = 14.0f;
    n->layout_w      = -1;   /* match_parent */
    n->layout_h      = -2;   /* wrap_content */
    n->layout_weight = 0.0f;
    return n;
}

/* Forward declaration */
static AineViewNode *parse_element(XmlBuf *xb, AineResMap *map, int depth);

static AineViewNode *parse_element(XmlBuf *xb, AineResMap *map, int depth)
{
    if (depth > 32) return NULL;
    xb_skip_ws(xb);
    if (xb->pos >= xb->len) return NULL;

    /* <!-- comment --> */
    if (xb->pos + 3 < xb->len &&
        xb->buf[xb->pos] == '!' &&
        xb->buf[xb->pos+1] == '-' &&
        xb->buf[xb->pos+2] == '-') {
        xb->pos += 3;
        while (xb->pos + 2 < xb->len) {
            if (xb->buf[xb->pos]   == '-' &&
                xb->buf[xb->pos+1] == '-' &&
                xb->buf[xb->pos+2] == '>') { xb->pos += 3; break; }
            xb->pos++;
        }
        return NULL;
    }

    /* </end> or </ */
    if (xb->pos < xb->len && xb->buf[xb->pos] == '/') {
        while (xb->pos < xb->len && xb->buf[xb->pos] != '>') xb->pos++;
        if (xb->pos < xb->len) xb->pos++;
        return NULL;
    }

    /* ?<?xml…?> or <!DOCTYPE…> */
    if (xb->pos < xb->len && (xb->buf[xb->pos] == '?' || xb->buf[xb->pos] == '!')) {
        while (xb->pos < xb->len && xb->buf[xb->pos] != '>') xb->pos++;
        if (xb->pos < xb->len) xb->pos++;
        return NULL;
    }

    /* Read element name */
    char tag[128];
    if (xb_read_name(xb, tag, sizeof(tag)) == 0) return NULL;

    AineViewNode *node = node_new();
    node->vtype = tag_to_vtype(tag);

    /* Parse attributes */
    for (;;) {
        xb_skip_ws(xb);
        if (xb->pos >= xb->len) break;
        char c = xb->buf[xb->pos];
        if (c == '>' || c == '/' || c == '<') break;

        char attr[128];
        if (xb_read_name(xb, attr, sizeof(attr)) == 0) break;

        xb_skip_ws(xb);
        if (xb->pos >= xb->len || xb->buf[xb->pos] != '=') {
            /* Valueless attribute — skip */
            continue;
        }
        xb->pos++;  /* skip '=' */

        char val[256];
        xb_read_value(xb, val, sizeof(val));

        const char *a = strip_ns(attr);

        if (strcmp(a, "id") == 0) {
            node->res_id = (int)resolve_id(val, map);
        } else if (strcmp(a, "layout_width") == 0) {
            float f = parse_dimen(val);
            node->layout_w = (f == -1.0f) ? -1 : (f == -2.0f) ? -2 : (int)f;
        } else if (strcmp(a, "layout_height") == 0) {
            float f = parse_dimen(val);
            node->layout_h = (f == -1.0f) ? -1 : (f == -2.0f) ? -2 : (int)f;
        } else if (strcmp(a, "layout_weight") == 0) {
            node->layout_weight = strtof(val, NULL);
        } else if (strcmp(a, "orientation") == 0) {
            node->vtype = (strcmp(val, "horizontal") == 0)
                          ? AINE_VIEW_LINEAR_H : AINE_VIEW_LINEAR_V;
        } else if (strcmp(a, "background") == 0) {
            node->bg_color = parse_color(val);
            /* Mark has_bg even for black (0xFF000000) */
            node->has_bg = (val[0] == '#' || strncmp(val, "@android:color/", 15) == 0);
        } else if (strcmp(a, "textColor") == 0) {
            uint32_t c2 = parse_color(val);
            if (c2) node->text_color = c2;
        } else if (strcmp(a, "textSize") == 0) {
            float sz = parse_dimen(val);
            if (sz > 0.0f) node->text_size = sz;
        } else if (strcmp(a, "text") == 0) {
            const char *t = resolve_text(val, map);
            if (t && t[0]) strncpy(node->text, t, AINE_VIEW_MAX_TEXT - 1);
        } else if (strcmp(a, "gravity") == 0) {
            if (strstr(val, "end") || strstr(val, "right"))
                node->gravity_end = 1;
            else if (strcmp(val, "center") == 0 ||
                     strstr(val, "center_horizontal") != NULL)
                node->gravity_end = 2;
        } else if (strcmp(a, "padding") == 0) {
            float p = parse_dimen(val);
            if (p > 0)
                node->padding_l = node->padding_t = node->padding_r = node->padding_b = p;
        } else if (strcmp(a, "paddingLeft") == 0 || strcmp(a, "paddingStart") == 0) {
            float p = parse_dimen(val); if (p >= 0) node->padding_l = p;
        } else if (strcmp(a, "paddingRight") == 0 || strcmp(a, "paddingEnd") == 0) {
            float p = parse_dimen(val); if (p >= 0) node->padding_r = p;
        } else if (strcmp(a, "paddingTop") == 0) {
            float p = parse_dimen(val); if (p >= 0) node->padding_t = p;
        } else if (strcmp(a, "paddingBottom") == 0) {
            float p = parse_dimen(val); if (p >= 0) node->padding_b = p;
        } else if (strcmp(a, "layout_margin") == 0) {
            float m = parse_dimen(val); if (m > 0) node->margin = m;
        } else if (strcmp(a, "layout_marginStart") == 0 ||
                   strcmp(a, "layout_marginEnd")   == 0 ||
                   strcmp(a, "layout_marginTop")   == 0 ||
                   strcmp(a, "layout_marginBottom") == 0) {
            float m = parse_dimen(val);
            if (m > 0 && node->margin < m) node->margin = m; /* use max for uniform */
        }
        /* Remaining attributes (textStyle, typeface, etc.) are silently ignored */
    }

    /* Self-close '/>' or opening '>' */
    xb_skip_ws(xb);
    int self_close = 0;
    if (xb->pos < xb->len && xb->buf[xb->pos] == '/') {
        self_close = 1; xb->pos++;
    }
    if (xb->pos < xb->len && xb->buf[xb->pos] == '>') xb->pos++;

    if (!self_close) {
        /* Parse children until our end tag */
        for (;;) {
            xb_skip_to_lt(xb);
            if (xb->pos >= xb->len) break;
            xb->pos++; /* skip '<' */
            xb_skip_ws(xb);
            if (xb->pos < xb->len && xb->buf[xb->pos] == '/') {
                /* End tag — consume + stop */
                while (xb->pos < xb->len && xb->buf[xb->pos] != '>') xb->pos++;
                if (xb->pos < xb->len) xb->pos++;
                break;
            }
            AineViewNode *child = parse_element(xb, map, depth + 1);
            if (child && node->n_children < AINE_VIEW_MAX_CHILDREN)
                node->children[node->n_children++] = child;
        }
    }

    return node;
}

AineViewNode *aine_layout_inflate(AineResMap *map, uint32_t res_id)
{
    if (!map) return NULL;
    /* If the map has a layout, inflate it regardless of exact ID match
     * (apps typically have only one main layout). */
    if (!map->layout_xml[0]) {
        fprintf(stderr, "[layout] inflate: no layout XML in resource map\n");
        return NULL;
    }

    FILE *f = fopen(map->layout_xml, "r");
    if (!f) {
        fprintf(stderr, "[layout] inflate: cannot open %s\n", map->layout_xml);
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f); rewind(f);
    char *buf = malloc((size_t)sz + 1);
    if (!buf) { fclose(f); return NULL; }
    fread(buf, 1, (size_t)sz, f); fclose(f);
    buf[sz] = 0;

    XmlBuf xb = { buf, (int)sz, 0 };
    AineViewNode *root = NULL;

    for (;;) {
        xb_skip_to_lt(&xb);
        if (xb.pos >= xb.len) break;
        xb.pos++;   /* skip '<' */
        xb_skip_ws(&xb);
        if (xb.pos < xb.len && (xb.buf[xb.pos] == '?' || xb.buf[xb.pos] == '!')) {
            while (xb.pos < xb.len && xb.buf[xb.pos] != '>') xb.pos++;
            if (xb.pos < xb.len) xb.pos++;
            continue;
        }
        root = parse_element(&xb, map, 0);
        if (root) break;
    }

    free(buf);

    if (root)
        fprintf(stderr, "[layout] inflated 0x%x: %d children\n",
                res_id, root->n_children);
    else
        fprintf(stderr, "[layout] inflate 0x%x: parse failed\n", res_id);
    return root;
}

/* ═══════════════════════════════════════════════════════════════════════════ *
 * 5.  Layout measurement: LinearLayout with weight distribution               *
 * ═══════════════════════════════════════════════════════════════════════════ */

/* Estimated wrap_content height/width of a text-bearing node (in pixels). */
static float wrap_height(AineViewNode *n)
{
    float ts = n->text_size * DP_SCALE;
    if (ts < 4.0f) ts = 16.0f;
    return ts * 1.8f + n->margin * DP_SCALE * 2.0f;
}

static float wrap_width(AineViewNode *n)
{
    float ts = n->text_size * DP_SCALE;
    if (ts < 4.0f) ts = 16.0f;
    int chars = (int)strlen(n->text);
    if (chars < 2) chars = 2;
    return ts * 0.7f * (float)chars + n->padding_l * DP_SCALE + n->padding_r * DP_SCALE +
           n->margin * DP_SCALE * 2.0f;
}

void aine_layout_measure(AineViewNode *node, float x, float y, float w, float h)
{
    if (!node) return;
    float m = node->margin * DP_SCALE;
    node->x = x + m;
    node->y = y + m;
    node->w = w - 2.0f * m;
    node->h = h - 2.0f * m;
    if (node->w < 0) node->w = 0;
    if (node->h < 0) node->h = 0;

    if (node->n_children == 0) return;

    float pl = node->padding_l * DP_SCALE;
    float pt = node->padding_t * DP_SCALE;
    float pr = node->padding_r * DP_SCALE;
    float pb = node->padding_b * DP_SCALE;
    float ix  = node->x + pl;
    float iy  = node->y + pt;
    float iw  = node->w - pl - pr; if (iw < 0) iw = 0;
    float ih  = node->h - pt - pb; if (ih < 0) ih = 0;

    int vertical = (node->vtype != AINE_VIEW_LINEAR_H);

    /* Pass 1: accumulate total weight and fixed sizes. */
    float total_weight = 0.0f;
    float fixed_sz     = 0.0f;

    for (int i = 0; i < node->n_children; i++) {
        AineViewNode *c = node->children[i];
        if (c->layout_weight > 0.0f) {
            total_weight += c->layout_weight;
        } else {
            if (vertical) {
                if (c->layout_h == -2)      fixed_sz += wrap_height(c);
                else if (c->layout_h == -1) fixed_sz += ih; /* will be clipped */
                else                        fixed_sz += (float)c->layout_h * DP_SCALE +
                                                         c->margin * DP_SCALE * 2.0f;
            } else {
                if (c->layout_w == -2)      fixed_sz += wrap_width(c);
                else if (c->layout_w == -1) fixed_sz += iw;
                else                        fixed_sz += (float)c->layout_w * DP_SCALE +
                                                         c->margin * DP_SCALE * 2.0f;
            }
        }
    }

    float remaining = (vertical ? ih : iw) - fixed_sz;
    if (remaining < 0) remaining = 0;

    /* Pass 2: assign positions + recurse. */
    float offset = 0.0f;
    for (int i = 0; i < node->n_children; i++) {
        AineViewNode *c = node->children[i];
        float cx, cy, cw, ch;
        if (vertical) {
            cx = ix;
            cw = iw;
            if (c->layout_weight > 0.0f && total_weight > 0.0f)
                ch = remaining * c->layout_weight / total_weight;
            else if (c->layout_h == -2)
                ch = wrap_height(c);
            else if (c->layout_h == -1)
                ch = ih;
            else
                ch = (float)c->layout_h * DP_SCALE + c->margin * DP_SCALE * 2.0f;
            cy = iy + offset; offset += ch;
        } else {
            cy = iy;
            ch = ih;
            if (c->layout_weight > 0.0f && total_weight > 0.0f)
                cw = remaining * c->layout_weight / total_weight;
            else if (c->layout_w == -2)
                cw = wrap_width(c);
            else if (c->layout_w == -1)
                cw = iw;
            else
                cw = (float)c->layout_w * DP_SCALE + c->margin * DP_SCALE * 2.0f;
            cx = ix + offset; offset += cw;
        }
        aine_layout_measure(c, cx, cy, cw, ch);
    }
}

/* ═══════════════════════════════════════════════════════════════════════════ *
 * 6.  Rendering                                                               *
 * ═══════════════════════════════════════════════════════════════════════════ */

#ifdef __APPLE__

/* Approximate text width in pixels for placement.
 * Helvetica proportions: average char ≈ 0.55 × font-size. */
static float approx_text_width(const char *text, float ts)
{
    if (!text || !text[0]) return 0.0f;
    return (float)strlen(text) * 0.55f * ts;
}

static void draw_node(AineViewNode *node)
{
    if (!node) return;

    float ts = node->text_size * DP_SCALE;

    /* Background */
    if (node->has_bg)
        aine_canvas_fill_rect(node->x, node->y, node->w, node->h, node->bg_color);

    switch (node->vtype) {
    case AINE_VIEW_TEXTVIEW: {
        if (!node->text[0]) break;
        float pl = node->padding_l * DP_SCALE;
        float pr = node->padding_r * DP_SCALE;
        float inner_x = node->x + pl;
        float inner_w = node->w - pl - pr;
        if (inner_w < 0) inner_w = 0;
        float tw = approx_text_width(node->text, ts);
        float tx, ty;
        if (node->gravity_end == 1) {
            /* right-align: position text so its right edge is at the inner right */
            tx = inner_x + inner_w - tw;
        } else if (node->gravity_end == 2) {
            tx = inner_x + (inner_w - tw) * 0.5f;
        } else {
            tx = inner_x;
        }
        /* Baseline: vertically centered with slight offset for descenders */
        ty = node->y + (node->h + ts * 0.45f) * 0.5f;
        aine_canvas_draw_text(tx, ty, node->text, ts, node->text_color);
        break;
    }
    case AINE_VIEW_BUTTON: {
        /* Button: dark-gray filled rect + centered white label */
        uint32_t btn_bg = node->has_bg ? node->bg_color : 0xFF2A2A2A;
        aine_canvas_fill_rect(node->x, node->y, node->w, node->h, btn_bg);
        aine_canvas_stroke_rect(node->x, node->y, node->w, node->h, 1.0f, 0xFF555555);
        if (node->text[0]) {
            float tw = approx_text_width(node->text, ts);
            float tx = node->x + (node->w - tw) * 0.5f;
            float ty = node->y + (node->h + ts * 0.45f) * 0.5f;
            aine_canvas_draw_text(tx, ty, node->text, ts, node->text_color);
        }
        break;
    }
    default: break;
    }

    /* Recurse into children */
    for (int i = 0; i < node->n_children; i++)
        draw_node(node->children[i]);
}

void aine_layout_draw(AineViewNode *root)
{
    if (!root) return;
    if (root->has_bg)
        aine_canvas_clear(root->bg_color);
    draw_node(root);
}

#else  /* non-Apple stub */
void aine_layout_draw(AineViewNode *root) { (void)root; }
#endif /* __APPLE__ */

/* ═══════════════════════════════════════════════════════════════════════════ *
 * 7.  Utilities                                                               *
 * ═══════════════════════════════════════════════════════════════════════════ */

AineViewNode *aine_layout_find_by_id(AineViewNode *root, int res_id)
{
    if (!root || res_id == 0) return NULL;
    if (root->res_id == res_id) return root;
    for (int i = 0; i < root->n_children; i++) {
        AineViewNode *r = aine_layout_find_by_id(root->children[i], res_id);
        if (r) return r;
    }
    return NULL;
}

void aine_layout_set_text(AineViewNode *node, const char *text)
{
    if (!node || !text) return;
    strncpy(node->text, text, AINE_VIEW_MAX_TEXT - 1);
    node->text[AINE_VIEW_MAX_TEXT - 1] = 0;
}

AineViewNode *aine_layout_hit_test(AineViewNode *root, float x, float y)
{
    if (!root) return NULL;
    /* Quick bounds check */
    if (x < root->x || x > root->x + root->w ||
        y < root->y || y > root->y + root->h)  return NULL;
    /* Depth-first: prefer innermost leaf with a resource ID (i.e., a labelled view) */
    for (int i = 0; i < root->n_children; i++) {
        AineViewNode *hit = aine_layout_hit_test(root->children[i], x, y);
        if (hit) return hit;
    }
    return (root->res_id != 0) ? root : NULL;
}

void aine_layout_free(AineViewNode *root)
{
    if (!root) return;
    for (int i = 0; i < root->n_children; i++)
        aine_layout_free(root->children[i]);
    free(root);
}
