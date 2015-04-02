/* usb_system_windows:  A wrapper around SetupDI and WinUSB APIs, allowing you to
 * write C programs that use WinUSB devices in Windows.  All the non-static functions
 * here are defined in usb_system.h; see that file for documentation!
 *
 * usb_system requirements:
 * 1) All public functions in this library will only return a non-zero error code
 *    if one of the following is true
 *    A) They have recently called u_error (to print an error message if enabled).
 *    B) A nested call to a public function in this library has recently failed,
 *       and the error code is just being passed up to the caller.
 */

// For Debug builds in Windows, enable memory leak tracking.
#if defined(_MSC_VER) && defined(_DEBUG)
#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>
#endif

// Standard headers
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <assert.h>

// Windows-specific headers
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winusb.h>
#include <setupapi.h>
#include <cfgmgr32.h>

// The header for this library.
#include "usb_system.h"

struct usbDeviceList
{
    uint8_t * indexMapping;
    uint8_t size;
    HDEVINFO handle;
};

struct usbDevice
{
    DEVINST deviceInstance;
    HANDLE deviceHandle;
    WINUSB_INTERFACE_HANDLE winusbInterfaceHandle;
};

static void u_error(const char * format, ...)
{
    va_list ap;
    if (usbVerbosity < 1){ return; }
    va_start(ap, format);
    fprintf(stderr, "usb-system: Error: ");
    vfprintf(stderr, format, ap);
    fprintf(stderr, "\n");
    va_end(ap);
}

static void u_win32_error(const char * format, ...)
{
    va_list ap;
    if (usbVerbosity < 1){ return; }
    va_start(ap, format);
    fprintf(stderr, "usb-system: Error: ");
    vfprintf(stderr, format, ap);
    fprintf(stderr, "  Error code 0x%x.\n", GetLastError());
    va_end(ap);
}

static void u_warning(const char * format, ...)
{
    va_list ap;
    if (usbVerbosity < 2){ return; }
    va_start(ap, format);
    fprintf(stderr, "usb-system: Warning: ");
    vfprintf(stderr, format, ap);
    fprintf(stderr, "\n");
    va_end(ap);
}

