cmake_minimum_required( VERSION 2.8 )

project( JsonCpp )

if( NOT CMAKE_RUNTIME_OUTPUT_DIRECTORY )
  set( CMAKE_RUNTIME_OUTPUT_DIRECTORY ${JsonCpp_BINARY_DIR}/bin )
endif()
if( NOT CMAKE_LIBRARY_OUTPUT_DIRECTORY )
  set( CMAKE_LIBRARY_OUTPUT_DIRECTORY ${JsonCpp_BINARY_DIR}/lib )
endif()
if( NOT CMAKE_ARCHIVE_OUTPUT_DIRECTORY )
  set( CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${JsonCpp_BINARY_DIR}/lib )
endif()

include_directories( include )

add_subdirectory( include )
add_subdirectory( src )

add_subdirectory( cmake )
