#pragma once

#include <string>
#include <vector>
#include <iostream>
#include <cstdint>

namespace IntelHex
{
    class Entry
    {
    public:
        Entry(uint32_t address, std::vector<uint8_t> data)
            : address(address), data(data)
        {
        }

        uint32_t address;
        std::vector<uint8_t> data;
    };

    class Data
    {
    public:
        void readFromFile(std::istream & file, const char * fileName,
            uint32_t * lineNumber = NULL);

        void writeToFile(std::ostream & file) const;

        std::vector<uint8_t> getImage(uint32_t startAddress, uint32_t size) const;

        void setImage(uint32_t startAddress, std::vector<uint8_t> image,
            uint32_t blockSize = 16);

        operator bool() const
        {
            return !entries.empty();
        }

    private:
        std::vector<Entry> entries;
    };

}
