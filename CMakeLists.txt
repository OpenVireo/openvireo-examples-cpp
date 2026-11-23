cmake_minimum_required(VERSION 3.21)
project(openvireo-examples-cpp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# libovcore: the prebuilt CMake package from the core's GitHub release (cmake/OpenVireoCore.cmake), or
# your own with -DOpenVireoCore_DIR=<pkg>/lib/cmake/OpenVireoCore
include(cmake/OpenVireoCore.cmake)
find_package(OpenVireoCore 1.0 REQUIRED)

# The examples' HTTP transport is libcurl
find_package(CURL REQUIRED)
find_package(Threads REQUIRED)

add_executable(examples
    src/main.cpp
    src/host.cpp
    src/common.cpp
    src/first_program.cpp
    src/hosted_account.cpp
    src/vaults_and_sharing.cpp
    src/events.cpp
    src/offline.cpp
    src/move_to_account.cpp
    src/recovery.cpp
    src/self_hosted_sync.cpp
    src/service_accounts.cpp
    src/share_links.cpp)
target_link_libraries(examples PRIVATE OpenVireoCore::ovcore CURL::libcurl Threads::Threads)
if(MSVC)
    target_compile_options(examples PRIVATE /W4 /WX)
else()
    target_compile_options(examples PRIVATE -Wall -Wextra -Werror)
endif()
