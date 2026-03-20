/*
 * aine-hals/liblog/log.c — Android liblog stub for macOS
 *
 * Implements android/log.h: __android_log_print, __android_log_write,
 * __android_log_vprint redirecting to stderr with priority labels.
 *
 * This dylib is loaded in place of /system/lib64/liblog.so via aine-loader.
 */
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/*
 * Android log priorities (android/log.h)
 */
typedef enum {
    ANDROID_LOG_UNKNOWN = 0,
    ANDROID_LOG_DEFAULT,
    ANDROID_LOG_VERBOSE,
    ANDROID_LOG_DEBUG,
    ANDROID_LOG_INFO,
    ANDROID_LOG_WARN,
    ANDROID_LOG_ERROR,
    ANDROID_LOG_FATAL,
    ANDROID_LOG_SILENT,
} android_LogPriority;

static const char *priority_char(int prio)
{
    switch (prio) {
    case ANDROID_LOG_VERBOSE: return "V";
    case ANDROID_LOG_DEBUG:   return "D";
    case ANDROID_LOG_INFO:    return "I";
    case ANDROID_LOG_WARN:    return "W";
    case ANDROID_LOG_ERROR:   return "E";
    case ANDROID_LOG_FATAL:   return "F";
    default:                  return "?";
    }
}

/* __android_log_vprint — varargs form */
int __android_log_vprint(int prio, const char *tag, const char *fmt, va_list ap)
{
    fprintf(stderr, "[%s/%s] ", priority_char(prio), tag ? tag : "?");
    int n = vfprintf(stderr, fmt, ap);
    /* ensure newline */
    if (fmt && fmt[0] && fmt[strlen(fmt) - 1] != '\n')
        fputc('\n', stderr);
    return n;
}

/* __android_log_print — printf-style */
__attribute__((visibility("default")))
int __android_log_print(int prio, const char *tag, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int n = __android_log_vprint(prio, tag, fmt, ap);
    va_end(ap);
    return n;
}

/* __android_log_write — simple string */
__attribute__((visibility("default")))
int __android_log_write(int prio, const char *tag, const char *text)
{
    return __android_log_print(prio, tag, "%s", text ? text : "(null)");
}

/* __android_log_buf_write — write to a named log buffer (ignore buffer id) */
__attribute__((visibility("default")))
int __android_log_buf_write(int bufID, int prio, const char *tag, const char *text)
{
    (void)bufID;
    return __android_log_write(prio, tag, text);
}

/* __android_log_buf_print — print to a named log buffer */
__attribute__((visibility("default")))
int __android_log_buf_print(int bufID, int prio, const char *tag, const char *fmt, ...)
{
    (void)bufID;
    va_list ap;
    va_start(ap, fmt);
    int n = __android_log_vprint(prio, tag, fmt, ap);
    va_end(ap);
    return n;
}

/* __android_log_assert — logs a fatal message and aborts */
__attribute__((visibility("default"))) __attribute__((noreturn))
void __android_log_assert(const char *cond, const char *tag,
                          const char *fmt, ...)
{
    va_list ap;
    fprintf(stderr, "[F/%s] ASSERTION FAILED: %s\n", tag ? tag : "?",
            cond ? cond : "");
    if (fmt && fmt[0]) {
        va_start(ap, fmt);
        vfprintf(stderr, fmt, ap);
        va_end(ap);
        fputc('\n', stderr);
    }
    __builtin_trap();
}
