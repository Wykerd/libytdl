FIND_PATH(UV_INCLUDE_DIR NAMES uv.h)
FIND_LIBRARY(UV_LIBRARIES NAMES uv libuv)

if(WIN32)
  list(APPEND UV_LIBRARIES iphlpapi)
  list(APPEND UV_LIBRARIES psapi)
  list(APPEND UV_LIBRARIES userenv)
  list(APPEND UV_LIBRARIES ws2_32)
endif()

INCLUDE(FindPackageHandleStandardArgs)
find_package_handle_standard_args(UV DEFAULT_MSG UV_LIBRARIES UV_INCLUDE_DIR)
