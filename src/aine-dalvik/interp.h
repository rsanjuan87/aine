// aine-dalvik/interp.h — Dalvik bytecode interpreter
#pragma once
#include "dex.h"
#include "heap.h"

// Opaque interpreter context
typedef struct AineInterp AineInterp;

// Create an interpreter bound to a loaded DEX file
AineInterp *interp_new(const DexFile *df);
void        interp_free(AineInterp *interp);

// Execute the static main([Ljava/lang/String;)V method of class_descriptor
// Returns 0 on success, non-zero on error
int interp_run_main(AineInterp *interp, const char *class_descriptor);
