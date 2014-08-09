#ifndef _EXIT_CODES
#define _EXIT_CODES

// Process exit codes:
typedef enum ExitCode
{
    // EXIT_SUCCESS (0) means success
    ERROR_BAD_ARGS = 1,
    ERROR_OPERATION_FAILED = 2,
    ERROR_BOOTLOADER_NOT_FOUND = 3,
} ExitCode;

#endif
