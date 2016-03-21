/* Main source file for p-load, the Pololu USB Bootloader Utility. */

#include "p-load.h"

static const char help[] =
    "p-load: Pololu USB Bootloader Utility\n"
    "Version " VERSION "\n"
    "Usage: p-load OPTIONS\n"
    "\n"
    "Options available:\n"
    "  -t TYPE                     Specifies device type (e.g. p-star).\n"
    "  -d SERIALNUMBER             Specifies the serial number of the device.\n"
    "  --list                      Lists devices connected to computer.\n"
    "  --list-supported            Lists all supported device types.\n"
    "  --start-bootloader          Gets the device into bootloader mode.\n"
    "  --wait                      Waits up to 10 seconds for bootloader to appear.\n"
    "  -w FILE                     Writes to device, then restarts it.\n"
    "  --write FILE                Writes to device.\n"
    "  --write-flash HEXFILE       Writes to flash only.\n"
    "  --write-eeprom HEXFILE      Writes to EEPROM only.\n"
    "  --erase                     Erases device.\n"
    "  --erase-flash               Erases flash only.\n"
    "  --erase-eeprom              Erases EEPROM only.\n"
    "  --read HEXFILE              Reads from device and saves to file.\n"
    "  --read-flash HEXFILE        Reads flash only and saves to file.\n"
    "  --read-eeprom HEXFILE       Reads EEPROM only and saves to file.\n"
    "  --restart                   Restarts the device so it can run the new code.\n"
    "  --pause-on-error            Pause at the end if an error happens.\n"
    "  --pause                     Pause at the end.\n"
    "  -h, --help                  Show this help screen.\n"
    "\n"
    "HEXFILE is the name of the .HEX file to be used.\n"
    "FILE is the name of the .HEX or .FMI file to be used.\n"
    "\n"
    "Example: p-load -t p-star -w app.hex\n"
    "Example: p-load -w pgm04a-v1.00.fmi\n"
    "Example: p-load -d 12345678 --wait --write-flash app.hex --restart\n"
    "Example: p-load -t p-star --erase\n"
    "\n";

// GCC 4.6 doesn't support the override keyword.
#if defined(__GNUC__) && __GNUC__ <= 4
#define override
#endif

class Action;

static DeviceSelector selector;

static Output output;

// These variables store the results from parsing the command-line arguments.
static std::vector<Action *> actions;
static bool showHelpFlag = false;
static bool listDevicesFlag = false;
static bool listSupportedFlag = false;
static bool startBootloaderFlag = false;
static bool waitForBootloaderFlag = false;
static bool restartBootloaderFlag = false;
static bool pauseFlag = false;
static bool pauseOnErrorFlag = false;

// True if we have printed the name and serial number of the device we are
// operating on.
static bool deviceInfoPrinted = false;

// Returns true if we actually want to get to the state where a bootloader is
// connected to the computer and we have selected it.
static bool bootloaderHandleNeeded()
{
    return startBootloaderFlag ||
        restartBootloaderFlag ||
        actions.size() > 0;
}

// Returns true if some sort of action was specified on the command line.
static bool someCommandSpecified()
{
    return showHelpFlag ||
        listDevicesFlag ||
        listSupportedFlag ||
        startBootloaderFlag ||
        waitForBootloaderFlag ||
        restartBootloaderFlag ||
        pauseFlag ||
        pauseOnErrorFlag ||
        actions.size() > 0;
}

template<typename T> static void printUsbIdsAndName(const T & item)
{
    std::cout << "  " << std::hex << std::nouppercase << std::setfill('0');
    std::cout << std::setw(4) << item.usbVendorId << ":";
    std::cout << std::setw(4) << item.usbProductId << ": ";
    std::cout << item.name << std::endl;
}

static void listSupported()
{
    std::cout << "Supported device types:" << std::endl;
    for (const PloaderUserType & userType : ploaderUserTypes)
    {
        std::cout << "  " << userType.codeName << ": "
                  << userType.name << std::endl;
    }
    std::cout << std::endl;

    std::cout << "Supported devices by USB vendor ID and product ID:" << std::endl;
    for (const PloaderAppType & type : ploaderAppTypes)
    {
        printUsbIdsAndName(type);
    }
    for (const PloaderType & type : ploaderTypes)
    {
        printUsbIdsAndName(type);
    }
}

// Returns a human-readable string representing what state the bootloader
// is in, for use in the "list" action.
static const char * getStatus(const PloaderInstance & instance)
{
    try
    {
        PloaderHandle handle(instance);
        return handle.checkApplication() ? "App present" : "No app present";
    }
    catch(const libusbp::error & error)
    {
        return "?";
    }
}

