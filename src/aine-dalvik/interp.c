// aine-dalvik/interp.c — Dalvik register-based bytecode interpreter
//
// Implements the opcodes present in HelloWorld.dex (M1) and enough of
// the instruction set for general utility apps (no JIT, interpreter only).
//
// Dalvik reference: https://source.android.com/docs/core/runtime/dalvik-bytecode
//
// Register file: each register holds either a primitive (int32/int64) or an
// object pointer (AineObj *).  We use a tagged union to keep it simple.

#include "interp.h"
#include "dex.h"
#include "heap.h"
#include "jni.h"
#include "handler.h"
#ifdef __APPLE__
#include "canvas.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>    /* clock_gettime, nanosleep */
#include <stdint.h>
#include <math.h>    /* fmodf, fmod */

// ── Register ────────────────────────────────────────────────────────────
typedef enum { REG_PRIM, REG_OBJ } RegKind;

typedef struct {
    RegKind  kind;
    union {
        int64_t  prim;
        AineObj *obj;
    };
} Reg;

static inline AineObj *reg_obj(const Reg *r) {
    return (r->kind == REG_OBJ) ? r->obj : NULL;
}
static inline void reg_set_obj(Reg *r, AineObj *o) {
    r->kind = REG_OBJ; r->obj = o;
}
static inline void reg_set_prim(Reg *r, int64_t v) {
    r->kind = REG_PRIM; r->prim = v;
}
static inline int64_t reg_prim(const Reg *r) {
    return (r->kind == REG_PRIM) ? r->prim : 0;
}

// ── Interpreter context ──────────────────────────────────────────────────
struct AineInterp {
    const DexFile *df;
};

AineInterp *interp_new(const DexFile *df) {
    AineInterp *i = calloc(1, sizeof(AineInterp));
    i->df = df;
    return i;
}

void interp_free(AineInterp *interp) { free(interp); }

// ── Decode helpers ─────────────────────────────────────────────────────────
static inline int byte_hi(uint16_t w) { return (w >> 8) & 0xff; }

// ── Forward declaration ──────────────────────────────────────────────────────
static int exec_code(AineInterp *interp, const DexCodeItem *ci,
                     Reg *in_regs, int in_count, Reg *result_out);

// ── Method dispatch: try DEX first, fall back to JNI ─────────────────────────
// all_args[0..all_count-1] = this (if instance) then explicit args.
static void exec_method(AineInterp *interp,
                        const char *class_desc, const char *method_name,
                        Reg *all_args, int all_count, int is_static,
                        Reg *result_out) {
    if (result_out) { result_out->kind = REG_PRIM; result_out->prim = 0; }
    // Try DEX — walk the superclass chain within this DEX file
    int cdef = dex_find_class(interp->df, class_desc);
    if (cdef >= 0) {
        int at = cdef;
        while (at >= 0) {
            int midx = dex_find_method(interp->df, at, method_name, NULL);
            if (midx >= 0) {
                const DexCodeItem *ci = dex_code_item(interp->df, at, midx);
                if (ci) { exec_code(interp, ci, all_args, all_count, result_out); return; }
                /* Found abstract/native — fall out to JNI */
                goto jni_fallthrough;
            }
            /* Method not in this class: try DEX superclass */
            const char *super = dex_class_super(interp->df, at);
            if (!super) break;
            at = dex_find_class(interp->df, super);
        }
        /* Not found in DEX hierarchy — fall through to JNI */
    }
    jni_fallthrough:;
    // JNI
    AineObj *this_obj = NULL;
    int jni_start = 0;
    if (!is_static && all_count > 0) { this_obj = reg_obj(&all_args[0]); jni_start = 1; }
    int jni_n = all_count - jni_start;
    /* Box primitive args as string objects so JNI dispatch can read them.
     * Use heap_string() so the boxing survives after exec_method returns
     * (callers may store the result in heap arrays or fields). */
    char    prim_bufs[8][32];
    AineObj *args[8] = {0};
    for (int a = 0; a < jni_n && a < 8; a++) {
        const Reg *r = &all_args[jni_start + a];
        if (r->kind == REG_OBJ) {
            args[a] = r->obj;
        } else {
            snprintf(prim_bufs[a], sizeof(prim_bufs[a]), "%lld", (long long)r->prim);
            args[a] = heap_string(prim_bufs[a]);  /* heap-allocated: safe to store */
        }
    }
    JniResult r = jni_dispatch(class_desc, method_name, this_obj, args, jni_n, is_static);
    if (result_out && !r.is_void) {
        if (r.obj) reg_set_obj(result_out, r.obj);
        else reg_set_prim(result_out, r.prim);
    }
}

// ── 35c arg decode helper ─────────────────────────────────────────────────────
static int decode_35c(uint16_t insn, uint16_t packed, int arg_regs[5]) {
    int ac = (insn >> 12) & 0xf;
    arg_regs[0] = (packed >> 0) & 0xf;  arg_regs[1] = (packed >> 4) & 0xf;
    arg_regs[2] = (packed >> 8) & 0xf;  arg_regs[3] = (packed >> 12) & 0xf;
    arg_regs[4] = (insn >> 8) & 0xf;
    return ac;
}

// ── Core interpreter ─────────────────────────────────────────────────────────
#define MAX_REGS 512

