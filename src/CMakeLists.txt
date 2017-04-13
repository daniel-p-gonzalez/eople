file(GLOB eople_SOURCES *.cpp)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -m64 -O3 -pthread -std=c++11")
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

add_executable(eople ${eople_SOURCES})
include_directories(${CURL_INCLUDE_DIRS})
target_link_libraries(eople -lreadline ${CURL_LIBRARIES})

install(TARGETS eople DESTINATION /usr/local/bin)
# install(FILES ../icon/eople.png DESTINATION /usr/share/icons)
