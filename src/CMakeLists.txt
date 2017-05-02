cmake_minimum_required(VERSION 2.8.8)

file(GLOB eople_SOURCES *.cpp)
list(REMOVE_ITEM eople_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/eople_tests.cpp)
list(REMOVE_ITEM eople_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/eople_eve.cpp)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -m64 -O3 -pthread -std=c++11 -Wfatal-errors")
  else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -pthread -std=c++11")
  endif()
endif()

find_package(CURL)
if(CURL_FOUND)
  message(STATUS "Curl libraries found at: ${CURL_LIBRARIES}")
  message(STATUS "Curl includes found at: ${CURL_INCLUDE_DIRS}")
else()
  message(SEND_ERROR "Missing cURL dependency.")
endif(CURL_FOUND)

add_library(eople_objs OBJECT ${eople_SOURCES})

add_executable(eople ${CMAKE_CURRENT_SOURCE_DIR}/eople_eve.cpp $<TARGET_OBJECTS:eople_objs>)
include_directories(${CURL_INCLUDE_DIRS})
target_link_libraries(eople -lreadline ${CURL_LIBRARIES})

add_executable(tests ${CMAKE_CURRENT_SOURCE_DIR}/eople_tests.cpp $<TARGET_OBJECTS:eople_objs>)
target_link_libraries(tests -lreadline ${CURL_LIBRARIES})

install(TARGETS eople DESTINATION /usr/local/bin)
# install(FILES ../icon/eople.png DESTINATION /usr/share/icons)