static int exec_code(AineInterp *interp, const DexCodeItem *ci,
                     Reg *in_regs, int in_count, Reg *result_out) {
    const uint16_t *insns = dex_insns(ci);
    int N = (int)ci->registers_size;
    if (N > MAX_REGS) { fprintf(stderr, "[aine-dalvik] registers_size=%d\n", N); return -1; }

    Reg regs[MAX_REGS];
    memset(regs, 0, sizeof(Reg) * (size_t)N);

    // Incoming args → last ins_size registers
    int ins = (int)ci->ins_size;
    int base = N - ins;
    for (int i = 0; i < in_count && i < ins; i++) regs[base + i] = in_regs[i];

    Reg result_reg = {0};
    uint32_t pc = 0;

    while (1) {
        uint16_t insn = insns[pc];
        uint8_t  op   = insn & 0xff;

        switch (op) {

        // ── 0x00 nop ──────────────────────────────────────────────────
        case 0x00:
            pc++;
            break;

        // ── 0x01 move vx, vy  (4-bit regs) 12x
        case 0x01: {
            int vx = (insn >> 8) & 0xf;
            int vy = (insn >> 12) & 0xf;
            regs[vx] = regs[vy];
            pc++;
            break;
        }
        // ── 0x02 move/from16 vAA, vBBBB  22x
        case 0x02: {
            int vx = (insn >> 8) & 0xff, vy = insns[pc + 1];
            regs[vx] = regs[vy]; pc += 2; break;
        }
        // ── 0x03 move/16 vAAAA, vBBBB  32x
        case 0x03: {
            int vx = insns[pc + 1], vy = insns[pc + 2];
            if (vx < N) regs[vx] = (vy < N) ? regs[vy] : regs[vx];
            pc += 3; break;
        }

        // ── 0x07 move-object vx, vy ───────────────────────────────────
        case 0x07: {
            int vx = (insn >> 8) & 0xf;
            int vy = (insn >> 12) & 0xf;
            regs[vx] = regs[vy];
            pc++;
            break;
        }

        // ── 0x0a/0x0b/0x0c move-result(-wide|-object) vx ─────────────
        case 0x0a: case 0x0b: case 0x0c: {
            regs[byte_hi(insn)] = result_reg;
            pc++; break;
        }

        // ── 0x0e return-void ──────────────────────────────────────────
        case 0x0e:
            return 0;

        // ── 0x0f return vx ─────────────────────────────────────────────
        case 0x0f: {
            result_reg = regs[byte_hi(insn)];            if (result_out) *result_out = result_reg;            return 0;
        }

        // ── 0x11 return-object vx ─────────────────────────────────────
        case 0x11: {
            result_reg = regs[byte_hi(insn)];            if (result_out) *result_out = result_reg;            return 0;
        }

        // ── 0x12 const/4 vx, #+D ─────────────────────────────────────
        // 11n: vx = (insn>>8)&0xf, literal = sign_extend((insn>>12)&0xf, 4)
        case 0x12: {
            int vx  = (insn >> 8) & 0xf;
            int lit = (int)(insn >> 12) & 0xf;
            if (lit & 0x8) lit |= ~0xf;  // sign extend
            reg_set_prim(&regs[vx], lit);
            pc++;
            break;
        }

        // ── 0x13 const/16 vx, #+CCCC ─────────────────────────────────
        case 0x13: {
            int vx  = byte_hi(insn);
            int16_t lit = (int16_t)insns[pc + 1];
            reg_set_prim(&regs[vx], lit);
            pc += 2;
            break;
        }

        // ── 0x14 const vx, #+BBBBBBBB ────────────────────────────────
        case 0x14: {
            int vx = byte_hi(insn);
            int32_t lit = (int32_t)((uint32_t)insns[pc+1] | ((uint32_t)insns[pc+2] << 16));
            reg_set_prim(&regs[vx], lit);
            pc += 3;
            break;
        }

        // ── 0x1a const-string vx, string@BBBB ────────────────────────
        // 21c: vx = (insn>>8)&0xff; string_idx = insns[pc+1]
        case 0x1a: {
            int vx = byte_hi(insn);
            uint16_t sidx = insns[pc + 1];
            const char *s = dex_string(interp->df, sidx);
            reg_set_obj(&regs[vx], heap_string(s));
            pc += 2;
            break;
        }

        // ── 0x1b const-string/jumbo ────────────────────────────────────
        case 0x1b: {
            int vx = byte_hi(insn);
            uint32_t sidx = (uint32_t)insns[pc+1] | ((uint32_t)insns[pc+2] << 16);
            const char *s = dex_string(interp->df, sidx);
            reg_set_obj(&regs[vx], heap_string(s));
            pc += 3;
            break;
        }

        // ── 0x22 new-instance vx, type@CCCC  21c ────────────────────
        case 0x22: {
            int vx = byte_hi(insn);
            const char *type = dex_type_name(interp->df, insns[pc + 1]);
            AineObj *obj = NULL;
            if (type) {
                if (strcmp(type, "Ljava/lang/StringBuilder;") == 0) {
                    obj = heap_sb_new();                } else if (strcmp(type, "Ljava/util/ArrayList;") == 0 ||
                           strcmp(type, "Ljava/util/LinkedList;") == 0) {
                    obj = heap_arraylist_new();
                    obj->class_desc = type;
                } else if (strcmp(type, "Ljava/util/HashMap;") == 0 ||
                           strcmp(type, "Ljava/util/LinkedHashMap;") == 0 ||
                           strcmp(type, "Ljava/util/TreeMap;") == 0) {
                    obj = heap_hashmap_new();
                    obj->class_desc = type;                } else {
                    int cd = dex_find_class(interp->df, type);
                    obj = (cd >= 0) ? heap_userclass(cd)
                                    : (AineObj *)calloc(1, sizeof(AineObj));
                    if (obj) obj->class_desc = type;  /* for Runnable dispatch */
                }
            } else { obj = calloc(1, sizeof(AineObj)); }
            reg_set_obj(&regs[vx], obj);
            pc += 2;
            break;
        }

        // ── 0x27 throw vAA  11x ──────────────────────────────────────────
        case 0x27: {
            int vA = byte_hi(insn);
            AineObj *exc = reg_obj(&regs[vA]);
            const char *exc_type = (exc && exc->class_desc)  ? exc->class_desc
                                 : (exc && exc->type == OBJ_STRING) ? "Ljava/lang/Exception;"
                                 : "Ljava/lang/Exception;";
            int32_t handler_pc = dex_find_catch_handler(interp->df, ci, pc, exc_type);
            if (handler_pc >= 0) {
                result_reg.kind = REG_OBJ; result_reg.obj = exc;
                pc = (uint32_t)handler_pc;
            } else {
                return 1;  /* propagate up */
            }
            break;
        }

        // ── 0x32..0x37 if-test vA, vB, +CCCC  22t ────────────────────────
        case 0x32: case 0x33: case 0x34: case 0x35: case 0x36: case 0x37: {
            int vA = (insn >> 8) & 0xf, vB = (insn >> 12) & 0xf;
            int64_t a = reg_prim(&regs[vA]), b = reg_prim(&regs[vB]);
            int16_t off = (int16_t)insns[pc + 1];
            int taken = 0;
            switch (op) {
                case 0x32: taken = (a == b); break;
                case 0x33: taken = (a != b); break;
                case 0x34: taken = (a <  b); break;
                case 0x35: taken = (a >= b); break;
                case 0x36: taken = (a >  b); break;
                case 0x37: taken = (a <= b); break;
            }
            if (taken) pc = (uint32_t)((int32_t)pc + off);
            else       pc += 2;
            break;
        }

        // ── 0x38..0x3d if-testz vAA, +BBBB  21t ─────────────────────────
        case 0x38: case 0x39: case 0x3a: case 0x3b: case 0x3c: case 0x3d: {
            int vA = (insn >> 8) & 0xff;
            /* For object registers treat non-null as 1, null as 0 */
            int64_t a = (regs[vA].kind == REG_OBJ)
                        ? (regs[vA].obj != NULL ? 1 : 0)
                        : reg_prim(&regs[vA]);
            int16_t off = (int16_t)insns[pc + 1];
            int taken = 0;
            switch (op) {
                case 0x38: taken = (a == 0); break;
                case 0x39: taken = (a != 0); break;
                case 0x3a: taken = (a <  0); break;
                case 0x3b: taken = (a >= 0); break;
                case 0x3c: taken = (a >  0); break;
                case 0x3d: taken = (a <= 0); break;
            }
            if (taken) pc = (uint32_t)((int32_t)pc + off);
            else       pc += 2;
            break;
        }

        // ── 0x52..0x58 iget variants  22c ─────────────────────────────
        // vA = vB.<field@CCCC>
        case 0x52: case 0x53: case 0x54: case 0x55:
        case 0x56: case 0x57: case 0x58: {
            int vA = (insn >> 8) & 0xf;
            int vB = (insn >> 12) & 0xf;
            uint16_t fidx = insns[pc + 1];
            const char *fname = dex_field_name(interp->df, fidx);
            AineObj *obj = reg_obj(&regs[vB]);
            if (op == 0x54) {  /* iget-object only (0x58=iget-short is prim) */
                AineObj *val = obj ? heap_iget_obj(obj, fname) : NULL;
                reg_set_obj(&regs[vA], val);
            } else {
                int64_t val = obj ? heap_iget_prim(obj, fname) : 0;
                reg_set_prim(&regs[vA], val);
            }
            pc += 2; break;
        }
        // ── 0x59..0x5f iput variants  22c ────────────────────────────────
        // vB.<field@CCCC> = vA
        case 0x59: case 0x5a: case 0x5b: case 0x5c:
        case 0x5d: case 0x5e: case 0x5f: {
            int vA = (insn >> 8) & 0xf;
            int vB = (insn >> 12) & 0xf;
            uint16_t fidx = insns[pc + 1];
            const char *fname = dex_field_name(interp->df, fidx);
            AineObj *obj = reg_obj(&regs[vB]);
            if (op == 0x5b) {  /* iput-object only (0x5f=iput-short is prim) */
                if (obj) heap_iput_obj(obj, fname, reg_obj(&regs[vA]));
            } else {
                if (obj) heap_iput_prim(obj, fname, reg_prim(&regs[vA]));
            }
            pc += 2; break;
        }

        // ── 0x60..0x6d sget/sput variants ────────────────────────────────
        // 0x60..0x65: sget-* 21c: vAA, field@BBBB
        case 0x60: case 0x61: case 0x62: case 0x63:
        case 0x64: case 0x65: {
            int vx = byte_hi(insn);
            uint16_t fidx = insns[pc + 1];
            const char *cls   = dex_field_class(interp->df, fidx);
            const char *fname = dex_field_name(interp->df, fidx);
            if (op == 0x62) {  /* sget-object */
                AineObj *stored = heap_sget_obj(cls, fname);
                if (!stored) stored = jni_sget_object(cls, fname);
                reg_set_obj(&regs[vx], stored);
            } else {
                int64_t pv = heap_sget_prim(cls, fname);
                if (pv == 0) pv = jni_sget_prim(cls, fname);
                reg_set_prim(&regs[vx], pv);
            }
            pc += 2;
            break;
        }
        // 0x66..0x67 sget-short/sget-byte
        case 0x66: case 0x67: {
            int vx = (insn >> 8) & 0xff;
            uint16_t fidx2 = insns[pc + 1];
            const char *cls2   = dex_field_class(interp->df, fidx2);
            const char *fname2 = dex_field_name(interp->df, fidx2);
            reg_set_prim(&regs[vx], heap_sget_prim(cls2, fname2));
            pc += 2; break;
        }
        // 0x68..0x6d sput-* 21c: field@BBBB, vAA
        case 0x68: case 0x69: case 0x6a: case 0x6b:
        case 0x6c: case 0x6d: {
            int vx3 = byte_hi(insn);
            uint16_t fidx3 = insns[pc + 1];
            const char *cls3   = dex_field_class(interp->df, fidx3);
            const char *fname3 = dex_field_name(interp->df, fidx3);
            if (op == 0x6a) {  /* sput-object */
                heap_sput_obj(cls3, fname3, reg_obj(&regs[vx3]));
            } else {
                heap_sput_prim(cls3, fname3, reg_prim(&regs[vx3]));
            }
            pc += 2; break;
        }
        case 0x28: {  // goto +AA  10t
            int8_t off = (int8_t)(uint8_t)(insn >> 8);
            pc = (uint32_t)((int32_t)pc + (int32_t)off); break;
        }
        case 0x29: {  // goto/16  20t
            int16_t off = (int16_t)insns[pc + 1];
            pc = (uint32_t)((int32_t)pc + (int32_t)off); break;
        }
        case 0x2a: {  // goto/32  30t
            int32_t off = (int32_t)((uint32_t)insns[pc+1]|(uint32_t)insns[pc+2]<<16);
            pc = (uint32_t)((int32_t)pc + off); break;
        }

        // ── Invoke group 35c (0x6e..0x72) ────────────────────────────
        case 0x6e: case 0x6f: case 0x70: case 0x71: case 0x72: {
            int arg_regs[5];
            int ac = decode_35c(insn, insns[pc + 2], arg_regs);
            uint16_t midx = insns[pc + 1];
            const char *cls  = dex_method_class(interp->df, midx);
            const char *name = dex_method_name(interp->df, midx);
            int is_static    = (op == 0x71);
            Reg all_args[5];
            for (int i = 0; i < ac && i < 5; i++) all_args[i] = regs[arg_regs[i]];
            exec_method(interp, cls, name, all_args, ac, is_static, &result_reg);
            pc += 3; break;
        }

        // ── Invoke-range group 3rc (0x74..0x78) ──────────────────────
        // 3rc: {vCCCC .. vCCCC+AA-1}, method@BBBB
        case 0x74: case 0x75: case 0x76: case 0x77: case 0x78: {
            int ac       = (insn >> 8) & 0xff;
            uint16_t midx = insns[pc + 1];
            int base_reg  = insns[pc + 2];
            const char *cls  = dex_method_class(interp->df, midx);
            const char *name = dex_method_name(interp->df, midx);
            int is_static = (op == 0x77);
            Reg all_args[256];
            for (int i = 0; i < ac && i < 256 && (base_reg + i) < N; i++)
                all_args[i] = regs[base_reg + i];
            exec_method(interp, cls, name, all_args, ac, is_static, &result_reg);
            pc += 3; break;
        }

        // ── 0x23 new-array vx, vy, type@CCCC  22c ────────────────────
        case 0x23: {
            int vx = (insn >> 8) & 0xf;
            int vy = (insn >> 12) & 0xf;
            int len = (int)reg_prim(&regs[vy]);
            AineObj *arr = heap_array_new(len);
            reg_set_obj(&regs[vx], arr);
            pc += 2; break;
        }

        // ── 0x44..0x4d aget/aput variants  23x: vAA, vBB, vCC ────────
        // insns[0]=(vAA<<8)|op   insns[1]=(vCC<<8)|vBB
        case 0x44: case 0x45: case 0x46: case 0x47:
        case 0x48: case 0x49: case 0x4a: {
            // aget: vA <- vB[vC]
            int vA = (insn >> 8) & 0xff;
            int vB = insns[pc + 1] & 0xff;          /* array ref */
            int vC = (insns[pc + 1] >> 8) & 0xff;   /* index */
            AineObj *arr = reg_obj(&regs[vB]);
            int idx = (int)reg_prim(&regs[vC]);
            if (!arr || arr->type != OBJ_ARRAY) {
                reg_set_prim(&regs[vA], 0); pc += 2; break;
            }
            if (op == 0x46) {  /* aget-object */
                if (idx >= 0 && idx < arr->arr_len)
                    reg_set_obj(&regs[vA], arr->arr_obj[idx]);
                else reg_set_obj(&regs[vA], NULL);
            } else {
                if (idx >= 0 && idx < arr->arr_len)
                    reg_set_prim(&regs[vA], arr->arr_prim[idx]);
                else reg_set_prim(&regs[vA], 0);
            }
            pc += 2; break;
        }
        // aput variants 0x4b..0x51: vB[vC] <- vA
        case 0x4b: case 0x4c: case 0x4d: case 0x4e: case 0x4f:
        case 0x50: case 0x51: {
            int vA = (insn >> 8) & 0xff;             /* value */
            int vB = insns[pc + 1] & 0xff;           /* array ref */
            int vC = (insns[pc + 1] >> 8) & 0xff;   /* index */
            AineObj *arr = reg_obj(&regs[vB]);
            int idx = (int)reg_prim(&regs[vC]);
            if (arr && arr->type == OBJ_ARRAY && idx >= 0 && idx < arr->arr_len) {
                if (op == 0x4d) {  /* aput-object */
                    arr->arr_obj[idx] = reg_obj(&regs[vA]);
                } else {
                    arr->arr_prim[idx] = reg_prim(&regs[vA]);
                }
            }
            pc += 2; break;
        }

        // ── Arithmetic opcodes 0x90..0xcf ─────────────────────────────
        // 23x: vAA, vBB, vCC
        // 12x: vA, vB  (2-address form, dest == src1)
#define ARITH_BINOP(dst_r, a, b, op_code) do {             \
    switch (op_code) {                                      \
        case 0x90: case 0xb0: reg_set_prim(dst_r,(int32_t)(a)+(int32_t)(b)); break; \
        case 0x91: case 0xb1: reg_set_prim(dst_r,(int32_t)(a)-(int32_t)(b)); break; \
        case 0x92: case 0xb2: reg_set_prim(dst_r,(int32_t)(a)*(int32_t)(b)); break; \
        case 0x93: case 0xb3: reg_set_prim(dst_r,(b)!=0?(int32_t)(a)/(int32_t)(b):0); break; \
        case 0x94: case 0xb4: reg_set_prim(dst_r,(b)!=0?(int32_t)(a)%(int32_t)(b):0); break; \
        case 0x95: case 0xb5: reg_set_prim(dst_r,(int32_t)(a)&(int32_t)(b)); break;  \
        case 0x96: case 0xb6: reg_set_prim(dst_r,(int32_t)(a)|(int32_t)(b)); break;  \
        case 0x97: case 0xb7: reg_set_prim(dst_r,(int32_t)(a)^(int32_t)(b)); break;  \
        case 0x98: case 0xb8: reg_set_prim(dst_r,(int32_t)(a)<<((b)&31)); break;     \
        case 0x99: case 0xb9: reg_set_prim(dst_r,(int32_t)(a)>>((b)&31)); break;     \
        case 0x9a: case 0xba: reg_set_prim(dst_r,(int32_t)((uint32_t)(a)>>((b)&31)));break;\
        /* long (64-bit) variants 0xa0..0xaa */ \
        case 0xa0: case 0xbb: reg_set_prim(dst_r,(int64_t)(a)+(int64_t)(b)); break;  \
        case 0xa1: case 0xbc: reg_set_prim(dst_r,(int64_t)(a)-(int64_t)(b)); break;  \
        case 0xa2: case 0xbd: reg_set_prim(dst_r,(int64_t)(a)*(int64_t)(b)); break;  \
        case 0xa3: case 0xbe: reg_set_prim(dst_r,(b)!=0?(int64_t)(a)/(int64_t)(b):0);break;\
        case 0xa4: case 0xbf: reg_set_prim(dst_r,(b)!=0?(int64_t)(a)%(int64_t)(b):0);break;\
        case 0xa5: case 0xc0: reg_set_prim(dst_r,(int64_t)(a)&(int64_t)(b)); break;  \
        case 0xa6: case 0xc1: reg_set_prim(dst_r,(int64_t)(a)|(int64_t)(b)); break;  \
        case 0xa7: case 0xc2: reg_set_prim(dst_r,(int64_t)(a)^(int64_t)(b)); break;  \
        case 0xa8: case 0xc3: reg_set_prim(dst_r,(int64_t)(a)<<((b)&63)); break;     \
        case 0xa9: case 0xc4: reg_set_prim(dst_r,(int64_t)(a)>>((b)&63)); break;     \
        case 0xaa: case 0xc5: reg_set_prim(dst_r,(int64_t)((uint64_t)(a)>>((b)&63)));break;\
        /* float/double (0xab..0xb0 float, 0xc6..0xcf double) treated as int */ \
        default: reg_set_prim(dst_r, 0); break;            \
    }                                                       \
} while(0)

        // ── Integer 23x: 0x90-0x9a  add-int through ushr-int ────────────────
        case 0x90: case 0x91: case 0x92: case 0x93: case 0x94:
        case 0x95: case 0x96: case 0x97: case 0x98: case 0x99: case 0x9a: {
            int vA = (insn >> 8) & 0xff;
            int vB = insns[pc + 1] & 0xff;
            int vC = (insns[pc + 1] >> 8) & 0xff;
            ARITH_BINOP(&regs[vA], reg_prim(&regs[vB]), reg_prim(&regs[vC]), op);
            pc += 2; break;
        }

        // ── Long 23x: 0x9b-0xa5  add-long through ushr-long ─────────────────
        case 0x9b: case 0x9c: case 0x9d: case 0x9e: case 0x9f:
        case 0xa0: case 0xa1: case 0xa2: case 0xa3: case 0xa4: case 0xa5: {
            int vA = (insn >> 8) & 0xff;
            int vB = insns[pc + 1] & 0xff;
            int vC = (insns[pc + 1] >> 8) & 0xff;
            int64_t a = reg_prim(&regs[vB]), b = reg_prim(&regs[vC]), r = 0;
            switch (op) {
                case 0x9b: r = a + b; break;
                case 0x9c: r = a - b; break;
                case 0x9d: r = a * b; break;
                case 0x9e: r = b ? a / b : 0; break;
                case 0x9f: r = b ? a % b : 0; break;
                case 0xa0: r = a & b; break;
                case 0xa1: r = a | b; break;
                case 0xa2: r = a ^ b; break;
                case 0xa3: r = a << (b & 63); break;
                case 0xa4: r = a >> (b & 63); break;
                case 0xa5: r = (int64_t)((uint64_t)a >> (b & 63)); break;
            }
            reg_set_prim(&regs[vA], r); pc += 2; break;
        }

        // ── Float 23x: 0xa6-0xaa  add/sub/mul/div/rem-float ─────────────────
        case 0xa6: case 0xa7: case 0xa8: case 0xa9: case 0xaa: {
            int vA = (insn >> 8) & 0xff;
            int vB = insns[pc + 1] & 0xff;
            int vC = (insns[pc + 1] >> 8) & 0xff;
            float fa, fb; float fr = 0.0f;
            uint32_t ua=(uint32_t)reg_prim(&regs[vB]), ub=(uint32_t)reg_prim(&regs[vC]);
            memcpy(&fa,&ua,4); memcpy(&fb,&ub,4);
            switch (op) {
                case 0xa6: fr = fa + fb; break;
                case 0xa7: fr = fa - fb; break;
                case 0xa8: fr = fa * fb; break;
                case 0xa9: fr = fb != 0.0f ? fa / fb : 0.0f; break;
                case 0xaa: fr = fb != 0.0f ? fmodf(fa, fb) : 0.0f; break;
            }
            uint32_t ur; memcpy(&ur, &fr, 4);
            reg_set_prim(&regs[vA], (int64_t)(uint32_t)ur); pc += 2; break;
        }

        // ── Double 23x: 0xab-0xaf  add/sub/mul/div/rem-double ───────────────
        case 0xab: case 0xac: case 0xad: case 0xae: case 0xaf: {
            int vA = (insn >> 8) & 0xff;
            int vB = insns[pc + 1] & 0xff;
            int vC = (insns[pc + 1] >> 8) & 0xff;
            double da, db; double dr = 0.0;
            int64_t ia=reg_prim(&regs[vB]), ib=reg_prim(&regs[vC]);
            memcpy(&da,&ia,8); memcpy(&db,&ib,8);
            switch (op) {
                case 0xab: dr = da + db; break;
                case 0xac: dr = da - db; break;
                case 0xad: dr = da * db; break;
                case 0xae: dr = db != 0.0 ? da / db : 0.0; break;
                case 0xaf: dr = db != 0.0 ? fmod(da, db) : 0.0; break;
            }
            int64_t ur; memcpy(&ur, &dr, 8);
            reg_set_prim(&regs[vA], ur); pc += 2; break;
        }

        // ── Int/long 2-addr: 0xb0-0xc5  12x: vA, vB (dest = vA op vB) ──────
        case 0xb0: case 0xb1: case 0xb2: case 0xb3: case 0xb4:
        case 0xb5: case 0xb6: case 0xb7: case 0xb8: case 0xb9: case 0xba:
        case 0xbb: case 0xbc: case 0xbd: case 0xbe: case 0xbf:
        case 0xc0: case 0xc1: case 0xc2: case 0xc3: case 0xc4: case 0xc5: {
            int vA = (insn >> 8) & 0xf, vB = (insn >> 12) & 0xf;
            ARITH_BINOP(&regs[vA], reg_prim(&regs[vA]), reg_prim(&regs[vB]), op);
            pc++; break;
        }

        // float/2addr 0xc6..0xca and double/2addr 0xcb..0xcf  12x: vA, vB
        case 0xc6: case 0xc7: case 0xc8: case 0xc9: case 0xca:
        case 0xcb: case 0xcc: case 0xcd: case 0xce: case 0xcf: {
            int vA = (insn >> 8) & 0xf, vB = (insn >> 12) & 0xf;
            if (op <= 0xca) {  /* float */
                uint32_t ua = (uint32_t)reg_prim(&regs[vA]);
                uint32_t ub = (uint32_t)reg_prim(&regs[vB]);
                float fa, fb;
                memcpy(&fa, &ua, 4); memcpy(&fb, &ub, 4);
                float fr = 0.0f;
                switch (op) {
                    case 0xc6: fr = fa + fb; break;
                    case 0xc7: fr = fa - fb; break;
                    case 0xc8: fr = fa * fb; break;
                    case 0xc9: fr = fb != 0.0f ? fa / fb : 0.0f; break;
                    case 0xca: fr = fb != 0.0f ? fmodf(fa, fb) : 0.0f; break;
                }
                uint32_t ur; memcpy(&ur, &fr, 4);
                reg_set_prim(&regs[vA], ur);
            } else {  /* double */
                int64_t ia = reg_prim(&regs[vA]);
                int64_t ib = reg_prim(&regs[vB]);
                double da, db;
                memcpy(&da, &ia, 8); memcpy(&db, &ib, 8);
                double dr = 0.0;
                switch (op) {
                    case 0xcb: dr = da + db; break;
                    case 0xcc: dr = da - db; break;
                    case 0xcd: dr = da * db; break;
                    case 0xce: dr = db != 0.0 ? da / db : 0.0; break;
                    case 0xcf: dr = db != 0.0 ? fmod(da, db) : 0.0; break;
                }
                int64_t ir; memcpy(&ir, &dr, 8);
                reg_set_prim(&regs[vA], ir);
            }
            pc++; break;
        }

        // lit16 arithmetic 0xd0..0xd7 22s: vA, vB, #+CCCC
        case 0xd0: case 0xd1: case 0xd2: case 0xd3:
        case 0xd4: case 0xd5: case 0xd6: case 0xd7: {
            int vA = (insn >> 8) & 0xf, vB = (insn >> 12) & 0xf;
            int16_t lit = (int16_t)insns[pc + 1];
            int64_t a = reg_prim(&regs[vB]);
            switch (op) {
                case 0xd0: reg_set_prim(&regs[vA], (int32_t)a + lit); break;
                case 0xd1: reg_set_prim(&regs[vA], (int32_t)a - lit); break;
                case 0xd2: reg_set_prim(&regs[vA], (int32_t)a * lit); break;
                case 0xd3: reg_set_prim(&regs[vA], lit ? (int32_t)a / lit : 0); break;
                case 0xd4: reg_set_prim(&regs[vA], lit ? (int32_t)a % lit : 0); break;
                case 0xd5: reg_set_prim(&regs[vA], (int32_t)a & lit); break;
                case 0xd6: reg_set_prim(&regs[vA], (int32_t)a | lit); break;
                case 0xd7: reg_set_prim(&regs[vA], (int32_t)a ^ lit); break;
            }
            pc += 2; break;
        }

        // lit8 arithmetic 0xd8..0xe2 22b: vAA, vBB, #+CC
        case 0xd8: case 0xd9: case 0xda: case 0xdb:
        case 0xdc: case 0xdd: case 0xde: case 0xdf:
        case 0xe0: case 0xe1: case 0xe2: {
            int vA = (insn >> 8) & 0xff;
            int vB = insns[pc + 1] & 0xff;
            int8_t lit = (int8_t)((insns[pc + 1] >> 8) & 0xff);
            int64_t a  = reg_prim(&regs[vB]);
            switch (op) {
                case 0xd8: reg_set_prim(&regs[vA], (int32_t)a + lit); break;
                case 0xd9: reg_set_prim(&regs[vA], (int32_t)a - lit); break;
                case 0xda: reg_set_prim(&regs[vA], (int32_t)a * lit); break;
                case 0xdb: reg_set_prim(&regs[vA], lit ? (int32_t)a / lit : 0); break;
                case 0xdc: reg_set_prim(&regs[vA], lit ? (int32_t)a % lit : 0); break;
                case 0xdd: reg_set_prim(&regs[vA], (int32_t)a & lit); break;
                case 0xde: reg_set_prim(&regs[vA], (int32_t)a | lit); break;
                case 0xdf: reg_set_prim(&regs[vA], (int32_t)a ^ lit); break;
                case 0xe0: reg_set_prim(&regs[vA], (int32_t)a << (lit & 31)); break;
                case 0xe1: reg_set_prim(&regs[vA], (int32_t)a >> (lit & 31)); break;
                case 0xe2: reg_set_prim(&regs[vA], (int32_t)((uint32_t)a >> (lit & 31))); break;
            }
            pc += 2; break;
        }

        // Type conversion opcodes 0x81..0x8f  12x: vA, vB
        // All float/double conversions use memcpy to correctly handle IEEE 754 bit patterns.
        case 0x81: case 0x82: case 0x83: case 0x84: case 0x85:
        case 0x86: case 0x87: case 0x88: case 0x89: case 0x8a:
        case 0x8b: case 0x8c: case 0x8d: case 0x8e: case 0x8f: {
            int vA = (insn >> 8) & 0xf, vB = (insn >> 12) & 0xf;
            int64_t v = reg_prim(&regs[vB]);
            switch (op) {
                case 0x81: reg_set_prim(&regs[vA], (int64_t)(int32_t)v); break; // int-to-long
                case 0x82: { /* int-to-float: store IEEE 754 bits */
                    float f = (float)(int32_t)v; uint32_t u; memcpy(&u,&f,4);
                    reg_set_prim(&regs[vA], (int64_t)(uint32_t)u); break; }
                case 0x83: { /* int-to-double */
                    double d = (double)(int32_t)v; int64_t u; memcpy(&u,&d,8);
                    reg_set_prim(&regs[vA], u); break; }
                case 0x84: reg_set_prim(&regs[vA], (int32_t)(int64_t)v); break; // long-to-int
                case 0x85: { /* long-to-float */
                    float f = (float)(int64_t)v; uint32_t u; memcpy(&u,&f,4);
                    reg_set_prim(&regs[vA], (int64_t)(uint32_t)u); break; }
                case 0x86: { /* long-to-double */
                    double d = (double)(int64_t)v; int64_t u; memcpy(&u,&d,8);
                    reg_set_prim(&regs[vA], u); break; }
                case 0x87: { /* float-to-int */
                    float f; uint32_t bits=(uint32_t)v; memcpy(&f,&bits,4);
                    reg_set_prim(&regs[vA], (int64_t)(int32_t)f); break; }
                case 0x88: { /* float-to-long */
                    float f; uint32_t bits=(uint32_t)v; memcpy(&f,&bits,4);
                    reg_set_prim(&regs[vA], (int64_t)f); break; }
                case 0x89: { /* float-to-double */
                    float f; uint32_t bits=(uint32_t)v; memcpy(&f,&bits,4);
                    double d=(double)f; int64_t du; memcpy(&du,&d,8);
                    reg_set_prim(&regs[vA], du); break; }
                case 0x8a: { /* double-to-int */
                    double d; memcpy(&d,&v,8);
                    reg_set_prim(&regs[vA], (int64_t)(int32_t)d); break; }
                case 0x8b: { /* double-to-long */
                    double d; memcpy(&d,&v,8);
                    reg_set_prim(&regs[vA], (int64_t)d); break; }
                case 0x8c: { /* double-to-float */
                    double d; memcpy(&d,&v,8); float f=(float)d;
                    uint32_t u; memcpy(&u,&f,4);
                    reg_set_prim(&regs[vA], (int64_t)(uint32_t)u); break; }
                case 0x8d: reg_set_prim(&regs[vA], (int32_t)(int8_t)v); break;  // int-to-byte
                case 0x8e: reg_set_prim(&regs[vA], (int32_t)(uint16_t)v); break;// int-to-char
                case 0x8f: reg_set_prim(&regs[vA], (int32_t)(int16_t)v); break; // int-to-short
                default:   reg_set_prim(&regs[vA], v); break;
            }
            pc++; break;
        }
        case 0x7b: case 0x7c: case 0x7d: case 0x7e:
        case 0x7f: case 0x80: {
            // unary: neg-int, not-int, neg-long, not-long, neg-float, neg-double
            int vA = (insn >> 8) & 0xf, vB = (insn >> 12) & 0xf;
            int64_t v = reg_prim(&regs[vB]);
            switch (op) {
                case 0x7b: reg_set_prim(&regs[vA], -(int32_t)v); break; // neg-int
                case 0x7c: reg_set_prim(&regs[vA], ~(int32_t)v); break; // not-int
                case 0x7d: reg_set_prim(&regs[vA], -(int64_t)v); break; // neg-long
                case 0x7e: reg_set_prim(&regs[vA], ~(int64_t)v); break; // not-long
                default:   reg_set_prim(&regs[vA], -v); break;
            }
            pc++; break;
        }

        // ── 0x15 const/high16 vAA, #+BBBB0000 ───────────────────────
        case 0x15: {
            int vx = byte_hi(insn);
            int32_t lit = (int32_t)((uint32_t)insns[pc + 1] << 16);
            reg_set_prim(&regs[vx], lit);
            pc += 2; break;
        }
        // ── 0x16 const-wide/16 vAA, #+BBBB ──────────────────────────
        case 0x16: {
            int vx = byte_hi(insn);
            int64_t lit = (int64_t)(int16_t)insns[pc + 1];
            reg_set_prim(&regs[vx], lit);
            pc += 2; break;
        }
        // ── 0x17 const-wide/32 ──────────────────────────────────────
        case 0x17: {
            int vx = byte_hi(insn);
            int64_t lit = (int64_t)(int32_t)((uint32_t)insns[pc+1]|(uint32_t)insns[pc+2]<<16);
            reg_set_prim(&regs[vx], lit);
            pc += 3; break;
        }
        // ── 0x18 const-wide ─────────────────────────────────────────
        case 0x18: {
            int vx = byte_hi(insn);
            uint64_t lo = (uint32_t)insns[pc+1] | ((uint32_t)insns[pc+2] << 16);
            uint64_t hi = (uint32_t)insns[pc+3] | ((uint32_t)insns[pc+4] << 16);
            reg_set_prim(&regs[vx], (int64_t)(lo | (hi << 32)));
            pc += 5; break;
        }
        // ── 0x19 const-wide/high16 ───────────────────────────────────
        case 0x19: {
            int vx = byte_hi(insn);
            int64_t lit = (int64_t)((uint64_t)(uint16_t)insns[pc+1] << 48);
            reg_set_prim(&regs[vx], lit);
            pc += 2; break;
        }

        // ── 0x04 move-wide vA, vB 12x ──────────────────────────────
        case 0x04: {
            int vA = (insn >> 8) & 0xf, vB = (insn >> 12) & 0xf;
            regs[vA] = regs[vB]; pc++; break;
        }
        // ── 0x05 move-wide/from16 22x ──────────────────────────────
        case 0x05: {
            int vA = (insn >> 8) & 0xff, vB = insns[pc+1];
            regs[vA] = regs[vB]; pc += 2; break;
        }
        // ── 0x08 move-object/from16 22x ────────────────────────────
        case 0x08: {
            int vA = (insn >> 8) & 0xff, vB = insns[pc+1];
            regs[vA] = regs[vB]; pc += 2; break;
        }
        // ── 0x09 move-object/16 32x ─────────────────────────────────
        case 0x09: {
            int vA = insns[pc+1], vB = insns[pc+2];
            if (vA < N && vB < N) regs[vA] = regs[vB];
            pc += 3; break;
        }
        // ── 0x0d move-exception vAA ──────────────────────────────────
        case 0x0d:
            regs[byte_hi(insn)] = result_reg;  /* result_reg holds the thrown exc */
            pc++; break;

        // ── 0x10 return-wide vAA ─────────────────────────────────────
        case 0x10: {
            result_reg = regs[byte_hi(insn)];            if (result_out) *result_out = result_reg;            return 0;
        }

        // ── 0x1c const-class vAA, type@BBBB 21c ─────────────────────
        case 0x1c: {
            int vx = byte_hi(insn);
            const char *type = dex_type_name(interp->df, insns[pc + 1]);
            reg_set_obj(&regs[vx], heap_string(type ? type : ""));
            pc += 2; break;
        }
        // ── 0x1d monitor-enter / 0x1e monitor-exit ──────────────────
        case 0x1d: case 0x1e: pc++; break;
        // ── 0x1f check-cast vAA, type@CCCC 21c ──────────────────────
        case 0x1f: pc += 2; break;
        // ── 0x20 instance-of vA, vB, type@CCCC 22c ──────────────────
        case 0x20: {
            int vA = (insn >> 8) & 0xf;
            int vB = (insn >> 12) & 0xf;
            const char *type = dex_type_name(interp->df, insns[pc + 1]);
            AineObj *obj = reg_obj(&regs[vB]);
            int r = 0;
            if (obj && type) {
                if (strcmp(type, "Ljava/lang/Object;") == 0) {
                    r = 1;  /* everything extends Object */
                } else if (strcmp(type, "Ljava/lang/String;") == 0) {
                    r = (obj->type == OBJ_STRING || obj->type == OBJ_STRINGBUILDER) ? 1 : 0;
                } else if (obj->class_desc && strcmp(obj->class_desc, type) == 0) {
                    r = 1;
                } else if (strcmp(type, "Ljava/util/List;") == 0 ||
                           strcmp(type, "Ljava/util/Collection;") == 0) {
                    r = (obj->type == OBJ_ARRAYLIST) ? 1 : 0;
                } else if (strcmp(type, "Ljava/util/Map;") == 0) {
                    r = (obj->type == OBJ_HASHMAP) ? 1 : 0;
                }
            }
            reg_set_prim(&regs[vA], r);
            pc += 2; break;
        }
        // ── 0x21 array-length vA, vB 12x ────────────────────────────
        case 0x21: {
            int vA = (insn >> 8) & 0xf, vB = (insn >> 12) & 0xf;
            AineObj *arr = reg_obj(&regs[vB]);
            int len = (arr && arr->type == OBJ_ARRAY) ? arr->arr_len : 0;
            reg_set_prim(&regs[vA], len);
            pc++; break;
        }
        // ── 0x24 filled-new-array 35c ────────────────────────────────────────────
        case 0x24: {
            int arg_rs[5]; int ac = decode_35c(insn, insns[pc + 2], arg_rs);
            AineObj *arr = heap_array_new(ac);
            if (arr) {
                for (int i = 0; i < ac; i++) {
                    if (regs[arg_rs[i]].kind == REG_OBJ)
                        arr->arr_obj[i]  = reg_obj(&regs[arg_rs[i]]);
                    else
                        arr->arr_prim[i] = reg_prim(&regs[arg_rs[i]]);
                }
            }
            result_reg.kind = REG_OBJ; result_reg.obj = arr;
            pc += 3; break;
        }
        // ── 0x25 filled-new-array/range 3rc ────────────────────────────────────────
        case 0x25: {
            int ac = (insn >> 8) & 0xff, base_r = insns[pc + 2];
            AineObj *arr = heap_array_new(ac);
            if (arr) {
                for (int i = 0; i < ac && (base_r + i) < N; i++) {
                    if (regs[base_r + i].kind == REG_OBJ)
                        arr->arr_obj[i]  = reg_obj(&regs[base_r + i]);
                    else
                        arr->arr_prim[i] = reg_prim(&regs[base_r + i]);
                }
            }
            result_reg.kind = REG_OBJ; result_reg.obj = arr;
            pc += 3; break;
        }
        // ── 0x26 fill-array-data vAA, +BBBBBBBB 31t ────────────────────────
        // Payload at pc+rel: ident(0x0300), elem_width, count(32bit), data
        case 0x26: {
            int vA = byte_hi(insn);
            int32_t rel = (int32_t)((uint32_t)insns[pc+1] | ((uint32_t)insns[pc+2] << 16));
            AineObj *arr = reg_obj(&regs[vA]);
            if (arr && arr->type == OBJ_ARRAY && rel != 0) {
                const uint16_t *payload = insns + (int32_t)pc + rel;
                if (payload[0] == 0x0300) {
                    uint16_t ew    = payload[1];
                    uint32_t count = (uint32_t)payload[2] | ((uint32_t)payload[3] << 16);
                    const uint8_t *data = (const uint8_t *)(payload + 4);
                    uint32_t n = count < (uint32_t)arr->arr_len ? count : (uint32_t)arr->arr_len;
                    for (uint32_t i = 0; i < n; i++) {
                        int64_t v = 0;
                        switch (ew) {
                            case 1: v = (int8_t)data[i]; break;
                            case 2: v = (int16_t)((uint16_t)data[2*i] | ((uint16_t)data[2*i+1] << 8)); break;
                            case 4: { uint32_t u = (uint32_t)data[4*i]|(uint32_t)data[4*i+1]<<8|(uint32_t)data[4*i+2]<<16|(uint32_t)data[4*i+3]<<24; v=(int32_t)u; break; }
                            case 8: { uint64_t u=0; for(int b=0;b<8;b++) u|=((uint64_t)data[8*i+b]<<(8*b)); v=(int64_t)u; break; }
                        }
                        arr->arr_prim[i] = v;
                    }
                }
            }
            pc += 3; break;
        }

        // ── 0x2b packed-switch 31t ──────────────────────────────────
        case 0x2b: {
            int vA = byte_hi(insn);
            int32_t rel = (int32_t)((uint32_t)insns[pc+1]|(uint32_t)insns[pc+2]<<16);
            int32_t key = (int32_t)reg_prim(&regs[vA]);
            const uint16_t *payload = insns + (int32_t)pc + rel;
            // payload: 0x0100, size, first_key, [targets...]
            if (payload[0] == 0x0100) {
                uint16_t sz = payload[1];
                int32_t first_key = (int32_t)((uint32_t)payload[2]|(uint32_t)payload[3]<<16);
                int idx = key - first_key;
                if (idx >= 0 && idx < (int)sz) {
                    int32_t off = (int32_t)((uint32_t)payload[4+2*idx]|(uint32_t)payload[5+2*idx]<<16);
                    pc = (uint32_t)((int32_t)pc + off); break;
                }
            }
            pc += 3; break;
        }
        // ── 0x2c sparse-switch 31t ──────────────────────────────────
        case 0x2c: {
            int vA = byte_hi(insn);
            int32_t rel = (int32_t)((uint32_t)insns[pc+1]|(uint32_t)insns[pc+2]<<16);
            int32_t key = (int32_t)reg_prim(&regs[vA]);
            const uint16_t *payload = insns + (int32_t)pc + rel;
            // payload: 0x0200, size, [keys...], [targets...]
            if (payload[0] == 0x0200) {
                uint16_t sz = payload[1];
                for (uint16_t i = 0; i < sz; i++) {
                    int32_t k = (int32_t)((uint32_t)payload[2+2*i]|(uint32_t)payload[3+2*i]<<16);
                    if (k == key) {
                        int32_t off = (int32_t)((uint32_t)payload[2+2*sz+2*i]|(uint32_t)payload[3+2*sz+2*i]<<16);
                        pc = (uint32_t)((int32_t)pc + off); goto next_insn;
                    }
                }
            }
            pc += 3; break;
        }

        // ── 0x2d..0x31 cmp-* variants ───────────────────────────────
        case 0x2d: case 0x2e: case 0x2f: case 0x30: case 0x31: {
            int vA = (insn >> 8) & 0xff;
            int vB = insns[pc + 1] & 0xff;
            int vC = (insns[pc + 1] >> 8) & 0xff;
            int64_t a = reg_prim(&regs[vB]), b_ = reg_prim(&regs[vC]);
            int32_t cmp_res = (a < b_) ? -1 : (a > b_) ? 1 : 0;
            reg_set_prim(&regs[vA], cmp_res);
            pc += 2; break;
        }

        // ── Unhandled ─────────────────────────────────────────────────
        default:
            fprintf(stderr, "[aine-dalvik] unhandled opcode 0x%02x at pc=%u\n", op, pc);
            pc++; break;
        }
        continue;
        next_insn: ;  /* target for goto (sparse-switch) */
    }
}

