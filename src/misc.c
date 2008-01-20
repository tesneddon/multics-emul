#include "hw6180.h"
#include <stdio.h>
#include <stdarg.h>

extern DEVICE cpu_dev;

static void msg(const char* tag, const char *who, const char* format, va_list ap);

void debug_msg(const char* who, const char* format, ...)
{
    if (cpu_dev.dctrl == 0)     // todo: should CPU control all debug settings?
        return;
    va_list ap;
    va_start(ap, format);
    msg("Debug", who, format, ap);
    va_end(ap);
}

void warn_msg(const char* who, const char* format, ...)
{
    va_list ap;
    va_start(ap, format);
    msg("WARNING", who, format, ap);
    va_end(ap);
}

void complain_msg(const char* who, const char* format, ...)
{
    va_list ap;
    va_start(ap, format);
    msg("ERROR", who, format, ap);
    va_end(ap);
}


void out_msg(const char* format, ...)
{
    va_list ap;
    va_start(ap, format);

    fflush(stdout);
    vprintf(format, ap);
    if (*(format + strlen(format) - 1) == '\n') {
        printf("\r");
    }
    fflush(stdout);
}

static void msg(const char* tag, const char *who, const char* format, va_list ap)
{
    fflush(stdout);
    //printf("%s:%*s %s: ", who, 15-strlen(who), "", tag);
    printf("%s: %*s %s: %*s", tag, 7-strlen(tag), "", who, 18-strlen(who), "");
    vprintf(format, ap);
    if (*(format + strlen(format) - 1) == '\n') {
        printf("\r");
    }
    fflush(stdout);
}