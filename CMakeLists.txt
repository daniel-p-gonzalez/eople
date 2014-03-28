cmake_minimum_required(VERSION 2.6)

set(CMAKE_INSTALL_PREFIX "/usr")
project(eople)

set(VERSION_MAJOR "0")
set(VERSION_MINOR "2")
set(VERSION_PATCH "0")
set(VERSION "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}")

set(CMAKE_BUILD_TYPE "Release")

include_directories("${eople_SOURCE_DIR}/Inc")
add_subdirectory(Src)
