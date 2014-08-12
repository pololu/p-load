// For Debug builds in Windows, enable memory leak tracking.
#if defined(_MSC_VER) && defined(_DEBUG)
#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>
#endif

#ifdef _DEBUG
#include <stdio.h>
#endif

#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>

#include "actions.h"
#include "arg_reader.h"
#include "message.h"
#include "exit_codes.h"

actionInstance * actions = NULL;
unsigned int actionsCount = 0;
unsigned int actionsCapacity = 0;

ExitCode actionsAdd(const actionType * type)
{
    assert(type != NULL);

    // Allocate memory for the new action in the list.
    if (actionsCount >= actionsCapacity)
    {
        actionsCapacity += 10;
        actions = realloc(actions, sizeof(actionInstance) * actionsCapacity);
        if (actions == NULL)
        {
            error("Could not allocate memory for option list.");
            return ERROR_OPERATION_FAILED;
        }
    }

    actionInstance * action = &actions[actionsCount++];
    action->type = type;

    // By default, set data to NULL so we can call the
    // free function and get defined behavior even if the
    // allocate functions have not been called.
    action->data = NULL;

    // Call the allocator (for actions that store additional data).
    if (action->type->allocate != NULL)
    {
        ExitCode exitCode = action->type->allocate(&action->data);
        if (exitCode) { return exitCode; }
    }

    // Call the parser, which might read additional arguments.
    if (action->type->parse != NULL)
    {
        ExitCode exitCode = action->type->parse(action->data);
        if (exitCode) { return exitCode; }
    }

    return EXIT_SUCCESS;
}

ExitCode actionsPrepare()
{
    for (unsigned int i = 0; i < actionsCount; i++)
    {
        actionInstance * action = &actions[i];
        if (action->type->prepare != NULL)
        {
            ExitCode exitCode = action->type->prepare(action->data);
            if (exitCode) { return exitCode; }
        }
    }
    return EXIT_SUCCESS;
}

ExitCode actionsExecute()
{
    for(unsigned int i = 0; i < actionsCount; i++)
    {
        actionInstance * action = &actions[i];
        if (action->type->execute != NULL)
        {
            ExitCode exitCode = action->type->execute(action->data);
            if (exitCode) { return exitCode; }
        }
    }
    return EXIT_SUCCESS;
}

void actionsFree()
{
    for(unsigned int i = 0; i < actionsCount; i++)
    {
        actionInstance * action = &actions[i];
        if (action->type->freeFunc != NULL)
        {
            action->type->freeFunc(action->data);
        }
    }
    free(actions);
}
