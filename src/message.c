#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#else
#include <unistd.h>
#endif

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "message.h"

#ifdef _MSC_VER
#include <io.h>
#define isatty _isatty
#define fileno _fileno
#endif

static bool currentLineHasBar = false;
static int currentBarLength;
static char currentMessage[500] = "";

void startNewLine()
{
    if (currentLineHasBar)
    {
        putchar('\n');
    }
    currentLineHasBar = false;
    currentBarLength = -1;
    currentMessage[0] = 0;
}

void error(const char * format, ...)
{
    startNewLine();
    va_list ap;
    va_start(ap, format);
    fprintf(stderr, "Error: ");
    vfprintf(stderr, format, ap);
    fprintf(stderr, "\n");
    va_end(ap);
}

void info(const char * format, ...)
{
    if (!isatty(fileno(stdout)))
    {
        return;
    }

    startNewLine();
    va_list ap;
    va_start(ap, format);
    vfprintf(stdout, format, ap);
    fprintf(stdout, "\n");
    va_end(ap);
}

void statusCallback(const char * status, uint32_t progress, uint32_t maxProgress)
{
    if (!isatty(fileno(stdout)))
    {
        return;
    }

    if (strcmp(status, currentMessage))
    {
        startNewLine();
        strcpy(&currentMessage[0], status);
        puts(currentMessage);
    }

    if (maxProgress)
    {
        const uint32_t barWidth = 58;
        uint32_t scaledProgress = progress * barWidth / maxProgress;

        if (currentBarLength != scaledProgress)
        {
            uint32_t i;
            printf("\rProgress: |");
            for (i = 0; i < scaledProgress; i++){ putchar('#'); }
            for (; i < barWidth; i++){ putchar(' '); }
            putchar('|');
            currentLineHasBar = true;
            currentBarLength = scaledProgress;
            if (progress == maxProgress)
            {
                printf(" Done.");
                startNewLine();
            }
            fflush(stdout);
        }
    }
}