static void printListItem(std::string serialNumber, std::string name, std::string status)
{
    std::cout << std::left << std::setfill(' ');
    std::cout << std::setw(17) << serialNumber + "," << " ";
    std::cout << std::setw(45) << name + "," << " ";
    std::cout << status;
    std::cout << std::endl;
}

static void printSelectedDeviceInfo(std::string name, std::string serialNumber)
{
    std::cout << "Device:        " << name << std::endl;
    std::cout << "Serial number: " << serialNumber << std::endl;
}

// Prints a list of bootloaders and apps connected to the computer.
static void listDevices()
{
    auto bootloaderList = selector.listBootloaders();
    auto appList = selector.listApps();

    for (const PloaderInstance & instance : bootloaderList)
    {
        printListItem(
            instance.serialNumber,
            instance.type.name,
            getStatus(instance));
    }

    for (const PloaderAppInstance & instance : appList)
    {
        printListItem(
            instance.serialNumber,
            instance.type.name,
            "App running");
    }

    if (bootloaderList.size() == 0 && appList.size() == 0
        && output.shouldPrintInfo())
    {
        std::cout << selector.deviceNotFoundMessage() << std::endl;
    }
}

static bool launchBootloaderIfNeeded()
{
    if (!bootloaderHandleNeeded())
    {
        return false;
    }

    PloaderAppInstance app = selector.selectAppToLaunchBootloader();
    if (!app)
    {
        return false;
    }

    if (output.shouldPrintInfo() && !deviceInfoPrinted)
    {
        deviceInfoPrinted = true;
        printSelectedDeviceInfo(app.type.name, app.serialNumber);
    }

    app.launchBootloader();

    if (output.shouldPrintInfo())
    {
        std::cout << "Sent command to start bootloader." << std::endl;
    }

    return true;
}

static PloaderHandle bootloaderHandleOpen()
{
    PloaderInstance instance = selector.selectBootloader();

    if (output.shouldPrintInfo() && !deviceInfoPrinted)
    {
        deviceInfoPrinted = true;
        printSelectedDeviceInfo(instance.type.name, instance.serialNumber);
    }

    PloaderHandle handle(instance);
    handle.setStatusListener(&output);
    return handle;
}

static void waitForBootloader()
{
    auto bootloaderList = selector.listBootloaders();
    if (bootloaderList.size () > 0)
    {
        return;
    }

    if (output.shouldPrintInfo())
    {
        std::cout << "Waiting for bootloader..." << std::endl;
    }

    time_t waitStartTime = time(NULL);

    while(1)
    {
        auto bootloaderList = selector.listBootloaders();

        if (bootloaderList.size() > 0)
        {
            return;
        }

        if (difftime(time(NULL), waitStartTime) > 10)
        {
            throw selector.deviceNotFoundError();
        }

	// Sleep so that we don't take up 100% CPU time.
#ifdef _MSC_VER
        Sleep(100);     // 100 ms
#else
        usleep(100000); // 100 ms
#endif

        // The previous lists of devices we had are now stale because we
        // delayed.  Clear them.  (This is our way of telling the device
        // selector that the program is delaying; the function could have been
        // named something like "handleDelay" just as well.)
        selector.clearDeviceLists();
    }
}

static void restartBootloader(PloaderHandle & handle)
{
    handle.restartDevice();

    if (output.shouldPrintInfo())
    {
        std::cout << "Sent command to restart device." << std::endl;
    }
}

/* Every Action represents a read or write from memory on the bootloader.
 * If any actions are specified by the user, we will attempt to get
 * the device into bootloader mode and open a handle to the bootloader. */
class Action
{
public:

    // Parses arguments and stores anything it will need for later.
    // Does not open any handles to external devices.
    virtual void parseArguments(ArgReader &) { }

    virtual void readFiles() { }

    // Raises an exception if this action is not compatible with the selected
    // bootloader.
    virtual void ensureBootloaderCompatibility(const PloaderHandle &) = 0;

    virtual void writeFiles() { }

    // Actually executes the action.
    virtual void execute(PloaderHandle &) = 0;

    virtual ~Action() { }
};

class ActionWriteMemory : public Action
{
public:
    ActionWriteMemory(MemorySet ms) : fileName(NULL), memorySet(ms) { }

    void parseArguments(ArgReader & argReader) override
    {
        const char * arg = argReader.next();
        if (arg == NULL)
        {
            throw ExceptionWithExitCode(PLOAD_ERROR_BAD_ARGS,
                std::string("Expected a filename after ") + argReader.last() + ".");
        }
        fileName = arg;
    }

