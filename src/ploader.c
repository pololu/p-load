/* This file uses the usb_system code to talk to the bootloaders, and
 * provides a higher-level interface to application code that hides as
 * many differences as possible between the different bootloaders.
 */

#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#endif

// For Debug builds in Windows, enable memory leak tracking.
#if defined(_MSC_VER) && defined(_DEBUG)
#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>
#endif

#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include "ploader.h"
#include "message.h"

// Request codes used to talk to the bootloader.
#define REQUEST_INITIALIZE         0x80
#define REQUEST_ERASE_FLASH        0x81
#define REQUEST_WRITE_FLASH_BLOCK  0x82
#define REQUEST_GET_LAST_ERROR     0x83
#define REQUEST_CHECK_APPLICATION  0x84
#define REQUEST_READ_FLASH         0x86
#define REQUEST_SET_DEVICE_CODE    0x87
#define REQUEST_READ_EEPROM        0x88
#define REQUEST_WRITE_EEPROM       0x89
#define REQUEST_RESTART            0xFE

#define DEVICE_CODE_SIZE           16

//static const GUID wixelBootloaderGuid = { 0x8c278fa6, 0xa2fc, 0x4a75, { 0xbc, 0xa0, 0x7d, 0x10, 0xdd, 0x90, 0x15, 0xfd } };
static const GUID nativeUsbBootloaderGuid = { 0x82959cfa, 0x7a2d, 0x431f, { 0xa9, 0xa1, 0x50, 0x0b, 0x55, 0xd9, 0x09, 0x50 } };

// This table holds information about all the supported bootloaders.
// This allows us to detect the bootloaders and know the details of
// communicating with them.
const ploaderProperties * ploaderTable = (const ploaderProperties[])
{
    {
        .idVendor = 0x1FFB,
        .idProduct = 0x0102,
        .name = "Pololu P-Star 25K50 Bootloader",
        .appAddress = 0x2000,
        .appSize = 0x6000,
        .writeBlockSize = 0x40,
        .supportsReadingFlash = true,
        .eepromAddress = 0,
        .eepromAddressHexFile = 0xF00000,
        .eepromSize = 0x100,
        .supportsEepromAccess = true,
        .deviceCode = NULL,
        .deviceInterfaceGuid = &nativeUsbBootloaderGuid,
    },
    { 0 },  // terminating entry
};

struct ploaderList
{
    usbDeviceList * deviceList;
};

struct ploaderHandle
{
    usbDevice * device;
    const ploaderProperties * properties;
    ploaderInfo * info;
};

static const ploaderProperties * ploaderTableLookup(uint16_t idVendor, uint16_t idProduct)
{
    for (const ploaderProperties * prop = ploaderTable; prop->name; prop++)
    {
        if (prop->idVendor == idVendor && prop->idProduct == idProduct)
        {
            return prop;
        }
    }
    return NULL;
}

PLOADER_RESULT ploaderListCreate(ploaderList ** list)
{
    USB_RESULT usbResult;
    usbDeviceList * usbList;

    assert(list != NULL);

    *list = NULL;  // By default, return a null pointer.

    // Get a list of all USB devices.
    usbResult = usbListCreate(&usbList);
    if (usbResult)
    {
        return usbResult;
    }

    // Filter out things that are not bootloaders.
    for (uint32_t i = 0; i < usbListSize(usbList); i++)
    {
        usbDeviceInfo usbInfo;
        usbResult = usbListGetDeviceInfo(usbList, i, &usbInfo);
        if (usbResult || !ploaderTableLookup(usbInfo.idVendor, usbInfo.idProduct))
        {
            // This is not a bootloader, so remove it from the list.
            usbListRemove(usbList, i);
            i--;
        }
    }

    // Allocate memory for the list.
    uint32_t size = usbListSize(usbList);
    ploaderList * newList = malloc(sizeof(ploaderList));
    if (newList == NULL)
    {
        error("Failed to allocate memory for bootloader list.");
        free(newList);
        usbListFree(usbList);
        return USB_ERROR_NO_MEM;
    }

    newList->deviceList = usbList;

    *list = newList;
    return PLOADER_SUCCESS;
}

uint32_t ploaderListSize(const ploaderList * list)
{
    assert(list != NULL);
    return usbListSize(list->deviceList);
}

PLOADER_RESULT ploaderListFilterBySerialNumber(ploaderList * list, const char * serialNumber)
{
    // Since we are not storing any extra metadata about entries in the list, we can
    // simply do this:
    assert(list != NULL);
    assert(serialNumber != NULL);
    return usbListFilterBySerialNumber(list->deviceList, serialNumber);
}