static void u_win32_warning(const char * format, ...)
{
    va_list ap;
    if (usbVerbosity < 1){ return; }
    va_start(ap, format);
    fprintf(stderr, "usb-system: Warning: ");
    vfprintf(stderr, format, ap);
    fprintf(stderr, "  Error code 0x%x.\n", GetLastError());
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

USB_RESULT usbListGetDeviceInfo_core(HDEVINFO list_handle, uint32_t sys_index, usbDeviceInfo * info)
{
    BOOL success;
    CONFIGRET cresult;
    SP_DEVINFO_DATA device_info_data;
    char buffer[100];
    int result;

    assert(info != NULL);

    memset(info, 0, sizeof(usbDeviceInfo));

    // Get the device instance number (DEVINST); it is part of SP_DEVINFO_DATA.
    device_info_data.cbSize = sizeof(device_info_data);
    success = SetupDiEnumDeviceInfo(list_handle, sys_index, &device_info_data);
    if (!success)
    {
        u_win32_error("Unexpected error getting device info.");
        return USB_ERROR_UNEXPECTED;
    }

    // Get the Device Instance Id string (e.g. "USB\VID_1FFB&PID_0081\00000000").
    cresult = CM_Get_Device_IDA(device_info_data.DevInst, buffer, sizeof(buffer), 0);
    if (cresult != CR_SUCCESS)
    {
        u_win32_error("Unable to get the device instance id string.");
        return USB_ERROR_UNEXPECTED;
    }

    if (buffer[4] == 'R')
    {
        // This device is probably a USB ROOT HUB device, e.g. USB\ROOT_HUB20\4&28495488&0.
        // Just return all zeroes.
        info->idVendor = 0;
        info->idProduct = 0;
        info->bcdDevice = 0;
        return USB_SUCCESS;
    }

    result = sscanf(buffer+4, "VID_%04hx&PID_%04hx", &(info->idVendor), &(info->idProduct));
    if (result != 2)
    {
        u_error("Unable to determine vendor ID and product ID from this Device Instance ID: \"%s\".", buffer);
        return USB_ERROR_UNEXPECTED;
    }

    // TODO: actually figure out how to get bcdDevice using IoGetDevicePropery:
    // http://msdn.microsoft.com/en-us/library/ff549203%28v=vs.85%29.aspx
    info->bcdDevice = 0xFFFF;

    return USB_SUCCESS;
}

static USB_RESULT computeLengthOfList(HDEVINFO list_handle, uint8_t * length)
{
    assert(length != NULL);

    *length = 0;  // By default, return 0.

    uint32_t index = 0;
    while (1)
    {
        BOOL success;
        SP_DEVINFO_DATA device_info_data;
        device_info_data.cbSize = sizeof(device_info_data);
        success = SetupDiEnumDeviceInfo(list_handle, index, &device_info_data);
        if (!success)
        {
            if (GetLastError() != ERROR_NO_MORE_ITEMS)
            {
                u_win32_error("Unable to get device info for USB device %d.", index);
                return USB_ERROR_UNEXPECTED;
            }

            *length = index;
            return USB_SUCCESS;
        }
        index++;
    }
}


void usbListRemove(usbDeviceList * list, uint32_t index)
{
    assert(list != NULL);
    assert(index < list->size);

    list->size--;
    for (uint32_t i = index; i < list->size; i++)
    {
        list->indexMapping[i] = list->indexMapping[i + 1];
    }
}

USB_RESULT usbListCreate(usbDeviceList ** list)
{
    usbDeviceList * new_list;
    USB_RESULT result;
    uint32_t i;

    assert(list != NULL);

    // By default, return a null pointer.  This is useful because it lets the user safely call
    // usb_list_free no matter what this function did, or the user could ignore the return code.
    *list = 0;

    // Allocate memory for the list.
    new_list = (usbDeviceList *)malloc(sizeof(usbDeviceList));
    if (new_list == NULL)
    {
        u_error("Failed to allocate memory for USB device list.");
        return USB_ERROR_NO_MEM;
    }

    // Get a list of devices connected.
    new_list->handle = SetupDiGetClassDevsA(NULL, "USB", 0, DIGCF_ALLCLASSES | DIGCF_PRESENT);
    if (new_list->handle == INVALID_HANDLE_VALUE)
    {
        u_win32_error("Unable to get list of USB devices.");
        free(new_list);
        return USB_ERROR_UNEXPECTED;
    }

    // Compute the length
    result = computeLengthOfList(new_list->handle, &new_list->size);
    if (result)
    {
        SetupDiDestroyDeviceInfoList(new_list->handle);
        free(new_list);
        return result;
    }

    // Create the indexMapping array.
    new_list->indexMapping = (uint8_t *)malloc(new_list->size);
    if (new_list->indexMapping == NULL)
    {
        u_error("Failed to allocate memory for USB device list (2).");
        SetupDiDestroyDeviceInfoList(new_list->handle);
        free(new_list);
        return USB_ERROR_NO_MEM;
    }
    for (i = 0; i < new_list->size; i++)
    {
        new_list->indexMapping[i] = i;
    }

    // Return the list and its length to the user.
    *list = new_list;
    return USB_SUCCESS;
}

uint32_t usbListSize(const usbDeviceList * list)
{
    assert(list != NULL);
    return list->size;
}

void usbListFree(usbDeviceList * list)
{
    if (list)
    {
        SetupDiDestroyDeviceInfoList(list->handle);
        if (list->indexMapping){ free(list->indexMapping); }
        free(list);
    }
}

// Takes two Windows USB device lists and checks to see if a device
// (identified by its system index in list1) is also present in list2.
// list1_sys_index must be the system index used by Windows, not the index
// presented to the outside world by this library.
USB_RESULT deviceInBothLists(HDEVINFO list1, uint32_t list1_sys_index, HDEVINFO list2, bool * present)
{
    assert(present != NULL);
    assert(list1 != NULL);
    assert(list2 != NULL);

    *present = false;

    bool success;

    // Get the DEVINST, which we use to uniquely identify this device.
    SP_DEVINFO_DATA device_info_data;
    device_info_data.cbSize = sizeof(device_info_data);
    success = SetupDiEnumDeviceInfo(list1, list1_sys_index, &device_info_data);
    if (!success)
    {
        u_win32_error("Unexpected error getting device info (deviceInBothLists-1).");
        return USB_ERROR_UNEXPECTED;
    }
    DEVINST devInst = device_info_data.DevInst;

    // Search tmp_list to see if the device appears there.
    uint32_t i = 0;
    while (true)
    {
        device_info_data.cbSize = sizeof(device_info_data);
        success = SetupDiEnumDeviceInfo(list2, i, &device_info_data);
        if (!success)
        {
            if (GetLastError() == ERROR_NO_MORE_ITEMS)
            {
                // We reached the end of list2 and did not find the device.
                *present = false;
                return USB_SUCCESS;
            }
            else
            {
                u_win32_error("Unexpected error getting device info in order to filter by Device Interface GUID (deviceInBothLists-2).");
                return USB_ERROR_UNEXPECTED;
            }
        }

        if (device_info_data.DevInst == devInst)
        {
            // Found the device in list2.
            *present = true;
            return USB_SUCCESS;
        }

        i++;
    }
}

USB_RESULT usbListFilterByDeviceInterfaceGuid(usbDeviceList * list, const GUID * deviceInterfaceGuid)
{
    assert(list != NULL);
    assert(deviceInterfaceGuid != NULL);

    // Create a temporary list that just has devices with the specified device interface GUID.
    // (This is just like what we do in usbDeviceOpenFromList.)
    HDEVINFO tmp_list = SetupDiGetClassDevsA(deviceInterfaceGuid, NULL, 0, DIGCF_DEVICEINTERFACE | DIGCF_PRESENT);
    if (tmp_list == INVALID_HANDLE_VALUE)
    {
        u_win32_error("Unable to get a list of USB devices filtered by Device Interface GUID.");
        return USB_ERROR_UNEXPECTED;
    }

    // Remove devices from list that are not present in tmp_list, using the DEVINST field
    // to uniquely identify devices.
    for (uint32_t i = 0; i < usbListSize(list); i++)
    {
        bool inBothLists;
        USB_RESULT result = deviceInBothLists(list->handle, list->indexMapping[i], tmp_list, &inBothLists);
        if (result)
        {
            SetupDiDestroyDeviceInfoList(tmp_list);
            return result;
        }

        if (!inBothLists)
        {
            usbListRemove(list, i);
            i--;
        }
    }

    SetupDiDestroyDeviceInfoList(tmp_list);
    return USB_SUCCESS;
}

USB_RESULT usbListGetDeviceInfo(const usbDeviceList * list, uint32_t index, usbDeviceInfo * info)
{
    if (index >= list->size)
    {
        u_error("Unable to get device info because index parameter is out of range (%d >= %d)", index, list->size);
        return USB_ERROR_OUT_OF_RANGE;
    }

    return usbListGetDeviceInfo_core(list->handle, list->indexMapping[index], info);
}

static USB_RESULT u_get_serial_number(DEVINST device_instance, char * buffer, uint32_t buffer_size)
{
    CONFIGRET cresult;
    errno_t error;
    char id_buffer[100];

    // Get the Device Instance Id string (e.g. "USB\VID_1FFB&PID_0081\01-23-4A-BC").
    cresult = CM_Get_Device_IDA(device_instance, id_buffer, sizeof(id_buffer), 0);
    if (cresult != CR_SUCCESS)
    {
        u_win32_error("Unable to get the device instance id string in order to get serial number.");
        return USB_ERROR_UNEXPECTED;
    }

    error = strcpy_s(buffer, buffer_size, id_buffer + 22);
    if (error)
    {
        u_win32_error("Unable to copy serial number string.");
        return USB_ERROR_UNEXPECTED;
    }

    return USB_SUCCESS;
}


USB_RESULT usbListGetSerialNumber(const usbDeviceList * list, uint32_t index, char * buffer, uint32_t buffer_size)
{
    BOOL success;
    SP_DEVINFO_DATA device_info_data;

    assert(buffer_size != 0);
    assert(buffer != NULL);

    // By default, return an empty string so that the user can
    // easily ignore the return code.
    buffer[0] = 0;

    if (index >= list->size)
    {
        u_error("Unable to get serial number because index parameter is out of range (%d >= %d)", index, list->size);
        return USB_ERROR_OUT_OF_RANGE;
    }

    // Get the device instance number (DEVINST); it is part of SP_DEVINFO_DATA.
    device_info_data.cbSize = sizeof(device_info_data);
    success = SetupDiEnumDeviceInfo(list->handle, list->indexMapping[index], &device_info_data);
    if (!success)
    {
        u_win32_error("Unexpected error getting device info (for serial number).");
        return USB_ERROR_UNEXPECTED;
    }

    return u_get_serial_number(device_info_data.DevInst, buffer, buffer_size);
}

USB_RESULT usbDeviceOpenFromList(const usbDeviceList * list, uint32_t index, usbDevice ** device, const GUID * deviceInterfaceGuid)
{
    BOOL success;
    SP_DEVINFO_DATA device_info_data;
    DEVINST devInst;
    HDEVINFO tmp_list;
    uint32_t i;
    SP_DEVICE_INTERFACE_DATA device_interface_data;
    SP_DEVICE_INTERFACE_DETAIL_DATA_A * device_interface_detail_data;
    DWORD RequiredSize;
    HANDLE deviceHandle;
    WINUSB_INTERFACE_HANDLE winusbHandle;
    unsigned int triesLeft;

    /* Check parameters *******************************************************/
    assert(device != NULL);
    assert(index < list->size);
    *device = 0;

    /* Get the device instance ************************************************/
    device_info_data.cbSize = sizeof(device_info_data);
    success = SetupDiEnumDeviceInfo(list->handle, list->indexMapping[index], &device_info_data);
    if (!success)
    {
        u_win32_error("Unexpected error getting device info in order to open a handle (1).", index, list->size);
        return USB_ERROR_UNEXPECTED;
    }
    devInst = device_info_data.DevInst;

    /* Create a new temporary Device Information Set which contains the interface we want **/
    tmp_list = SetupDiGetClassDevsA(deviceInterfaceGuid, NULL, 0, DIGCF_DEVICEINTERFACE | DIGCF_PRESENT);
    if (tmp_list == INVALID_HANDLE_VALUE)
    {
        u_win32_error("Unable to get a list of USB devices filtered by Device Interface GUID in order to open device.");
        SetupDiDestroyDeviceInfoList(tmp_list);
        return USB_ERROR_UNEXPECTED;
    }

    /* Find the DEVINST that corresponds to our device in that list ***********/
    i = 0;
    while(true)
    {
        device_info_data.cbSize = sizeof(device_info_data);
        success = SetupDiEnumDeviceInfo(tmp_list, i, &device_info_data);
        if (!success)
        {
            if (GetLastError() == ERROR_NO_MORE_ITEMS)
            {
                // This can happen if the drivers are not installed properly
                // or if the device was very recently plugged into the computer
                // and hasn't fully been set up yet.
                u_error("Device interface not found.  Look in the Device Manager to make sure the drivers have finished installing, and try restarting the computer.");
            }
            else
            {
                u_win32_error("Unexpected error getting device info in order to open a handle (2).", index, list->size);
            }
            SetupDiDestroyDeviceInfoList(tmp_list);
            return USB_ERROR_UNEXPECTED;
        }

        if (device_info_data.DevInst == devInst)
        {
            // We found the device.
            break;
        }

        i++;
    }

    /* Get the DeviceInterfaceData struct *************************************/
    device_interface_data.cbSize = sizeof(device_interface_data);
    success = SetupDiEnumDeviceInterfaces(tmp_list, &device_info_data, deviceInterfaceGuid, 0, &device_interface_data);
    if (!success)
    {
        u_win32_error("Unexpected error getting device interface data to open USB device %d out of %d.", index, list->size);
        SetupDiDestroyDeviceInfoList(tmp_list);
        return USB_ERROR_UNEXPECTED;
    }

    /* Get the DeviceInterfaceDetailData struct size **************************/
    success = SetupDiGetDeviceInterfaceDetail(tmp_list, &device_interface_data, NULL, 0, &RequiredSize, NULL);
    if (!success && GetLastError() != ERROR_INSUFFICIENT_BUFFER)
    {
        u_win32_error("Unexpected error getting the size of the device interface detail data struct.");
        SetupDiDestroyDeviceInfoList(tmp_list);
        return USB_ERROR_UNEXPECTED;
    }

    /* Get the DeviceInterfaceDetailData struct data **************************/
    device_interface_detail_data = (SP_DEVICE_INTERFACE_DETAIL_DATA_A *)malloc(RequiredSize);
    if (device_interface_detail_data == 0)
    {
        u_error("Unable to allocate %d bytes for device interface detail data struct.", RequiredSize);
        SetupDiDestroyDeviceInfoList(tmp_list);
        return USB_ERROR_NO_MEM;
    }
    device_interface_detail_data->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_A);
    success = SetupDiGetDeviceInterfaceDetailA(tmp_list, &device_interface_data, device_interface_detail_data, RequiredSize, &RequiredSize, NULL);
    if (!success)
    {
        u_win32_error("Unexpected error getting the device interface detail data struct.");
        free(device_interface_detail_data);
        SetupDiDestroyDeviceInfoList(tmp_list);
        return USB_ERROR_UNEXPECTED;
    }

    /* Get the device handle and free DeviceInterfaceDetailData ***************/
    triesLeft = 10;
    while(true)
    {
        deviceHandle = CreateFileA(device_interface_detail_data->DevicePath,
            GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
            NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);

        if (deviceHandle != INVALID_HANDLE_VALUE)
        {
            break;
        }

        if (GetLastError() == ERROR_ACCESS_DENIED)
        {
            // We got an access denied error, so retry up to 10 times.
            // This is necessary for Wixels since the Wixel Configuration Utility
            // does USB IO on every Wixel Bootloader every second.

            triesLeft--;
            if (triesLeft > 0)
            {
                Sleep(10);
                continue;
            }

            u_error("Access denied when trying to open device.  Try closing all other programs that are using the device.");
            free(device_interface_detail_data);
            SetupDiDestroyDeviceInfoList(tmp_list);
            return USB_ERROR_ACCESS;
        }
        else
        {
            u_win32_error("Unexpected error opening the device.");
            free(device_interface_detail_data);
            SetupDiDestroyDeviceInfoList(tmp_list);
            return USB_ERROR_UNEXPECTED;
        }
    }
    free(device_interface_detail_data);
    SetupDiDestroyDeviceInfoList(tmp_list);

    /* Make a WinUSB Interface Handle *****************************************/
    success = WinUsb_Initialize(deviceHandle, &winusbHandle);
    if (!success)
    {
        u_win32_error("Unexpected error creating WinUSB handle for the device.");
        return USB_ERROR_UNEXPECTED;
    }

    ULONG timeout = usbControlTransferTimeout;
    success = WinUsb_SetPipePolicy(winusbHandle, 0, PIPE_TRANSFER_TIMEOUT, sizeof(timeout), &timeout);
    if (!success)
    {
        u_win32_warning("Unexpected error while setting the control transfer timeout period.");
    }

    /* Create the usb_device struct and return ********************************/
    *device = (usbDevice *)malloc(sizeof(struct usbDevice));
    if (*device == 0)
    {
        u_error("Failed to allocate memory for usb_device struct.");
        return USB_ERROR_NO_MEM;
    }

    (*device)->deviceInstance = device_info_data.DevInst;
    (*device)->deviceHandle = deviceHandle;
    (*device)->winusbInterfaceHandle = winusbHandle;

    return USB_SUCCESS;
}

