cmake_minimum_required(VERSION 3.16)
project(servd VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ============================================================
# Dependencies via CPM
# ============================================================
include(cmake/CPM.cmake)

find_package(Botan REQUIRED)

# ============================================================
# Library
# ============================================================
add_library(servd_core
    src/Server.cpp
    src/UringEngine/EngineCore.cpp
    src/UringEngine/IoUring.cpp
    src/UringEngine/IoUringMsg.cpp
    src/UringEngine/SessionBroadcast.cpp
    src/UringEngine/FrameIO.cpp
    src/UringEngine/ClientHandlers.cpp
    src/UringEngine/CommandRoute.cpp
    src/UringEngine/EncryptedHandler.cpp
    src/UringEngine/ConnectionImpl.cpp
    src/UringEngine/UdpConnection.cpp
    Logger/Logger.cpp
)

target_include_directories(servd_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/Logger
)

target_link_libraries(servd_core PUBLIC Botan::Botan uring)

# ============================================================
# Example executable
# ============================================================
add_executable(servd app/main.cpp)
target_link_libraries(servd PRIVATE servd_core)

# ============================================================
# Install rules
# ============================================================
include(GNUInstallDirs)

install(TARGETS servd_core
    EXPORT servdTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(DIRECTORY include/servd
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(EXPORT servdTargets
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/servd
    NAMESPACE servd::
)

# ============================================================
# CMake config file for find_package(servd)
# ============================================================
include(CMakePackageConfigHelpers)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/servdConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/servdConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/servdConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/servd
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/servdConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/servdConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/servd
)
