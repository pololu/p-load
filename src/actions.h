#ifndef _ACTIONS_H
#define _ACTIONS_H

#include "exit_codes.h"

typedef enum ACTION_TYPE_ID ACTION_TYPE_ID;

typedef struct actionType {
    const char * name;

    unsigned int sortOrder;

    // Prepares any memory that the action needs to hold, or just
    // returns NULL if it doesn't need any.
    ExitCode (*allocate)(void **);

    // Parses arguments and stores anything it will need for later.
    // Returns an exit code.  This code should not open any handles
    // to external devices.
    ExitCode (*parse)(void *);

    // Reads any inputs files.  This code can open a handle to an external
    // device but should not perform any I/O.
    ExitCode (*prepare)(void *);

    // Actually executes the action.
    ExitCode (*execute)(void *);

    // Frees anything allocated by any of the other functions.
    // Can't use 'free' as the name because in Debug builds on Windows that will get defined
    // as a preprocessor macro.
    void (*freeFunc)(void *);

} actionType;

typedef struct actionInstance {
    const actionType * type;
    void * data;
} actionInstance;

ExitCode actionsCreateFromArgs(const actionType * types);
ExitCode actionsSort();
ExitCode actionsPrepare();
ExitCode actionsExecute();
void actionsFree();

ExitCode allocateNothing(void ** data);
ExitCode parseNothing(void * data);
ExitCode prepareNothing(void * data);
ExitCode executeNothing(void * data);
void freeNothing(void * data);

#endif
