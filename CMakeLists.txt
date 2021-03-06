cmake_minimum_required (VERSION 2.8)

option (ATTIC_STATIC_LIB "Build Attic Static lib" ON)
option (ATTIC_BINARY "Build Attic Binary test executable" OFF)
option (DEBUG "Debug Build" ON)

# Set compilers to use
set (CMAKE_CXX_COMPILER clang++)
#set (CMAKE_CXX_COMPILER g++-4.8)
set (CMAKE_C_COMPILER clang)
#set (CMAKE_C_COMPILER gcc-4.8)

project(libattic)
if(ATTIC_STATIC_LIB)
    message(STATUS "Bulding Attic Static library")
    if(DEBUG)
        set(CMAKE_CXX_FLAGS "-w -g3 -ggdb")# -Wall")
    else(DEBUG)
        set(CMAKE_CXX_FLAGS "-w")
    endif(DEBUG)
endif(ATTIC_STATIC_LIB)

if(ATTIC_BINARY)
    message(STATUS "Bulding Attic Test Binary")
    if(DEBUG)
        #set(CMAKE_C_FLAGS "-fPIC")
        set(CMAKE_CXX_FLAGS "-g3 -ggdb -Wall") # -Wextra")# -fsanitize=memory")
    else(DEBUG)
        set(CMAKE_CXX_FLAGS "-w")
    endif(DEBUG)
endif(ATTIC_BINARY)

# Build dependencies
# Build gtest libs
add_subdirectory("vendor/jsoncpp")
add_subdirectory("vendor/cryptopp")

set(CMAKE_MODULE_PATH ${libattic_SOURCE_DIR}/cmake)

if(ATTIC_BINARY)
    message(STATUS " including googletest includes")
    add_subdirectory("vendor/googletest")
endif(ATTIC_BINARY)

# Set additional include directories
if(ATTIC_BINARY)
    message(STATUS " including googletest includes")
    include_directories(vendor/googletest/include)
endif(ATTIC_BINARY)

include_directories(src)
include_directories(vendor/cryptopp)
include_directories(vendor/jsoncpp/include)
include_directories(vendor/sqlite)
include_directories(vendor/scrypt)
include_directories(vendor/base64)
include_directories(vendor/miniz)

if(ATTIC_BINARY)
    # This is where binary files will go
    file(MAKE_DIRECTORY bin)
    set (EXECUTABLE_OUTPUT_PATH bin)
endif(ATTIC_BINARY)

if(ATTIC_STATIC_LIB)
    file(MAKE_DIRECTORY lib)
    set(LIBRARY_OUTPUT_PATH lib)
    file(MAKE_DIRECTORY include)
    file(COPY src/libattic.h DESTINATION include)
endif(ATTIC_STATIC_LIB)


file(GLOB_RECURSE libattic_SOURCE
    "src/*.cpp"
    "src/*.c"
    "vendor/sqlite/*.c"
    "vendor/scrypt/*.c"
    #    "vendor/miniz/*.c"
    "vendor/base64/*.cpp")

if(ATTIC_STATIC_LIB)
    file(GLOB MAIN_CPP "src/main.cpp")
    LIST(REMOVE_ITEM libattic_SOURCE ${MAIN_CPP})
endif(ATTIC_STATIC_LIB)

set(libattic_INCLUDE_DIRS "")

foreach (_headerFile ${libattic_HEADERS})
    get_filename_component(_dir ${_headerFile} PATH)
    list (APPEND libattic_INCLUDE_DIRS ${_dir})
endforeach()

list(REMOVE_DUPLICATES libattic_INCLUDE_DIRS)

# Include our files
include_directories(${libattic_INCLUDE_DIRS})

# Add gtest lib to statically link
#add_library(imp_gtest STATIC IMPORTED)
#set_property(TARGET imp_gtest PROPERTY IMPORTED_LOCATION ${gtest_lib})

# Boost
include(ExternalProject)
ExternalProject_Add(boost
  URL "http://sourceforge.net/projects/boost/files/boost/1.53.0/boost_1_53_0.tar.bz2/download"
  BUILD_IN_SOURCE 1
  CONFIGURE_COMMAND <SOURCE_DIR>/bootstrap.sh
  BUILD_COMMAND <SOURCE_DIR>/b2 install -j4 link=static --with-thread --with-system --with-date_time --with-filesystem --with-timer --with-chrono --prefix=<INSTALL_DIR>
  INSTALL_COMMAND ""
#--with-test --with-regex --with-date_time --with-filesystem --with-program_options 
)

