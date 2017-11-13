project(cdc_connector)
cmake_minimum_required(VERSION 2.8)
set(VERSION "1.0.0")

include(GNUInstallDirs)

set(CMAKE_BUILD_TYPE "RelWithDebInfo")
set(CMAKE_CXX_FLAGS "-fPIC -std=c++0x")
set(CMAKE_CXX_FLAGS_DEBUG "-g")
set(CMAKE_CXX_FLAGS_RELEASE "-O2")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-g -O2")

if (DEFINED JANSSON_INCLUDE_DIR)
  include_directories(${JANSSON_INCLUDE_DIR})
endif()

# Shared version of the library
add_library(cdc_connector SHARED cdc_connector.cpp)
target_link_libraries(cdc_connector jansson crypto)
set_target_properties(cdc_connector PROPERTIES VERSION "1.0.0")

# Static version of the library
add_library(cdc_connector_static STATIC cdc_connector.cpp)
set_target_properties(cdc_connector_static PROPERTIES OUTPUT_NAME cdc_connector)

install(TARGETS cdc_connector DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(TARGETS cdc_connector_static DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(FILES cdc_connector.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

#
# Packaging
#

option(RPM "Build an RPM" OFF)

if (RPM)
  SET(CPACK_GENERATOR "RPM")
  SET(CPACK_RPM_PACKAGE_VERSION ${VERSION})
  SET(CPACK_RPM_PACKAGE_RELEASE "1")
  SET(CPACK_RPM_PACKAGE_NAME "maxscale-cdc-connector")
  SET(ENGINE_ARCH "x86_64")
  SET(CPACK_PACKAGE_FILE_NAME "${CPACK_RPM_PACKAGE_NAME}-${CPACK_RPM_PACKAGE_VERSION}-${CPACK_RPM_PACKAGE_RELEASE}-${ENGINE_ARCH}")
  if (PACKAGE_SUFFIX)
    SET(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_FILE_NAME}-${PACKAGE_SUFFIX}")
  endif()
  include(CPack)
endif()

option(DEB "Build a DEB" OFF)

if (DEB)
  SET(CPACK_GENERATOR "DEB")
  SET(CPACK_DEBIAN_PACKAGE_VERSION ${VERSION})
  SET(CPACK_DEBIAN_PACKAGE_RELEASE "1")
  SET(CPACK_DEBIAN_PACKAGE_NAME "maxscale-cdc-connector")
  SET(CPACK_PACKAGE_CONTACT "MariaDB Corporation")
  SET(ENGINE_ARCH "x86_64")
  SET(CPACK_PACKAGE_FILE_NAME "${CPACK_DEBIAN_PACKAGE_NAME}-${CPACK_DEBIAN_PACKAGE_VERSION}-${CPACK_DEBIAN_PACKAGE_RELEASE}-${ENGINE_ARCH}")
  if (PACKAGE_SUFFIX)
    SET(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_FILE_NAME}-${PACKAGE_SUFFIX}")
  endif()
  include(CPack)
endif()
