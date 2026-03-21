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

// Invoke a Runnable's run() method using the class stored in runnable->class_desc.
// No-op if runnable is NULL or has no class_desc.
void interp_run_runnable(AineInterp *interp, AineObj *runnable);