ExternalProject_Get_Property(boost INSTALL_DIR)
set(BOOST_DIR ${INSTALL_DIR})
include_directories(${BOOST_DIR}/include)

add_library(boost_thread STATIC IMPORTED)
set_property(TARGET boost_thread PROPERTY IMPORTED_LOCATION ${BOOST_DIR}/lib/libboost_thread.a)

add_library(boost_system STATIC IMPORTED)
set_property(TARGET boost_system PROPERTY IMPORTED_LOCATION ${BOOST_DIR}/lib/libboost_system.a)

add_library(boost_filesystem STATIC IMPORTED)
set_property(TARGET boost_filesystem PROPERTY IMPORTED_LOCATION ${BOOST_DIR}/lib/libboost_filesystem.a)

add_library(boost_datetime STATIC IMPORTED)
set_property(TARGET boost_datetime PROPERTY IMPORTED_LOCATION ${BOOST_DIR}/lib/libboost_date_time.a)

add_library(boost_timer STATIC IMPORTED)
set_property(TARGET boost_timer PROPERTY IMPORTED_LOCATION ${BOOST_DIR}/lib/libboost_timer.a)

add_library(boost_chrono STATIC IMPORTED)
set_property(TARGET boost_chrono PROPERTY IMPORTED_LOCATION ${BOOST_DIR}/lib/libboost_chrono.a)

# libsodium
set (nacl_ver 0.4.1)
ExternalProject_Add(libsodium
    URL http://download.dnscrypt.org/libsodium/releases/libsodium-${nacl_ver}.tar.gz 
    BUILD_IN_SOURCE 1
    CONFIGURE_COMMAND <SOURCE_DIR>/autogen.sh
    BUILD_COMMAND <SOURCE_DIR>/configure --prefix=<INSTALL_DIR> 
    INSTALL_COMMAND make -j4
)

ExternalProject_Get_Property(libsodium INSTALL_DIR)
set(NACL_DIR ${INSTALL_DIR})
include_directories(${NACL_DIR}/src/libsodium/src/libsodium/include)

add_library(nacl_lib STATIC IMPORTED)
set_property(TARGET nacl_lib PROPERTY IMPORTED_LOCATION ${NACL_DIR}/src/libsodium/src/libsodium/.libs/libsodium.a)

# libb64
#ExternalProject_Add(libb64
#    URL http://downloads.sourceforge.net/project/libb64/libb64/libb64/libb64-1.2.src.zip
#    #DOWNLOAD_COMMAND ""
##SOURCE_DIR "vendor/libb64-1.2"
#   BUILD_IN_SOURCE 1
#   UPDATE_COMMAND ""
#    CONFIGURE_COMMAND ""
#    BUILD_COMMAND ""
#    INSTALL_COMMAND make -j4
#    )

#ExternalProject_Get_Property(libb64 INSTALL_DIR)
#set(B64_DIR ${INSTALL_DIR})
#message(STATUS " b64 install dir " )
#message(STATUS ${B64_DIR})

#include_directories(${B64_DIR}/src/libb64/include)

#add_library(libb64_lib STATIC IMPORTED)
#set_property(TARGET libb64_lib PROPERTY IMPORTED_LOCATION ${B64_DIR}/src/libb64/src/libb64.a)


if(ATTIC_STATIC_LIB)
    add_library(attic ${libattic_SOURCE} $<TARGET_OBJECTS:cryptlib> $<TARGET_OBJECTS:json> )
    #link lib
    target_link_libraries(attic
                          boost_thread
                          boost_system
                          boost_filesystem
                          boost_datetime
                          boost_timer
                          boost_chrono
                          nacl_lib
                          #libb64_lib
                          pthread
                          ssl)


    include(mergestaticlibs)
    merge_static_libs(atticlib 
                      attic 
                      boost_thread 
                      boost_system 
                      boost_filesystem 
                      boost_datetime 
                      boost_timer 
                      boost_chrono 
                      nacl_lib)
                      #                      libb64_lib)
endif(ATTIC_STATIC_LIB)

if(ATTIC_BINARY)
    add_executable(attic ${libattic_SOURCE} $<TARGET_OBJECTS:cryptlib> $<TARGET_OBJECTS:json>)
    #link libs
    target_link_libraries(attic
                          boost_thread
                          boost_system
                          boost_filesystem
                          boost_datetime
                          boost_timer
                          boost_chrono
                          pthread
                          crypto
                          nacl_lib
                          #libb64_lib
                          dl
                          ssl
                          rt
                          gtest) #pulls gtest from google subdirectory

endif(ATTIC_BINARY)

add_dependencies(attic boost)
