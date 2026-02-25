cmake_minimum_required(VERSION 3.16)
project(E2Eye)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Simple build flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2 -DNDEBUG")

# Find packages
find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)  # ADD THIS for ssl/crypto

# Find libsodium
pkg_check_modules(LIBSODIUM REQUIRED libsodium)

# Download httplib (FIX: Add this!)
include(FetchContent)
FetchContent_Declare(
    httplib
    GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
    GIT_TAG v0.14.3
)
FetchContent_MakeAvailable(httplib)

# Download FTXUI
include(FetchContent)
FetchContent_Declare(
    ftxui
    GIT_REPOSITORY https://github.com/ArthurSonzogni/FTXUI.git
    GIT_TAG v5.0.0
)
FetchContent_MakeAvailable(ftxui)

# Include directories
include_directories(${LIBSODIUM_INCLUDE_DIRS})
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${httplib_SOURCE_DIR})  # ADD THIS for httplib
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
    OpenSSL::SSL      # FIX: Use OpenSSL::SSL instead of ssl
    OpenSSL::Crypto   # FIX: Use OpenSSL::Crypto instead of crypto
    httplib::httplib
)

# Client executable
add_executable(client
    client/main.cpp
    common/user_manager.cpp
    common/encryption.cpp
)

target_link_libraries(client
    ftxui::component
    ftxui::screen
    ftxui::dom
    ${LIBSODIUM_LIBRARIES}
    Threads::Threads
    OpenSSL::SSL      # FIX: Use OpenSSL::SSL instead of ssl
    OpenSSL::Crypto   # FIX: Use OpenSSL::Crypto instead of crypto
    httplib::httplib
)
