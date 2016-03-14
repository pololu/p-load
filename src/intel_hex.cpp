/* intel_hex.cpp:
 * Simple library for reading Intel hex (.ihx or .hex) files.
 */

#include "intel_hex.h"
#include <cassert>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <typeinfo>
#include <stdexcept>

using namespace IntelHex;

static int hexDigitValue(unsigned char c)
{
    if (c >= 'a' && c <= 'f') { return c - 'a' + 10; }
    if (c >= 'A' && c <= 'F') { return c - 'A' + 10; }
    if (c >= '0' && c <= '9') { return c - '0'; }
    return -1;
}

static uint8_t readHexByte(std::istream & s)
{
    std::istream::sentry sentry(s, true);
    if (sentry)
    {
        char c1 = 0, c2 = 0;
        s.get(c1);
        s.get(c2);

        if (s.fail())
        {
            if (s.eof())
            {
                throw std::runtime_error("Unexpected end of line.");
            }
            else
            {
                throw std::runtime_error("Failed to read hex digit.");
            }
        }

        int v1 = hexDigitValue(c1);
        int v2 = hexDigitValue(c2);

        if (v1 < 0 || v2 < 0)
        {
            throw std::runtime_error("Invalid hex digit.");
        }

        return v1 * 16 + v2;
    }
    return 0;
}

static uint16_t readHexShort(std::istream & s)
{
    uint16_t r = readHexByte(s) << 8;
    return r + readHexByte(s);
}

// Returns true if the line indicates the HEX file is done.
static void processLine(std::istream & file,
    std::vector<IntelHex::Entry> & entries,
    uint16_t & addressHigh,
    bool & done)
{
    std::string lineString;
    std::getline(file, lineString);
    if (file.fail())
    {
        if (file.eof())
        {
            throw std::runtime_error("Unexpected end of file.");
        }
        else
        {
            throw std::runtime_error("Failed to read HEX file line.");
        }
    }

    std::istringstream lineStream(lineString);

    char start;
    lineStream.get(start);
    if (start != ':')
    {
        throw std::runtime_error("Hex line does not start with colon (:).");
    }

    // Read the indentifying information of the line.
    uint8_t byteCount = readHexByte(lineStream);
    uint16_t addressLow = readHexShort(lineStream);
    uint8_t recordType = readHexByte(lineStream);

    // Read the data
    std::vector<uint8_t> data(byteCount);
    for(uint32_t i = 0; i < byteCount; i++)
    {
        data[i] = readHexByte(lineStream);
    }

    // Read the checksum.
    uint8_t checksum = readHexByte(lineStream);

    // Check the checksum.
    uint8_t sum = byteCount + (addressLow & 0xFF) + (addressLow >> 8) + recordType;
    for(uint32_t i = 0; i < byteCount; i++)
    {
        sum += data[i];
    }
    uint8_t expected_checksum = -sum;
    if (checksum != expected_checksum)
    {
        std::ostringstream message;
        message << std::hex << std::uppercase << std::setfill('0') << std::right;
        message << "Incorrect checksum, expected \"";
        message << std::setw(2) << (unsigned int)expected_checksum;
        message << "\".";
        throw std::runtime_error(message.str());
    }

    // Check for extra stuff at the end of the line.
    if (lineStream.get() != EOF)
    {
        throw std::runtime_error("Extra data after checksum.");
    }

    switch(recordType)
    {
    default:
        throw std::runtime_error("Unrecognized record type.");

    case 4:  // Extended Linear Address Record (sets high 16 bits)
        if (byteCount != 2)
        {
            throw std::runtime_error("Extended Linear Address record has "
                "wrong number of bytes (expected 2).");
        }
        addressHigh = (data[0] << 8) + data[1];
        break;

    case 2:  // Extended Segment Address Record (basically sets bits 4-20 of the address)
    case 5:  // Start Linear Address Record (sets a 32-bit address)
        throw std::runtime_error("Unimplemented record type.");

    case 0:  // Data record
    {
        uint32_t address = addressLow + (addressHigh << 16);
        entries.push_back(Entry(address, data));
        break;
    }

    case 3: // Start Segment Address Record (specific to 80x86 processors)
        // Ignore this type.
        break;

    case 1: // End of File record
        done = true;
        break;
    }
}

void IntelHex::Data::readFromFile(std::istream & file,
    const char * fileName, uint32_t * lineNumber)
{
    // Assume the high 16 bits of the address are zero initially.
    uint16_t addressHigh = 0;

    assert(fileName != NULL);

    uint32_t internalLineNumber = 0;
    if (lineNumber == NULL)
    {
        lineNumber = &internalLineNumber;
    }

    try
    {
        while(1)
        {
            (*lineNumber)++;
            bool done = false;
            processLine(file, entries, addressHigh, done);
            if (done) { break; }
        }
    }
    catch(const std::runtime_error & e)
    {
        throw std::runtime_error(
            std::string(fileName) + ":" +
            std::to_string(*lineNumber) + ": " +
            std::string(e.what()));
    }
}

std::vector<uint8_t> IntelHex::Data::getImage(uint32_t startAddress, uint32_t size) const
{
    // Initialize the image to have all bytes set to 0xFF.
    std::vector<uint8_t> image(size, 0xFF);

    for (const Entry & entry : entries)
    {
        uint32_t start = std::max(startAddress, entry.address);
        uint32_t end = std::min(startAddress + size,
          (uint32_t)(entry.address + entry.data.size()));
        for(uint32_t a = start; a < end; a++)
        {
            image[a - startAddress] = entry.data[a - entry.address];
        }
    }

    return image;
}

void IntelHex::Data::setImage(uint32_t startAddress,
    std::vector<uint8_t> image, const uint32_t blockSize)
{
    const uint32_t endAddress = startAddress + image.size();

    // The next address to write to.
    uint32_t address = startAddress;

    while(address < endAddress)
    {
        uint32_t entrySize = blockSize;
        if (address + entrySize > endAddress)
        {
            entrySize = endAddress - address;
        }

        std::vector<uint8_t> data(entrySize);
        for (uint32_t i = 0; i < entrySize; i++)
        {
            data[i] = image[address - startAddress + i];
        }

        entries.push_back(Entry(address, data));
        address += entrySize;
    }
}

static void writeHexLine(std::ostream & file, uint8_t recordType,
    uint16_t addressLow, const std::vector<uint8_t> & data)
{
    assert(data.size() <= 0xFF);

    file << ':';
    file << std::setw(2) << data.size();
    file << std::setw(4) << addressLow;
    file << std::setw(2) << (unsigned int)recordType;

    uint8_t sum = data.size() + (addressLow >> 8) + addressLow + recordType;
    for(uint8_t dataByte : data)
    {
        file << std::setw(2) << (unsigned int)dataByte;
        sum += dataByte;
    }

    uint8_t checksum = -sum;
    file << std::setw(2) << (unsigned int)checksum;

    file << std::endl;
}

void IntelHex::Data::writeToFile(std::ostream & file) const
{
    uint32_t lastAddress = 0;

    file << std::uppercase;
    file << std::hex << std::setfill('0') << std::right;

    for(const Entry & entry : entries)
    {
        uint32_t address = entry.address;

        if ((address >> 16) != (lastAddress >> 16))
        {
            // Emit an extended linear address record because the high 16 bits changed.
            writeHexLine(file, 4, 0, {(uint8_t)(address >> 24), (uint8_t)(address >> 16)});
        }

        writeHexLine(file, 0, address & 0xFFFF, entry.data);

        lastAddress = address;
    }
    writeHexLine(file, 1, 0, {});  // End of file.
}
