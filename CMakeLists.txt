project(cdc_connector)
cmake_minimum_required(VERSION 2.8)

include(GNUInstallDirs)

set(CMAKE_BUILD_TYPE "RelWithDebInfo")
set(CMAKE_CXX_FLAGS "-fPIC -std=c++0x")
set(CMAKE_CXX_FLAGS_DEBUG "-g")
set(CMAKE_CXX_FLAGS_RELEASE "-O2")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-g -O2")

if (DEFINED JANSSON_INCLUDE_DIR)
  include_directories(${JANSSON_INCLUDE_DIR})
endif()

add_library(cdc_connector STATIC cdc_connector.cpp)

install(TARGETS cdc_connector DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(FILES cdc_connector.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
