/* usb_system requirements:
 * 1) All public functions in this library will only return a non-zero error code
 *    if one of the following is true
 *    A) They have recently called u_error (to print an error message if enabled).
 *    B) A nested call to a public function in this library has recently failed,
 *       and the error code is just being passed up to the caller.
 */

#include <libusb-1.0/libusb.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <assert.h>

#include "usb_system.h"

struct usbDeviceList
{
    uint8_t size;
    libusb_device ** libusbDeviceList;
    uint32_t length;
};

struct usbDevice
{
    libusb_device_handle * handle;
};

// Private function for writing an error message to stderr.
static void u_error(const char * format, ...) __attribute__((format (printf, 1, 2)));
static void u_error(const char * format, ...)
{
    if (usbVerbosity < 1){ return; }
    va_list ap;
    va_start(ap, format);
    fprintf(stderr, "usb-system: Error: ");
    vfprintf(stderr, format, ap);
    fprintf(stderr, "\n");
    va_end(ap);
}

// Private function for writing a warning message to stderr.
static void u_warning(const char * format, ...) __attribute__((format (printf, 1, 2)));
static void u_warning(const char * format, ...)
{
    if (usbVerbosity < 2){ return; }
    va_list ap;
    va_start(ap, format);
    fprintf(stderr, "usb-system: Warning: ");
    vfprintf(stderr, format, ap);
    fprintf(stderr, "\n");
    va_end(ap);
}

const char * usbGetErrorDescription(USB_RESULT code)
{
    switch(code)
    {
    case USB_SUCCESS: return "Success.";
    case USB_ERROR_IO: return "Input/output error.";
    case USB_ERROR_INVALID_PARAM: return "Invalid parameter.";
#ifdef _WIN32
    case USB_ERROR_ACCESS: return "Access denied.  Try closing all other programs that are using the device.";
#else
    case USB_ERROR_ACCESS: return "Access denied.";
#endif
    case USB_ERROR_NO_DEVICE: return "No such device.";
    case USB_ERROR_NOT_FOUND: return "Device not found.";
    case USB_ERROR_BUSY: return "Resource busy.";
    case USB_ERROR_TIMEOUT: return "Operation timed out.";
    case USB_ERROR_OVERFLOW: return "Buffer overflow.";
    case USB_ERROR_PIPE: return "Pipe error or invalid request.";
    case USB_ERROR_INTERRUPTED: return "System call interrupted.";
    case USB_ERROR_NO_MEM: return "Insufficient memory.";
    case USB_ERROR_NOT_SUPPORTED: return "Operation not supported or unimplemented on this platform.";
    case USB_ERROR_OUT_OF_RANGE: return "Index parameter was out of range.";
    case USB_ERROR_OTHER_LIBUSB: return "Unspecified error from Libusb.";
    case USB_ERROR_UNEXPECTED: return "Unexpected error.";
    case USB_ERROR_INSUFFICIENT_BUFFER: return "Data buffer provided was too small.";
    case USB_ERROR_NO_SERIAL_NUMBER: return "Device has no serial number.";
    default:
        u_error("Invalid error code: %d.", code);
        return "Invalid error code.";
    }
}

static libusb_context * get_context()
{
    static libusb_context * context;
    static int tried = 0;
    if (!tried)
    {
        tried = 1;
        int result = libusb_init(&context);
        if (result)
        {
            u_warning("Unable to create libusb context: %s  Will use the default context instead.\n", usbGetErrorDescription(result));
            context = 0;
        }
    }
    return context;
}

void usbVerbosityChanged()
{
    libusb_set_debug(get_context(), usbVerbosity);
}

USB_RESULT usbListCreate(usbDeviceList ** list)
{
    assert(list != NULL);

    // By default, return a null pointer.  This is useful because it lets the user
    // safely call usb_list_free no matter what this function did, or the user could
    // ignore the return code.
    *list = 0;

    // Allocate memory for the list.
    usbDeviceList * new_list = malloc(sizeof(usbDeviceList));
    if (new_list == 0)
    {
        u_error("Failed to allocate memory for USB device list.");
        return USB_ERROR_NO_MEM;
    }

    // Retrieve a list from libusb.
    ssize_t result = libusb_get_device_list(get_context(), &new_list->libusbDeviceList);
    if (result < 0)
    {
        u_error("Failed to get list of all USB devices: %s", usbGetErrorDescription(result));
        free(new_list);
        return result;
    }
    new_list->size = result;

    // Success.  Return the list to the caller.
    *list = new_list;
    return USB_SUCCESS;
}

