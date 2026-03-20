// AINE: src/aine-shim/common/logging.c
// Sistema de logging unificado — funciona igual en macOS y Linux

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>

typedef enum {
    AINE_LOG_ERROR = 0,
    AINE_LOG_WARN  = 1,
    AINE_LOG_INFO  = 2,
    AINE_LOG_DEBUG = 3,
} aine_log_level_t;

static aine_log_level_t g_log_level = AINE_LOG_INFO;

__attribute__((constructor))
static void aine_logging_init(void) {
    const char *level = getenv("AINE_LOG_LEVEL");
    if (!level) return;
    if (strcmp(level, "debug") == 0) g_log_level = AINE_LOG_DEBUG;
    else if (strcmp(level, "info")  == 0) g_log_level = AINE_LOG_INFO;
    else if (strcmp(level, "warn")  == 0) g_log_level = AINE_LOG_WARN;
    else if (strcmp(level, "error") == 0) g_log_level = AINE_LOG_ERROR;
}

void aine_log(aine_log_level_t level, const char *tag, const char *fmt, ...) {
    if (level > g_log_level) return;

    const char *prefix[] = { "ERROR", "WARN ", "INFO ", "DEBUG" };
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);

    fprintf(stderr, "[AINE/%.5s][%7.3f][%s] ",
            prefix[level],
            ts.tv_sec + ts.tv_nsec / 1e9,
            tag ? tag : "shim");

    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fprintf(stderr, "\n");
}

#define AINE_LOGD(tag, ...) aine_log(AINE_LOG_DEBUG, tag, __VA_ARGS__)
#define AINE_LOGI(tag, ...) aine_log(AINE_LOG_INFO,  tag, __VA_ARGS__)
#define AINE_LOGW(tag, ...) aine_log(AINE_LOG_WARN,  tag, __VA_ARGS__)
#define AINE_LOGE(tag, ...) aine_log(AINE_LOG_ERROR, tag, __VA_ARGS__)
