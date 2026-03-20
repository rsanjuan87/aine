/*
 * aine-hals/libandroid/native_activity.c — ANativeActivity stubs
 *
 * Provides enough surface for apps that call ANativeActivity_onCreate
 * and access AAssetManager. Full implementation deferred to F6 (ART).
 */
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- ANativeActivity types (minimal, matching Android NDK layout) ---- */

struct ANativeActivityCallbacks;

typedef struct ANativeActivity {
    struct ANativeActivityCallbacks *callbacks;
    void  *vm;              /* JavaVM* */
    void  *env;             /* JNIEnv* */
    void  *clazz;           /* jobject */
    const char *internalDataPath;
    const char *externalDataPath;
    int32_t sdkVersion;
    void   *instance;
    void   *assetManager;   /* AAssetManager* */
    const char *obbPath;
} ANativeActivity;

typedef struct ANativeActivityCallbacks {
    void (*onStart)(ANativeActivity *);
    void (*onResume)(ANativeActivity *);
    void *(*onSaveInstanceState)(ANativeActivity *, size_t *);
    void (*onPause)(ANativeActivity *);
    void (*onStop)(ANativeActivity *);
    void (*onDestroy)(ANativeActivity *);
    void (*onWindowFocusChanged)(ANativeActivity *, int);
    void (*onNativeWindowCreated)(ANativeActivity *, void *);
    void (*onNativeWindowResized)(ANativeActivity *, void *);
    void (*onNativeWindowRedrawNeeded)(ANativeActivity *, void *);
    void (*onNativeWindowDestroyed)(ANativeActivity *, void *);
    void (*onInputQueueCreated)(ANativeActivity *, void *);
    void (*onInputQueueDestroyed)(ANativeActivity *, void *);
    void (*onContentRectChanged)(ANativeActivity *, const void *);
    void (*onConfigurationChanged)(ANativeActivity *);
    void (*onLowMemory)(ANativeActivity *);
} ANativeActivityCallbacks;

/*
 * ANativeActivity_onCreate — called by system when native activity starts.
 * Apps implement this symbol; AINE calls it after dalvikvm bootstrap.
 * Here we provide a "do-nothing" default so the dylib loads without errors.
 */
__attribute__((visibility("default"))) __attribute__((weak))
void ANativeActivity_onCreate(ANativeActivity *activity,
                              void *savedState, size_t savedStateSize)
{
    (void)savedState; (void)savedStateSize;
    fprintf(stderr, "[aine-android] ANativeActivity_onCreate (stub): "
            "no native implementation registered\n");
    if (activity && activity->callbacks)
        memset(activity->callbacks, 0, sizeof(ANativeActivityCallbacks));
}