// ── interp_run_runnable ───────────────────────────────────────────────────
void interp_run_runnable(AineInterp *interp, AineObj *runnable)
{
    if (!runnable || !runnable->class_desc) return;
    Reg r;
    r.kind = REG_OBJ;
    r.obj  = runnable;
    exec_method(interp, runnable->class_desc, "run", &r, 1, 0, NULL);
}

/* ── Activity finish flag ─────────────────────────────────────────────────
 * On macOS, window.m provides aine_activity_should_finish() (reads an atomic).
 * On other platforms (or headless builds), default to always-running: finish
 * is triggered only by the time-based deadline below. */
#ifdef __APPLE__
extern int  aine_activity_should_finish(void);
extern void aine_activity_request_finish(void);
#else
static int  aine_activity_should_finish(void)  { return 0; }
static void aine_activity_request_finish(void) {}
#endif

static int64_t interp_now_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000000000LL + ts.tv_nsec;
}

/* Set by aine_window_run() when the --window flag is active.
 * Controls whether activity_event_loop uses interactive mode. */
static int g_window_mode = 0;
void interp_set_window_mode(int enabled) { g_window_mode = enabled; }

/* Max onDraw frames before auto-exit (0 = unlimited). */
static int g_max_frames = 0;
static int g_draw_count = 0;
void interp_set_max_frames(int n) { g_max_frames = n; g_draw_count = 0; }

