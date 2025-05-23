cmake_minimum_required(VERSION 3.16)

# Set policy for Boost to avoid warnings
if(POLICY CMP0167)
    cmake_policy(SET CMP0167 NEW)
endif()

project(CFPing)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
find_package(Qt5 REQUIRED COMPONENTS Core Widgets Gui Network)

# Handle Boost differently for vcpkg and system installation
if(DEFINED ENV{VCPKG_ROOT} OR EXISTS "${CMAKE_SOURCE_DIR}/vcpkg")
    # Using vcpkg - use CONFIG mode to avoid FindBoost module
    find_package(Boost REQUIRED COMPONENTS system CONFIG)
    # For vcpkg, also find asio (it might be header-only)
    find_path(BOOST_ASIO_INCLUDE_DIR boost/asio.hpp 
              PATHS ${CMAKE_PREFIX_PATH}/include 
                    ${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/installed/x64-windows/include
              NO_DEFAULT_PATH)
    
    if(NOT BOOST_ASIO_INCLUDE_DIR)
        message(FATAL_ERROR "Boost.Asio not found! Please install boost-asio via vcpkg")
    endif()
    
    # For vcpkg, use target-based linking
    set(BOOST_LIBRARIES Boost::system)
    
    # Debug: Print Boost information
    message(STATUS "Using vcpkg Boost")
    message(STATUS "Boost_FOUND: ${Boost_FOUND}")
    message(STATUS "Boost_VERSION: ${Boost_VERSION}")
    message(STATUS "Boost.Asio include dir: ${BOOST_ASIO_INCLUDE_DIR}")
    if(TARGET Boost::system)
        get_target_property(BOOST_INCLUDE_DIRS Boost::system INTERFACE_INCLUDE_DIRECTORIES)
        message(STATUS "Boost include dirs: ${BOOST_INCLUDE_DIRS}")
    endif()
else()
    # System installation
    find_package(Boost REQUIRED COMPONENTS system)
    set(BOOST_LIBRARIES ${Boost_LIBRARIES})
    include_directories(${Boost_INCLUDE_DIRS})
endif()

# Enable Qt5 MOC
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# Add compiler flags for coroutines
if(MINGW)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fcoroutines")
endif()

# Source files
set(SOURCES
    src/main.cpp
    src/mainwindow.cpp
    src/pingworker.cpp
    src/iputils.cpp
    src/cidrexpander.cpp
    src/pingresultmodel.cpp
    src/logmodel.cpp
)

set(HEADERS
    src/mainwindow.h
    src/pingworker.h
    src/iputils.h
    src/cidrexpander.h
    src/pingresultmodel.h
    src/logmodel.h
)

# Create executable
add_executable(CFPing ${SOURCES} ${HEADERS})

# Link libraries - use target_link_libraries with proper targets
target_link_libraries(CFPing 
    Qt5::Core 
    Qt5::Widgets 
    Qt5::Gui
    Qt5::Network
)

# Link Boost properly
if(TARGET Boost::system)
    target_link_libraries(CFPing Boost::system)
else()
    target_link_libraries(CFPing ${BOOST_LIBRARIES})
    target_include_directories(CFPing PRIVATE ${Boost_INCLUDE_DIRS})
endif()

# Windows specific settings
if(WIN32)
    target_link_libraries(CFPing ws2_32 iphlpapi)
    # Set subsystem to windows for MinGW
    if(MINGW)
        set_target_properties(CFPing PROPERTIES
            WIN32_EXECUTABLE TRUE
            LINK_FLAGS "-Wl,--subsystem,windows"
        )
    else()
        # For MSVC
        set_target_properties(CFPing PROPERTIES
            WIN32_EXECUTABLE TRUE
            LINK_FLAGS "/SUBSYSTEM:WINDOWS"
        )
    endif()
endif()

# Install target
install(TARGETS CFPing
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

# Install Qt5 libraries (Windows)
if(WIN32)
    # Install Qt5 DLLs
    get_target_property(QT_QMAKE_EXECUTABLE Qt5::qmake IMPORTED_LOCATION)
    get_filename_component(QT_WINDEPLOYQT_EXECUTABLE ${QT_QMAKE_EXECUTABLE} PATH)
    set(QT_WINDEPLOYQT_EXECUTABLE "${QT_WINDEPLOYQT_EXECUTABLE}/windeployqt.exe")

    install(CODE "
        execute_process(COMMAND ${QT_WINDEPLOYQT_EXECUTABLE} \${CMAKE_INSTALL_PREFIX}/bin/CFPing.exe --dir \${CMAKE_INSTALL_PREFIX}/bin)
    ")
endif()

# CPack configuration
set(CPACK_PACKAGE_NAME "CFPing")
set(CPACK_PACKAGE_VENDOR "shuangtuanmi")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "CF IP Ping Tool")
set(CPACK_PACKAGE_VERSION_MAJOR "1")
set(CPACK_PACKAGE_VERSION_MINOR "0")
set(CPACK_PACKAGE_VERSION_PATCH "0")
set(CPACK_PACKAGE_VERSION "${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}.${CPACK_PACKAGE_VERSION_PATCH}")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "CFPing")

# Windows NSIS specific
if(WIN32)
    set(CPACK_GENERATOR "NSIS")
    set(CPACK_NSIS_PACKAGE_NAME "CFPing")
    set(CPACK_NSIS_DISPLAY_NAME "CFPing - CF IP Ping Tool")
    set(CPACK_NSIS_HELP_LINK "https://github.com/shuangtuanmi/cfping")
    set(CPACK_NSIS_URL_INFO_ABOUT "https://github.com/shuangtuanmi/cfping")
    set(CPACK_NSIS_CONTACT "your.email@example.com")
    set(CPACK_NSIS_MODIFY_PATH ON)
    set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
    
    # Create desktop shortcut
    set(CPACK_NSIS_CREATE_ICONS_EXTRA
        "CreateShortCut '$DESKTOP\\\\CFPing.lnk' '$INSTDIR\\\\bin\\\\CFPing.exe'"
    )
    set(CPACK_NSIS_DELETE_ICONS_EXTRA
        "Delete '$DESKTOP\\\\CFPing.lnk'"
    )
    
    # Start menu shortcuts
    set(CPACK_NSIS_MENU_LINKS
        "bin/CFPing.exe" "CFPing"
    )
endif()

# Linux DEB specific
if(UNIX AND NOT APPLE)
    set(CPACK_GENERATOR "DEB;TGZ")
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "your.email@example.com")
    set(CPACK_DEBIAN_PACKAGE_DEPENDS "libqt5widgets5, libqt5network5, libboost-system1.74.0")
endif()

# macOS specific
if(APPLE)
    set(CPACK_GENERATOR "DragNDrop")
    set(CPACK_DMG_FORMAT "UDBZ")
    set(CPACK_DMG_VOLUME_NAME "CFPing")
endif()

include(CPack)
