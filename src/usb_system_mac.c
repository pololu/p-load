// Mac OS X class heirarchy:
//  C++ Kernel Object  IOKitLib C Object
//  --------------------------------------
//  OSObject
//  IORegistryEntry    io_registry_entry_t
//  IOService          io_service_t
//  IOUsbNub
//  IOUsbDevice

#include "usb_system.h"

// Standard Posix/Unix headers.
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <assert.h>

// CoreFoundation is a Mac thing that lets you have some objects with reference counts.
#include <CoreFoundation/CoreFoundation.h>

// mach is the boundary between the user space and the kernel, which powers IOKitLib.
#include <mach/error.h>
#include <mach/mach_error.h>

// IOKitLib exposes the C++ kernel classes of IOKit to our C program.
// These files are in /System/Library/Frameworks/IOKit.framework/Headers/
#include <IOKit/IOKitLib.h>
#include <IOKit/IOCFBundle.h>
#include <IOKit/IOCFPlugIn.h>
#include <IOKit/usb/IOUSBLib.h>
#include <IOKit/serial/IOSerialKeys.h>
#include <IOKit/serial/ioss.h>

// We will use IOUSBDeviceInterfaceID182 because it is easier to make control
// transfers with timeouts.  This means our minimum supported OS is 10.0.4.
#ifndef kIOUSBDeviceInterfaceID182
#error "IOUSBFamily is too old."
#endif
typedef IOUSBDeviceInterface182 IOUSBDeviceInterfaceX;
#define DeviceInterfaceID kIOUSBDeviceInterfaceID182

struct usbDeviceList
{
    uint8_t size;
    io_service_t * list;   // points to array of IOUSBDevices
};