/* ── Input dispatch (macOS only) ──────────────────────────────────────────
 * Include inputflinger header for AineInputEvent; compile-guarded on Apple. */
#ifdef __APPLE__
#include "inputflinger.h"

static int dispatch_input_events(AineInterp *interp,
                                  const char *class_descriptor,
                                  Reg *this_reg)
{
    int count = 0;
    AineInputEvent ev;
    while (aine_input_poll(&ev)) {
        count++;
        if (ev.kind == AINE_INPUT_KEY) {
            /* Build: KeyEvent(int action, int keyCode) */
            AineObj *ke = calloc(1, sizeof(AineObj));
            ke->type = OBJ_USERCLASS;
            ke->class_desc = "Landroid/view/KeyEvent;";
            heap_iput_prim(ke, "action",  (int64_t)ev.key.action);
            heap_iput_prim(ke, "keycode", (int64_t)ev.key.keycode);
            heap_iput_prim(ke, "meta",    (int64_t)ev.key.meta_state);

            /* Build call args: [this, keycode_int, keyevent_obj] */
            Reg args[3];
            args[0] = *this_reg;
            reg_set_prim(&args[1], (int64_t)ev.key.keycode);
            reg_set_obj (&args[2], ke);

            if (ev.key.action == AKEY_ACTION_DOWN)
                exec_method(interp, class_descriptor, "onKeyDown", args, 3, 0, NULL);
            else
                exec_method(interp, class_descriptor, "onKeyUp",   args, 3, 0, NULL);

        } else if (ev.kind == AINE_INPUT_MOTION) {
            /* Build MotionEvent object */
            AineObj *me = calloc(1, sizeof(AineObj));
            me->type = OBJ_USERCLASS;
            me->class_desc = "Landroid/view/MotionEvent;";
            heap_iput_prim(me, "action", (int64_t)ev.motion.action);
            /* Store x/y as integer bits of float for simplicity */
            union { float f; int64_t i; } cx, cy;
            cx.f = ev.motion.x; cy.f = ev.motion.y;
            heap_iput_prim(me, "x", cx.i);
            heap_iput_prim(me, "y", cy.i);

            Reg args[2];
            args[0] = *this_reg;
            reg_set_obj(&args[1], me);
            exec_method(interp, class_descriptor, "onTouchEvent", args, 2, 0, NULL);
        }
    }
    return count;
}
#endif /* __APPLE__ */