    void readFiles() override
    {
        assert(fileName != NULL);
        assert(!data);
        data.readFromFile(fileName);
        selector.specifyFirmwareData(data);
    }

    void ensureBootloaderCompatibility(const PloaderHandle & handle) override
    {
        data.ensureBootloaderCompatibility(handle.type, memorySet);
    }

    void execute(PloaderHandle & handle) override
    {
        data.writeToBootloader(handle, memorySet);
    }

private:
    const char * fileName;
    FirmwareData data;
    MemorySet memorySet;
};

class ActionEraseMemory : public Action
{
public:
    ActionEraseMemory(MemorySet ms) : memorySet(ms) { }

    void ensureBootloaderCompatibility(const PloaderHandle & handle) override
    {
        handle.type.ensureErasing(memorySet);
    }

    void execute(PloaderHandle & handle) override
    {
        if (handle.type.memorySetIncludesFlash(memorySet))
        {
            handle.initialize();
            handle.eraseFlash();
        }

        if (handle.type.memorySetIncludesEeprom(memorySet))
        {
            handle.eraseEeprom();
        }
    }

    MemorySet memorySet;
};

class ActionReadMemory : public Action
{
public:
    ActionReadMemory(MemorySet ms) : fileName(NULL), memorySet(ms) { }

    void parseArguments(ArgReader & argReader) override
    {
        const char * arg = argReader.next();
        if (arg == NULL)
        {
            throw ExceptionWithExitCode(PLOAD_ERROR_BAD_ARGS,
                std::string("Expected a filename after ") + argReader.last() + ".");
        }
        fileName = arg;
    }

    void ensureBootloaderCompatibility(const PloaderHandle & handle) override
    {
        handle.type.ensureReading(memorySet);
    }

    void execute(PloaderHandle & handle) override
    {
        const PloaderType & type = handle.type;

        // Read from the bootloader's flash if needed.
        if (type.memorySetIncludesFlash(memorySet))
        {
            MemoryImage flash(type.appSize);
            handle.readFlash(&flash[0]);
            hexData.setImage(type.appAddress, flash);
        }

        // Read from the bootloader's EEPROM if needed.
        if (type.memorySetIncludesEeprom(memorySet))
        {
            MemoryImage eeprom(type.eepromSize);
            handle.readEeprom(&eeprom[0]);
            hexData.setImage(type.eepromAddressHexFile, eeprom);
        }
    }

    void writeFiles() override
    {
        assert(fileName != NULL);
        assert(hexData);

        auto filePtr = openFileOrPipeOutput(fileName);
        hexData.writeToFile(*filePtr);
    }

private:
    const char * fileName;
    IntelHex::Data hexData;
    MemorySet memorySet;
};

void addAction(Action * action, ArgReader & argReader)
{
    action->parseArguments(argReader);
    actions.push_back(action);
}

