#pragma once

#include "ploader.h"
#include "firmware_data.h"
#include "exit_codes.h"

/* A DeviceSelector object contains logic for finding bootloaders and apps and
 * selecting the right ones to operate on. */
class DeviceSelector
{
public:
    DeviceSelector();

    void specifySerialNumber(const std::string &);
    void specifyFirmwareData(const FirmwareData &);
    void specifyUserType(const PloaderUserType &);

    void clearDeviceLists();

    std::vector<PloaderAppInstance> listApps();
    std::vector<PloaderInstance> listBootloaders();

    PloaderAppInstance selectAppToLaunchBootloader();
    PloaderInstance selectBootloader();

    bool serialNumberWasSpecified() const;

    std::string deviceNotFoundMessage() const;
    ExceptionWithExitCode deviceNotFoundError() const;
    ExceptionWithExitCode deviceMultipleFoundError() const;

private:
    bool appSelected;
    PloaderAppInstance app;

    PloaderInstance bootloader;

    bool serialNumberSpecified;
    std::string serialNumber;

    bool typesSpecified;
    bool userTypeSpecified;
    bool firmwareDataSpecified;
    std::vector<PloaderAppType> appTypes;
    std::vector<PloaderType> bootloaderTypes;

    bool appListInitialized;
    std::vector<PloaderAppInstance> appList;

    bool bootloaderListInitialized;
    std::vector<PloaderInstance> bootloaderList;
};
