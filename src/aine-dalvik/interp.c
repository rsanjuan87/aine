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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
    // Try DEX
    int cdef = dex_find_class(interp->df, class_desc);
    if (cdef >= 0) {
        int midx = dex_find_method(interp->df, cdef, method_name, NULL);
        if (midx >= 0) {
            const DexCodeItem *ci = dex_code_item(interp->df, cdef, midx);
            if (ci) { exec_code(interp, ci, all_args, all_count, result_out); return; }
        }
        return; // abstract/native/constructor — no-op
    }
    // JNI
    AineObj *this_obj = NULL;
    int jni_start = 0;
    if (!is_static && all_count > 0) { this_obj = reg_obj(&all_args[0]); jni_start = 1; }
    int jni_n = all_count - jni_start;
    /* Box primitive args as string objects so JNI dispatch can read them */
    AineObj prim_boxes[8];
    char    prim_bufs[8][32];
    AineObj *args[8] = {0};
    for (int a = 0; a < jni_n && a < 8; a++) {
        const Reg *r = &all_args[jni_start + a];
        if (r->kind == REG_OBJ) {
            args[a] = r->obj;
        } else {
            snprintf(prim_bufs[a], sizeof(prim_bufs[a]), "%lld", (long long)r->prim);
            prim_boxes[a] = (AineObj){0};
            prim_boxes[a].type = OBJ_STRING;
            prim_boxes[a].str  = prim_bufs[a];
            args[a] = &prim_boxes[a];
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
                    obj = heap_sb_new();
                } else {
                    int cd = dex_find_class(interp->df, type);
                    obj = (cd >= 0) ? heap_userclass(cd)
                                    : (AineObj *)calloc(1, sizeof(AineObj));
                }
            } else { obj = calloc(1, sizeof(AineObj)); }
            reg_set_obj(&regs[vx], obj);
            pc += 2;
            break;
        }

        // ── 0x27 throw vAA  11x ──────────────────────────────────────────
        case 0x27:
            return 1;  // unwind; caller treats non-zero as exception

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
            int64_t a = reg_prim(&regs[vA]);
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

        // ── 0x52..0x58 iget variants  22c — stub: return 0/null ─────────
        case 0x52: case 0x53: case 0x54: case 0x55:
        case 0x56: case 0x57: case 0x58: {
            int vA = (insn >> 8) & 0xf;
            memset(&regs[vA], 0, sizeof(Reg));
            pc += 2; break;
        }
        // ── 0x59..0x5f iput variants  22c — stub: ignore ─────────────────
        case 0x59: case 0x5a: case 0x5b: case 0x5c:
        case 0x5d: case 0x5e: case 0x5f:
            pc += 2; break;

        // ── 0x60..0x6d sget/sput variants ────────────────────────────────
        // 0x60..0x65: sget (object version via JNI, others return 0)
        case 0x60: case 0x61: case 0x62: case 0x63:
        case 0x64: case 0x65: {
            int vx = byte_hi(insn);
            uint16_t fidx = insns[pc + 1];
            const char *cls   = dex_field_class(interp->df, fidx);
            const char *fname = dex_field_name(interp->df, fidx);
            AineObj *obj = jni_sget_object(cls, fname);
            reg_set_obj(&regs[vx], obj);
            pc += 2;
            break;
        }
        // 0x66..0x67 sget-short/sget-byte stubs
        case 0x66: case 0x67: {
            int vx = (insn >> 8) & 0xff;
            reg_set_prim(&regs[vx], 0); pc += 2; break;
        }
        // 0x68..0x6d sput variants — stub: ignore
        case 0x68: case 0x69: case 0x6a: case 0x6b:
        case 0x6c: case 0x6d:
            pc += 2; break;
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

        // add-int/sub-int/.../rem-long 23x: vAA, vBB, vCC
        case 0x90: case 0x91: case 0x92: case 0x93: case 0x94:
        case 0x95: case 0x96: case 0x97: case 0x98: case 0x99: case 0x9a:
        case 0xa0: case 0xa1: case 0xa2: case 0xa3: case 0xa4:
        case 0xa5: case 0xa6: case 0xa7: case 0xa8: case 0xa9: case 0xaa: {
            int vA = (insn >> 8) & 0xff;
            int vB = insns[pc + 1] & 0xff;
            int vC = (insns[pc + 1] >> 8) & 0xff;
            ARITH_BINOP(&regs[vA], reg_prim(&regs[vB]), reg_prim(&regs[vC]), op);
            pc += 2; break;
        }

        // 2-addr forms: 0xb0..0xcf 12x: vA, vB (dest = vA op vB)
        case 0xb0: case 0xb1: case 0xb2: case 0xb3: case 0xb4:
        case 0xb5: case 0xb6: case 0xb7: case 0xb8: case 0xb9: case 0xba:
        case 0xbb: case 0xbc: case 0xbd: case 0xbe: case 0xbf:
        case 0xc0: case 0xc1: case 0xc2: case 0xc3: case 0xc4: case 0xc5: {
            int vA = (insn >> 8) & 0xf, vB = (insn >> 12) & 0xf;
            ARITH_BINOP(&regs[vA], reg_prim(&regs[vA]), reg_prim(&regs[vB]), op);
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

        // int-to-* / long-to-* conversions 0x81..0x8f 12x: vA, vB
        case 0x81: case 0x82: case 0x83: case 0x84: case 0x85:
        case 0x86: case 0x87: case 0x88: case 0x8b: case 0x8c:
        case 0x8d: case 0x8e: case 0x8f: {
            int vA = (insn >> 8) & 0xf, vB = (insn >> 12) & 0xf;
            int64_t v = reg_prim(&regs[vB]);
            switch (op) {
                case 0x81: reg_set_prim(&regs[vA], (int64_t)(int32_t)v); break; // int-to-long
                case 0x82: reg_set_prim(&regs[vA], (int64_t)(float)(int32_t)v); break;
                case 0x83: reg_set_prim(&regs[vA], (int64_t)(double)(int32_t)v); break;
                case 0x84: reg_set_prim(&regs[vA], (int32_t)(int64_t)v); break; // long-to-int
                case 0x85: reg_set_prim(&regs[vA], (int64_t)(float)(int64_t)v); break;
                case 0x86: reg_set_prim(&regs[vA], (int64_t)(double)(int64_t)v); break;
                case 0x87: reg_set_prim(&regs[vA], (int64_t)(int32_t)(float)v); break; // float-to-int
                case 0x88: reg_set_prim(&regs[vA], (int64_t)(int64_t)(float)v); break; // float-to-long
                case 0x8b: reg_set_prim(&regs[vA], (int64_t)(int32_t)(double)v); break; // double-to-int
                case 0x8c: reg_set_prim(&regs[vA], (int64_t)(double)v); break;
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
            memset(&regs[byte_hi(insn)], 0, sizeof(Reg));
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
            reg_set_prim(&regs[vA], 0);  // stub: always false for now
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
        // ── 0x24 filled-new-array 35c — stub: build int array ────────
        case 0x24: {
            int arg_rs[5]; int ac = decode_35c(insn, insns[pc + 2], arg_rs);
            AineObj *arr = heap_array_new(ac);
            if (arr) {
                for (int i = 0; i < ac; i++)
                    arr->arr_prim[i] = reg_prim(&regs[arg_rs[i]]);
            }
            result_reg.kind = REG_OBJ; result_reg.obj = arr;
            pc += 3; break;
        }
        // ── 0x25 filled-new-array/range 3rc ──────────────────────────
        case 0x25: {
            int ac = (insn >> 8) & 0xff, base_r = insns[pc + 2];
            AineObj *arr = heap_array_new(ac);
            if (arr) {
                for (int i = 0; i < ac && (base_r + i) < N; i++)
                    arr->arr_prim[i] = reg_prim(&regs[base_r + i]);
            }
            result_reg.kind = REG_OBJ; result_reg.obj = arr;
            pc += 3; break;
        }
        // ── 0x26 fill-array-data vAA, +BBBBBBBB 31t ──────────────────
        // Payload at pc+offset; skip for now (just advance)
        case 0x26:
            pc += 3; break;

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

        // ── 0x52..0x58 iget real impl (lookup via DEX field table) ────
        // Already handled above as stubs — real impl:
        // for now just leave as zero-return stubs

        // ── Unhandled ─────────────────────────────────────────────────
        default:
            fprintf(stderr, "[aine-dalvik] unhandled opcode 0x%02x at pc=%u\n", op, pc);
            pc++; break;
        }
        continue;
        next_insn: ;  /* target for goto (sparse-switch) */
    }
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
