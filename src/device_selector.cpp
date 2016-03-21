#include "p-load.h"
#include "device_selector.h"

DeviceSelector::DeviceSelector()
{
    serialNumberSpecified = false;
    typesSpecified = false;
    firmwareDataSpecified = false;
    userTypeSpecified = false;
    appListInitialized = false;
    bootloaderListInitialized = false;
}

void DeviceSelector::specifySerialNumber(const std::string & str)
{
    assert(!serialNumberSpecified);
    assert(!appListInitialized);
    assert(!appSelected && !app);
    assert(!bootloaderListInitialized);
    assert(!bootloader);

    serialNumber = str;
    serialNumberSpecified = true;
}

void DeviceSelector::specifyFirmwareData(const FirmwareData & data)
{
    assert(!appListInitialized);
    assert(!appSelected && !app);
    assert(!bootloader);
    assert(!bootloaderListInitialized);

    if (data.firmwareArchiveData)
    {
        if (userTypeSpecified)
        {
            // Types were already specified by the user, so we should not infer
            // bootloader/app types from the firmware archive.  Adding to the
            // set of allowed bootloaders or apps here would be bad because it
            // diminishes the control that specifyUserType ("-t") has.
            // Restricting the set of allowed apps might be OK, but I don't
            // see why it would be needed.
            // Restricting the set of allowed bootloaders might be OK, but that
            // will get checked later before we do anything to the bootloader.
            return;
        }

        for (const FirmwareArchive::Image & image : data.firmwareArchiveData.images)
        {
            uint16_t usbVendorId = image.usbVendorId;
            uint16_t usbProductId = image.usbProductId;
            const PloaderType * type = ploaderTypeLookup(usbVendorId, usbProductId);
            if (type == NULL) { continue; }

            bootloaderTypes.push_back(*type);

            for (const PloaderAppType & appType : type->getMatchingAppTypes())
            {
                appTypes.push_back(appType);
            }
        }
        typesSpecified = true;
        firmwareDataSpecified = true;
    }
}

void DeviceSelector::specifyUserType(const PloaderUserType & userType)
{
    assert(!appListInitialized);
    assert(!appSelected && !app);
    assert(!bootloader);
    assert(!bootloaderListInitialized);

    // This assertion is required by the logic in specifyFirmwareData.
    // Eventually, it would be nicer to refactor things to allow user types and
    // firmware data to be specified in any order.
    assert(!firmwareDataSpecified);

    // It is fine for to specifiy multiple high-level types.  The sets of types
    // get added together rather than taking an intersection.  This behavior
    // will be necessary if the device is running an app that has a different
    // high-level type than its bootloader; the user could specify the
    // "-t" option twice to support that case.

    for (const PloaderAppType & appType : userType.getMatchingAppTypes())
    {
        appTypes.push_back(appType);
    }

    for (const PloaderType & type : userType.getMatchingTypes())
    {
        bootloaderTypes.push_back(type);
    }

    typesSpecified = true;
    userTypeSpecified = true;
}

bool DeviceSelector::serialNumberWasSpecified() const
{
    return serialNumberSpecified;
}

void DeviceSelector::clearDeviceLists()
{
    assert(!bootloader);
    appListInitialized = false;
    appList.clear();
    bootloaderListInitialized = false;
    bootloaderList.clear();
}

template<typename T>
static std::vector<T> filterBySerialNumber(
    const std::vector<T> & in, std::string serialNumber)
{
    std::vector<T> out;
    for (const T & item : in)
    {
        if (serialNumber == item.serialNumber)
        {
            out.push_back(item);
        }
    }
    return out;
}

template<typename L, typename T>
static std::vector<L> filterByType(
    const std::vector<L> & in, const std::vector<T> & types)
{
    std::vector<L> out;
    for (const L & item : in)
    {
        const T & actualType = item.type;

        for (const T & type : types)
        {
            if (type == actualType)
            {
                out.push_back(item);
                break;
            }
        }
    }
    return out;
}

std::vector<PloaderAppInstance> DeviceSelector::listApps()
{
    if (!appListInitialized)
    {
        appListInitialized = true;
        appList = ploaderListApps();

        if (serialNumberSpecified)
        {
            appList = filterBySerialNumber(appList, serialNumber);
        }

        if (typesSpecified)
        {
            appList = filterByType(appList, appTypes);
        }
    }
    return appList;
}

std::vector<PloaderInstance> DeviceSelector::listBootloaders()
{
    if (!bootloaderListInitialized)
    {
        assert(!bootloader);

        bootloaderListInitialized = true;
        bootloaderList = ploaderListBootloaders();

        if (serialNumberSpecified)
        {
            bootloaderList = filterBySerialNumber(bootloaderList, serialNumber);
        }

        if (app)
        {
            bootloaderList = filterBySerialNumber(bootloaderList, app.serialNumber);
        }

        if (typesSpecified)
        {
            bootloaderList = filterByType(bootloaderList, bootloaderTypes);
        }

    }
    return bootloaderList;
}

PloaderAppInstance DeviceSelector::selectAppToLaunchBootloader()
{
    if (appSelected) { return app; }

    appSelected = true;

    assert(!app);
    assert(!bootloader);

    auto appList = listApps();
    auto bootloaderList = listBootloaders();

    if (bootloaderList.size() + appList.size() > 1)
    {
        throw deviceMultipleFoundError();
    }

    if (appList.size() > 0)
    {
        // There is one matching device and it is in app mode, so we will need
        // to restart it.
        app = appList[0];
    }
    else
    {
        // Return the null app.
    }

    return app;
}

PloaderInstance DeviceSelector::selectBootloader()
{
    if (bootloader) { return bootloader; }

    auto appList = listApps();
    auto bootloaderList = listBootloaders();

    if (bootloaderList.size() == 0)
    {
        throw deviceNotFoundError();
    }

    if (appList.size() + bootloaderList.size() > 1)
    {
        throw deviceMultipleFoundError();
    }

    bootloader = bootloaderList[0];
    return bootloader;
}

std::string DeviceSelector::deviceNotFoundMessage() const
{
    std::string r = "No device found";

    if (typesSpecified)
    {
        r += " of the specified type";
    }

    if (serialNumberSpecified)
    {
        r += std::string(" with serial number '") +
            serialNumber + "'";
    }

    r += ".";
    return r;
}

ExceptionWithExitCode DeviceSelector::deviceNotFoundError() const
{
    return ExceptionWithExitCode(PLOAD_ERROR_DEVICE_NOT_FOUND,
        deviceNotFoundMessage());
}

ExceptionWithExitCode DeviceSelector::deviceMultipleFoundError() const
{
    return ExceptionWithExitCode(PLOAD_ERROR_DEVICE_MULTIPLE_FOUND,
        "There are multiple qualifying devices connected to this computer.\n"
        "Use the -t or -d options to specify which device you want to use,\n"
        "or disconnect the others.");
}