PLOADER_RESULT ploaderListCreateInfo(const ploaderList * list, uint32_t index, ploaderInfo ** info)
{
    USB_RESULT usbResult;

    assert(list != NULL);
    assert(info != NULL);

    // By default, return a null pointer.
    *info = NULL;

    // Make sure the index is valid.
    if (index >= usbListSize(list->deviceList))
    {
        error("Invalid index %d of %d.", index, usbListSize(list->deviceList));
        return USB_ERROR_OUT_OF_RANGE;
    }

    ploaderInfo * newInfo = malloc(sizeof(ploaderInfo));
    if (info == NULL)
    {
        error("Failed to allocate memory for bootloader info.");
        return USB_ERROR_NO_MEM;
    }

    // Get the USB vendor and product IDs.
    usbDeviceInfo usbInfo;
    usbResult = usbListGetDeviceInfo(list->deviceList, index, &usbInfo);
    if (usbResult)
    {
        free(newInfo);
        return usbResult;
    }
    newInfo->usbProductId = usbInfo.idProduct;
    newInfo->usbVendorId = usbInfo.idVendor;

    // Get the name of this type of bootloader
    const ploaderProperties * properties = ploaderTableLookup(newInfo->usbVendorId, newInfo->usbProductId);
    if (properties == NULL)
    {
        // This should never happen because we already filtered out those entries.
        error("Failed to look up properties for bootloader (%04x:%04x).", newInfo->usbVendorId, newInfo->usbProductId);
        return USB_ERROR_UNEXPECTED;
    }
    newInfo->name = properties->name;
    newInfo->appAddress = properties->appAddress;
    newInfo->appSize = properties->appSize;
    newInfo->eepromAddress = properties->eepromAddress;
    newInfo->eepromAddressHexFile = properties->eepromAddressHexFile;
    newInfo->eepromSize = properties->eepromSize;

    // Get the serial number.
    char * serialNumber = malloc(USB_SERIAL_NUMBER_MAX_SIZE);
    if (serialNumber == NULL)
    {
        error("Failed to allocate memory for serial number in bootloader list.");
        free(newInfo);
        return USB_ERROR_NO_MEM;
    }

    usbResult = usbListGetSerialNumber(list->deviceList, index, serialNumber, USB_SERIAL_NUMBER_MAX_SIZE);
    if (usbResult)
    {
        free(newInfo);
        return usbResult;
    }

    newInfo->serialNumber = serialNumber;

    *info = newInfo;

    return PLOADER_SUCCESS;
}

PLOADER_RESULT ploaderInfoCreateCopy(ploaderInfo ** dest, const ploaderInfo * source)
{
    assert(dest != NULL);
    assert(source != NULL);

    *dest = NULL;

    ploaderInfo * newInfo = malloc(sizeof(ploaderInfo));
    if (newInfo == NULL)
    {
        error("Failed to allocate memory for copying bootloader info.");
        return USB_ERROR_NO_MEM;
    }

    // Make a simple copy for most fields.
    memcpy(newInfo, source, sizeof(ploaderInfo));

    // Make a real copy of the serial number.
    if (source->serialNumber == NULL)
    {
        newInfo->serialNumber = NULL;
    }
    else
    {
        char * newSerialNumber = malloc(strlen(source->serialNumber) + 1);
        if (newSerialNumber == NULL)
        {
            error("Failed to allocate memory for copying bootloader serial number.");
            free(newInfo);
            return USB_ERROR_NO_MEM;
        }
        strcpy(newSerialNumber, source->serialNumber);
        newInfo->serialNumber = newSerialNumber;
    }

    *dest = newInfo;

    return PLOADER_SUCCESS;
}

PLOADER_RESULT ploaderCreateInfo(ploaderHandle * handle, ploaderInfo ** info)
{
    return ploaderInfoCreateCopy(info, handle->info);
}

void ploaderInfoFree(ploaderInfo * info)
{
    if (info != NULL)
    {
        free(info->serialNumber);
        free(info);
    }
}