struct usbDevice
{
    IOUSBDeviceInterfaceX ** dev;
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

USB_RESULT usbListCreate(usbDeviceList ** list)
{
    assert(list != NULL);

    // By default, return a null pointer.  This is useful because it lets the user
    // safely call usb_list_free no matter what this function did, or the user could
    // ignore the return code.
    *list = 0;

    usbDeviceList * newList = malloc(sizeof(usbDeviceList));
    if (newList == NULL)
    {
        u_error("Failed to allocate memory for USB device list (1).");
        return USB_ERROR_NO_MEM;
    }
    // defer: free(newList)

    // TODO: test the realloc code by setting array_size to 1 some time
    int array_size = 128;

    newList->list = malloc(array_size * sizeof(io_service_t));
    if (newList->list == NULL)
    {
        u_error("Failed to allocate memory for USB device list (2).");
        free(newList);
        return USB_ERROR_NO_MEM;
    }
    // defer: free(newList->list);

    // Create a dictionary that says "IOProviderClass" => "IOUSBDevice"
    // This dictionary is CFReleased by IOServiceGetMatchingServices.
    CFMutableDictionaryRef classesToMatch = IOServiceMatching(kIOUSBDeviceClassName);
    if (classesToMatch == NULL)
    {
        u_error("IOServiceMatching returned null.");
        free(newList->list);
        free(newList);
        return USB_ERROR_UNEXPECTED;
    }

    // Create an iterator for all the connected USB devices.
    io_iterator_t usbDeviceIterator;
    kern_return_t result = IOServiceGetMatchingServices(kIOMasterPortDefault, classesToMatch, &usbDeviceIterator);
    if (KERN_SUCCESS != result)
    {
        u_error("IOServiceGetMatchingServices failed.  %s.", mach_error_string(result));
        free(newList->list);
        free(newList);
        return USB_ERROR_UNEXPECTED;
    }
    // defer: IOObjectRelease(usbDeviceIterator);

    newList->size = 0;
    io_service_t device;
    while((device = IOIteratorNext(usbDeviceIterator)))
    {
        if (newList->size >= array_size)
        {
            array_size *= 2;
            io_service_t * new_array = realloc(newList->list, array_size * sizeof(io_service_t));
            if (new_array == NULL)
            {
                u_error("Failed to allocate memory for USB device list (3).");
                free(newList->list);
                free(newList);
                return USB_ERROR_NO_MEM;
            }
            newList->list = new_array;
        }

        newList->list[newList->size] = device;
        newList->size++;
    }

    IOObjectRelease(usbDeviceIterator);
    *list = newList;
    return USB_SUCCESS;
}

void usbListFree(usbDeviceList * list)
{
    if (list == 0) { return; }

    for (int i = 0; i < list->size; i++)
    {
        IOObjectRelease(list->list[i]);
    }

    free(list->list);
    free(list);
}

void usbListRemove(usbDeviceList * list, uint32_t index)
{
    assert(list != NULL);
    assert(index < list->size);

    IOObjectRelease(list->list[index]);
    
    list->size--;
    for (uint32_t i = index; i < list->size; i++)
    {
        list->list[i] = list->list[i + 1];
    }
}

// Gets an SInt32 from a property on an IORegistryEntry.
static USB_RESULT getSInt32(io_registry_entry_t device, CFStringRef name, SInt32 * value)
{
    CFTypeRef cf_value = IORegistryEntryCreateCFProperty(device, name, kCFAllocatorDefault, 0);
    if (cf_value == 0)
    {
        u_error("Error getting integer from IORegistryEntry.");
        return USB_ERROR_UNEXPECTED;
    }
    if (CFGetTypeID(cf_value) != CFNumberGetTypeID())
    {
        u_error("Error getting integer from IORegistryEntry: got a non-number.");
        CFRelease(cf_value);
        return USB_ERROR_UNEXPECTED;
    }
    CFNumberGetValue(cf_value, kCFNumberSInt32Type, value);
    CFRelease(cf_value);
    return USB_SUCCESS;
}

// Helper function to generate good error messages for getString.
static void getStringError(const char * message, io_registry_entry_t device, CFStringRef name, char * buffer, uint32_t buffer_size)
{
    io_name_t devName, propName;
    if (!CFStringGetCString(name, propName, sizeof(propName), kCFStringEncodingASCII)) { propName[0] = 0; }
    if (KERN_SUCCESS != IORegistryEntryGetName(device, devName)){ devName[0] = 0; }
    u_error("%s (device=%s, name=%s, buffer_size=%d).", message, devName, propName, buffer_size);
}

// Gets an SInt32 from a property on an IORegistryEntry.
static USB_RESULT getString(io_registry_entry_t device, CFStringRef name, char * buffer, uint32_t buffer_size)
{
    CFTypeRef cf_value = IORegistryEntryCreateCFProperty(device, name, kCFAllocatorDefault, 0);
    if (cf_value == 0)
    {
        getStringError("Error getting string from IORegistry", device, name, buffer, buffer_size);
        return USB_ERROR_UNEXPECTED;
    }
    if (CFGetTypeID(cf_value) != CFStringGetTypeID())
    {
        getStringError("Error getting string from IORegistryEntry: got a non-string", device, name, buffer, buffer_size);
        CFRelease(cf_value);
        return USB_ERROR_UNEXPECTED;
    }

    Boolean success = CFStringGetCString(cf_value, buffer, buffer_size, kCFStringEncodingASCII);
    CFRelease(cf_value);
    if (!success)
    {
        // Perhaps the buffer was too small; we could check with CFStringGetLength().
        u_error("Error converting serial number to C string.");
        return USB_ERROR_UNEXPECTED;
    }

    return USB_SUCCESS;
}

USB_RESULT usbListGetDeviceInfo(const usbDeviceList * list, uint32_t index, usbDeviceInfo * info)
{
    assert(info != NULL);

    io_service_t device = list->list[index];

    SInt32 idVendor, idProduct, bcdDevice;
    if (getSInt32(device, CFSTR(kUSBVendorID), &idVendor)
        || getSInt32(device, CFSTR(kUSBProductID), &idProduct)
        || getSInt32(device, CFSTR(kUSBDeviceReleaseNumber), &bcdDevice))
    {
        return USB_ERROR_UNEXPECTED;
    }

    info->idVendor = idVendor;
    info->idProduct = idProduct;
    info->bcdDevice = bcdDevice;
    return USB_SUCCESS;
}

USB_RESULT usbListGetSerialNumber(const usbDeviceList * list, uint32_t index, char * buffer, uint32_t buffer_size)
{
    assert(buffer_size != 0);
    assert(buffer != NULL);

    // TODO: return USB_ERROR_NO_SERIAL_NUMBER if the device simply has no serial number

    return getString(list->list[index], CFSTR(kUSBSerialNumberString), buffer, buffer_size);
}

USB_RESULT usbDeviceOpenFromList(const usbDeviceList * list, uint32_t index, usbDevice ** device, const GUID * deviceInterfaceGuid)
{
    assert(list && device);
    assert(index < list->size);

    *device = 0;

    kern_return_t kr;  // Note: IOReturn is the same as kern_return_t

    // Create the plug-in interface.
    SInt32 score;
    IOCFPlugInInterface ** plugIn;
    kr = IOCreatePlugInInterfaceForService(list->list[index],
        kIOUSBDeviceUserClientTypeID, kIOCFPlugInInterfaceID,
        &plugIn, &score);
    if (KERN_SUCCESS != kr)
    {
        u_error("Error opening device.  Failed to create plug-in interface.  %s.", mach_error_string(kr));
        return USB_ERROR_UNEXPECTED;
    }
    // defer: (*plugIn)->Release(plugIn);

    // Create the device interface.
    IOUSBDeviceInterfaceX ** dev = NULL;
    HRESULT hr = (*plugIn)->QueryInterface(plugIn, CFUUIDGetUUIDBytes(DeviceInterfaceID), (LPVOID *)&dev);
    (*plugIn)->Release(plugIn);
    if (hr)
    {
        u_error("Error opening device.  Failed to create device interface.  Error code 0x%08x.", hr);
        return USB_ERROR_UNEXPECTED;
    }

    //Open the device and get exclusive access.
    kr = (*dev)->USBDeviceOpenSeize(dev);
    if (kIOReturnSuccess != kr)
    {
        (*dev)->Release(dev);
        if (kr == kIOReturnExclusiveAccess)
        {
            u_error("Error opening device.  %s.", usbGetErrorDescription(USB_ERROR_ACCESS));
            return USB_ERROR_ACCESS;
        }
        else
        {
            u_error("Error opening device.  Error code 0x%08x\n.", kr);
            return USB_ERROR_UNEXPECTED;
        }
    }

    // Allocate memory for the usbDevice struct.
    usbDevice * newDevice = malloc(sizeof(usbDevice));
    if (newDevice == NULL)
    {
        u_error("Error opening device.  Not enough memory to allocate usbDevice struct.");
        (*dev)->USBDeviceClose(dev);
        (*dev)->Release(dev);
        return USB_ERROR_NO_MEM;
    }

    // Populate the usbDevice struct (just has a pointer) and return it to the caller.
    newDevice->dev = dev;
    *device = newDevice;

    // Get the device into configuration 1.
    // (This happens automatically on Linux and Windows when the device is enumerated.)
    UInt8 configNum = 0;
    kr = (*dev)->GetConfiguration(dev, &configNum);
    if (kIOReturnSuccess != kr)
    {
        u_error("Error getting USB device configuration.");
        free(newDevice);
        (*dev)->USBDeviceClose(dev);
        (*dev)->Release(dev);
        return USB_ERROR_UNEXPECTED;
    }
    if (configNum != 1)
    {
        kr = (*dev)->SetConfiguration(dev, 1);
        if (kIOReturnSuccess != kr)
        {
            u_error("Error setting USB device configuration.");
            free(newDevice);
            (*dev)->USBDeviceClose(dev);
            (*dev)->Release(dev);
            return USB_ERROR_UNEXPECTED;            
        }
    }

    return USB_SUCCESS;
}

void usbDeviceClose(usbDevice * device)
{
    if (device == 0){ return; }
    IOUSBDeviceInterfaceX ** dev = device->dev;
    (*dev)->USBDeviceClose(dev);
    (*dev)->Release(dev);
    free(device);
}

USB_RESULT usbDeviceControlTransfer(usbDevice * device, uint8_t bmRequestType, uint8_t bRequest, uint16_t wValue, uint16_t wIndex, void * data, uint16_t wLength, uint32_t * lengthTransferred)
{
    assert(device);

    if (lengthTransferred)
    {
        *lengthTransferred = 0;
    }

    // Set up a struct describing the request.
    IOUSBDevRequestTO request;
    request.bmRequestType = bmRequestType;
    request.bRequest = bRequest;
    request.wValue = wValue;
    request.wIndex = wIndex;
    request.wLength = wLength;
    request.pData = data;
    request.completionTimeout = usbControlTransferTimeout;
    request.wLenDone = 0xFFFFFFFF;  // just to make sure it gets written by DeviceRequestTO.

    // Perform the request.
    IOUSBDeviceInterfaceX ** dev = device->dev;
    IOReturn ir = (*dev)->DeviceRequestTO(dev, &request);
    if (kIOReturnSuccess != ir)
    {
        // Documentation of error codes:
        // https://developer.apple.com/library/mac/#documentation/Darwin/Reference/IOKit/IOUSBLib_h/Classes/IOUSBDeviceInterface182/index.html#//apple_ref/doc/com/intfm/IOUSBDeviceInterface182/DeviceRequestTO
        // http://developer.apple.com/library/mac/#qa/qa1075/_index.html
        // /System/Library/Frameworks/IOKit.framework/Versions/A/Headers/IOReturn.h
        // http://www.opensource.apple.com/source/IOUSBFamily/IOUSBFamily-190.4.1/IOUSBFamily/Headers/USB.h
        const char * why = "";
        USB_RESULT result = USB_ERROR_UNEXPECTED;
        switch(ir)
        {
        case sys_iokit | sub_iokit_common | kIOReturnNoDevice:
            // We have not encountered this error code before.
            why = "  No connection to an IO service.";
            break;
        case sys_iokit | sub_iokit_common | kIOReturnAborted:
            // We have not encountered this error code before.
            why = "  Thread was interrupted.";
            break;
        case sys_iokit | sub_iokit_usb | kIOUSBPipeStalled:
            // Possible reasons: device was disconnected, or request was invalid according to device.
            why = "  Pipe stalled.";
            result = USB_ERROR_PIPE;
            break;
        case sys_iokit | sub_iokit_usb | kIOUSBTransactionTimeout:
            why = "  Transaction timeout.";
            result = USB_ERROR_TIMEOUT;
            break;
        case sys_iokit | sub_iokit_common | kIOReturnNotResponding: // 0xe00002ed
            why = "  Device not responding.";
            result = USB_ERROR_TIMEOUT;
            break;
        }
        u_error("Control transfer failed (%d, %d, %d, %d, %d).  Error code 0x%08x.%s", bmRequestType, bRequest, wValue, wIndex, wLength, ir, why);
        return result;
    }

    // Success!
    if (lengthTransferred)
    {
        *lengthTransferred = request.wLenDone;
    }
    return USB_SUCCESS;
}

uint32_t usbListSize(const usbDeviceList * list)
{
    assert(list != NULL);
    return list->size;
}

void usbVerbosityChanged()
{
}