void usbListFree(usbDeviceList * list)
{
    // Note: The usbListFilter functions modify the contents of
    // the libusbDeviceList in a way that is incompatible with:
    //   libusb_free_device_list(list->libusbDeviceList, 1);
    // (they don't leave a null entry at the end of the list).
    // The source code of libusb_free_device_list is here:
    //   http://www.libusb.org/browser/libusb/libusb/core.c
    // Therefore, we unreference the devices ourselves.

    if (list == NULL){ return; }

    for(int i = 0; i < list->size; i++)
    {
        libusb_unref_device(list->libusbDeviceList[i]);
    }
    libusb_free_device_list(list->libusbDeviceList, 0);

    free(list);
}

void usbListRemove(usbDeviceList * list, uint32_t index)
{
    assert(list != NULL);
    assert(index < list->size);

    libusb_unref_device(list->libusbDeviceList[index]);

    list->size--;
    for(uint32_t i = index; i < list->size; i++)
    {
        list->libusbDeviceList[i] = list->libusbDeviceList[i + 1];
    }
}

USB_RESULT usbListGetDeviceInfo(const usbDeviceList * list, uint32_t index, usbDeviceInfo * info)
{
    assert(info != NULL);

    if (index >= list->size)
    {
        u_error("Unable to get device info because index parameter is out of range (%d >= %d)", index, list->size);
        return USB_ERROR_OUT_OF_RANGE;
    }

    libusb_device * device = list->libusbDeviceList[index];

    struct libusb_device_descriptor device_descriptor;
    int result = libusb_get_device_descriptor(device, &device_descriptor);
    if (result)
    {
        u_error("Failed to get device descriptor: %s", usbGetErrorDescription(result));
        return result;
    }

    info->idVendor = device_descriptor.idVendor;
    info->idProduct = device_descriptor.idProduct;
    info->bcdDevice = device_descriptor.bcdDevice;
    return USB_SUCCESS;
}

/* Assumption: English(US) is one of the languages supported by this device.
   If this assumption is not true, then we are violating the USB spec from:
   http://www.usb.org/developers/docs/USB_LANGIDs.pdf
   which says: "System software should never request a LANGID not defined in
   the LANGID code array (string index = 0) presented by the device.
*/
static USB_RESULT usbDeviceGetSerialNumber(usbDevice * device, char * data, uint32_t length)
{
    if (length == 0)
    {
        u_error("Unable to get serial number because buffer_size parameter is 0.");
        return USB_ERROR_INSUFFICIENT_BUFFER;
    }

    // By default, return an empty string so that the user can
    // easily ignore the return code.
    data[0] = 0;

    // Get the device desriptor so we can use iSerialNumber.
    struct libusb_device_descriptor device_descriptor;
    int result = libusb_get_device_descriptor(libusb_get_device(device->handle), &device_descriptor);
    if (result)
    {
        u_error("Unable to get serial number.  Unable to get device decriptor.  %s", usbGetErrorDescription(result));
        return result;
    }

    if (device_descriptor.iSerialNumber == 0)
    {
        // This device does not have a serial number descriptor.
        u_error("Unable to get serial number.  Device does not have a serial number.");
        return USB_ERROR_NO_SERIAL_NUMBER;
    }

    unsigned char buffer[255];      // Maximum string descriptor length is 255 bytes.
    const uint16_t lang_id = 0x409; // Assumption: using English(US) is ok (see above).

    result = libusb_get_string_descriptor(device->handle, device_descriptor.iSerialNumber, lang_id, buffer, sizeof(buffer));
    if (result < 0)
    {
        u_error("Unable to get serial number.  %s", usbGetErrorDescription(result));
        return result;
    }

    if (result == 0)
    {
        u_error("Unable to get serial number.  Received 0 bytes from device.");
        return result;
    }

    if (result < buffer[0])
    {
        u_error("Unable to get serial number.  Received %d bytes from device, expected %d.", result, buffer[0]);
        return LIBUSB_ERROR_IO;
    }

    if (buffer[0] % 2)
    {
        u_error("Unable to get serial number.  Expected descriptor length to be even but it is odd (%d).", buffer[0]);
        return LIBUSB_ERROR_IO;
    }

    if (buffer[1] != LIBUSB_DT_STRING)
    {
        u_error("Unable to get serial number.  Second byte of string descriptor is %d, expected %d.", buffer[1], LIBUSB_DT_STRING);
        return LIBUSB_ERROR_IO;
    }

    unsigned int destIndex, srcIndex;
    for (destIndex = 0, srcIndex = 2; srcIndex < buffer[0]; srcIndex += 2, destIndex++)
    {
        if (destIndex >= (length - 1))
        {
            data[destIndex] = 0;
            u_error("Unable to get entire serial number because it is longer than the provided buffer.");
            return USB_ERROR_INSUFFICIENT_BUFFER;
        }

        if (buffer[srcIndex + 1])
        {
            data[destIndex] = '?';     // The codepoint for this character is greater than 255.
        }
        else
        {
            data[destIndex] = buffer[srcIndex];
        }
    }
    data[destIndex] = 0;
    return USB_SUCCESS;
}