PLOADER_RESULT ploaderOpenFromList(const ploaderList * list, uint32_t index, ploaderHandle ** handle)
{
    USB_RESULT usbResult;

    *handle = NULL;  // By default, return NULL.

    // Allocate memory
    ploaderHandle * newHandle = malloc(sizeof(ploaderHandle));
    if (newHandle == NULL)
    {
        error("Failed to allocate memory for new bootloader handle.");
        return USB_ERROR_NO_MEM;
    }

    // Store a copy of ploaderInfo because we might need it later.
    PLOADER_RESULT result = ploaderListCreateInfo(list, index, &newHandle->info);
    if (result)
    {
        return result;
    }

    // Get the properties
    const ploaderProperties * ploaderProperties = ploaderTableLookup(newHandle->info->usbVendorId, newHandle->info->usbProductId);
    if (ploaderProperties == NULL)
    {
        free(newHandle);
        error("Cannot find properties for bootloader (%04x:%04x).", newHandle->info->usbVendorId, newHandle->info->usbProductId);
        return USB_ERROR_UNEXPECTED;
    }
    newHandle->properties = ploaderProperties;

    // Open the device
    usbDevice * device;
    usbResult = usbDeviceOpenFromList(list->deviceList, index, &device, ploaderProperties->deviceInterfaceGuid);
    if (usbResult)
    {
        free(newHandle);
        return usbResult;
    }
    newHandle->device = device;

    // Success
    *handle = newHandle;
    return PLOADER_SUCCESS;
}

void ploaderListFree(ploaderList * list)
{
    if (list != NULL)
    {
        usbListFree(list->deviceList);
        free(list);
    }
}

// This can be called after a USB request for writing EEPROM or flash fails.
// It attempts to make another request to get a more specific error code from the device.
// If anything goes wrong, it just returns the original USB error.
PLOADER_RESULT ploaderReportError(ploaderHandle * handle, USB_RESULT usbResult, const char * context)
{
    assert(handle != NULL);

    if (usbResult != USB_ERROR_PIPE)
    {
        // This is an unusual error that was not just caused by a STALL packet,
        // so don't attempt to do anything.  Maybe the device didn't even see
        // the original USB request so the value returned by REQUEST_GET_LAST_ERROR
        // would be meaningless.
        return usbResult;
    }

    uint8_t errorCode;
    uint32_t lengthTransferred;
    USB_RESULT usbResult2 = usbDeviceControlTransfer(handle->device, 0xC0, REQUEST_GET_LAST_ERROR,
            0, 0, &errorCode, 1, &lengthTransferred);
    if (usbResult2 || lengthTransferred != 1)
    {
        // Something went wrong getting a more specific error code.  Just return
        // the original USB error.
        return usbResult;
    }

    error("%s: %s", context, ploaderGetErrorDescription(errorCode));
    return errorCode;
}

PLOADER_RESULT ploaderEraseFlash(ploaderHandle * handle, ploaderStatusCallback statusCallback)
{
    assert(handle != NULL);
    USB_RESULT usbResult;

    uint8_t * deviceCode = (uint8_t *)handle->properties->deviceCode;
    if (deviceCode != NULL)
    {
        usbResult = usbDeviceControlTransfer(handle->device, 0x40, REQUEST_SET_DEVICE_CODE, 0, 0,
            deviceCode, DEVICE_CODE_SIZE, NULL);
        if (usbResult)
        {
            return usbResult;
        }
    }

    usbResult = usbDeviceControlTransfer(handle->device, 0x40, REQUEST_INITIALIZE, 2, 0, NULL, 0, NULL);
    if (usbResult)
    {
        return usbResult;
    }

    int maxProgress = 0;

    while (true)
    {
        uint8_t response[2];
        uint32_t lengthTransferred;
        usbResult = usbDeviceControlTransfer(handle->device, 0xC0, REQUEST_ERASE_FLASH, 0, 0, &response, 2, &lengthTransferred);
        if (usbResult)
        {
            return usbResult;
        }
        if (lengthTransferred != 2)
        {
            error("Expected 2-byte response to Erase Flash request, got %d.", lengthTransferred);
            return USB_ERROR_UNEXPECTED;
        }
        uint8_t errorCode = response[0];
        uint8_t progressLeft = response[1];
        if (errorCode)
        {
            error("Error erasing page: %s\n", ploaderGetErrorDescription(errorCode));
            return errorCode;
        }

        if (maxProgress < progressLeft)
        {
            maxProgress = progressLeft +  1;
        }

        if (statusCallback)
        {
            statusCallback("Erasing flash...", maxProgress - progressLeft, maxProgress);
        }

        if (progressLeft == 0)
        {
            return PLOADER_SUCCESS;
        }
    }
}

