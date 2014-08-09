#ifndef _PLOADER_H
#define _PLOADER_H

/** STANDARD TYPES ************************************************************/

#include <stdint.h>
#include <stdbool.h>
#include "usb_system.h"


/** PLOADER (Pololu USB Bootloader) *******************************************/

// A PLOADER_RESULT can be any of the constants defined below or it can be
// any members of the USB_RESULT enum defined in usb_system.h.
typedef int PLOADER_RESULT;
#define PLOADER_SUCCESS         0

// Error codes returned by REQUEST_ERASE_FLASH and REQUEST_GET_LAST_ERROR.
#define PLOADER_ERROR_STATE                1
#define PLOADER_ERROR_LENGTH               2
#define PLOADER_ERROR_PROGRAMMING          3
#define PLOADER_ERROR_WRITE_PROTECTION     4
#define PLOADER_ERROR_VERIFICATION         5
#define PLOADER_ERROR_ADDRESS_RANGE        6
#define PLOADER_ERROR_ADDRESS_ORDER        7
#define PLOADER_ERROR_ADDRESS_ALIGNMENT    8
#define PLOADER_ERROR_WRITE                9
#define PLOADER_ERROR_EEPROM_VERIFICATION 10

typedef void (*ploaderStatusCallback)(const char * status, uint32_t progress, uint32_t maxProgress);

typedef struct ploaderProperties
{
    uint16_t idVendor;
    uint16_t idProduct;
    const char * name;
    uint32_t appAddress;
    uint32_t appSize;
    uint16_t writeBlockSize;
    bool supportsReadingFlash;
    uint32_t eepromAddress;
    uint32_t eepromAddressHexFile;
    uint32_t eepromSize;
    bool supportsEepromAccess;
    const uint8_t * deviceCode;
    const GUID * deviceInterfaceGuid;
} ploaderProperties;

const ploaderProperties * ploaderTable;

typedef struct ploaderInfo
{
    /* serialNumber: A pointer to an ASCII string of the USB serial number, as defined
     * in the USB 2.0 specification, except that it has been converted from UTF-16 to ASCII.
     * For now, we assume that this serialNumber is always available, and it is unique even
     * across different vendor/product IDs. */
    char * serialNumber;

    /* Current USB vendor ID of the bootloader, as defined in the USB 2.0 specification. */
    uint16_t usbVendorId;

    /* Current USB product ID of the bootloader, as defined in the USB 2.0 specification. */
    uint16_t usbProductId;

    /* A pointer to an ASCII string with the name of the bootloader.  This comes from an
     * internal table in the software but it should be the same as the USB product string
     * descriptor. */
    const char * name;

    /* The address of the first byte of the app (used in the USB protocol). */
    uint32_t appAddress;

    /* The number of bytes in the app. */
    uint32_t appSize;

    /* The address of the first byte of EEPROM (used in the USB protocol). */
    uint32_t eepromAddress;

    /* The address used for the first byte of EEPROM in the HEX file. */
    uint32_t eepromAddressHexFile;

    /* The number of bytes in EEPROM. */
    uint32_t eepromSize;
} ploaderInfo;

struct ploaderHandle;

typedef struct ploaderHandle ploaderHandle;

typedef struct ploaderList ploaderList;

/* FUNCTION DECLARATIONS ******************************************************/

/* ploaderListCreate:
 * Detects all the bootloaders that are currently connected to the computer.
 * This will only detect devices if they are actually in bootloader mode. */
PLOADER_RESULT ploaderListCreate(ploaderList ** list);

/* Returns the number of bootloaders in the list. */
uint32_t ploaderListSize(const ploaderList * list);

/* Removes entries from the list that do not match the specified serial number. */
PLOADER_RESULT ploaderListFilterBySerialNumber(ploaderList * list, const char * serialNumber);

/* Gets information about the bootloader entry in the list.
* The caller must free the information later by calling ploaderInfoFree. */
PLOADER_RESULT ploaderListCreateInfo(const ploaderList * list, uint32_t index, ploaderInfo ** info);

/* Frees the memory used for the information from ploaderListCreateInfo. */
void ploaderInfoFree(ploaderInfo * info);

/* Frees the memory associated with the bootloader list. */
void ploaderListFree(ploaderList * list);

/* Opens a handle to a device in the list. */
PLOADER_RESULT ploaderOpenFromList(const ploaderList * list, uint32_t index, ploaderHandle ** handle);

/* Gets information about the bootloader corresponding to the handle.
* The caller must free the information later by calling ploaderInfoFree. */
PLOADER_RESULT ploaderCreateInfo(ploaderHandle * handle, ploaderInfo ** info);

/* ploaderEraseFlash: Erases the entire application flash region. */
PLOADER_RESULT ploaderEraseFlash(ploaderHandle * handle, ploaderStatusCallback statusCallback);

/* ploaderWriteImage:
 * Takes care of all the details of writing an app image to the flash of a device.
 * image must be a pointer to a memory block of the right size.
 * This function takes care of:
 * - Erasing the current application.
 * - Writing the new application.
 * Note: On some bootloaders, this has the side effect of erasing the first byte
 * of EEPROM (setting to 0xFF).
 * After calling this function, you will usually want to call ploaderRestartDevice.
 */
PLOADER_RESULT ploaderWriteFlash(ploaderHandle * handle, const uint8_t * image, ploaderStatusCallback statusCallback);

/* ploaderReadImage:
 * Takes care of all the details of reading an app image from the flash on a device.
 * image must be a pointer to a memory block of the right size which will
 * be written to by this function.  This function takes care of getting the Wixel
 * in to bootloader mode (if needed) and reading the image. */
PLOADER_RESULT ploaderReadFlash(ploaderHandle * handle, uint8_t * image, ploaderStatusCallback statusCallback);

/* Just like ploaderWriteFlash, but for EEPROM instead */
PLOADER_RESULT ploaderWriteEeprom(ploaderHandle * handle, const uint8_t * image, ploaderStatusCallback statusCallback);

/* Just like ploaderReadFlash, but for EEPROM instead. */
PLOADER_RESULT ploaderReadEeprom(ploaderHandle * handle, uint8_t * image, ploaderStatusCallback statusCallback);

/* ploaderRestartDevice:
 * Sends the Restart command, which causes the device device to reset.
 * This is usually used to allow a newly-loaded application to start running. */
PLOADER_RESULT ploaderRestartDevice(ploaderHandle * handle);

/* ploaderCheckApplication:
 * Asks the bootloader if the currently-loaded application is valid or not.
 * appValid should be a pointer to a boolean that can accept the result.
 * If this function succeeds, then *appValid will be set to true or false.
 */
PLOADER_RESULT ploaderCheckApplication(ploaderHandle * handle, bool * appValid);

/* Closes the handle.  Passing in NULL is a no-op. */
void ploaderClose(ploaderHandle * handle);

/* ploaderGetErrorDescription:
 * Returns an English string describing the specified error code.  This string can be
 * shown to the user.
 */
const char * ploaderGetErrorDescription(PLOADER_RESULT result);

#endif
