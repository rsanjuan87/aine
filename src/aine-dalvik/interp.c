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
    AineObj *args[8] = {0};
    for (int a = 0; a < jni_n && a < 8; a++) args[a] = reg_obj(&all_args[jni_start + a]);
    JniResult r = jni_dispatch(class_desc, method_name, this_obj, args, jni_n, is_static);
    if (result_out && !r.is_void) reg_set_obj(result_out, r.obj);
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
            result_reg = regs[byte_hi(insn)];
            return 0;
        }

        // ── 0x11 return-object vx ─────────────────────────────────────
        case 0x11: {
            result_reg = regs[byte_hi(insn)];
            return 0;
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

        // ── Unhandled ─────────────────────────────────────────────────
        default:
            fprintf(stderr, "[aine-dalvik] unhandled opcode 0x%02x at pc=%u\n", op, pc);
            pc++; break;
        }
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
        fprintf(stderr, "[aine-dalvik] main() not found in %s\n", class_descriptor);
        return 1;
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
