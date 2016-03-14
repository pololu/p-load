# Build tinyxml2 as a static library.

use_cxx11()

add_library (tinyxml2 STATIC
  tinyxml2.cpp
)

include_directories (
  "${CMAKE_CURRENT_SOURCE_DIR}"
)


