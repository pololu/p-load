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

#include "actions.h"
#include "arg_reader.h"
#include "message.h"
#include "exit_codes.h"

actionInstance * actions;
unsigned int actionsCount;
unsigned int actionsCapacity;

static const actionType * findActionTypeByName(const actionType * types, const char * name)
{
    for(const actionType * type = types; type->name; type++)
    {
        if (strcmp(type->name, name) == 0)
        {
            return type;
        }
    }
    return NULL;  // not found
}

ExitCode actionsCreateFromArgs(const actionType * types)
{
    actionsCount = 0;
    actionsCapacity = 0;
    actions = NULL;

    const char * arg;
    while(1)
    {
        arg = argReaderNext();

        if (arg == NULL)
        {
            break;  // Done reading arguments.
        }

        const actionType * type = findActionTypeByName(types, arg);
        if (type == NULL)
        {
            error("Unknown option: %s", arg);
            return ERROR_BAD_ARGS;
        }

        // Allocate memory for the new action in the list.
        if (actionsCount >= actionsCapacity)
        {
            actionsCapacity += 10;
            actions = realloc(actions, sizeof(actionInstance) * actionsCapacity);
            if (actions == NULL)
            {
                error("Could not allocate memory for option list.");
            }
        }

        actionInstance * action = &actions[actionsCount++];
        action->type = type;

        // By default, set data to NULL so we can call the
        // free function and get defined behavior even if the
        // allocate functions have not been called.
        action->data = NULL;

        // Call the allocator (for actions that store additional data).
        ExitCode exitCode = action->type->allocate(&action->data);
        if (exitCode) { return exitCode; }

        // Call the parser, which might read additional arguments.
        exitCode = action->type->parse(action->data);
        if (exitCode) { return exitCode; }
    }
    return EXIT_SUCCESS;
}

ExitCode actionsSort()
{
    // In-place bubble sort.
    while(true)
    {
        bool swapPerformed = false;

        // Iterate over each consecutive pair of actions.
        for (unsigned int i = 0; i < actionsCount - 1; i++)
        {
            actionInstance * action1 = &actions[i];
            actionInstance * action2 = &actions[i + 1];
            if(action1->type->sortOrder > action2->type->sortOrder)
            {
                // Actions are out of order, so swap them.
                actionInstance tmp = *action1;
                *action1 = *action2;
                *action2 = tmp;
                swapPerformed = true;
            }
        }

        if (!swapPerformed)
        {
            break;
        }
    }

#ifdef _DEBUG
    printf("Action order: ");
    for(unsigned int i = 0; i < actionsCount; i++)
    {
        printf("%s ", actions[i].type->name);
    }
    printf("\n");
#endif
    return EXIT_SUCCESS;
}

ExitCode actionsPrepare()
{
    for (unsigned int i = 0; i < actionsCount; i++)
    {
        actionInstance * action = &actions[i];
        ExitCode exitCode = action->type->prepare(action->data);
        if (exitCode) { return exitCode; }
    }
    return EXIT_SUCCESS;
}

ExitCode actionsExecute()
{
    for(unsigned int i = 0; i < actionsCount; i++)
    {
        actionInstance * action = &actions[i];
        ExitCode exitCode = action->type->execute(action->data);
        if (exitCode) { return exitCode; }
    }
    return EXIT_SUCCESS;
}

void actionsFree()
{
    for(unsigned int i = 0; i < actionsCount; i++)
    {
        actionInstance * action = &actions[i];
        action->type->freeFunc(action->data);
    }
    free(actions);
}

ExitCode allocateNothing(void ** data)
{
    return EXIT_SUCCESS;
}

ExitCode parseNothing(void * data)
{
    return EXIT_SUCCESS;
}

ExitCode prepareNothing(void * data)
{
    return EXIT_SUCCESS;
}

ExitCode executeNothing(void * data)
{
    return EXIT_SUCCESS;
}

void freeNothing(void * data)
{
}
