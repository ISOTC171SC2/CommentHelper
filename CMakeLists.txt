cmake_minimum_required(VERSION 3.0.0)
project(CommentHelper VERSION 1.0.0.30 
                DESCRIPTION "Convert between the ISO Comment Template and GitHub issues"
                LANGUAGES CXX)

# enable these lines to turn on testing
# include(CTest)
# enable_testing()

# Get the git version, so we can use it in the app version
find_package(Git QUIET)
if(GIT_FOUND AND EXISTS "${PROJECT_SOURCE_DIR}/.git")
    execute_process(COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
                    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
                    OUTPUT_VARIABLE PACKAGE_GIT_VERSION
                    ERROR_QUIET
                    OUTPUT_STRIP_TRAILING_WHITESPACE)
#    message(STATUS "${PACKAGE_GIT_VERSION}")
endif()

# sets the version into a header file & sets the dir for includes
configure_file(CommentHelperVers.h.in "${PROJECT_SOURCE_DIR}/src/CommentHelperVers.h")

# setup the sources
file(GLOB app_headers "${PROJECT_SOURCE_DIR}/src/*.h")
file(GLOB app_source "${PROJECT_SOURCE_DIR}/src/*.cpp")

# set some other variables we'll be using
# set(jws_headers "${CMAKE_CURRENT_SOURCE_DIR}/jws-jwt-cpp/include")

# sets the name of the executable and its main
# this *MUST* be early in the makefile
add_executable(CommentHelper ${app_headers} ${app_source})

# specify the C++ standard
# and define _DEBUG in debug mode...
target_compile_features(CommentHelper PRIVATE cxx_std_11)
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -D_DEBUG=1")

# add include directories
target_include_directories(CommentHelper 
                            PUBLIC
                                "${PROJECT_SOURCE_DIR}/src"
                                "${PROJECT_BINARY_DIR}"
                            PRIVATE
#                                "${jws_headers}"
                            )

# Add boost
set(Boost_USE_STATIC_LIBS ON) 
set(Boost_USE_MULTITHREADED ON)  
set(Boost_USE_STATIC_RUNTIME OFF) 
find_package(Boost 1.45.0 REQUIRED COMPONENTS program_options filesystem regex iostreams log) 
if(Boost_FOUND)
    target_include_directories(CommentHelper PRIVATE "${Boost_INCLUDE_DIRS}") 
    target_link_libraries(CommentHelper "${Boost_LIBRARIES}")
else()
    message(ERROR ": Boost not found")
endif()

# add ZLib
find_package(ZLIB)
if(ZLIB_FOUND)
    target_link_libraries(CommentHelper "${ZLIB_LIBRARIES}")
else()
    message(ERROR ": Zlib not found")
endif()

# add platform-specific stuff
if(APPLE)   # carbon & security - on Mac
    find_library(CARBON NAMES Carbon)
    target_link_libraries(CommentHelper "${CARBON}")
elseif(UNIX)    # extra unix-centric libraries
    if(CMAKE_DL_LIBS)
        target_link_libraries(CommentHelper ${CMAKE_DL_LIBS})
    else()
        message(ERROR ": DL not found")
    endif()

    find_package ( Threads REQUIRED )
    if ( Threads_FOUND )
        target_link_libraries ( CommentHelper ${CMAKE_THREAD_LIBS_INIT} )
    else()
        message(ERROR ": pthreads not found")
    endif()
endif()
