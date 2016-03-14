#include "p-load.h"

// This function should never get called.  If it is called, it's a bug.
static void noDataError()
{
    assert(0);
    throw std::runtime_error("FirmwareData object has no data.");
}

FirmwareData::operator bool() const
{
    return hexData || firmwareArchiveData;
}

void FirmwareData::readFromFile(const char * fileName)
{
    assert(!*this);

    std::string fileNameStr(fileName);

    auto filePtr = openFileOrPipeInput(fileNameStr);
    std::istream & file = *filePtr;

    // Get the first character so we can figure out what kind of file this is.
    char first_char;
    file.get(first_char);
    if (file.fail())
    {
        throw std::runtime_error(fileNameStr + ": Failed to read first character.");
    }

    // Put that character back in the buffer.
    file.unget();
    if (file.fail())
    {
        throw std::runtime_error(fileNameStr + ": Failed to unget character from file.");
    }

    if (first_char == ':')
    {
        hexData.readFromFile(file, fileName);
    }
    else
    {
        firmwareArchiveData.readFromFile(file, fileName);
    }

    if (!*this)
    {
        throw std::runtime_error(fileNameStr + ": file contains no firmware data.");
    }
}

void FirmwareData::ensureBootloaderCompatibility(const PloaderType & type,
    MemorySet memorySet) const
{
    if (hexData)
    {
        if (type.memorySetIncludesFlash(memorySet))
        {
            type.ensureFlashPlainWriting();
        }

        if (type.memorySetIncludesEeprom(memorySet))
        {
            type.ensureEepromAccess();
        }
    }
    else if (firmwareArchiveData)
    {
        if (!firmwareArchiveData.matchesBootloader(type.usbVendorId, type.usbProductId))
        {
            throw std::runtime_error(
                "The firmware file does not match the selected bootloader.");
        }

        if (memorySet != MEMORY_SET_ALL)
        {
            throw std::runtime_error(
                "FMI files do not support writing to a specific memory.");
        }
    }
    else
    {
        noDataError();
    }
}

void FirmwareData::writeToBootloader(PloaderHandle & handle,
    MemorySet memorySet) const
{
    const PloaderType & type = handle.type;

    if (hexData)
    {
        // EEPROM is written before flash to ensure there is no risk of running
        // the application (either the old one or the new one) with the wrong
        // values in EEPROM.

        if (type.memorySetIncludesFlash(memorySet))
        {
            handle.initialize(UPLOAD_TYPE_PLAIN);
            handle.eraseFlash();
        }

        if (type.memorySetIncludesEeprom(memorySet))
        {
            MemoryImage eeprom = hexData.getImage(type.eepromAddressHexFile, type.eepromSize);
            handle.writeEeprom(&eeprom[0]);
        }

        if (type.memorySetIncludesFlash(memorySet))
        {
            MemoryImage flash = hexData.getImage(type.appAddress, type.appSize);
            handle.writeFlash(&flash[0]);
        }
    }
    else if (firmwareArchiveData)
    {
        const FirmwareArchive::Image & image = firmwareArchiveData.findImage(
            type.usbVendorId, type.usbProductId);
        handle.applyImage(image);
    }
    else
    {
        noDataError();
    }
}
