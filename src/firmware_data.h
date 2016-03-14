#pragma once

#include "p-load.h"
#include "intel_hex.h"
#include "ploader.h"
#include "firmware_archive.h"

// FirmwareData abstracts away the differences between different types of
// firmware files and how they are written to the bootloader.  It also knows how
// to detect which type of file has been given.
class FirmwareData
{
public:
    void readFromFile(const char * fileName);

    /** Raises an exception if the specified memory sets from this data
     * cannot be written to the specified type of bootloader. */
    void ensureBootloaderCompatibility(const PloaderType &, MemorySet) const;

    void writeToBootloader(PloaderHandle &, MemorySet) const;

    operator bool() const;

    IntelHex::Data hexData;
    FirmwareArchive::Data firmwareArchiveData;
};
