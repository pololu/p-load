use_cxx11()

pkg_check_modules(LIBUSBP REQUIRED libusbp-1)
STRING(REPLACE ";" " " LIBUSBP_LDFLAGS "${LIBUSBP_LDFLAGS}")

if (USE_SYSTEM_TINYXML2)
  pkg_check_modules(TINYXML2 REQUIRED tinyxml2)
  STRING(REPLACE ";" " " TINYXML2_LDFLAGS "${TINYXML2_LDFLAGS}")
else ()
  set (TINYXML2_LDFLAGS tinyxml2)
  include_directories ("${CMAKE_SOURCE_DIR}/tinyxml2")
endif ()

include_directories (
  "${CMAKE_CURRENT_BINARY_DIR}"
)

set (CMAKE_CXX_FLAGS "${LIBUSBP_CFLAGS} ${TINYXML2_CFLAGS} ${CMAKE_CXX_FLAGS}")
# Define cross-platform source files.
set (sources
  intel_hex.cpp
  output.cpp
  ploader.cpp
  ploader_data.cpp
  device_selector.cpp
  p-load.cpp
  firmware_data.cpp
  firmware_archive.cpp
  file_utils.cpp)

# Define operating system-specific source files.
if (WIN32)
  set (sources ${sources}  ${CMAKE_CURRENT_BINARY_DIR}/p-load.rc)
elseif (LINUX)
elseif (APPLE)
endif ()

add_executable (p-load ${sources})

target_link_libraries(p-load "${LIBUSBP_LDFLAGS}" "${TINYXML2_LDFLAGS}")

configure_file (
  "p-load.rc.in"
  "p-load.rc"
)

configure_file (
  "version.h.in"
  "version.h"
)

install(TARGETS p-load DESTINATION bin)
