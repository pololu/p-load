#include "file_utils.h"
#include <stdexcept>

namespace
{
    struct noop {
        void operator()(std::ios_base *) const noexcept { }
    };
}

std::shared_ptr<std::istream> openFileOrPipeInput(std::string fileName)
{
    std::shared_ptr<std::istream> file;
    if (fileName == "-")
    {
        file.reset(&std::cin, noop());
    }
    else
    {
        std::ifstream * diskFile = new std::ifstream();
        file.reset(diskFile);
        diskFile->open(fileName);
        if (!*diskFile)
        {
            int error_code = errno;
            throw std::runtime_error(fileName + ": " + strerror(error_code) + ".");
        }
    }
    return file;
}

std::shared_ptr<std::ostream> openFileOrPipeOutput(std::string fileName)
{
    std::shared_ptr<std::ostream> file;
    if (fileName == "-")
    {
        file.reset(&std::cout, noop());
    }
    else
    {
        std::ofstream * diskFile = new std::ofstream();
        file.reset(diskFile);
        diskFile->open(fileName);
        if (!*diskFile)
        {
            int error_code = errno;
            throw std::runtime_error(fileName + ": " + strerror(error_code) + ".");
        }
    }
    return file;
}
