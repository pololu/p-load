#ifndef _ARG_READER_H
#define _ARG_READER_H

void argReaderInit(int argc, char * argv[]);

// Advance and return the current argument, or null if we have reached the end.
// The first call of this returns argv[1] because argv[0] is just the program name.
const char * argReaderNext();

// Return the argument before the current one, or NULL.
const char * argReaderLast();
void argReaderRewind();

#endif