USB_RESULT usbListGetSerialNumber(const usbDeviceList * list, uint32_t index, char * buffer, uint32_t buffer_size)
{
    assert(buffer_size != 0);
    assert(buffer != NULL);

    // By default, return an empty string so that the user can
    // easily ignore the return code.
    buffer[0] = 0;

    // Open a handle to the device.
    USB_RESULT result;
    usbDevice * device;
    result = usbDeviceOpenFromList(list, index, &device, NULL);
    if (result)
    {
        return result;
    }

    result = usbDeviceGetSerialNumber(device, buffer, buffer_size);
    if (result)
    {
        return result;
    }

    usbDeviceClose(device);
    return USB_SUCCESS;
}

USB_RESULT usbDeviceOpenFromList(const usbDeviceList * list, uint32_t index, usbDevice ** device, const GUID * deviceInterfaceGuid)
{
    assert(device != 0);

    *device = 0;

    if (index >= list->size)
    {
        u_error("Unable to open device because index parameter is out of range (%d >= %d).", index, list->size);
        return USB_ERROR_OUT_OF_RANGE;
    }
    libusb_device * selected_device = list->libusbDeviceList[index];
    libusb_device_handle * handle;
    int result = libusb_open(selected_device, &handle);
    if (result)
    {
        u_error("Failed to open device %d of %d: %s", index+1, list->size, usbGetErrorDescription(result));
        return result;
    }

    usbDevice * new_device = malloc(sizeof(usbDevice));
    if (new_device == 0)
    {
        u_error("Failed to allocate memory for usb device struct.");
        return USB_ERROR_NO_MEM;
    }

    new_device->handle = handle;

    *device = new_device;
    return USB_SUCCESS;
}

void usbDeviceClose(usbDevice * device)
{
    if (device == 0){ return; }
    libusb_close(device->handle);
    free(device);
}

USB_RESULT usbDeviceControlTransfer(usbDevice * device,
    uint8_t bmRequestType, uint8_t bRequest, uint16_t wValue, uint16_t wIndex,
    void * data, uint16_t wLength, uint32_t * lengthTransferred)
{
    if (lengthTransferred)
    {
        *lengthTransferred = 0;
    }

    USB_RESULT result = libusb_control_transfer(
        device->handle, bmRequestType, bRequest, wValue, wIndex,
        data, wLength, usbControlTransferTimeout);

    if (result < 0)
    {
        u_error("Control transfer failed (%02x %02x %02x %02x %02x): %s",
            bmRequestType, bRequest, wValue, wIndex, wLength,
            usbGetErrorDescription(result));
        return result;
    }
    if (lengthTransferred)
    {
        *lengthTransferred = result;
    }
    return USB_SUCCESS;
}

uint32_t usbListSize(const usbDeviceList * list)
{
    assert(list != NULL);
    return list->size;
}