PLOADER_RESULT ploaderWriteFlash(ploaderHandle * handle, const uint8_t * image, ploaderStatusCallback statusCallback)
{
    assert(handle != NULL);
    assert(image != NULL);

    PLOADER_RESULT result;

    // First erase the flash.
    result = ploaderEraseFlash(handle, statusCallback);
    if (result)
    {
        return result;
    }

    uint32_t writeBlockSize = handle->properties->writeBlockSize;
    uint32_t appAddress = handle->properties->appAddress;
    uint32_t appSize = handle->properties->appSize;

    uint32_t address = appAddress + appSize;
    while (address > appAddress)
    {
        // Advance to the next block.
        address -= writeBlockSize;
        assert((address % writeBlockSize) == 0);
        const uint8_t * block = &image[address - appAddress];

        // If the block is empty, don't write to it.
        bool blockIsEmpty = true;
        for (uint32_t i = 0; i < writeBlockSize; i++)
        {
            if (block[i] != 0xFF)
            {
                blockIsEmpty = false;
                break;
            }
        }
        if (blockIsEmpty)
        {
            continue;
        }

        // Write the block to flash.
        uint32_t lengthTransferred;
        USB_RESULT usbResult = usbDeviceControlTransfer(handle->device, 0x40, REQUEST_WRITE_FLASH_BLOCK,
            address & 0xFFFF, address >> 16 & 0xFFFF, (uint8_t *)block, writeBlockSize, &lengthTransferred);
        if (usbResult)
        {
            return ploaderReportError(handle, usbResult, "Failed to write flash");
        }
        if (lengthTransferred != writeBlockSize)
        {
            error("Tried to write %d bytes to flash but only sent %d.", writeBlockSize, lengthTransferred);
            return USB_ERROR_UNEXPECTED;
        }

        if (statusCallback)
        {
            // These progress numbers aren't very good because they don't account for how
            // many blocks actually need to be written to flash.
            uint32_t progress = appSize - (address - appAddress);
            statusCallback("Writing flash...", progress, appSize);
        }
    }
    return PLOADER_SUCCESS;
}

PLOADER_RESULT ploaderReadFlash(ploaderHandle * handle, uint8_t * image, ploaderStatusCallback statusCallback)
{
    if (!handle->properties->supportsReadingFlash)
    {
        error("This bootloader does not support reading flash memory.");
        return USB_ERROR_NOT_SUPPORTED;
    }

    const uint32_t appAddress = handle->properties->appAddress;
    const uint32_t appSize = handle->properties->appSize;
    const uint32_t endAddress = appAddress + appSize;

    uint32_t address = appAddress;
    while(address < endAddress)
    {
        const int blockSize = 1024;
        assert(address + blockSize <= endAddress);

        uint32_t lengthTransferred;
        USB_RESULT usbResult = usbDeviceControlTransfer(handle->device, 0xC0, REQUEST_READ_FLASH,
            address & 0xFFFF, address >> 16 & 0xFFFF,
            &image[address - appAddress], blockSize, &lengthTransferred);
        if (usbResult)
        {
            return usbResult;
        }
        if (lengthTransferred != blockSize)
        {
            error("Tried to read %d bytes but got %d.", blockSize, lengthTransferred);
            return USB_ERROR_UNEXPECTED;
        }

        address += blockSize;

        if (statusCallback)
        {
            statusCallback("Reading flash...", address - appAddress, appSize);
        }
    }

    return PLOADER_SUCCESS;
}

PLOADER_RESULT ploaderWriteEeprom(ploaderHandle * handle, const uint8_t * image, ploaderStatusCallback statusCallback)
{
    const uint32_t eepromAddress = handle->properties->eepromAddress;
    const uint32_t eepromSize = handle->properties->eepromSize;
    const uint32_t endAddress = eepromAddress + eepromSize;

    if (eepromSize == 0)
    {
        error("This device does not have EEPROM.");
        return USB_ERROR_NOT_SUPPORTED;
    }
    if (!handle->properties->supportsEepromAccess)
    {
        error("This bootloader does not support accessing EEPROM.");
        return USB_ERROR_NOT_SUPPORTED;
    }

    // Set the message to "Erasing EEPROM..." if the image happens to be all 0xFF.
    // This is less surprising for people who were not intentionally trying to
    // put anything in EEPROM using software that calls this function to erase it.
    const char * message = "Erasing EEPROM...";
    for (uint32_t i = 0; i < eepromSize; i++)
    {
        if (image[i] != 0xFF)
        {
            message = "Writing EEPROM...";
            break;
        }
    }

    uint32_t address = eepromAddress;
    while (address < endAddress)
    {
        const uint32_t blockSize = 32;
        assert(address + blockSize <= endAddress);

        // Write the block to flash.
        uint32_t lengthTransferred;
        USB_RESULT usbResult = usbDeviceControlTransfer(handle->device, 0x40, REQUEST_WRITE_EEPROM,
            address & 0xFFFF, address >> 16 & 0xFFFF, (uint8_t *)&image[address], blockSize, &lengthTransferred);
        if (usbResult)
        {
            return ploaderReportError(handle, usbResult, "Failed to write EEPROM");
        }
        if (lengthTransferred != blockSize)
        {
            error("Tried to write %d bytes to EEPROM but only sent %d.", blockSize, lengthTransferred);
            return USB_ERROR_UNEXPECTED;
        }

        address += blockSize;

        if (statusCallback)
        {
            statusCallback(message, address - eepromAddress, eepromSize);
        }
    }
    return PLOADER_SUCCESS;
}