void parseArgs(int argc, char ** argv)
{
    ArgReader argReader(argc, argv);

    while (1)
    {
        const char * argCStr = argReader.next();

        if (argCStr == NULL)
        {
            break;  // Done reading arguments.
        }

        std::string arg = argCStr;

        if (arg == "-t")
        {
            const char * s = argReader.next();
            if (s == NULL)
            {
                throw ExceptionWithExitCode(PLOAD_ERROR_BAD_ARGS,
                    "Expected a device type after '" + std::string(argReader.last()) + "'.");
            }
            const PloaderUserType * userType = ploaderUserTypeLookup(s);
            if (userType == NULL)
            {
                throw ExceptionWithExitCode(PLOAD_ERROR_BAD_ARGS,
                    "Invalid device type '" + std::string(s) + "'.");
            }
            selector.specifyUserType(*userType);
        }
        else if (arg == "-d")
        {
            if (selector.serialNumberWasSpecified())
            {
                throw ExceptionWithExitCode(PLOAD_ERROR_BAD_ARGS,
                    "A serial number can only be specified once.");
            }
            const char * s = argReader.next();
            if (s == NULL)
            {
                throw ExceptionWithExitCode(PLOAD_ERROR_BAD_ARGS,
                    "Expected a serial number after '" + std::string(argReader.last()) + "'.");
            }
            if (s[0] == 0)
            {
                throw ExceptionWithExitCode(PLOAD_ERROR_BAD_ARGS,
                    "An empty serial number was specified.");
            }
            selector.specifySerialNumber(s);
        }
        else if (arg == "--list")
        {
            listDevicesFlag = true;
        }
        else if (arg == "--list-supported")
        {
            listSupportedFlag = true;
        }
        else if (arg == "--start-bootloader")
        {
            startBootloaderFlag = true;
        }
        else if (arg == "--wait")
        {
            waitForBootloaderFlag = true;
        }
        else if (arg == "-w")
        {
            addAction(new ActionWriteMemory(MEMORY_SET_ALL), argReader);
            restartBootloaderFlag = true;
        }
        else if (arg == "--write")
        {
            addAction(new ActionWriteMemory(MEMORY_SET_ALL), argReader);
        }
        else if (arg == "--write-flash")
        {
            addAction(new ActionWriteMemory(MEMORY_SET_FLASH), argReader);
        }
        else if (arg == "--write-eeprom")
        {
            addAction(new ActionWriteMemory(MEMORY_SET_EEPROM), argReader);
        }
        else if (arg == "--erase")
        {
            addAction(new ActionEraseMemory(MEMORY_SET_ALL), argReader);
        }
        else if (arg == "--erase-flash")
        {
            addAction(new ActionEraseMemory(MEMORY_SET_FLASH), argReader);
        }
        else if (arg == "--erase-eeprom")
        {
            addAction(new ActionEraseMemory(MEMORY_SET_EEPROM), argReader);
        }
        else if (arg == "--read")
        {
            addAction(new ActionReadMemory(MEMORY_SET_ALL), argReader);
        }
        else if (arg == "--read-flash")
        {
            addAction(new ActionReadMemory(MEMORY_SET_FLASH), argReader);
        }
        else if (arg == "--read-eeprom")
        {
            addAction(new ActionReadMemory(MEMORY_SET_EEPROM), argReader);
        }
        else if (arg == "--restart")
        {
            restartBootloaderFlag = true;
        }
        else if (arg == "--pause")
        {
            pauseFlag = true;
        }
        else if (arg == "--pause-on-error")
        {
            pauseOnErrorFlag = true;
        }
        else if (arg == "-h" || arg == "--help")
        {
            showHelpFlag = true;
        }
        else
        {
            throw ExceptionWithExitCode(PLOAD_ERROR_BAD_ARGS,
                std::string("Unknown option: '") + arg + "'.");
        }
    }

    if (!someCommandSpecified())
    {
        throw ExceptionWithExitCode(PLOAD_ERROR_BAD_ARGS,
            "Arguments do not specify anything to do.");
    }
}

static void run(int argc, char ** argv)
{
    parseArgs(argc, argv);

    if (showHelpFlag)
    {
        std::cout << help;
        return;
    }

    if (listDevicesFlag)
    {
        if (waitForBootloaderFlag)
        {
            waitForBootloader();
        }
        listDevices();
        return;
    }

    if (listSupportedFlag)
    {
        listSupported();
        return;
    }

    for (Action * action : actions)
    {
        action->readFiles();
    }

    bool launchedBootloader = launchBootloaderIfNeeded();

    if (launchedBootloader || waitForBootloaderFlag)
    {
        waitForBootloader();
    }

    if (bootloaderHandleNeeded())
    {
        PloaderHandle handle = bootloaderHandleOpen();

        for (Action * action : actions)
        {
            action->ensureBootloaderCompatibility(handle);
        }

        for (Action * action : actions)
        {
            action->execute(handle);
        }

        for (Action * action : actions)
        {
            action->writeFiles();
        }

        if (restartBootloaderFlag)
        {
            restartBootloader(handle);
        }
    }
}

// main: This is the first function to run when this utility is started.
int main(int argc, char ** argv)
{
#if defined(_MSC_VER) && defined(_DEBUG)
    // For a Debug build in Windows, send a report of memory leaks to
    // the Debug pane of the Output window in Visual Studio.
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

    if (argc <= 1)
    {
        std::cout << help;
        return 0;
    }

    int exitCode = 0;

    try
    {
        run(argc, argv);
    }
    catch(const ExceptionWithExitCode & error)
    {
        output.startNewLine();
        std::cerr << "Error: " << error.what() << std::endl;
        exitCode = error.getCode();
    }
    catch(const std::exception & error)
    {
        output.startNewLine();
        std::cerr << "Error: " << error.what() << std::endl;
        exitCode = PLOAD_ERROR_OPERATION_FAILED;
    }

    // Free the memory for the actions.
    for (Action * action : actions)
    {
        delete action;
    }
    actions.clear();

    if (pauseFlag || (pauseOnErrorFlag && exitCode))
    {
        std::cout << "Press enter to continue." << std::endl;
        char input;
        std::cin.get(input);
    }

    return exitCode;
}
