cmake_minimum_required(VERSION 3.16)
project(E2Eye)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# SPEED: Persistent download cache
if(NOT FETCHCONTENT_BASE_DIR)
    set(FETCHCONTENT_BASE_DIR "$ENV{HOME}/.cache/e2eye/fetchcontent" CACHE PATH "Directory to keep downloaded dependencies")
endif()
set(FETCHCONTENT_UPDATES_DISCONNECTED ON CACHE BOOL "Disable network updates" FORCE)

# Simple build flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2 -DNDEBUG")
if(NOT MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pipe")
endif()

# Find packages
find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)

# Find libsodium
pkg_check_modules(LIBSODIUM REQUIRED libsodium)

# Download httplib
include(FetchContent)
FetchContent_Declare(
    httplib
    GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
    GIT_TAG v0.14.3
)
FetchContent_MakeAvailable(httplib)

# Find system FTXUI
find_package(ftxui QUIET COMPONENTS component screen dom)
if(ftxui_FOUND)
    message(STATUS "Found system FTXUI in ${ftxui_DIR}")
else()
    message(STATUS "System FTXUI not found, downloading...")
    include(FetchContent)
    FetchContent_Declare(
        ftxui
        GIT_REPOSITORY https://github.com/ArthurSonzogni/FTXUI.git
        GIT_TAG v5.0.0
    )
    FetchContent_MakeAvailable(ftxui)
endif()

# Include directories
include_directories(${LIBSODIUM_INCLUDE_DIRS})
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${httplib_SOURCE_DIR})  
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/common)

# Server executable
add_executable(server 
    server/main.cpp
    common/user_manager.cpp
    common/encryption.cpp
)

target_link_libraries(server
    ${LIBSODIUM_LIBRARIES}
    Threads::Threads
    OpenSSL::SSL
    OpenSSL::Crypto
    httplib::httplib
)

# Client executable
add_executable(client
    client/main.cpp
)

target_link_libraries(client
    ftxui::component
    ftxui::screen
    ftxui::dom
    ${LIBSODIUM_LIBRARIES}
    Threads::Threads
    OpenSSL::SSL
    OpenSSL::Crypto
    httplib::httplib
)

# Enable ccache
find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "${CCACHE_PROGRAM}")
    message(STATUS "ccache enabled")
endif()