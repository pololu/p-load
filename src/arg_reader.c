#include <stddef.h>
#include "arg_reader.h"

static int argc;
static char ** argv;
static int index;

void argReaderInit(int new_argc, char ** new_argv)
{
    argc = new_argc;
    argv = new_argv;
    index = 0;
}

const char * argReaderNext()
{
    if (index < argc)
    {
        index++;
        return argv[index];
    }
    else
    {
        return NULL;
    }
}

const char * argReaderLast()
{
    if (index > 0)
    {
        return argv[index - 1];
    }
    else
    {
        return NULL;
    }
}

void argReaderRewind()
{
    if (index > 0)
    {
        index--;
    }
}
