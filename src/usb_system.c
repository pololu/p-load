/* This file contains functions for the usb_system API that are
 * not operating-system specific. */

#include <string.h>
#include "usb_system.h"

int usbControlTransferTimeout = 300;

int usbVerbosity = 0;

void usbSetVerbosity(int newVerbosity)
{
    usbVerbosity = newVerbosity;
    usbVerbosityChanged();
}

USB_RESULT usbListFilterBySerialNumber(usbDeviceList * list, const char * serialNumber)
{
    for (uint32_t i = 0; i < usbListSize(list); i++)
    {
        char actualSerialNumber[USB_SERIAL_NUMBER_MAX_SIZE];
        USB_RESULT result = usbListGetSerialNumber(list, i, actualSerialNumber, USB_SERIAL_NUMBER_MAX_SIZE);
        if (result){ return result; }

        if (strcmp(actualSerialNumber, serialNumber) != 0)
        {
            usbListRemove(list, i);
            i--;
        }
    }
    return USB_SUCCESS;
}

#ifndef _WIN32
USB_RESULT usbListFilterByDeviceInterfaceGuid(usbDeviceList * list, const GUID * deviceInterfaceGuid)
{
    assert(list != NULL);
    assert(deviceInterfaceGuid != NULL);
    return USB_SUCCESS;
}
#endif