PLOADER_RESULT ploaderReadEeprom(ploaderHandle * handle, uint8_t * image, ploaderStatusCallback statusCallback)
{
    const uint32_t eepromAddress = handle->properties->eepromAddress;
    const uint32_t eepromSize = handle->properties->eepromSize;
    const uint32_t endAddress = eepromAddress + eepromSize;

    if (eepromSize == 0)
    {
        error("This device does not have EEPROM.");
        return USB_ERROR_NOT_SUPPORTED;
    }
    if (!handle->properties->supportsEepromAccess)
    {
        error("This bootloader does not support accessing EEPROM.");
        return USB_ERROR_NOT_SUPPORTED;
    }

    uint32_t address = eepromAddress;
    while (address < endAddress)
    {
        const uint32_t blockSize = 32;
        assert(address + blockSize <= endAddress);

        uint32_t lengthTransferred;
        USB_RESULT usbResult = usbDeviceControlTransfer(handle->device, 0xC0, REQUEST_READ_EEPROM,
            address & 0xFFFF, address >> 16 & 0xFFFF, &image[address], blockSize, &lengthTransferred);
        if (usbResult)
        {
            return usbResult;
        }
        if (lengthTransferred != blockSize)
        {
            error("Tried to read %d bytes from EEPROM but only got %d.", blockSize, lengthTransferred);
            return USB_ERROR_UNEXPECTED;
        }

        address += blockSize;

        if (statusCallback)
        {
            statusCallback("Reading EEPROM...", address - eepromAddress, eepromSize);
        }
    }
    return PLOADER_SUCCESS;
}

PLOADER_RESULT ploaderRestartDevice(ploaderHandle * handle)
{
    assert(handle != NULL);
    const uint16_t durationMs = 500;
    return usbDeviceControlTransfer(handle->device, 0x40,
        REQUEST_RESTART, durationMs, 0, NULL, 0, NULL);
}

PLOADER_RESULT ploaderCheckApplication(ploaderHandle * handle, bool * appValid)
{
    assert(handle != NULL);
    assert(appValid != NULL);

    uint8_t response;
    uint32_t lengthTransferred;
    USB_RESULT result = usbDeviceControlTransfer(handle->device, 0xC0,
        REQUEST_CHECK_APPLICATION, 0, 0, &response, 1, &lengthTransferred);

    if (result != 0)
    {
        error("Error checking to see if the application is valid.\n");
        return result;
    }
    if (lengthTransferred != 1)
    {
        error("Error checking to see if the application is valid.  "
            "Expected a 1-byte response but got %d bytes.\n", lengthTransferred);
        return USB_ERROR_IO;
    }
    *appValid = response;
    return PLOADER_SUCCESS;
}

void ploaderClose(ploaderHandle * handle)
{
    if (handle == NULL) { return; }
    usbDeviceClose(handle->device);
    ploaderInfoFree(handle->info);
    free(handle);
}

const char * ploaderGetErrorDescription(PLOADER_RESULT result)
{
    switch (result)
    {
    case PLOADER_SUCCESS: return "Success.";
    case PLOADER_ERROR_STATE: return "Device is not in the correct state.";
    case PLOADER_ERROR_LENGTH: return "Invalid data length.";
    case PLOADER_ERROR_PROGRAMMING: return "Programming error.";
    case PLOADER_ERROR_WRITE_PROTECTION: return "Write protection error.";
    case PLOADER_ERROR_VERIFICATION: return "Verification error.";
    case PLOADER_ERROR_ADDRESS_RANGE: return "Address is not in the correct range.";
    case PLOADER_ERROR_ADDRESS_ORDER: return "Address was not accessed in the correct order.";
    case PLOADER_ERROR_ADDRESS_ALIGNMENT: return "Address does not have the correct alignment.";
    case PLOADER_ERROR_WRITE: return "Write error.";
    case PLOADER_ERROR_EEPROM_VERIFICATION: return "EEPROM verification error.";
    default: return usbGetErrorDescription((USB_RESULT)result);
    }
}
