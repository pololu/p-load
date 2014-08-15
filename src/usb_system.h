#ifndef _USB_SYSTEM_H
#define _USB_SYSTEM_H

#include <stdint.h>
#include <stdbool.h>

#ifdef WIN32
#define _WIN32_LEAN_AND_MEAN
#include <windows.h>
#else

typedef struct GUID
{
    uint32_t Data1;
    uint16_t Data2;
    uint16_t Data3;
    uint8_t  Data4[8];
} GUID;

#endif

// Maximum possible size for a USB serial number, in characters.
#define USB_SERIAL_NUMBER_MAX_SIZE 126

// This struct represents a list of USB devices connected to the computer.
struct usbDeviceList;

// This struct represents an open handle to a USB device.
struct usbDevice;

typedef struct usbDeviceList usbDeviceList;
typedef struct usbDevice usbDevice;

// This struct represents useful data about a USB device, all of which
// can be determined without actually opening a handle to the device and
// doing IO in either Windows or Linux.
typedef struct usbDeviceInfo
{
    uint16_t idVendor;
    uint16_t idProduct;
    uint16_t bcdDevice;
} usbDeviceInfo;

/* Return codes will be USB_RESULTs:
 * - Represented by an int.
 * - All negative.
 * - A superset of the libusb error codes.
 * - Expected error codes from WinUSB will translated to equivalent USB_RESULT codes.
 * - Unexpected error codes from WinUSB will be returned as USB_ERROR_UNEXPECTED.
 *   When an unexpected error occurs, we should add some code to usb_system*.c that
 *   expects it and converts it to a USB_RESULT error code.  You can help diagnose this
 *   bug by running usb_get_last_error() to see the error code from the system.
*/
enum USB_RESULT
{
    USB_SUCCESS = 0,

    /* Errors from libusb: */
    USB_ERROR_IO = -1,             /* Input/output error */
    USB_ERROR_INVALID_PARAM = -2,  /* Invalid parameter */
    USB_ERROR_ACCESS = -3,         /* Access denied (insufficient permissions in Linux, or other
                                    * program is using device in Windows) */
    USB_ERROR_NO_DEVICE = -4,      /* No such device (it may have been disconnected) */
    USB_ERROR_NOT_FOUND = -5,      /* Entity not found */
    USB_ERROR_BUSY = -6,           /* Resource busy */
    USB_ERROR_TIMEOUT = -7,        /* Operation timed out */
    USB_ERROR_OVERFLOW = -8,       /* Overflow */
    USB_ERROR_PIPE = -9,           /* Pipe error */
    USB_ERROR_INTERRUPTED = -10,   /* System call interrupted (perhaps due to signal) */
    USB_ERROR_NO_MEM = -11,        /* Insufficient memory */
    USB_ERROR_NOT_SUPPORTED = -12, /* Operation not supported or unimplemented on this platform */

    USB_ERROR_OUT_OF_RANGE = -30,  /* Index parameter was out of range. */
    USB_ERROR_INSUFFICIENT_BUFFER = -31,  /* Data buffer provided was too small. */
    USB_ERROR_NO_SERIAL_NUMBER = -32,  /* Device has no serial number. */

    USB_ERROR_OTHER_LIBUSB = -99,  /* Other error (this code is from Libusb) */
    USB_ERROR_UNEXPECTED = -100,   /* An error we did not anticipate. */
};

typedef enum USB_RESULT USB_RESULT;

/* Sets the verbosity of the USB system.
 * Level 0: no messages ever printed by the library
 * Level 1: error messages are printed to stderr
 * Level 2: warning and error messages are printed to stderr
 * Level 3: informational messages are printed to stdout, warning and error messages are printed to stderr (default)
*/
void usbSetVerbosity(int new_verbosity);

/* Creates a list of USB devices.  Be sure to call usbListFree later to free it. */
USB_RESULT usbListCreate(usbDeviceList ** list);

/* Frees a list of USB devices. */
void usbListFree(usbDeviceList * list);

/* Returns the size of the list. */
uint32_t usbListSize(const usbDeviceList * list);

/* Removes the specified USB device from the list. */
void usbListRemove(usbDeviceList * list, uint32_t index);

/* Removes devices that don't have the specified serial number. */
USB_RESULT usbListFilterBySerialNumber(usbDeviceList * list, const char * serialNumber);

/* Removes devices that don't have the specified Windows Device Interface GUID.
 * If you don't call this function, then usbDeviceOpenFromList might fail on Windows
 * because the device will not have the specified interface GUID if the drivers have not been
 * installed or if the device was very recently plugged in.
 * This is NOT a substitute for filtering by VID and PID, because this function only works
 * on Windows, and will just return USB_SUCCESS on other systems.
 */
USB_RESULT usbListFilterByDeviceInterfaceGuid(usbDeviceList * list, const GUID * deviceInterfaceGuid);

/* Stores result in *info.  This is all info available from Windows, Linux, and
   and Mac OS X without actually opening a handle to the device.
   Giving an invalid index will result in an error.
   For some unusual devices like root hubs, the numbers returned will just be zeros. */
USB_RESULT usbListGetDeviceInfo(const usbDeviceList * list, uint32_t index, usbDeviceInfo * info);

/* Serial number is stored in char * data as an ASCII string (null-terminated).
   As long as buffer_size is greater than 0, a string will always we written to
   the data buffer--if there is an error other than USB_ERROR_INSUFFICIENT_BUFFER
   then the returned string will be the empty string.
   Non-ascii serial numbers are not supported.
   One expected cause of failure will be that buffer is too small.
   Implementation note: In Linux this actually connects to the device, in Windows
   it just fetches and parses a string.
*/
USB_RESULT usbListGetSerialNumber(const usbDeviceList * list, uint32_t index, char * data, uint32_t length);

/* Opens a handle to the specified device in the list.
   The handle should be closed later with usbDeviceClose. */
USB_RESULT usbDeviceOpenFromList(const usbDeviceList * list, uint32_t index, usbDevice ** device,
    const GUID * deviceInterfaceGuid);

/* Closes a USB device handle. */
void usbDeviceClose(usbDevice * device);

/* Performs a USB control transfer to send or receive data from the device. */
USB_RESULT usbDeviceControlTransfer(usbDevice * device, uint8_t bmRequestType, uint8_t bRequest,
    uint16_t wValue, uint16_t wIndex, void * data, uint16_t wLength, uint32_t * lengthTransferred);

/* Returns a string describing the error code, which may have some OS-specific hints included. */
const char * usbGetErrorDescription(USB_RESULT code);

// Private; these variables are only for internal use inside usb_system:
extern int usbControlTransferTimeout;
extern int usbVerbosity;
void usbVerbosityChanged();

#endif
