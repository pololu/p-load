#pragma once

// Remove some warnings about safety in Windows.
#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#endif

// For Debug builds in Windows, enable memory leak tracking.
#if defined(_MSC_VER) && defined(_DEBUG)
#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>
#endif

#ifndef _MSC_VER
#include <unistd.h>
#endif

#ifdef _MSC_VER
#include <io.h>
#define isatty _isatty
#define fileno _fileno
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <time.h>

#include <vector>
#include <string>
#include <system_error>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>

#include <libusbp.hpp>

#include "version.h"
#include "exit_codes.h"
#include "output.h"
#include "arg_reader.h"
#include "ploader.h"
#include "device_selector.h"
#include "intel_hex.h"
#include "firmware_archive.h"
#include "firmware_data.h"
#include "file_utils.h"

typedef std::vector<uint8_t> MemoryImage;