/* ── Interactive Activity event loop ─────────────────────────────────────
 * Interactive (--window) mode: runs 60 s cap, exits on finish() or window close.
 * Also exits if handler queue + input have both been empty for 2 consecutive
 * seconds (so test Activities that don't call finish() exit gracefully).
 * Headless (no --window):  drain handler queue once, up to 10 s, then stop. */
static void activity_event_loop(AineInterp *interp,
                                const char *class_descriptor,
                                Reg *this_reg)
{
#ifdef __APPLE__
    if (g_window_mode) {
        /* Interactive: 60-second safety cap; exits early on finish signal */
        int64_t deadline_ns   = interp_now_ns() + 60LL * 1000000000LL;
        int64_t idle_since_ns = interp_now_ns();
        int64_t last_draw_ns  = 0;  /* timestamp of last onDraw — for 60fps cap */
        static const int64_t FRAME_NS = 16666667LL;  /* 1/60 s in ns */

        while (!aine_activity_should_finish() && interp_now_ns() < deadline_ns) {
            handler_drain(interp, 50);  /* 50 ms chunk */
            int had_input = dispatch_input_events(interp, class_descriptor, this_reg);

            /* onDraw dispatch: if the content view was invalidated, call onDraw */
            AineObj *view = jni_get_content_view();
            if (view && view->class_desc && jni_pop_invalidated()) {
                /* 60 fps cap: if last frame was less than 16.67 ms ago, wait */
                int64_t now = interp_now_ns();
                if (last_draw_ns > 0) {
                    int64_t elapsed = now - last_draw_ns;
                    if (elapsed < FRAME_NS) {
                        struct timespec ts;
                        int64_t wait = FRAME_NS - elapsed;
                        ts.tv_sec  = wait / 1000000000LL;
                        ts.tv_nsec = wait % 1000000000LL;
                        nanosleep(&ts, NULL);
                    }
                }
                static AineObj s_canvas = {
                    .type = OBJ_USERCLASS,
                    .class_desc = "Landroid/graphics/Canvas;"
                };
                Reg draw_args[2];
                draw_args[0].kind = REG_OBJ; draw_args[0].obj = view;
                draw_args[1].kind = REG_OBJ; draw_args[1].obj = &s_canvas;
#ifdef __APPLE__
                aine_canvas_begin_frame();
#endif
                exec_method(interp, view->class_desc, "onDraw", draw_args, 2, 0, NULL);
#ifdef __APPLE__
                aine_canvas_end_frame();   /* marks dirty once for the complete frame */
#endif
                last_draw_ns  = interp_now_ns();
                idle_since_ns = last_draw_ns; /* drawing resets idle clock */
                g_draw_count++;
                if (g_max_frames > 0 && g_draw_count >= g_max_frames) {
                    fprintf(stderr, "[arcs] frames-complete:%d\n", g_draw_count);
                    return;
                }
            }

            if (!handler_pending() && !had_input) {
                /* Idle — 2-second auto-exit */
                if (interp_now_ns() - idle_since_ns > 2LL * 1000000000LL) {
                    break;  /* Nothing to do; exit gracefully */
                }
                struct timespec ts = {0, 20000000L}; /* 20 ms */
                nanosleep(&ts, NULL);
            } else {
                idle_since_ns = interp_now_ns(); /* reset idle clock */
            }
        }
        return;
    }
#endif
    /* Headless: drain once up to 10 seconds then stop */
    handler_drain(interp, 10000);
}