void usbDeviceClose(usbDevice * device)
{
    WinUsb_Free(device->winusbInterfaceHandle);
    CloseHandle(device->deviceHandle);
    free(device);
}

USB_RESULT usbDeviceControlTransfer(usbDevice * device, uint8_t bmRequestType, uint8_t bRequest, uint16_t wValue, uint16_t wIndex, void * data, uint16_t wLength, uint32_t * lengthTransferred)
{
    WINUSB_SETUP_PACKET setup_packet;
    ULONG length = 0;
    BOOL success;

    if (lengthTransferred){ *lengthTransferred = 0; }

    assert(wLength == 0 || data != NULL);

    setup_packet.RequestType = bmRequestType;
    setup_packet.Request = bRequest;
    setup_packet.Value = wValue;
    setup_packet.Index = wIndex;
    setup_packet.Length = wLength;
    success = WinUsb_ControlTransfer(device->winusbInterfaceHandle, setup_packet, (void *)data, wLength, &length, NULL);
    if (!success)
    {
        const char * errorString;
        USB_RESULT error;

        switch(GetLastError())
        {
        case ERROR_GEN_FAILURE:
            errorString = "Control transfer failed: general failure.";
            error = USB_ERROR_PIPE;
            break;

        case ERROR_SEM_TIMEOUT:     // This code can be returned by WinUsb_ControlTransfer when we do synchronous transfers.
        case ERROR_IO_INCOMPLETE:   // This code can be returned by WaitForSingleObject when we do asynchronous transfers.
        case ERROR_TIMEOUT:
            errorString = "Control transfer timed out.";
            error = USB_ERROR_TIMEOUT;
            break;

        default:
            u_win32_error("Control transfer failed.");
            return USB_ERROR_UNEXPECTED;
        }

        u_error("%s (%02x %02x %02x %02x %02x): %s", errorString, bmRequestType, bRequest, wValue, wIndex, wLength, usbGetErrorDescription(error));
        return error;
    }

    if (lengthTransferred){ *lengthTransferred = length; }

    return USB_SUCCESS;
}

void usbVerbosityChanged()
{
}
