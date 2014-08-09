#ifndef _MESSAGE_H
#define _MESSAGE_H

#include <stdint.h>

#ifdef __GNUC__
#define PRINTF_ATTR(s, f) __attribute__((format (printf, s, f)))
#else
#define PRINTF_ATTR(s, f)
#endif

void error(const char * format, ...) PRINTF_ATTR(1, 2);
void info(const char * format, ...) PRINTF_ATTR(1, 2);
void statusCallback(const char * status, uint32_t progress, uint32_t maxProgress);

#endif