// ── Public entry point ─────────────────────────────────────────────────────
int interp_run_main(AineInterp *interp, const char *class_descriptor) {
    int cdef_idx = dex_find_class(interp->df, class_descriptor);
    if (cdef_idx < 0) {
        fprintf(stderr, "[aine-dalvik] class not found: %s\n", class_descriptor);
        return 1;
    }

    int method_idx = dex_find_method(interp->df, cdef_idx,
                                     "main", "([Ljava/lang/String;)V");
    if (method_idx < 0) {
        /* No main() — try Activity lifecycle mode (onCreate → … → onDestroy) */
        fprintf(stderr, "[aine-dalvik] Activity mode: %s\n", class_descriptor);

        static AineObj s_this  = { .type = OBJ_NULL };
        static AineObj s_null  = { .type = OBJ_NULL };
        Reg this_reg;    reg_set_obj(&this_reg, &s_this);
        Reg null_reg;    reg_set_obj(&null_reg, &s_null);
        Reg two_regs[2]; two_regs[0] = this_reg; two_regs[1] = null_reg;

        /* <init>()V */
        exec_method(interp, class_descriptor, "<init>", &this_reg, 1, 0, NULL);
        /* onCreate(Bundle)V — pass null bundle */
        exec_method(interp, class_descriptor, "onCreate", two_regs, 2, 0, NULL);
        /* onStart()V */
        exec_method(interp, class_descriptor, "onStart", &this_reg, 1, 0, NULL);
        /* onResume()V */
        exec_method(interp, class_descriptor, "onResume", &this_reg, 1, 0, NULL);
        /* Interactive or drain-only event loop */
        activity_event_loop(interp, class_descriptor, &this_reg);
        /* Lifecycle teardown */
        exec_method(interp, class_descriptor, "onPause",   &this_reg, 1, 0, NULL);
        exec_method(interp, class_descriptor, "onStop",    &this_reg, 1, 0, NULL);
        exec_method(interp, class_descriptor, "onDestroy", &this_reg, 1, 0, NULL);
        return 0;
    }

    const DexCodeItem *ci = dex_code_item(interp->df, cdef_idx, method_idx);
    if (!ci) {
        fprintf(stderr, "[aine-dalvik] main() has no code\n");
        return 1;
    }

    Reg null_arg = {0};
    Reg result   = {0};
    return exec_code(interp, ci, &null_arg, 1, &result);
}
