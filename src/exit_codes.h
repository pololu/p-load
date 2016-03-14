#pragma once

#include "p-load.h"

// Process exit codes:
#define PLOAD_ERROR_BAD_ARGS 1
#define PLOAD_ERROR_OPERATION_FAILED 2
#define PLOAD_ERROR_DEVICE_NOT_FOUND 3
#define PLOAD_ERROR_DEVICE_MULTIPLE_FOUND 4

class ExceptionWithExitCode : public std::exception
{
public:
    explicit ExceptionWithExitCode(uint8_t code, std::string message)
        : code(code), msg(message)
    {
    }

    // This destructor is required in GCC 4.6.
    virtual ~ExceptionWithExitCode() throw() {}

    virtual const char * what() const noexcept
    {
        return msg.c_str();
    }

    std::string message() const
    {
        return msg;
    }

    uint8_t getCode() const noexcept
    {
        return code;
    }

private:
    uint8_t code;
    std::string msg;
};
