file(GLOB eople_SOURCES *.cpp)

if(CMAKE_COMPILER_IS_GNUCC)
  if(CMAKE_SIZEOF_VOID_P EQUAL 8)
  	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -m64 -O3 -pthread -std=c++11")
  else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -pthread -std=c++11")
  endif()
endif()

find_package(GTK2 2.6 REQUIRED gtk)
if(GTK2_FOUND)
  include_directories(${GTK2_INCLUDE_DIRS})
  add_executable(eople ${eople_SOURCES})
  target_link_libraries(eople ${GTK2_LIBRARIES})
endif()

install(TARGETS eople DESTINATION /usr/local/bin)
install(FILES ../Icon/eople.png DESTINATION /usr/share/icons)
